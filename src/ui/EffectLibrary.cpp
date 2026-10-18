// Effect editor library page and edit sessions: browse effects / blends / brushes
// with a search, start from a template (a copy), open the original (user items, or
// protected ones after an explicit unlock), and cancel an edit to get back to the
// state and the panel from before.
#include "ui/CodeEdit.h"
#include "ui/App.h"
#include "ui/Theme.h"
#include "doc/Effects.h"
#include "doc/Brushes.h"
#include "doc/EffectGraph.h"
#include "gpu/ShaderSystem.h"

#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace sx {

namespace {
std::string LowerStr(std::string s) {
    for (auto& c : s) c = (char)tolower((unsigned char)c);
    return s;
}
std::string FileText(const std::string& path) {
    std::ifstream f(Utf8Path(path), std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}
}  // namespace

void App::BeginEditSession(const char* prevWindow) {
    DocView* v = Active();
    auto& S = efEd.session;
    S = {};
    S.active = v != nullptr;
    if (v) S.before = v->doc->S();
    S.brushBefore = brush;
    S.eraserBefore = eraser;
    S.prevWindow = prevWindow ? prevWindow : "";
    S.prevSideTab = sideTab;
    S.prevHideBottom = hideBottom;
    efEd.browse = false;
}

void App::EndEditSession(bool cancel) {
    DocView* v = Active();
    auto& S = efEd.session;
    if (cancel && S.active && v) {
        // Back to the document state from before the edit (undoable).
        DocState cur = v->doc->S();
        v->doc->S() = S.before;
        v->doc->InvalidateSurfaces();
        v->history.PushState("Annuler l'édition", cur, v->doc->S());
        brush = S.brushBefore;
        eraser = S.eraserBefore;
        efEd.cachedJson.clear();
        Status("Modifications annulées");
    }
    // Back to the panel the editor was opened from.
    if (!S.prevWindow.empty()) {
        ShowSideTab(S.prevSideTab);
        ImGui::SetWindowFocus(S.prevWindow.c_str());
    }
    hideBottom = S.prevHideBottom;
    S = {};
}

std::string App::LibraryTextOf(const EffectInstance& fx) {
    if (!fx.graph.empty()) {
        EffectGraph g;
        if (!EffectGraph::Deserialize(fx.graph, g)) return "";
        EGCompileResult cr = CompileEffectGraph(g);
        return "//@graph " + g.ToJson().Dump(0) + "\n" + cr.code;
    }
    return fx.customCode;
}

bool App::SaveOriginal(std::string* err) {
    // Overwrites the library file the session was opened from (lock bypassed explicitly).
    auto& S = efEd.session;
    DocView* v = Active();
    if (S.libId.empty() || !v) return false;
    std::string text;
    if (S.libKind == 2) {
        if (!brush.tipGraph.empty()) {
            EffectInstance fx;
            fx.graph = brush.tipGraph;
            text = LibraryTextOf(fx);
        } else {
            text = brush.tipCode;
        }
    } else {
        EffectInstance* fx = S.libKind == 1 ? (v->doc->ActiveLayer() ? &v->doc->ActiveLayer()->blendFx : nullptr) : EditedEffect(*v, nullptr);
        if (!fx) { if (err) *err = "rien à enregistrer"; return false; }
        text = LibraryTextOf(*fx);
    }
    if (text.empty()) { if (err) *err = "le graphe ne compile pas"; return false; }
    std::string dir = S.libKind == 2 ? Brushes().Dir() : gpu::Shaders().ShaderDir() + "/effects";
    std::string path = dir + "/" + S.libId + ".slang";
    // Keep the original metadata (name, category, user / blend flags), the code is replaced.
    std::string old = FileText(path), meta;
    {
        std::istringstream in(old);
        for (std::string line; std::getline(in, line);)
            if (line.rfind("//@name", 0) == 0 || line.rfind("//@category", 0) == 0 || line.rfind("//@user", 0) == 0 || line.rfind("//@blend", 0) == 0)
                meta += line + "\n";
    }
    std::string body;
    {
        std::istringstream in(text);
        for (std::string line; std::getline(in, line);)
            if (line.rfind("//@name", 0) != 0 && line.rfind("//@category", 0) != 0 && line.rfind("//@user", 0) != 0 && line.rfind("//@locked", 0) != 0 &&
                line.rfind("//@blend", 0) != 0)
                body += line + "\n";
    }
    // Backup of the previous version next to it.
    std::error_code ec;
    std::filesystem::copy_file(Utf8Path(path), Utf8Path(path + ".bak"), std::filesystem::copy_options::overwrite_existing, ec);
    std::ofstream f(Utf8Path(path), std::ios::binary);
    f << meta << body;
    if (!f) { if (err) *err = "écriture impossible : " + path; return false; }
    f.close();
    if (S.libKind == 2) Brushes().Reload();
    else {
        Effects().Reload();
        fxBrowser.previews.clear();
        blendBrowser.previews.clear();
    }
    Status("Original remplacé dans la bibliothèque : " + S.libId + " (ancienne version : " + S.libId + ".slang.bak)");
    return true;
}

void App::OpenLibraryItem(int kind, const std::string& id, bool original) {
    DocView* v = Active();
    if (!v) { Status("Ouvrez un document pour éditer"); return; }
    Document& d = *v->doc;
    const char* prev = efEd.session.active ? efEd.session.prevWindow.c_str() : nullptr;
    std::string keepPrev = prev ? prev : "";
    if (!efEd.session.active) BeginEditSession(nullptr);
    else efEd.browse = false;
    if (!keepPrev.empty()) efEd.session.prevWindow = keepPrev;
    efEd.session.libId = original ? id : "";
    efEd.session.libKind = kind;
    if (kind == 2) {
        auto bd = Brushes().Find(id);
        if (!bd) return;
        BrushSettings& b = tool == Tool::Eraser ? eraser : brush;
        SelectTip(b, id);
        b.tipId.clear();
        if (bd->def && !bd->def->graph.empty()) b.tipGraph = bd->def->graph;
        else b.tipCode = bd->text;
        efEd.target = 2;
        efEd.cachedJson.clear();
        return;
    }
    auto def = Effects().Find(id);
    if (!def) return;
    EffectInstance fx;
    if (!def->graph.empty()) fx.graph = def->graph;
    else fx.customCode = FileText(gpu::Shaders().ShaderDir() + "/effects/" + id + ".slang");
    fx.params = def->DefaultParams();
    Edit e(d, v->history, original ? "Éditer l'original" : "Nouveau depuis un modèle");
    if (kind == 1) {
        Layer* l = d.ActiveLayer();
        if (!l || d.Mode() != DocMode::Layers) { Status("Sélectionnez un calque pour éditer une fusion"); return; }
        l->blendFx = fx;
        l->passThrough = false;
        efEd.target = 1;
    } else if (d.Mode() == DocMode::Layers) {
        Layer* l = d.ActiveLayer();
        if (l && (l->kind == LayerKind::Pixel || l->kind == LayerKind::Smart || l->kind == LayerKind::Text || l->kind == LayerKind::Shape)) {
            l->filters.push_back(fx);
            efEd.filterIndex = (int)l->filters.size() - 1;
        } else {
            Layer& nl = d.AddEffectLayer(def->inputs.empty() ? LayerKind::Fill : LayerKind::Adjustment, "");
            nl.effect = fx;
            nl.name = def->name;
        }
        efEd.target = 0;
    } else {
        Node& n = d.AddEffectNode("", "", nodeEd.contextPos);
        n.fx = fx;
        n.name = def->name;
        n.inputs.assign(NodeInputCount(n), 0);
        efEd.target = 0;
    }
    d.SyncGraph();
    e.Commit();
    efEd.cachedJson.clear();
}

void App::DrawEffectLibraryPage() {
    auto& ed = efEd;
    ImGui::BeginChild("##libpage", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding);
    // Kind and search.
    for (int k = 0; k < 3; ++k) {
        static const char* kinds[] = {"Effets", "Fusions", "Pinceaux"};
        if (k) ImGui::SameLine();
        if (ImGui::RadioButton(kinds[k], ed.browseKind == k)) { ed.browseKind = k; ed.browseSel.clear(); }
    }
    ImGui::SameLine(0, 24 * dpi);
    ImGui::SetNextItemWidth(260 * dpi);
    ImGui::InputTextWithHint("##libsearch", "Rechercher...", &ed.browseSearch);
    ImGui::SameLine();
    ImGui::Checkbox("Graphes seulement", &ed.browseGraphs);
    ui::HelpTooltip("Seulement les éléments construits avec des nœuds (marqués « graphe »), toutes catégories ouvertes.");
    if (ed.session.active) {
        ImGui::SameLine();
        if (ImGui::Button("Retour à l'édition")) ed.browse = false;
    }
    std::string q = LowerStr(ed.browseSearch);
    // Items: id, name, category, graph?, user?, locked?
    struct Item { std::string id, name, cat; bool graph, user, locked; };
    std::vector<Item> items;
    const LibKind lk = ed.browseKind == 2 ? LibKind::Brush : ed.browseKind == 1 ? LibKind::Blend : LibKind::Effect;
    if (ed.browseKind == 2) {
        for (auto& b : Brushes().All())
            if (b->def) items.push_back({b->id, b->def->name, b->def->category, !b->def->graph.empty(), IsUserLibraryItem(lk, b->id), LibraryItemLocked(lk, b->id)});
    } else {
        for (auto& e : Effects().All())
            if (e->blend == (ed.browseKind == 1))
                items.push_back({e->id, e->name, e->category, !e->graph.empty(), IsUserLibraryItem(lk, e->id), LibraryItemLocked(lk, e->id)});
    }
    std::sort(items.begin(), items.end(), [](auto& a, auto& b) { return a.cat != b.cat ? a.cat < b.cat : a.name < b.name; });
    const float listW = std::max(260 * dpi, ImGui::GetContentRegionAvail().x * 0.35f);
    ImGui::BeginChild("##liblist", ImVec2(listW, 0), ImGuiChildFlags_Borders);
    std::string cat;
    bool open = false;
    for (auto& it : items) {
        if (!q.empty() && LowerStr(it.name + " " + it.cat + " " + it.id).find(q) == std::string::npos) continue;
        if (ed.browseGraphs && !it.graph) continue;
        if (it.cat != cat) {
            if (open) ImGui::TreePop();
            cat = it.cat;
            if (!q.empty() || ed.browseGraphs) ImGui::SetNextItemOpen(true);
            open = ImGui::TreeNodeEx(cat.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth);
        }
        if (!open) continue;
        std::string label = it.name + (it.graph ? "  (graphe)" : "") + (it.user ? "  *" : "") + "##" + it.id;
        if (ImGui::Selectable(label.c_str(), ed.browseSel == it.id, ImGuiSelectableFlags_AllowDoubleClick)) {
            ed.browseSel = it.id;
            ed.unlock = false;
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) OpenLibraryItem(ed.browseKind, it.id, false);
        }
    }
    if (open) ImGui::TreePop();
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("##libinfo", ImVec2(0, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
    const Item* sel = nullptr;
    for (auto& it : items) if (it.id == ed.browseSel) sel = &it;
    if (ImGui::Button("Nouveau vide (graphe)")) {
        if (!ed.session.active) BeginEditSession(nullptr);
        ed.browse = false;
        if (ed.browseKind == 2) {
            brush.tipId.clear();
            brush.tipCode.clear();
            brush.tipGraph = EffectGraph::DefaultBrush().Serialize();
            if (auto nd = TipDef(brush); nd && nd->def) brush.tipParams = nd->def->DefaultParams();
            ed.target = 2;
        } else if (DocView* v = Active()) {
            Document& d = *v->doc;
            Edit e(d, v->history, "Nouvel effet en graphe");
            if (ed.browseKind == 1) {
                if (Layer* l = d.ActiveLayer()) {
                    l->blendFx = {};
                    l->blendFx.graph = EffectGraph::DefaultBlend().Serialize();
                    if (auto def = l->blendFx.Def()) l->blendFx.params = def->DefaultParams();
                    l->passThrough = false;
                }
                ed.target = 1;
            } else if (d.Mode() == DocMode::Layers) {
                Layer& l = d.AddEffectLayer(LayerKind::Adjustment, "");
                l.name = "Effet visuel";
                l.effect.graph = EffectGraph::Default().Serialize();
                ed.target = 0;
            } else {
                Node& n = d.AddEffectNode("", "", nodeEd.contextPos);
                n.fx.graph = EffectGraph::Default().Serialize();
                n.name = "Effet visuel";
                n.inputs.assign(NodeInputCount(n), 0);
                ed.target = 0;
            }
            d.SyncGraph();
            e.Commit();
        }
        ed.cachedJson.clear();
    }
    ImGui::SameLine();
    if (ImGui::Button("Importer du HLSL...")) hlslImportRequest = true;
    DrawHlslImportPopup(nullptr, {}, nullptr);
    ImGui::Separator();
    if (!sel) {
        ImGui::TextWrapped("Choisissez un élément : partez d'une copie (modèle) ou ouvrez l'original pour le modifier. Double-clic : nouveau à partir du modèle.");
    } else {
        ImGui::TextUnformatted(sel->name.c_str());
        ImGui::TextDisabled("%s  -  %s  -  %s", sel->cat.c_str(), sel->graph ? "graphe" : "code HLSL",
                            sel->locked ? (sel->user ? "verrouillé" : "fourni avec Shaderixel (protégé)") : "modifiable");
        ImGui::TextDisabled("Fichier : %s.slang", sel->id.c_str());
        ImGui::Spacing();
        if (ImGui::Button("Nouveau à partir de ce modèle", ImVec2(-1, 0))) OpenLibraryItem(ed.browseKind, sel->id, false);
        ui::HelpTooltip("Copie modifiable appliquée au calque actif (ou au pinceau) ; l'original reste intact.\nEnregistrez-la ensuite sous un nouveau nom.");
        const bool protectedItem = sel->locked;
        if (protectedItem) {
            ImGui::Checkbox("Déverrouiller : modifier l'original", &ed.unlock);
            ui::HelpTooltip("Contourne la protection : « Enregistrer l'original » remplacera le fichier de la bibliothèque\n(une sauvegarde .bak est gardée).");
        }
        ImGui::BeginDisabled(protectedItem && !ed.unlock);
        if (ImGui::Button("Éditer l'original", ImVec2(-1, 0))) OpenLibraryItem(ed.browseKind, sel->id, true);
        ImGui::EndDisabled();
    }
    ImGui::EndChild();
    ImGui::EndChild();
}

void App::DrawEditSessionBar() {
    auto& S = efEd.session;
    if (ImGui::Button("\xE2\x97\x80 Bibliothèque")) efEd.browse = true;
    ui::HelpTooltip("Liste des effets, fusions et pinceaux : modèles, originaux.");
    if (!S.active) return;
    ImGui::SameLine();
    if (ImGui::Button("Annuler les modifications")) { EndEditSession(true); return; }
    ui::HelpTooltip("Revient à l'état d'avant l'ouverture de l'éditeur et au panneau précédent.");
    ImGui::SameLine();
    if (ImGui::Button("Valider")) { EndEditSession(false); return; }
    ui::HelpTooltip("Garde les modifications et revient au panneau précédent.");
    if (!S.libId.empty()) {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(170, 90, 20, 255));
        if (ImGui::Button("Enregistrer l'original")) {
            std::string err;
            if (!SaveOriginal(&err)) Status("Enregistrement impossible : " + err);
        }
        ImGui::PopStyleColor();
        ui::HelpTooltip("Remplace le fichier de la bibliothèque (déverrouillé pour cette édition).");
    }
}

// Compacted HLSL of the library effects (imported presets): the code of the graph nodes and the generated
// shader laid out one statement per line. Only whitespace changes; returns the number of files rewritten.
int FormatLibraryCode(const std::string& dir) {
    int count = 0;
    std::error_code ec;
    for (auto& entry : std::filesystem::directory_iterator(Utf8Path(dir), ec)) {
        if (entry.path().extension() != ".slang") continue;
        const std::string path = Narrow(entry.path().wstring());
        std::string text;
        if (!ReadTextFile(path, text)) continue;
        std::string out, body;
        bool touched = false;
        for (size_t start = 0; start < text.size();) {
            size_t e = text.find('\n', start);
            if (e == std::string::npos) e = text.size();
            std::string line = text.substr(start, e - start);
            start = e + 1;
            if (line.rfind("//@graph ", 0) == 0) {
                EffectGraph g;
                if (EffectGraph::Deserialize(line.substr(9), g)) {
                    bool changed = false;
                    for (auto& n : g.nodes) {
                        if (n.op != "custom") continue;
                        std::string t = ui::FormatHlsl(n.text), d = ui::FormatHlsl(n.decl, 0);
                        if (t != n.text || d != n.decl) { n.text = t; n.decl = d; changed = true; }
                    }
                    if (changed) { line = "//@graph " + g.Serialize(); touched = true; }
                }
                out += line + "\n";
            } else if (line.rfind("//@", 0) == 0 && body.empty()) {
                out += line + "\n";
            } else {
                body += line + "\n";
            }
        }
        std::string fb = ui::FormatHlsl(body, 0);
        if (fb != body) {
            fb += "\n";
            if (fb != body) touched = true;
        }
        if (!touched) continue;
        std::ofstream f(Utf8Path(path), std::ios::binary);
        f << out << fb;
        ++count;
    }
    return count;
}

}  // namespace sx
