// Brushes panel: programmable brush library (search, categories, preview),
// code editor of the current tip (Tip / Dynamics), saving to the library.
#include "ui/App.h"
#include "ui/Theme.h"
#include "doc/Brushes.h"
#include "gpu/ShaderSystem.h"
#include "doc/EffectGraph.h"

#include <imgui_stdlib.h>

#include <filesystem>
#include <fstream>
#include <map>

namespace sx {

namespace {
std::string Lower(std::string s) {
    for (auto& c : s) c = (char)tolower((unsigned char)c);
    return s;
}

std::string Slug(const std::string& name) {
    std::string s;
    for (unsigned char c : name) {
        if (isalnum(c)) s += (char)tolower(c);
        else if (!s.empty() && s.back() != '_') s += '_';
    }
    while (!s.empty() && s.back() == '_') s.pop_back();
    return s.empty() ? "pinceau" : s;
}
}  // namespace

BrushDefPtr App::TipDef(const BrushSettings& b) {
    if (!b.tipGraph.empty()) {
        // Visual brush graph: compiled once per graph text.
        static std::map<std::string, BrushDefPtr> cache;
        auto it = cache.find(b.tipGraph);
        if (it != cache.end()) return it->second;
        if (cache.size() > 32) cache.clear();
        EffectGraph g;
        if (!EffectGraph::Deserialize(b.tipGraph, g)) return nullptr;
        EGCompileResult r = CompileEffectGraph(g);
        auto def = ParseBrush("graph", r.code);
        return cache[b.tipGraph] = def;
    }
    if (!b.tipCode.empty()) {
        // Edited code: parsed once per text.
        static std::map<std::string, BrushDefPtr> cache;
        auto it = cache.find(b.tipCode);
        if (it != cache.end()) return it->second;
        if (cache.size() > 32) cache.clear();
        return cache[b.tipCode] = ParseBrush("edit", b.tipCode);
    }
    return b.tipId.empty() ? nullptr : Brushes().Find(b.tipId);
}

void App::SelectTip(BrushSettings& b, const std::string& id) {
    b.tipId = id;
    b.tipCode.clear();
    b.tipGraph.clear();
    b.tipParams.clear();
    if (auto td = Brushes().Find(id)) {
        if (td->def) b.tipParams = td->def->DefaultParams();
        if (td->def && !td->def->graph.empty()) b.tipGraph = td->def->graph;  // saved from the graph editor
        if (td->spacing > 0) b.spacing = td->spacing;
    }
    brushEd.bufferKey.clear();  // the editor reloads the selected brush
}

void App::OpenGraphEditor(int target) {
    efEd.target = target;
    efEd.cachedJson.clear();
    hideBottom = false;
    ImGui::SetWindowFocus("Éditeur d'effets");
}

void App::SaveGraphToLibrary(const EffectGraph& g, const std::string& code) {
    // The graph rides along (//@graph) so the library item opens again as a graph.
    const bool isBrush = g.kind == EGKind::Brush;
    std::string id = (isBrush ? "user_" : "blend_user_") + Slug(g.name);
    std::string dir = isBrush ? Brushes().Dir() : gpu::Shaders().ShaderDir() + "/effects";
    std::string text = "//@graph " + g.ToJson().Dump(0) + "\n" + code;
    std::ofstream f(Utf8Path(dir + "/" + id + ".slang"), std::ios::binary);
    f << text;
    if (!f) { Status("Impossible d'écrire " + dir + "/" + id + ".slang"); return; }
    f.close();
    if (isBrush) {
        Brushes().Reload();
        BrushSettings& b = tool == Tool::Eraser ? eraser : brush;
        std::vector<float4> keep = b.tipParams;
        SelectTip(b, id);
        if (keep.size() == b.tipParams.size()) b.tipParams = keep;
        Status("Pinceau enregistré : " + g.name);
    } else {
        Effects().Reload();
        Status("Fusion enregistrée : " + g.name + " (panneau Fusions)");
    }
}

void App::DrawBrushPanel() {
    if (!ImGui::Begin("Pinceaux")) { ImGui::End(); return; }
    BrushSettings& b = tool == Tool::Eraser ? eraser : brush;
    auto td = TipDef(b);
    DocView* v = Active();
    // Wide (bottom zone): list | preview + code. Narrow (side tab): stacked.
    const bool wide = ImGui::GetContentRegionAvail().x > 640 * dpi;

    // Library.
    const float listW = wide ? std::max(220.0f * dpi, ImGui::GetContentRegionAvail().x * 0.28f) : 0.0f;
    const float listH = wide ? 0.0f : std::max(150.0f * dpi, ImGui::GetContentRegionAvail().y * 0.36f);
    ImGui::BeginChild("##brushlist", ImVec2(listW, listH), ImGuiChildFlags_Borders);
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##bsearch", "Rechercher un pinceau...", &brushEd.search);
    if (ImGui::Selectable("Ronde (standard)", !td)) SelectTip(b, "");
    std::string q = Lower(brushEd.search);
    std::vector<std::string> cats;
    for (auto& x : Brushes().All())
        if (x->def && std::find(cats.begin(), cats.end(), x->def->category) == cats.end()) cats.push_back(x->def->category);
    for (auto& c : cats) {
        bool any = false;
        for (auto& x : Brushes().All())
            if (x->def && x->def->category == c && (q.empty() || Lower(x->def->name + " " + c).find(q) != std::string::npos)) any = true;
        if (!any) continue;
        if (!q.empty()) ImGui::SetNextItemOpen(true);
        if (!ImGui::TreeNodeEx(c.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth | (c == (td && td->def ? td->def->category : "") ? ImGuiTreeNodeFlags_DefaultOpen : 0))) continue;
        for (auto& x : Brushes().All()) {
            if (!x->def || x->def->category != c) continue;
            if (!q.empty() && Lower(x->def->name + " " + c).find(q) == std::string::npos) continue;
            std::string label = x->def->name + (x->def->graph.empty() ? "" : "  (graphe)") + "##" + x->id;
            if (ImGui::Selectable(label.c_str(), b.tipId == x->id && b.tipCode.empty())) SelectTip(b, x->id);
        }
        ImGui::TreePop();
    }
    ImGui::EndChild();
    if (wide) ImGui::SameLine();

    // Preview + editing.
    ImGui::BeginChild("##brushedit", ImVec2(0, 0));
    auto newButtons = [&] {
        if (ImGui::Button("Nouveau (code)")) {
            b.tipId.clear();
            b.tipGraph.clear();
            b.tipCode = kBrushTemplate;
            b.tipParams = ParseBrush("edit", b.tipCode)->def->DefaultParams();
            brushEd.bufferKey.clear();
        }
        ui::HelpTooltip("Pinceau écrit en HLSL : Tip() dessine la pointe, Dynamics() place les touches.");
        ImGui::SameLine();
        if (ImGui::Button("Nouveau (graphe)")) {
            b.tipId.clear();
            b.tipCode.clear();
            b.tipGraph = EffectGraph::DefaultBrush().Serialize();
            if (auto nd = TipDef(b); nd && nd->def) b.tipParams = nd->def->DefaultParams();
            OpenGraphEditor(2);
        }
        ui::HelpTooltip("Pinceau construit en nœuds dans l'éditeur visuel (zone du bas).");
    };
    if (!td) {
        ImGui::TextWrapped("Pointe ronde standard. Choisissez un pinceau programmable, ou créez-en un :");
        newButtons();
        ImGui::Spacing();
        ImGui::TextWrapped("Un pinceau est un shader : la pointe donne la couleur et la couverture de chaque pixel d'une touche ; "
                           "le placement décide comment chaque touche est posée (suivre le trait, dispersion, aléa, temps).");
        ImGui::EndChild();
        ImGui::End();
        return;
    }
    // Preview stroke (current color and settings).
    std::string err;
    if (v) {
        float w = ImGui::GetContentRegionAvail().x, h = 90 * dpi;
        EffectInstance tip;
        tip.params = b.tipParams;
        BrushSettings ps = b;
        ps.size = std::min(b.size, h * 0.4f);
        if (gpu::Texture* t = v->renderer.BrushPreview(*ctx, *td, tip, ps, LinearColor(fg), (u32)w, (u32)h, &err)) {
            gpu::Texture* th = v->renderer.Thumbnail(*ctx, (1ull << 59) | 2, t, std::hash<std::string>{}(td->text) ^ (u64)w ^ ((u64)h << 20), (u32)w, (u32)h);
            ImGui::Image((ImTextureID)gpu::Dev().UiTextureId(th), ImVec2(w, h));
        }
    }
    const bool isGraph = !b.tipGraph.empty();
    ImGui::TextUnformatted(td->def ? td->def->name.c_str() : "Pinceau");
    ImGui::SameLine();
    ImGui::TextDisabled("%s%s", td->def ? td->def->category.c_str() : "", isGraph ? "  (graphe)" : b.tipCode.empty() ? "" : "  (modifié)");
    // Parameters of the tip.
    if (td->def && !td->def->params.empty()) {
        EffectInstance fx;
        fx.customCode = td->text;
        fx.params = b.tipParams;
        if (EffectParamsUI(fx, "tippanel")) b.tipParams = fx.params;
    }
    if (isGraph) {
        // Graph brush: edited in the visual editor.
        if (ImGui::Button("Éditer le graphe")) OpenGraphEditor(2);
        ImGui::SameLine();
        newButtons();
        if (ImGui::CollapsingHeader("Code généré")) {
            ImGui::PushFont(ui::GetFonts().mono, 0.0f);
            std::string code = td->text;
            ImGui::InputTextMultiline("##brushgen", &code, ImVec2(-1, 220 * dpi), ImGuiInputTextFlags_ReadOnly);
            ImGui::PopFont();
        }
    } else {
        // Code editor.
        std::string key = b.tipCode.empty() ? "lib:" + b.tipId : "edit";
        if (brushEd.bufferKey != key) {
            brushEd.bufferKey = key;
            brushEd.buffer = td->text;
        }
        ImGui::PushFont(ui::GetFonts().mono, 0.0f);
        ImGui::InputTextMultiline("##brushcode", &brushEd.buffer, ImVec2(-1, std::max(160.0f * dpi, ImGui::GetContentRegionAvail().y - 70 * dpi)),
                                  ImGuiInputTextFlags_AllowTabInput);
        ImGui::PopFont();
        bool apply = ImGui::Button("Appliquer (Ctrl+Entrée)") ||
                     (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsKeyPressed(ImGuiKey_Enter) && ImGui::GetIO().KeyCtrl);
        if (apply && brushEd.buffer != td->text) {
            auto old = td->def;
            b.tipCode = brushEd.buffer;
            brushEd.bufferKey = "edit";
            auto nd = TipDef(b);
            // Keep the values of parameters that still exist.
            std::vector<float4> params = nd->def->DefaultParams();
            if (old)
                for (size_t i = 0; i < nd->def->params.size(); ++i)
                    for (size_t k = 0; k < old->params.size() && k < b.tipParams.size(); ++k)
                        if (old->params[k].id == nd->def->params[i].id) params[i] = b.tipParams[k];
            b.tipParams = params;
            if (nd->spacing > 0) b.spacing = nd->spacing;
        }
        ImGui::SameLine();
        newButtons();
        if (ImGui::Button("Enregistrer dans la bibliothèque...")) {
            brushEd.saveName = td->def ? td->def->name : "Mon pinceau";
            if (b.tipCode.empty() && td->def) brushEd.saveCategory = td->def->category;
            ImGui::OpenPopup("##savebrush");
        }
        if (!b.tipCode.empty()) {
            ImGui::SameLine();
            if (ImGui::Button("Annuler les modifications")) {
                b.tipCode.clear();
                brushEd.bufferKey.clear();
                if (auto lib = Brushes().Find(b.tipId); lib && lib->def) b.tipParams = lib->def->DefaultParams();
            }
        }
        if (ImGui::BeginPopup("##savebrush")) {
            ImGui::InputText("Nom", &brushEd.saveName);
            ImGui::InputText("Catégorie", &brushEd.saveCategory);
            std::string id = "user_" + Slug(brushEd.saveName);
            std::string path = Brushes().Dir() + "/" + id + ".slang";
            bool exists = std::filesystem::exists(Utf8Path(path));
            ImGui::TextDisabled("brushes/%s.slang%s", id.c_str(), exists ? " (remplacé)" : "");
            if (ImGui::Button("Enregistrer", ImVec2(-1, 0)) && !brushEd.saveName.empty()) {
                // Name and category lines replaced, the rest kept.
                std::string text = brushEd.buffer, out;
                size_t pos = 0;
                out = "//@name " + brushEd.saveName + "\n//@category " + brushEd.saveCategory + "\n";
                while (pos <= text.size()) {
                    size_t eol = text.find('\n', pos);
                    if (eol == std::string::npos) eol = text.size();
                    std::string line = text.substr(pos, eol - pos);
                    if (line.rfind("//@name", 0) != 0 && line.rfind("//@category", 0) != 0) out += line + (eol < text.size() ? "\n" : "");
                    pos = eol + 1;
                }
                std::ofstream(Utf8Path(path), std::ios::binary) << out;
                Brushes().Reload();
                SelectTip(b, id);
                Status("Pinceau enregistré : " + brushEd.saveName);
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }
    if (!err.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.45f, 0.45f, 1));
        ImGui::TextWrapped("%s", err.c_str());
        ImGui::PopStyleColor();
    } else if (td->def && !td->def->error.empty()) {
        ImGui::TextColored(ImVec4(1, 0.6f, 0.3f, 1), "%s", td->def->error.c_str());
    } else if (!isGraph && brushEd.buffer != td->text) {
        ImGui::TextDisabled("Modifications non appliquées");
    }
    ImGui::EndChild();
    ImGui::End();
}

}  // namespace sx
