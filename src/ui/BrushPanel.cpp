// Brushes panel: programmable brush library (search, categories, preview),
// code editor of the current tip (Tip / Dynamics), saving to the library.
#include "ui/App.h"
#include <imgui_internal.h>
#include "ui/Theme.h"
#include "doc/Brushes.h"
#include "gpu/ShaderSystem.h"
#include "doc/EffectGraph.h"
#include "ui/Lang.h"
#include <shellapi.h>

#include <imgui_stdlib.h>

#include <filesystem>
#include <fstream>
#include <map>

#include <stb_image.h>

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

std::string App::TipTexturePath(const BrushSettings& b, const BrushDef& td, size_t i) const {
    if (i < b.tipTextures.size() && !b.tipTextures[i].empty()) return b.tipTextures[i];
    if (i >= td.textures.size() || td.textures[i].path.empty()) return "";
    std::filesystem::path p = Utf8Path(td.textures[i].path);
    return p.is_absolute() ? td.textures[i].path : Brushes().Dir() + "/" + td.textures[i].path;
}

gpu::Texture* App::BrushTexture(const std::string& path) {
    if (path.empty()) return nullptr;
    auto it = brushTextures.find(path);
    if (it != brushTextures.end()) return it->second.get();
    gpu::TexturePtr tex;
    FILE* f = nullptr;
    if (_wfopen_s(&f, Widen(path).c_str(), L"rb") == 0 && f) {
        int w = 0, h = 0, comp = 0;
        stbi_uc* px = stbi_load_from_file(f, &w, &h, &comp, 4);
        fclose(f);
        if (px && w > 0 && h > 0) {
            tex = gpu::Dev().CreateTexture((u32)w, (u32)h, gpu::Format::RGBA8, "BrushTexture");
            if (comp < 4) {
                // Images without alpha: the luminance is the shape (white = paint).
                for (int k = 0; k < w * h; ++k) {
                    stbi_uc* q = px + k * 4;
                    q[3] = (stbi_uc)((q[0] * 54 + q[1] * 183 + q[2] * 19) >> 8);
                }
            }
            ctx->Upload(tex.get(), px, (u32)w * 4);
        }
        if (px) stbi_image_free(px);
    }
    if (!tex) Status("Texture de pinceau illisible : " + path);
    return (brushTextures[path] = tex).get();
}

void App::BindTipTextures(BrushSettings& b, const BrushDef& td) {
    b.tipTexBound = {};
    for (size_t i = 0; i < td.textures.size() && i < 4; ++i) b.tipTexBound[i] = BrushTexture(TipTexturePath(b, td, i));
}

u64 App::BrushPreviewVersion(const BrushSettings& b, const BrushDef& td) const {
    u64 h = std::hash<std::string>{}(td.text);
    auto mix = [&](const void* p, size_t n) {
        const unsigned char* c = (const unsigned char*)p;
        for (size_t i = 0; i < n; ++i) h = (h ^ c[i]) * 1099511628211ull;
    };
    if (!b.tipParams.empty()) mix(b.tipParams.data(), b.tipParams.size() * sizeof(float4));
    for (auto* t : b.tipTexBound) mix(&t, sizeof(t));
    mix(&b.size, sizeof(float) * 4);
    mix(&fg, sizeof(fg));
    return h;
}

bool App::TipTexturesUI(BrushSettings& b, const BrushDef& td) {
    // One row per //@texture slot: thumbnail, choose an image, back to the default.
    bool changed = false;
    if (td.textures.empty()) return false;
    ImGui::SeparatorText("Textures du pinceau");
    for (size_t i = 0; i < td.textures.size() && i < 4; ++i) {
        ImGui::PushID((int)i);
        std::string path = TipTexturePath(b, td, i);
        float s = ImGui::GetFrameHeight() * 2;
        if (gpu::Texture* t = BrushTexture(path)) {
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + s, p.y + s), IM_COL32(40, 40, 48, 255), 3);
            ImGui::Image((ImTextureID)gpu::Dev().UiTextureId(t), ImVec2(s, s));
        } else {
            ImGui::Dummy(ImVec2(s, s));
        }
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::TextUnformatted(td.textures[i].label.c_str());
        ImGui::TextDisabled("%s", path.empty() ? "aucune image (blanc)" : Narrow(Utf8Path(path).filename().wstring()).c_str());
        if (ImGui::SmallButton("Image...")) {
            std::string pick = platform::OpenFileDialog(hwnd, L"Images\0*.png;*.jpg;*.jpeg;*.tga;*.bmp\0");
            if (!pick.empty()) {
                if (b.tipTextures.size() <= i) b.tipTextures.resize(i + 1);
                b.tipTextures[i] = pick;
                changed = true;
            }
        }
        ImGui::SameLine();
        // Images shipped with the brushes (textures folder).
        if (ImGui::SmallButton("Bibliothèque")) ImGui::OpenPopup("##texlib");
        if (ImGui::BeginPopup("##texlib")) {
            std::error_code ec;
            for (auto& e : std::filesystem::directory_iterator(Utf8Path(Brushes().Dir() + "/textures"), ec)) {
                std::string full = Narrow(e.path().wstring());
                if (ImGui::Selectable(Narrow(e.path().filename().wstring()).c_str(), full == path)) {
                    if (b.tipTextures.size() <= i) b.tipTextures.resize(i + 1);
                    b.tipTextures[i] = full;
                    changed = true;
                }
            }
            ImGui::EndPopup();
        }
        if (i < b.tipTextures.size() && !b.tipTextures[i].empty()) {
            ImGui::SameLine();
            if (ImGui::SmallButton("Défaut")) { b.tipTextures[i].clear(); changed = true; }
        }
        ImGui::EndGroup();
        ImGui::PopID();
    }
    return changed;
}

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
    b.tipTextures.clear();
    if (auto td = Brushes().Find(id)) {
        if (td->def) b.tipParams = td->def->DefaultParams();
        if (td->def && !td->def->graph.empty()) b.tipGraph = td->def->graph;  // saved from the graph editor
        if (td->spacing > 0) b.spacing = td->spacing;
    }
    brushEd.bufferKey.clear();  // the editor reloads the selected brush
}

void App::OpenGraphEditor(int target) {
    // An edit session remembers the state and the panel to come back to (Annuler / Valider).
    if (!efEd.session.active) {
        ImGuiWindow* nav = ImGui::GetCurrentContext()->NavWindow;
        if (nav) nav = nav->RootWindow;  // the panel, not one of its child regions
        std::string prev = nav ? nav->Name : "";
        BeginEditSession(prev.c_str());
    }
    efEd.browse = false;
    efEd.target = target;
    efEd.cachedJson.clear();
    hideBottom = false;
    FocusPanel("Éditeur d'effets");
}

namespace {
// The standard round tip has no library entry: a built-in equivalent draws its previews.
const BrushDef* RoundPreviewDef() {
    static BrushDefPtr round = ParseBrush("_round_preview",
                                          "//@name Ronde\n//@category Standard\nfloat4 Tip(Brush b)\n{\n"
                                          "    float d = length(b.local);\n    return float4(b.color, 1.0 - smoothstep(b.hardness * 0.999, 1.0, d));\n}\n");
    return round.get();
}
}  // namespace

// Stroke preview of the current tool: white ink on a transparent (checkered) background.
void App::BrushStrokePreview(BrushSettings& b, const BrushDef* td, float h) {
    DocView* v = Active();
    if (!v) return;
    const BrushDef* def = td ? td : RoundPreviewDef();
    if (!def) return;
    float w = ImGui::GetContentRegionAvail().x;
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    // Checkerboard.
    const float c = 8 * dpi;
    dl->PushClipRect(p, ImVec2(p.x + w, p.y + h), true);
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), IM_COL32(52, 52, 58, 255));
    for (int y = 0; y * c < h; ++y)
        for (int x = 0; x * c < w; ++x)
            if ((x + y) & 1) dl->AddRectFilled(ImVec2(p.x + x * c, p.y + y * c), ImVec2(p.x + (x + 1) * c, p.y + (y + 1) * c), IM_COL32(38, 38, 44, 255));
    dl->PopClipRect();
    EffectInstance tip;
    if (td) {
        tip.params = b.tipParams;
        BindTipTextures(b, *td);
    } else if (def->def) {
        tip.params = def->def->DefaultParams();
    }
    BrushSettings ps = b;
    ps.size = std::min(b.size, h * 0.42f);
    std::string err;
    if (gpu::Texture* t = v->renderer.BrushPreview(*ctx, *def, tip, ps, {1, 1, 1, 1}, (u32)w, (u32)h, &err)) {
        gpu::Texture* th = v->renderer.Thumbnail(*ctx, (1ull << 59) | 3, t, BrushPreviewVersion(ps, *def) ^ (u64)w ^ ((u64)h << 20) ^ 0xC0FFEE, (u32)w, (u32)h);
        ImGui::Image((ImTextureID)gpu::Dev().UiTextureId(th), ImVec2(w, h));
    } else {
        ImGui::Dummy(ImVec2(w, h));
    }
    dl->AddRect(p, ImVec2(p.x + w, p.y + h), cyui::palette().line, 3);
    if (!err.empty()) ImGui::TextColored(ImVec4(1, 0.45f, 0.45f, 1), "%s", err.substr(0, 300).c_str());
}

// Tip picker: button with the current tip, popup with search and categories.
void App::BrushTipPicker(BrushSettings& b) {
    auto td = TipDef(b);
    std::string name = td && td->def ? td->def->name : "Ronde (standard)";
    if (ImGui::Button((Tr(name) + "  \xE2\x96\xBE").c_str(), ImVec2(-FLT_MIN, 0))) ImGui::OpenPopup("##tippick");
    ui::HelpTooltip("Choisir la pointe (pinceaux programmables, pinceaux importés .abr)");
    ImGui::SetNextWindowSizeConstraints(ImVec2(260 * dpi, 120 * dpi), ImVec2(420 * dpi, 520 * dpi));
    if (ImGui::BeginPopup("##tippick")) {
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputTextWithHint("##tsearch", "Rechercher un pinceau...", &brushEd.search);
        std::string q = Lower(brushEd.search);
        ImGui::BeginChild("##tiplist", ImVec2(0, 400 * dpi));
        if (ImGui::Selectable("Ronde (standard)", !td)) { SelectTip(b, ""); ImGui::CloseCurrentPopup(); }
        std::vector<std::string> cats;
        for (auto& x : Brushes().All())
            if (x->def && std::find(cats.begin(), cats.end(), x->def->category) == cats.end()) cats.push_back(x->def->category);
        for (auto& c : cats) {
            bool any = false;
            for (auto& x : Brushes().All())
                if (x->def && x->def->category == c && (q.empty() || Lower(x->def->name + " " + c).find(q) != std::string::npos)) any = true;
            if (!any) continue;
            ImGui::SeparatorText(c.c_str());
            for (auto& x : Brushes().All()) {
                if (!x->def || x->def->category != c) continue;
                if (!q.empty() && Lower(x->def->name + " " + c).find(q) == std::string::npos) continue;
                if (ImGui::Selectable((x->def->name + "##" + x->id).c_str(), b.tipId == x->id && b.tipCode.empty())) {
                    SelectTip(b, x->id);
                    ImGui::CloseCurrentPopup();
                }
            }
        }
        ImGui::EndChild();
        ImGui::EndPopup();
    }
}

// Right-click menu of a brush in the library: use it, edit a copy, duplicate it into My brushes, favorites, show
// the file; rename / delete for my own brushes.
void App::BrushTileMenu(BrushSettings& b, const std::string& id, const BrushDef* def, const std::string& name) {
    namespace fs = std::filesystem;
    const bool round = id == "_round";
    const bool mine = !round && (id.rfind("user_", 0) == 0 || (def && def->def && def->def->category == "Mes pinceaux"));
    ImGui::TextDisabled("%s", Tr(name).c_str());
    ImGui::Separator();
    if (ImGui::MenuItem("Utiliser ce pinceau")) SelectTip(b, round ? "" : id);
    if (!round && ImGui::MenuItem("Modifier (code ou graphe)...")) {
        SelectTip(b, id);
        OpenGraphEditor(2);
    }
    if (!round && def && ImGui::MenuItem("Dupliquer dans Mes pinceaux")) {
        // A copy under a new id, in the "Mes pinceaux" category.
        std::error_code ec;
        int n = 1;
        while (fs::exists(Utf8Path(Brushes().Dir() + "/user_" + id + "_" + std::to_string(n) + ".slang"), ec)) ++n;
        const std::string newId = "user_" + id + "_" + std::to_string(n);
        std::string text = def->text;
        auto setMeta = [&](const std::string& key, const std::string& value) {
            const std::string tag = "//@" + key + " ";
            size_t at = text.find(tag);
            if (at == std::string::npos) { text = tag + value + "\n" + text; return; }
            size_t e = text.find('\n', at);
            text.replace(at, (e == std::string::npos ? text.size() : e) - at, tag + value);
        };
        setMeta("name", name + " " + Tr("(copie)"));
        setMeta("category", "Mes pinceaux");
        std::ofstream f(Utf8Path(Brushes().Dir() + "/" + newId + ".slang"), std::ios::binary);
        if (f << text) {
            f.close();
            Brushes().Reload();
            SelectTip(b, newId);
            Status(Tr("Pinceau dupliqué dans Mes pinceaux :") + " " + name);
        } else {
            Status(Tr("Dupliquer le pinceau : écriture impossible dans") + " " + Brushes().Dir());
        }
    }
    if (!round) {
        const bool fav = brushFavorites.count(id) > 0;
        if (ImGui::MenuItem(fav ? "Retirer des favoris" : "Ajouter aux favoris")) {
            if (fav) brushFavorites.erase(id);
            else brushFavorites.insert(id);
            SaveAiSettings();
        }
    }
    if (!round && ImGui::MenuItem("Afficher le fichier")) {
        const std::wstring args = L"/select,\"" + Utf8Path(Brushes().Dir() + "/" + id + ".slang").wstring() + L"\"";
        ShellExecuteW(hwnd, L"open", L"explorer.exe", args.c_str(), nullptr, SW_SHOWNORMAL);
    }
    if (mine) {
        ImGui::Separator();
        if (ImGui::BeginMenu("Renommer")) {
            static std::string rename;
            if (ImGui::IsWindowAppearing()) rename = name;
            ImGui::SetNextItemWidth(220 * dpi);
            if (ImGui::InputText("##rename", &rename, ImGuiInputTextFlags_EnterReturnsTrue) && !rename.empty() && def) {
                std::string text = def->text;
                const std::string tag = "//@name ";
                size_t at = text.find(tag);
                if (at == std::string::npos) text = tag + rename + "\n" + text;
                else {
                    size_t e = text.find('\n', at);
                    text.replace(at, (e == std::string::npos ? text.size() : e) - at, tag + rename);
                }
                std::ofstream(Utf8Path(Brushes().Dir() + "/" + id + ".slang"), std::ios::binary) << text;
                Brushes().Reload();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Supprimer ce pinceau")) {
            std::error_code ec;
            fs::remove(Utf8Path(Brushes().Dir() + "/" + id + ".slang"), ec);
            brushFavorites.erase(id);
            if (b.tipId == id) SelectTip(b, "");
            Brushes().Reload();
            Status(ec ? Tr("Supprimer le pinceau : impossible") : Tr("Pinceau supprimé :") + " " + name);
        }
    }
}

void App::DrawBrushPanel() {
    if (!ImGui::Begin("Pinceaux", PanelFlag("Pinceaux"))) { ImGui::End(); return; }
    PopOutCorner("Pinceaux");
    BrushSettings& b = tool == Tool::Eraser ? eraser : brush;
    auto td = TipDef(b);
    if (ImGui::Button(ui::ManualText(CurrentLanguage(),"Brush help"))) {
        documentation.Navigate(!b.tipCode.empty() || !b.tipGraph.empty() ? "authoring" :
            ui::ManualReferenceTopic("brush",b.tipId,"brushes_tablet"));
    }
    DocView* v = Active();
    // The current brush above everything: its stroke (current size and settings) and its name.
    BrushStrokePreview(b, td.get(), 64 * dpi);
    ImGui::TextUnformatted(Tr(td && td->def ? td->def->name : std::string("Ronde (standard)")).c_str());
    if (td && td->def) {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", Tr(td->def->category).c_str());
    }
    // Wide (bottom zone): list | preview + code. Narrow (side tab): stacked.
    const bool wide = ImGui::GetContentRegionAvail().x > 640 * dpi;

    // Library: each brush shown by its stroke (a thumbnail rendered once, kept in the cache), grouped by category,
    // the search filtering names and categories.
    const float listW = wide ? std::max(260.0f * dpi, ImGui::GetContentRegionAvail().x * 0.34f) : 0.0f;
    const float listH = wide ? 0.0f : std::max(220.0f * dpi, ImGui::GetContentRegionAvail().y * 0.55f);
    ImGui::BeginChild("##brushlist", ImVec2(listW, listH), ImGuiChildFlags_Borders);
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##bsearch", "Rechercher un pinceau...", &brushEd.search);
    std::string q = Lower(brushEd.search);
    int rendered = 0;  // (new thumbnails per frame: a few, the rest the next frames)
    const float tileW = 104 * dpi, tileH = 46 * dpi;
    auto tile = [&](const std::string& id, const BrushDef* def, const std::string& name, bool selected, bool graph) {
        ImGui::PushID(id.c_str());
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const float textH = ImGui::GetTextLineHeight();
        const bool clicked = ImGui::InvisibleButton("##tile", ImVec2(tileW, tileH + textH + 4 * dpi));
        const bool hovered = ImGui::IsItemHovered();
        ImGui::SetItemTooltip("%s%s", Tr(name).c_str(), graph ? TrC("  (graphe)") : "");
        if (ImGui::BeginPopupContextItem("##tilemenu")) {
            BrushTileMenu(b, id, def, name);
            ImGui::EndPopup();
        }
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(p, ImVec2(p.x + tileW, p.y + tileH), IM_COL32(30, 30, 36, 255), 4 * dpi);
        if (v && def && ImGui::IsItemVisible()) {
            const u64 key = (1ull << 58) | (std::hash<std::string>{}(id) & ((1ull << 56) - 1));
            const u64 hash = std::hash<std::string>{}(def->text) ^ (u64)tileW;
            gpu::Texture* th = v->renderer.CachedThumbnail(key);
            // Drawn three times (a brush's shader may still be compiling the first time: an empty stroke), then
            // kept until the brush's source changes.
            int& tries = brushEd.thumbTries[id];
            if (brushEd.thumbHash[id] != hash) { brushEd.thumbHash[id] = hash; tries = 0; }
            // (a shader still compiling: the preview would return an older image, so wait for it)
            if ((!th || tries < 3) && rendered < 2 && v->renderer.BrushPipeline(*def)) {
                BrushSettings ps = b;
                ps.size = tileH * 0.32f;
                ps.tipId = def == RoundPreviewDef() ? std::string() : id;
                ps.tipCode.clear();
                ps.tipGraph.clear();
                if (def->def) ps.tipParams = def->def->DefaultParams();
                BindTipTextures(ps, *def);
                EffectInstance tip;
                tip.params = ps.tipParams;
                std::string err;
                // (dark ink: the thumbnail's transparency shows as a light checkerboard, like paper)
                if (gpu::Texture* t = v->renderer.BrushPreview(*ctx, *def, tip, ps, {0.02f, 0.02f, 0.03f, 1}, (u32)tileW, (u32)tileH, &err))
                    th = v->renderer.Thumbnail(*ctx, key, t, hash ^ (u64)(tries + 1) ^ 0xD0, (u32)tileW, (u32)tileH);
                ++tries;
                ++rendered;
            }
            if (th) dl->AddImage((ImTextureID)gpu::Dev().UiTextureId(th), p, ImVec2(p.x + tileW, p.y + tileH));
        }
        const ImU32 frame = selected ? cyui::palette().accent : hovered ? cyui::palette().line : 0;
        if (frame) dl->AddRect(p, ImVec2(p.x + tileW, p.y + tileH), frame, 4 * dpi, 0, selected ? 2.0f : 1.0f);
        // The name under it, shortened to the tile.
        std::string shown = Tr(name);
        while (shown.size() > 3 && ImGui::CalcTextSize(shown.c_str()).x > tileW) shown = shown.substr(0, shown.size() - 4) + "...";
        dl->AddText(ImVec2(p.x + (tileW - ImGui::CalcTextSize(shown.c_str()).x) * 0.5f, p.y + tileH + 2 * dpi),
                    selected ? cyui::palette().text : cyui::palette().textDim, shown.c_str());
        ImGui::PopID();
        return clicked;
    };
    // Tiles flow in rows across the width.
    int col = 0;
    const int perRow = std::max(1, (int)((ImGui::GetContentRegionAvail().x + ImGui::GetStyle().ItemSpacing.x) / (tileW + ImGui::GetStyle().ItemSpacing.x)));
    auto place = [&] {
        if (col > 0 && col < perRow) ImGui::SameLine();
        col = col + 1 >= perRow ? 0 : col + 1;
    };
    if (q.empty() || Lower(Tr("Ronde (standard)") + " standard").find(q) != std::string::npos) {
        place();
        if (tile("_round", RoundPreviewDef(), "Ronde (standard)", !td, false)) SelectTip(b, "");
    }
    std::vector<std::string> cats;
    for (auto& x : Brushes().All())
        if (x->def && std::find(cats.begin(), cats.end(), x->def->category) == cats.end()) cats.push_back(x->def->category);
    // The essentials first, my brushes next, the others in their order.
    std::stable_sort(cats.begin(), cats.end(), [](const std::string& a, const std::string& b) {
        auto rank = [](const std::string& c) { return c == "Essentiels" ? 0 : c == "Mes pinceaux" ? 1 : 2; };
        return rank(a) < rank(b);
    });
    const std::string kFav = "\xE2\x98\x85 Favoris";
    if (!brushFavorites.empty()) cats.insert(cats.begin(), kFav);
    auto inCat = [&](const BrushDefPtr& x, const std::string& c) {
        return x->def && (c == kFav ? brushFavorites.count(x->id) > 0 : x->def->category == c);
    };
    for (auto& c : cats) {
        bool any = false;
        for (auto& x : Brushes().All())
            if (inCat(x, c) && (q.empty() || Lower(x->def->name + " " + c + " " + Tr(x->def->name) + " " + Tr(c)).find(q) != std::string::npos)) any = true;
        if (!any) continue;
        if (!q.empty()) ImGui::SetNextItemOpen(true);
        if (!ImGui::CollapsingHeader((Tr(c) + "##bcat" + c).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) continue;
        col = 0;
        for (auto& x : Brushes().All()) {
            if (!inCat(x, c)) continue;
            if (!q.empty() && Lower(x->def->name + " " + c + " " + Tr(x->def->name) + " " + Tr(c)).find(q) == std::string::npos) continue;
            place();
            if (tile(x->id, x.get(), x->def->name, b.tipId == x->id && b.tipCode.empty(), !x->def->graph.empty())) SelectTip(b, x->id);
        }
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
            OpenGraphEditor(2);
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
    // (the stroke preview is at the top of the panel)
    std::string err;
    const bool isGraph = !b.tipGraph.empty();
    if (isGraph || !b.tipCode.empty()) ImGui::TextDisabled("%s", TrC(isGraph ? "(graphe)" : "(copie modifiée)"));
    // Parameters of the tip.
    if (td->def && !td->def->params.empty()) {
        EffectInstance fx;
        fx.customCode = td->text;
        fx.params = b.tipParams;
        if (EffectParamsUI(fx, "tippanel")) b.tipParams = fx.params;
    }
    TipTexturesUI(b, *td);
    // Code / graph: edited in the effect editor (a copy, the library stays untouched).
    if (ImGui::Button("Éditer")) OpenGraphEditor(2);
    ui::HelpTooltip("Ouvre le pinceau dans l'Éditeur d'effets (code ou graphe).");
    ImGui::SameLine();
    newButtons();
    if (!err.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.45f, 0.45f, 1));
        ImGui::TextWrapped("%s", err.c_str());
        ImGui::PopStyleColor();
    } else if (td->def && !td->def->error.empty()) {
        ImGui::TextColored(ImVec4(1, 0.6f, 0.3f, 1), "%s", td->def->error.c_str());
    }
    ImGui::EndChild();
    ImGui::End();
}

}  // namespace sx
