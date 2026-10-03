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

void App::DrawBrushPanel() {
    if (!ImGui::Begin("Pinceaux", PanelFlag("Pinceaux"))) { ImGui::End(); return; }
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
    ImGui::TextDisabled("%s%s", td->def ? td->def->category.c_str() : "", isGraph ? "  (graphe)" : b.tipCode.empty() ? "" : "  (copie modifiée)");
    // Parameters of the tip.
    if (td->def && !td->def->params.empty()) {
        EffectInstance fx;
        fx.customCode = td->text;
        fx.params = b.tipParams;
        if (EffectParamsUI(fx, "tippanel")) b.tipParams = fx.params;
    }
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
