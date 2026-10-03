// Blend modes panel: built-in modes and library blends (HLSL or graph) with live
// previews of the active layer over what is below it; new blends in code or in
// the visual graph editor. Blends are separate from effects: inputs A (below),
// B (layer), mask, and the layer opacity.
#include "ui/App.h"
#include "ui/Theme.h"
#include "doc/EffectGraph.h"
#include "gpu/ShaderSystem.h"

#include <imgui_stdlib.h>

namespace sx {

namespace {
constexpr u32 kBlendPreview = 128;
constexpr u64 kBlendThumbBit = 1ull << 58;

std::string Lower(std::string s) {
    for (auto& c : s) c = (char)tolower((unsigned char)c);
    return s;
}

u64 HashString(const std::string& s) {
    u64 h = 1469598103934665603ull;
    for (unsigned char c : s) h = (h ^ c) * 1099511628211ull;
    return h;
}

// Built-in mode as an inline blend effect (previews only; layers use the fast composite path).
const EffectInstance& BuiltinBlendFx(int mode) {
    static std::map<int, EffectInstance> cache;
    auto it = cache.find(mode);
    if (it != cache.end()) return it->second;
    std::string code = std::string("//@name ") + BlendModeName((BlendMode)mode) +
                       "\n//@category Standard\n//@blend\n//@input A\n//@input B\n//@input Masque\n//@halo 0\n" +
                       gpu::Shaders().File("blend_modes.slang") +
                       "\nfloat4 Effect(EffectContext ctx)\n{\n"
                       "    float4 A = Input(0, ctx.px), B = Input(1, ctx.px);\n"
                       "    float ab = B.a, a = ab + A.a * (1.0 - ab);\n"
                       "    float3 rgb = ab * (1.0 - A.a) * B.rgb + ab * A.a * BlendColor(A.rgb, B.rgb, " + std::to_string(mode) + "u) + (1.0 - ab) * A.a * A.rgb;\n"
                       "    return float4(a > 0.0 ? rgb / a : (float3)0.0, a);\n}\n";
    EffectInstance fx;
    fx.customCode = code;
    return cache[mode] = fx;
}
}  // namespace

void App::DrawBlendPanel() {
    if (!ImGui::Begin("Fusions")) { ImGui::End(); return; }
    auto& B = blendBrowser;
    DocView* v = Active();
    Document* d = v ? v->doc.get() : nullptr;
    Layer* layer = d && d->Mode() == DocMode::Layers ? d->ActiveLayer() : nullptr;

    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##blsearch", "Rechercher une fusion...", &B.search);
    // Categories: Standard + library categories.
    std::vector<std::string> cats{"Standard"};
    for (auto& def : Effects().All())
        if (def->blend && std::find(cats.begin(), cats.end(), def->category) == cats.end()) cats.push_back(def->category);
    auto chip = [&](const char* label, bool on) {
        float w = ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2;
        if (ImGui::GetCursorPosX() > ImGui::GetStyle().WindowPadding.x + 1 && ImGui::GetContentRegionAvail().x < w + 4) ImGui::NewLine();
        if (on) ImGui::PushStyleColor(ImGuiCol_Button, ui::col::Accent);
        bool clicked = ImGui::SmallButton(label);
        if (on) ImGui::PopStyleColor();
        ImGui::SameLine();
        return clicked;
    };
    if (chip("Toutes", B.category.empty())) B.category.clear();
    for (auto& c : cats)
        if (chip(c.c_str(), B.category == c)) B.category = B.category == c ? "" : c;
    ImGui::NewLine();
    if (layer) ImGui::TextColored(ImVec4(0.45f, 0.8f, 0.95f, 1), "Calque : %s  (A = dessous, B = calque)", layer->name.c_str());
    else ImGui::TextDisabled(d ? "Mode nodal : utilisez le nœud Fusion du graphe" : "Aucun document");

    // Preview sources: below + layer, small copies refreshed when they change.
    u64 ha = 0, hb = 0;
    gpu::Texture* ta = layer ? LayerBelow(*v, ha) : nullptr;
    gpu::Texture* tb = layer ? ActiveLayerContent(*v, hb) : nullptr;
    u64 frame = gpu::Dev().FrameNumber();
    u64 h = ha * 31 + hb;
    if (ta && tb && h != B.sourceHash && (frame - B.sourceFrame >= 20 || !B.a)) {
        auto a = v->renderer.PreviewCopy(*ctx, ta, kBlendPreview, kBlendPreview);
        auto b = v->renderer.PreviewCopy(*ctx, tb, kBlendPreview, kBlendPreview);
        if (a && b) {
            B.a = a;
            B.b = b;
            B.sourceHash = h;
            B.sourceFrame = frame;
            B.previews.clear();
        }
    }

    // Items: built-in modes then library blends.
    struct Item {
        std::string key, name, category;
        int mode = -1;          // built-in
        EffectDefPtr def;       // library
    };
    std::vector<Item> items;
    std::string q = Lower(B.search);
    auto keep = [&](const std::string& name, const std::string& cat) {
        if (!B.category.empty() && B.category != cat) return false;
        return q.empty() || Lower(name + " " + cat).find(q) != std::string::npos;
    };
    for (int i = 0; i < kBlendMenuCount; ++i) {
        int m = kBlendMenuOrder[i];
        if (m >= 0 && keep(BlendModeName((BlendMode)m), "Standard")) items.push_back({"std:" + std::to_string(m), BlendModeName((BlendMode)m), "Standard", m, nullptr});
    }
    for (auto& def : Effects().All())
        if (def->blend && keep(def->name, def->category)) items.push_back({def->id, def->name, def->category, -1, def});

    int budget = 4;
    if (v && B.a && B.b)
        for (auto& it : items) {
            if (budget <= 0) break;
            if (B.previews.count(it.key)) continue;
            EffectInstance fx;
            if (it.mode >= 0) fx = BuiltinBlendFx(it.mode);
            else {
                fx.effectId = it.def->id;
                fx.params = it.def->DefaultParams();
            }
            if (auto out = v->renderer.ApplyEffectInputs(*d, *ctx, fx, {B.a.get(), B.b.get(), nullptr, nullptr}, kBlendPreview, kBlendPreview)) B.previews[it.key] = out;
            --budget;
        }

    // Grid.
    float actionsH = ImGui::GetFrameHeightWithSpacing() * 2.4f;
    ImGui::BeginChild("##blgrid", ImVec2(0, -actionsH));
    const float fs = ImGui::GetFontSize();
    const float cardW = 96 * dpi, cardH = cardW + fs * 2.4f;
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    int cols = std::max(1, (int)((ImGui::GetContentRegionAvail().x + spacing) / (cardW + spacing)));
    ImDrawList* dl = ImGui::GetWindowDrawList();
    for (size_t i = 0; i < items.size(); ++i) {
        const Item& it = items[i];
        if (i % cols) ImGui::SameLine();
        ImGui::PushID(it.key.c_str());
        ImVec2 p = ImGui::GetCursorScreenPos();
        bool current = layer && (it.mode >= 0 ? !layer->HasCustomBlend() && !layer->passThrough && (int)layer->blend == it.mode
                                              : layer->blendFx.effectId == it.def->id);
        if (ImGui::InvisibleButton("##card", ImVec2(cardW, cardH)) && layer) {
            Edit e(*d, v->history, "Mode de fusion");
            layer->passThrough = false;
            if (it.mode >= 0) {
                layer->blend = (BlendMode)it.mode;
                layer->blendFx = {};
            } else {
                layer->blendFx = {};
                layer->blendFx.effectId = it.def->id;
                layer->blendFx.params = it.def->DefaultParams();
            }
            d->SyncGraph();
            e.Commit();
        }
        bool hovered = ImGui::IsItemHovered();
        if (it.def && ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload(kEffectPayload, it.def->id.c_str(), it.def->id.size() + 1);
            ImGui::TextUnformatted(it.name.c_str());
            ImGui::TextDisabled("Sur un calque : son mode de fusion");
            ImGui::EndDragDropSource();
        }
        if (it.def && !it.def->graph.empty() && ImGui::BeginPopupContextItem("##ctx")) {
            if (ImGui::MenuItem("Éditer en graphe (sur le calque actif)", nullptr, false, layer != nullptr)) {
                Edit e(*d, v->history, "Fusion en graphe");
                layer->blendFx = {};
                layer->blendFx.graph = it.def->graph;
                if (auto nd = layer->blendFx.Def()) layer->blendFx.params = nd->DefaultParams();
                d->SyncGraph();
                e.Commit();
                OpenGraphEditor(1);
            }
            ImGui::EndPopup();
        }
        dl->AddRectFilled(p, ImVec2(p.x + cardW, p.y + cardH), current ? IM_COL32(60, 52, 110, 255) : hovered ? IM_COL32(44, 46, 56, 255) : IM_COL32(32, 33, 40, 255), 6);
        auto pv = B.previews.find(it.key);
        if (pv != B.previews.end() && v) {
            gpu::Texture* t = v->renderer.Thumbnail(*ctx, kBlendThumbBit | (HashString(it.key) & ((1ull << 57) - 1)), pv->second.get(), B.sourceHash ^ (u64)cardW,
                                                    (u32)cardW, (u32)cardW);
            dl->AddImageRounded((ImTextureID)gpu::Dev().UiTextureId(t), ImVec2(p.x + 3, p.y + 3), ImVec2(p.x + cardW - 3, p.y + cardW - 3), ImVec2(0, 0),
                                ImVec2(1, 1), IM_COL32_WHITE, 4);
        } else {
            const char* wait = layer ? "..." : "-";
            ImVec2 ts = ImGui::CalcTextSize(wait);
            dl->AddText(ImVec2(p.x + (cardW - ts.x) * 0.5f, p.y + (cardW - ts.y) * 0.5f), ui::col::TextDim, wait);
        }
        if (current) dl->AddRect(p, ImVec2(p.x + cardW, p.y + cardH), ui::col::Accent, 6, 0, 2.0f);
        ImVec4 clip(p.x + 4, p.y + cardW, p.x + cardW - 4, p.y + cardH);
        dl->AddText(nullptr, 0, ImVec2(p.x + 6, p.y + cardW + 2), ImGui::GetColorU32(ImGuiCol_Text), it.name.c_str(), nullptr, 0, &clip);
        dl->AddText(nullptr, 0, ImVec2(p.x + 6, p.y + cardW + fs + 3), IM_COL32(230, 120, 160, 255), it.category.c_str(), nullptr, 0, &clip);
        if (hovered) ImGui::SetTooltip("%s\n%s\nClic : mode de fusion du calque actif%s", it.name.c_str(), it.category.c_str(),
                                       it.def ? "\nGlisser sur un calque ou un nœud de calque" : "");
        ImGui::PopID();
    }
    if (items.empty()) ImGui::TextDisabled("Aucune fusion ne correspond.");
    ImGui::EndChild();

    // New blends / editing the current one.
    ImGui::BeginDisabled(!layer);
    if (ImGui::Button("Nouvelle (code)")) {
        Edit e(*d, v->history, "Fusion personnalisée");
        layer->blendFx = {};
        layer->blendFx.customCode = kCustomBlendTemplate;
        if (auto def = layer->blendFx.Def()) layer->blendFx.params = def->DefaultParams();
        layer->passThrough = false;
        d->SyncGraph();
        e.Commit();
        ShowSideTab(0);
        ImGui::SetWindowFocus("Propriétés");
    }
    ui::HelpTooltip("Fusion écrite en HLSL (A dessous, B calque, masque), éditée dans Propriétés.");
    ImGui::SameLine();
    if (ImGui::Button("Nouvelle (graphe)")) {
        Edit e(*d, v->history, "Fusion en graphe");
        layer->blendFx = {};
        layer->blendFx.graph = EffectGraph::DefaultBlend().Serialize();
        if (auto def = layer->blendFx.Def()) layer->blendFx.params = def->DefaultParams();
        layer->passThrough = false;
        d->SyncGraph();
        e.Commit();
        OpenGraphEditor(1);
    }
    ui::HelpTooltip("Fusion construite en nœuds dans l'éditeur visuel (zone du bas).");
    if (layer && !layer->blendFx.graph.empty()) {
        ImGui::SameLine();
        if (ImGui::Button("Éditer le graphe")) OpenGraphEditor(1);
    }
    ImGui::EndDisabled();
    ImGui::End();
}

}  // namespace sx
