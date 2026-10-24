// Effects browser (searchable grid with live previews computed on the active
// layer, favorites, categories) and the active layer preview panel.
#include "ui/App.h"
#include "ui/Lang.h"
#include <stb_image.h>
#include "ui/Theme.h"

#include <imgui_internal.h>
#include <imgui_stdlib.h>

#include <fstream>

namespace sx {

namespace {
constexpr u64 kFxThumbBit = 1ull << 61;
constexpr u64 kLayerPreviewKey = 1ull << 60;
constexpr u32 kPreviewSize = 160;

u64 HashString(const std::string& s) {
    u64 h = 1469598103934665603ull;
    for (unsigned char c : s) h = (h ^ c) * 1099511628211ull;
    return h;
}

std::string Lower(std::string s) {
    for (auto& c : s) c = (char)tolower((unsigned char)c);
    return s;
}

// Rendered content of the active layer (filters included) or of the active node.
gpu::Texture* ActiveContent(DocView& v, u64& hash) {
    Document& d = *v.doc;
    auto nodeOut = [&](Id id) -> gpu::Texture* {
        gpu::Texture* t = v.renderer.NodeOutput(id);
        if (t) hash = v.renderer.NodeHash(id);
        else if (const Node* n = d.S().graph.Find(id); n && n->kind == NodeKind::Surface)
            if (Surface* s = d.GetSurface(n->surface)) { t = s->tex.get(); hash = s->version; }
        return t;
    };
    if (d.Mode() == DocMode::Nodes) return d.S().activeNode ? nodeOut(d.S().activeNode) : nullptr;
    const Layer* l = d.ActiveLayer();
    if (!l) return nullptr;
    // The blend node of the layer receives its content (source + filters) on input 1.
    for (auto& n : d.S().graph.nodes)
        if (n.kind == NodeKind::Blend && n.layer == l->id && n.inputs.size() > 1 && n.inputs[1])
            if (gpu::Texture* t = nodeOut(n.inputs[1])) return t;
    if (Surface* s = d.GetSurface(l->surface)) { hash = s->version; return s->tex.get(); }
    return nullptr;
}
}  // namespace

gpu::Texture* ActiveLayerContent(DocView& v, u64& hash) { return ActiveContent(v, hash); }

gpu::Texture* LayerBelow(DocView& v, u64& hash) {
    // Input A of the active layer's blend node: everything under the layer.
    Document& d = *v.doc;
    const Layer* l = d.Mode() == DocMode::Layers ? d.ActiveLayer() : nullptr;
    if (!l) return nullptr;
    for (auto& n : d.S().graph.nodes)
        if (n.kind == NodeKind::Blend && n.layer == l->id && !n.inputs.empty() && n.inputs[0])
            if (gpu::Texture* t = v.renderer.NodeOutput(n.inputs[0])) {
                hash = v.renderer.NodeHash(n.inputs[0]);
                return t;
            }
    return nullptr;
}

// ------------------------------------------------------------------ applying an effect

void App::ApplyLibraryEffect(DocView& v, const std::string& id, FxApply how) {
    Document& d = *v.doc;
    auto def = Effects().Find(id);
    if (!def) return;
    {
        // Recently used effects (Effets panel > Récents).
        auto& r = fxBrowser.recent;
        r.erase(std::remove(r.begin(), r.end(), id), r.end());
        r.insert(r.begin(), id);
        if (r.size() > 24) r.resize(24);
        std::ofstream f(Utf8Path(ExeDir() + "/effect_recent.txt"));
        for (auto& x : r) f << x << "\n";
    }
    EffectInstance fx;
    fx.effectId = id;
    fx.params = def->DefaultParams();
    const bool layerMode = d.Mode() == DocMode::Layers;
    if (how == FxApply::Filter || how == FxApply::Destructive) {
        Json p = Json::Object();
        p["effect"] = id;
        p["destructive"] = how == FxApply::Destructive;
        RecordAction("filter", p);
    }
    if (how == FxApply::Destructive) {
        Id target = d.PaintTarget();
        if (!d.GetSurface(target)) { Status("Appliquer : sélectionnez un calque de pixels"); return; }
        if (def->inputs.empty()) { Status("Appliquer : cet effet est un générateur (utilisez un calque de remplissage)"); return; }
        if (v.renderer.ApplyEffectToSurface(d, *ctx, v.history, target, fx, def->name)) Status(def->name + " appliqué aux pixels");
        else Status("Effet en cours de compilation, réessayez");
        return;
    }
    if (how == FxApply::Adjustment && layerMode) {
        Edit e(d, v.history, def->inputs.empty() ? "Calque de remplissage" : "Calque de réglage");
        MaskFromSelection(v, d.AddEffectLayer(def->inputs.empty() ? LayerKind::Fill : LayerKind::Adjustment, id).id, false);
        e.Commit();
        return;
    }
    Edit e(d, v.history, "Filtre");
    if (layerMode) {
        Layer* l = d.ActiveLayer();
        if (!l) { Status("Aucun calque actif"); return; }
        if ((int)l->filters.size() >= kMaxLayerFilters) { Status("Ce calque a déjà " + std::to_string(kMaxLayerFilters) + " filtres (le maximum)"); return; }
        l->filters.push_back(fx);
        d.SyncGraph();
    } else {
        // Insert after the active node, rewiring its consumers.
        Id src = d.S().activeNode;
        Node* sn = d.S().graph.Find(src);
        float2 pos = sn ? sn->pos + float2{220, 0} : nodeEd.contextPos;
        Id nid = d.AddEffectNode(id, "", pos).id;
        if (src && src != d.S().graph.output && how != FxApply::Node) {
            for (auto& n : d.S().graph.nodes)
                for (auto& in : n.inputs) if (in == src && n.id != nid) in = nid;
            d.Link(src, nid, 0);
        }
    }
    e.Commit();
}

void App::DropEffectOnLayer(DocView& v, Id layer, const std::string& effectId) {
    Document& d = *v.doc;
    auto def = Effects().Find(effectId);
    Layer* l = d.FindLayer(layer);
    if (!def || !l) return;
    if (def->blend) {
        // A blend dropped on a layer becomes its blend mode.
        Edit e(d, v.history, "Mode de fusion");
        l->blendFx = {};
        l->blendFx.effectId = effectId;
        l->blendFx.params = def->DefaultParams();
        l->passThrough = false;
        d.SyncGraph();
        e.Commit();
        Status("Fusion « " + def->name + " » sur " + l->name);
        return;
    }
    if (def->inputs.empty()) {
        // Generator: new fill layer above.
        d.S().activeLayer = layer;
        ApplyLibraryEffect(v, effectId, FxApply::Adjustment);
        return;
    }
    Edit e(d, v.history, "Filtre");
    EffectInstance fx;
    fx.effectId = effectId;
    fx.params = def->DefaultParams();
    if ((int)l->filters.size() >= kMaxLayerFilters) { Status("Ce calque a déjà " + std::to_string(kMaxLayerFilters) + " filtres (le maximum)"); return; }
    l->filters.push_back(fx);
    d.S().activeLayer = layer;
    d.SyncGraph();
    e.Commit();
    Status(def->name + " : filtre ajouté à " + l->name);
}

// ------------------------------------------------------------------ reference image

// Reference images (N x N, sRGB 8 bits): 0 logo on a background, 1 color chart, 2 gradient (hues over a gray ramp),
// 3 checker, 4 white, 5 black.
std::vector<u8> App::ReferencePixels(int kind, int N) {
    std::vector<u8> px((size_t)N * N * 4);
    auto put = [&](int x, int y, float r, float g, float b) {
        u8* o = &px[((size_t)y * N + x) * 4];
        o[0] = (u8)std::clamp(r * 255.0f, 0.0f, 255.0f);
        o[1] = (u8)std::clamp(g * 255.0f, 0.0f, 255.0f);
        o[2] = (u8)std::clamp(b * 255.0f, 0.0f, 255.0f);
        o[3] = 255;
    };
    auto hsv = [](float h, float s, float val, float& r, float& g, float& b) {
        h -= std::floor(h);
        r = std::clamp(std::abs(h * 6 - 3) - 1, 0.0f, 1.0f);
        g = std::clamp(2 - std::abs(h * 6 - 2), 0.0f, 1.0f);
        b = std::clamp(2 - std::abs(h * 6 - 4), 0.0f, 1.0f);
        r = ((r - 1) * s + 1) * val;
        g = ((g - 1) * s + 1) * val;
        b = ((b - 1) * s + 1) * val;
    };
    if (kind == 1) {
        // Color chart: hues x brightness, a gray ramp, and swatches (primaries, secondaries, skin, sky).
        static const float sw[8][3] = {{0.85f, 0.1f, 0.1f}, {0.1f, 0.7f, 0.2f}, {0.1f, 0.3f, 0.85f}, {0.95f, 0.85f, 0.1f},
                                       {0.1f, 0.8f, 0.85f}, {0.85f, 0.15f, 0.8f}, {0.87f, 0.64f, 0.5f}, {0.35f, 0.6f, 0.9f}};
        for (int y = 0; y < N; ++y)
            for (int x = 0; x < N; ++x) {
                float r, g, b;
                if (y < N * 6 / 10) {
                    const float t = (float)y / (N * 0.6f);
                    hsv((float)x / N, std::min(1.0f, t * 2.0f), t < 0.5f ? 1.0f : 1.0f - (t - 0.5f) * 1.7f, r, g, b);
                } else if (y < N * 75 / 100) {
                    r = g = b = std::floor((float)x / N * 16) / 15.0f;
                } else {
                    const int k = std::min(7, x * 8 / N);
                    r = sw[k][0];
                    g = sw[k][1];
                    b = sw[k][2];
                }
                put(x, y, r, g, b);
            }
    } else if (kind == 2) {
        // Gradient: hues across, from white (top) through full color to black (bottom), a gray ramp at the bottom.
        for (int y = 0; y < N; ++y)
            for (int x = 0; x < N; ++x) {
                float r, g, b;
                const float u = (float)x / (N - 1), w = (float)y / (N - 1);
                if (w > 0.85f) r = g = b = u;
                else {
                    const float t = w / 0.85f;
                    hsv(u, std::min(1.0f, t * 2.0f), t < 0.5f ? 1.0f : 1.0f - (t - 0.5f) * 2.0f, r, g, b);
                }
                put(x, y, r, g, b);
            }
    } else if (kind == 3) {
        const int cell = std::max(1, N / 8);
        for (int y = 0; y < N; ++y)
            for (int x = 0; x < N; ++x) {
                const float c = ((x / cell + y / cell) & 1) ? 0.85f : 0.15f;
                put(x, y, c, c, c);
            }
    } else if (kind == 4 || kind == 5) {
        const float c = kind == 4 ? 1.0f : 0.0f;
        for (int y = 0; y < N; ++y)
            for (int x = 0; x < N; ++x) put(x, y, c, c, c);
    } else {
        // Logo on a background: gradient with a soft light, the Shaderixel logo in the middle.
        for (int y = 0; y < N; ++y)
            for (int x = 0; x < N; ++x) {
                const float u = (float)x / N, w = (float)y / N;
                const float dx = u - 0.35f, dy = w - 0.3f, glow = std::exp(-(dx * dx + dy * dy) * 6.0f);
                put(x, y, 0.08f + 0.25f * w + 0.25f * glow, 0.09f + 0.08f * w + 0.15f * glow, 0.2f + 0.3f * (1 - w) + 0.2f * glow);
            }
        int lw = 0, lh = 0;
        stbi_uc* logo = nullptr;
        for (const char* rel : {"/assets/logo.png", "/../../assets/logo.png"})
            if (!logo) logo = LoadImagePixels(ExeDir() + rel, lw, lh);
        if (logo && lw > 0 && lh > 0) {
            const int size = N * 7 / 10, x0 = (N - size) / 2, y0 = (N - size) / 2;
            for (int y = 0; y < size; ++y)
                for (int x = 0; x < size; ++x) {
                    const stbi_uc* s = &logo[((size_t)(y * lh / size) * lw + (x * lw / size)) * 4];
                    const float a = s[3] / 255.0f;
                    u8* o = &px[((size_t)(y0 + y) * N + x0 + x) * 4];
                    for (int c = 0; c < 3; ++c) o[c] = (u8)(s[c] * a + o[c] * (1 - a));
                }
            stbi_image_free(logo);
        }
    }
    return px;
}

gpu::Texture* App::ReferenceTexture(DocView& v) {
    auto& B = fxBrowser;
    const std::string key = std::to_string(B.refKind) + "|" + (B.refKind == 2 ? B.refPath : std::string());
    if (B.refTex && key == B.refKey) return B.refTex.get();
    const gpu::Format fmt = v.doc->GetPrecision() == Precision::Float32 ? gpu::Format::RGBA32F : gpu::Format::RGBA16F;
    if (B.refKind == 2 && !B.refPath.empty()) {
        int w = 0, h = 0;
        if (stbi_uc* px = LoadImagePixels(B.refPath, w, h)) {
            B.refTex = v.renderer.ImportTexture(*ctx, px, (u32)w, (u32)h, false, fmt);
            stbi_image_free(px);
            B.refKey = key;
            return B.refTex.get();
        }
    }
    const int N = 512;
    std::vector<u8> px = ReferencePixels(B.refKind == 1 ? 1 : 0, N);
    B.refTex = v.renderer.ImportTexture(*ctx, px.data(), N, N, false, fmt);
    B.refKey = key;
    return B.refTex.get();
}

// ------------------------------------------------------------------ effects browser

void App::DrawEffectBrowser() {
    if (!ImGui::Begin("Effets", PanelFlag("Effets"))) { ImGui::End(); return; }
    PopOutCorner("Effets");
    auto& B = fxBrowser;
    const std::string favPath = ExeDir() + "/effect_favorites.txt";
    if (!B.favLoaded) {
        B.favLoaded = true;
        std::ifstream f(Utf8Path(favPath));
        for (std::string line; std::getline(f, line);) if (!line.empty()) B.favorites.insert(line);
    }
    auto saveFavorites = [&] {
        std::ofstream f(Utf8Path(favPath));
        for (auto& id : B.favorites) f << id << "\n";
    };
    DocView* v = Active();

    // Search + filters.
    ImGui::SetNextItemWidth(-(std::floor(ImGui::GetFontSize() * 1.1f) + 8 * dpi));  // room for the pop-out icon
    ImGui::InputTextWithHint("##fxsearch", "Rechercher un effet (nom, catégorie)...", &B.search);
    auto chip = [&](const char* label, bool on) {
        if (on) ImGui::PushStyleColor(ImGuiCol_Button, ui::col::Accent);
        bool clicked = ImGui::SmallButton(label);
        if (on) ImGui::PopStyleColor();
        return clicked;
    };
    auto flowChip = [&](const char* label) {
        float w = ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2;
        if (ImGui::GetCursorPosX() > ImGui::GetStyle().WindowPadding.x + 1 && ImGui::GetContentRegionAvail().x < w + 4) ImGui::NewLine();
    };
    (void)chip;
    (void)flowChip;
    if (!B.recentLoaded) {
        B.recentLoaded = true;
        std::ifstream f(Utf8Path(ExeDir() + "/effect_recent.txt"));
        for (std::string line; std::getline(f, line);) if (!line.empty()) B.recent.push_back(line);
    }
    ImGui::SetNextItemWidth(130 * dpi);
    ImGui::Combo("##fxorigin", &B.origin, "Toutes sources\0Shaderixel\0CyTexture\0");
    ui::HelpTooltip("Effets de Shaderixel (et de vos bibliothèques) ou de la bibliothèque CyTexture importée.");
    ImGui::SameLine();
    ImGui::Checkbox("Variantes", &B.showVariants);
    ui::HelpTooltip("Affiche aussi les variantes et copies de test des effets importés (masquées par défaut).");
    // Preview source + size.
    ImGui::SetNextItemWidth(150 * dpi);
    ImGui::Combo("##fxsrc", &B.source, "Aperçu : calque actif\0Aperçu : image entière\0Aperçu : référence\0");
    ui::HelpTooltip("Les vignettes montrent l'effet appliqué au calque actif (filtres compris), à l'image entière, ou à une image de référence "
                    "(logo sur fond, mire de couleurs, ou votre image) pour juger l'effet sans dépendre du document.");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(std::max(60.0f, ImGui::GetContentRegionAvail().x));
    ImGui::SliderFloat("##fxsize", &B.thumb, 64, 220, "Taille %.0f");
    if (B.source == 2) {
        ImGui::SetNextItemWidth(150 * dpi);
        ImGui::Combo("##fxref", &B.refKind, "Logo sur fond\0Mire de couleurs\0Image...\0");
        ImGui::SameLine();
        if (ImGui::Button("Choisir une image...")) {
            std::string p = platform::OpenFileDialog(hwnd, L"Images\0*.png;*.jpg;*.jpeg;*.tga;*.bmp;*.psd;*.hdr;*.uasset\0");
            if (!p.empty()) { B.refPath = p; B.refKind = 2; }
        }
        if (B.refKind == 2 && !B.refPath.empty()) {
            ImGui::SameLine();
            ImGui::TextDisabled("%s", Narrow(Utf8Path(B.refPath).filename().wstring()).c_str());
        }
    }
    if (v) {
        Layer* l = v->doc->Mode() == DocMode::Layers ? v->doc->ActiveLayer() : nullptr;
        ImGui::TextColored(ImVec4(0.45f, 0.8f, 0.95f, 1), "Cible : %s", v->doc->Mode() == DocMode::Nodes ? "nœud actif (mode nodal)" : l ? l->name.c_str() : "aucun calque");
    }

    // Preview source: refreshed (throttled) when its content changes.
    u64 srcHash = 0;
    gpu::Texture* src = nullptr;
    if (v) {
        if (B.source == 0) src = ActiveContent(*v, srcHash);
        if (B.source == 2 && (src = ReferenceTexture(*v)) != nullptr) srcHash = std::hash<std::string>()(B.refKey) | 1;
        if (!src) { src = v->lastImage; srcHash = v->renderer.ImageVersion() ^ 0x5bd1e995; }
    }
    u64 frame = gpu::Dev().FrameNumber();
    if (src && srcHash != B.sourceHash && (frame - B.sourceFrame >= 20 || !B.sourceSmall)) {
        if (auto copy = v->renderer.PreviewCopy(*ctx, src, kPreviewSize, kPreviewSize)) {
            B.sourceSmall = copy;
            B.sourceHash = srcHash;
            B.sourceFrame = frame;
            B.previews.clear();
        }
    }

    // Filtered list.
    std::vector<EffectDefPtr> list;
    std::string q = Lower(B.search);
    for (auto& def : Effects().All()) {
        if (def->blend) continue;
        if (B.favoritesOnly && !B.favorites.count(def->id)) continue;
        if (B.recentOnly && std::find(B.recent.begin(), B.recent.end(), def->id) == B.recent.end()) continue;
        if (B.graphOnly && def->graph.empty()) continue;
        if (!B.family.empty() && EffectFamily(*def) != B.family) continue;
        if (B.origin && (B.origin == 2) != (std::string(EffectSource(*def)) == "CyTexture")) continue;
        if (!B.showVariants && EffectIsVariant(*def) && q.empty()) continue;
        if (!q.empty() && Lower(def->name).find(q) == std::string::npos && Lower(def->category).find(q) == std::string::npos &&
            Lower(def->id).find(q) == std::string::npos)
            continue;
        list.push_back(def);
    }
    if (B.recentOnly)  // most recent first
        std::stable_sort(list.begin(), list.end(), [&](const EffectDefPtr& a, const EffectDefPtr& b) {
            return std::find(B.recent.begin(), B.recent.end(), a->id) < std::find(B.recent.begin(), B.recent.end(), b->id);
        });

    // Previews: a few per frame, effects not yet compiled are retried.
    int budget = 3;
    if (v && B.sourceSmall)
        for (auto& def : list) {
            if (budget <= 0) break;
            if (B.previews.count(def->id)) continue;
            EffectInstance fx;
            fx.effectId = def->id;
            fx.params = def->DefaultParams();
            if (auto out = v->renderer.ApplyEffect(*v->doc, *ctx, fx, B.sourceSmall.get(), kPreviewSize, kPreviewSize)) B.previews[def->id] = out;
            --budget;
        }

    // Families on the left (with counts), the effects of the chosen one on the right.
    ImGui::Separator();
    const float bottom = B.selected.empty() ? 0 : -ImGui::GetFrameHeightWithSpacing() * 2.2f;
    const bool side = ImGui::GetContentRegionAvail().x > 380 * dpi;
    if (side) {
        std::map<std::string, int> counts;
        int total = 0, graphs = 0;
        for (auto& def : Effects().All()) {
            if (def->blend || (!B.showVariants && EffectIsVariant(*def))) continue;
            if (B.origin && (B.origin == 2) != (std::string(EffectSource(*def)) == "CyTexture")) continue;
            ++counts[EffectFamily(*def)];
            ++total;
            if (!def->graph.empty()) ++graphs;
        }
        ImGui::BeginChild("##fxfam", ImVec2(170 * dpi, bottom), ImGuiChildFlags_Borders);
        auto entry = [&](const std::string& label, bool on) { return ImGui::Selectable(label.c_str(), on); };
        if (entry("Tous (" + std::to_string(total) + ")", B.family.empty() && !B.favoritesOnly && !B.recentOnly && !B.graphOnly)) {
            B.family.clear();
            B.favoritesOnly = B.recentOnly = B.graphOnly = false;
        }
        if (entry("\xE2\x98\x85 Favoris (" + std::to_string(B.favorites.size()) + ")", B.favoritesOnly)) {
            B.favoritesOnly = !B.favoritesOnly;
            B.recentOnly = B.graphOnly = false;
            B.family.clear();
        }
        if (entry("Récents (" + std::to_string(B.recent.size()) + ")", B.recentOnly)) {
            B.recentOnly = !B.recentOnly;
            B.favoritesOnly = B.graphOnly = false;
            B.family.clear();
        }
        if (entry("Effets en graphe (" + std::to_string(graphs) + ")", B.graphOnly)) {
            B.graphOnly = !B.graphOnly;
            B.favoritesOnly = B.recentOnly = false;
            B.family.clear();
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Effets construits avec des nœuds (badge violet sur la vignette).\nClic droit sur un effet > Ouvrir son graphe : copie modifiable dans l'Éditeur d'effets.");
        ImGui::Separator();
        for (const std::string& f : EffectFamilies()) {
            auto it = counts.find(f);
            if (it == counts.end()) continue;
            if (entry(Tr(f) + " (" + std::to_string(it->second) + ")", B.family == f)) {
                B.family = B.family == f ? "" : f;
                B.favoritesOnly = B.recentOnly = B.graphOnly = false;
            }
        }
        ImGui::EndChild();
        ImGui::SameLine();
    } else {
        // Narrow panel: the families in a list.
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##fxfamc", B.favoritesOnly ? "Favoris" : B.recentOnly ? "Récents" : B.graphOnly ? "Effets en graphe" : B.family.empty() ? "Toutes les familles" : B.family.c_str())) {
            if (ImGui::Selectable("Toutes les familles", B.family.empty() && !B.favoritesOnly && !B.recentOnly && !B.graphOnly)) { B.family.clear(); B.favoritesOnly = B.recentOnly = B.graphOnly = false; }
            if (ImGui::Selectable("Favoris", B.favoritesOnly)) { B.favoritesOnly = true; B.recentOnly = B.graphOnly = false; B.family.clear(); }
            if (ImGui::Selectable("Récents", B.recentOnly)) { B.recentOnly = true; B.favoritesOnly = B.graphOnly = false; B.family.clear(); }
            if (ImGui::Selectable("Effets en graphe", B.graphOnly)) { B.graphOnly = true; B.favoritesOnly = B.recentOnly = false; B.family.clear(); }
            for (const std::string& f : EffectFamilies())
                if (ImGui::Selectable(f.c_str(), B.family == f)) { B.family = f; B.favoritesOnly = B.recentOnly = B.graphOnly = false; }
            ImGui::EndCombo();
        }
    }
    ImGui::BeginChild("##fxgrid", ImVec2(0, bottom), ImGuiChildFlags_None);
    const float fs = ImGui::GetFontSize();
    const float cardW = B.thumb * dpi, cardH = cardW + fs * 2.4f;
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    int cols = std::max(1, (int)((ImGui::GetContentRegionAvail().x + spacing) / (cardW + spacing)));
    ImDrawList* dl = ImGui::GetWindowDrawList();
    std::string applyNow;
    for (size_t i = 0; i < list.size(); ++i) {
        const EffectDef& def = *list[i];
        if (i % cols) ImGui::SameLine();
        ImGui::PushID(def.id.c_str());
        ImVec2 p = ImGui::GetCursorScreenPos();
        bool selected = B.selected == def.id;
        if (ImGui::InvisibleButton("##card", ImVec2(cardW, cardH))) B.selected = def.id;
        bool hovered = ImGui::IsItemHovered();
        if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) applyNow = def.id;
        // Drag to the graph (new node, on a link: inserted, on a layer node: filter), the layers or the canvas.
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            ImGui::SetDragDropPayload(kEffectPayload, def.id.c_str(), def.id.size() + 1);
            ImGui::TextUnformatted(def.name.c_str());
            ImGui::TextDisabled("Graphe : nœud (sur un lien : inséré)  |  Calque : filtre");
            ImGui::EndDragDropSource();
        }
        if (ImGui::BeginPopupContextItem("##ctx")) {
            B.selected = def.id;
            if (v) {
                bool layerMode = v->doc->Mode() == DocMode::Layers;
                if (ImGui::MenuItem(layerMode ? "Ajouter comme filtre (non destructif)" : "Insérer après le nœud actif")) ApplyLibraryEffect(*v, def.id, FxApply::Filter);
                if (layerMode && ImGui::MenuItem(def.inputs.empty() ? "Nouveau calque de remplissage" : "Nouveau calque de réglage"))
                    ApplyLibraryEffect(*v, def.id, FxApply::Adjustment);
                if (!layerMode && ImGui::MenuItem("Nouveau nœud (non relié)")) ApplyLibraryEffect(*v, def.id, FxApply::Node);
                if (ImGui::MenuItem("Appliquer aux pixels (destructif)", nullptr, false, !def.inputs.empty())) ApplyLibraryEffect(*v, def.id, FxApply::Destructive);
                ImGui::Separator();
                if (ImGui::MenuItem(def.graph.empty() ? "Ouvrir le code (copie modifiable)" : "Ouvrir son graphe (copie modifiable)")) {
                    OpenLibraryItem(0, def.id, false);
                    FocusPanel("Éditeur d'effets");
                }
            }
            ImGui::Separator();
            bool fav = B.favorites.count(def.id) != 0;
            if (ImGui::MenuItem(fav ? "Retirer des favoris" : "Ajouter aux favoris")) {
                if (fav) B.favorites.erase(def.id); else B.favorites.insert(def.id);
                saveFavorites();
            }
            ImGui::EndPopup();
        }
        // Card.
        ImVec2 ta = p, tb(p.x + cardW, p.y + cardW);
        dl->AddRectFilled(p, ImVec2(p.x + cardW, p.y + cardH), selected ? IM_COL32(60, 52, 110, 255) : hovered ? IM_COL32(44, 46, 56, 255) : IM_COL32(32, 33, 40, 255), 6);
        auto it = B.previews.find(def.id);
        if (it != B.previews.end() && v) {
            gpu::Texture* t = v->renderer.Thumbnail(*ctx, kFxThumbBit | (HashString(def.id) & ((1ull << 60) - 1)), it->second.get(),
                                                    B.sourceHash ^ (u64)cardW, (u32)cardW, (u32)cardW);
            dl->AddImageRounded((ImTextureID)gpu::Dev().UiTextureId(t), ImVec2(ta.x + 3, ta.y + 3), ImVec2(tb.x - 3, tb.y - 3), ImVec2(0, 0), ImVec2(1, 1),
                                IM_COL32_WHITE, 4);
        } else {
            const char* wait = "...";
            ImVec2 ts = ImGui::CalcTextSize(wait);
            dl->AddText(ImVec2(p.x + (cardW - ts.x) * 0.5f, p.y + (cardW - ts.y) * 0.5f), ui::col::TextDim, wait);
        }
        if (selected) dl->AddRect(p, ImVec2(p.x + cardW, p.y + cardH), ui::col::Accent, 6, 0, 2.0f);
        // Graph effect: two linked nodes (top left).
        if (!def.graph.empty()) {
            const float s = fs * 0.17f, bw = fs * 1.7f, bh = fs * 1.05f, m = fs * 0.15f;
            ImVec2 bp(p.x + 6, p.y + 6);
            dl->AddRectFilled(bp, ImVec2(bp.x + bw, bp.y + bh), IM_COL32(20, 20, 26, 210), 3);
            ImVec2 n1(bp.x + m, bp.y + m), n2(bp.x + bw - m - s * 2, bp.y + bh - m - s * 2);
            dl->AddBezierCubic(ImVec2(n1.x + s * 2, n1.y + s), ImVec2(n1.x + s * 3.2f, n1.y + s), ImVec2(n2.x - s * 1.2f, n2.y + s), ImVec2(n2.x, n2.y + s),
                               IM_COL32(190, 150, 255, 255), 1.5f);
            dl->AddRectFilled(n1, ImVec2(n1.x + s * 2, n1.y + s * 2), IM_COL32(170, 120, 255, 255), 1.5f);
            dl->AddRectFilled(n2, ImVec2(n2.x + s * 2, n2.y + s * 2), IM_COL32(170, 120, 255, 255), 1.5f);
        }
        // Favorite star (top right).
        bool fav = B.favorites.count(def.id) != 0;
        ImVec2 sc(p.x + cardW - fs * 0.9f, p.y + fs * 0.9f);
        bool starHot = ImGui::IsMouseHoveringRect(ImVec2(sc.x - fs * 0.7f, sc.y - fs * 0.7f), ImVec2(sc.x + fs * 0.7f, sc.y + fs * 0.7f));
        ImVec2 star[10];
        for (int k = 0; k < 10; ++k) {
            float r = (k % 2 ? 0.28f : 0.62f) * fs, a = -1.5707963f + k * 0.6283185f;
            star[k] = ImVec2(sc.x + std::cos(a) * r, sc.y + std::sin(a) * r);
        }
        if (fav) dl->AddConcavePolyFilled(star, 10, IM_COL32(255, 200, 70, 255));
        else dl->AddPolyline(star, 10, starHot ? IM_COL32_WHITE : IM_COL32(170, 170, 180, 200), ImDrawFlags_Closed, 1.5f);
        if (starHot && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            if (fav) B.favorites.erase(def.id); else B.favorites.insert(def.id);
            saveFavorites();
        }
        // Name + category, clipped to the card.
        ImVec4 clip(p.x + 4, p.y + cardW, p.x + cardW - 4, p.y + cardH);
        dl->AddText(nullptr, 0, ImVec2(p.x + 6, p.y + cardW + 2), ImGui::GetColorU32(ImGuiCol_Text), def.name.c_str(), nullptr, 0, &clip);
        const std::string fam = EffectFamily(def);
        const bool cy = std::string(EffectSource(def)) == "CyTexture";
        dl->AddText(nullptr, 0, ImVec2(p.x + 6, p.y + cardW + fs + 3), cy ? IM_COL32(120, 190, 240, 255) : IM_COL32(230, 170, 70, 255),
                    Tr(B.family.empty() ? fam : def.category).c_str(), nullptr, 0, &clip);
        if (hovered) ImGui::SetTooltip("%s\n%s  -  %s (%s)%s\nDouble-clic : %s\nClic droit : autres façons de l'appliquer", def.name.c_str(), fam.c_str(),
                                       def.category.c_str(), EffectSource(def), def.graph.empty() ? "" : "\nEffet en graphe (clic droit > Ouvrir son graphe)",
                                       v && v->doc->Mode() == DocMode::Nodes ? "insérer après le nœud actif" : "ajouter comme filtre au calque actif");
        ImGui::PopID();
    }
    if (list.empty()) ImGui::TextDisabled("Aucun effet ne correspond.");
    ImGui::EndChild();
    if (!applyNow.empty() && v) ApplyLibraryEffect(*v, applyNow, FxApply::Filter);

    // Actions for the selected effect.
    if (!B.selected.empty() && v)
        if (auto def = Effects().Find(B.selected)) {
            bool layerMode = v->doc->Mode() == DocMode::Layers;
            ImGui::TextUnformatted(def->name.c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("(%s)", def->category.c_str());
            if (ImGui::Button(layerMode ? "Filtre" : "Insérer")) ApplyLibraryEffect(*v, def->id, FxApply::Filter);
            ui::HelpTooltip(layerMode ? "Filtre non destructif sur le calque actif (réglable dans Propriétés)" : "Nouveau nœud inséré après le nœud actif");
            ImGui::SameLine();
            if (layerMode) {
                if (ImGui::Button(def->inputs.empty() ? "Calque de remplissage" : "Calque de réglage")) ApplyLibraryEffect(*v, def->id, FxApply::Adjustment);
            } else if (ImGui::Button("Nœud seul")) {
                ApplyLibraryEffect(*v, def->id, FxApply::Node);
            }
            ImGui::SameLine();
            ImGui::BeginDisabled(def->inputs.empty());
            if (ImGui::Button("Appliquer aux pixels")) ApplyLibraryEffect(*v, def->id, FxApply::Destructive);
            ImGui::EndDisabled();
            ui::HelpTooltip("Comme Photoshop : l'effet modifie directement les pixels du calque (dans la sélection), annulable");
        }
    ImGui::End();
}

// ------------------------------------------------------------------ active layer preview

void App::DrawLayerPreview() {
    if (!ImGui::Begin("Aperçu du calque", PanelFlag("Aperçu du calque"))) { ImGui::End(); return; }
    DocView* v = Active();
    if (!v) { ImGui::TextDisabled("Aucun document"); ImGui::End(); return; }
    Document& d = *v->doc;
    static int what = 0;
    Layer* l = d.Mode() == DocMode::Layers ? d.ActiveLayer() : nullptr;
    bool hasMask = l && l->mask;
    // Layer mode with a node selected in the graph: that node's result (blend, filter, mask, output...).
    const Node* selNode = d.Mode() == DocMode::Layers ? d.S().graph.Find(d.S().activeNode) : nullptr;
    if (selNode && selNode->layer && l && selNode->layer != l->id) selNode = nullptr;  // stale: another layer is active now
    if (selNode) {
        ImGui::TextDisabled("Nœud : %s", selNode->name.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Voir le calque")) d.S().activeNode = 0;
        u64 nh = 0;
        gpu::Texture* nt = v->renderer.NodeOutput(selNode->id);
        if (nt) nh = v->renderer.NodeHash(selNode->id);
        else if (selNode->kind == NodeKind::Output && v->lastImage) { nt = v->lastImage; nh = v->renderer.ImageVersion(); }
        else if (selNode->kind == NodeKind::Surface)
            if (Surface* s = d.GetSurface(selNode->surface)) { nt = s->tex.get(); nh = s->version; }
        ImVec2 av = ImGui::GetContentRegionAvail();
        if (!nt || av.x < 8 || av.y < 8) {
            ImGui::TextDisabled("Rien à afficher pour ce nœud");
            ImGui::End();
            return;
        }
        const float a = (float)nt->Width() / nt->Height();
        float w = av.x, h = w / a;
        if (h > av.y) { h = av.y; w = h * a; }
        const u32 tw = (u32)std::max(1.0f, w), th = (u32)std::max(1.0f, h);
        gpu::Texture* t = v->renderer.Thumbnail(*ctx, kLayerPreviewKey, nt, nh ^ ((u64)tw << 40) ^ ((u64)th << 20) ^ ((u64)selNode->id << 8) ^ 7, tw, th);
        ImVec2 p = ImGui::GetCursorScreenPos();
        p.x += (av.x - w) * 0.5f;
        ImGui::SetCursorScreenPos(p);
        ImGui::Image((ImTextureID)gpu::Dev().UiTextureId(t), ImVec2(w, h));
        ImGui::End();
        return;
    }
    if (d.Mode() == DocMode::Layers) {
        ImGui::RadioButton("Calque", &what, 0);
        ImGui::SameLine();
        ImGui::BeginDisabled(!hasMask);
        ImGui::RadioButton("Masque", &what, 1);
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextDisabled("%s", l ? l->name.c_str() : "aucun calque");
    } else {
        const Node* n = d.S().graph.Find(d.S().activeNode);
        ImGui::TextDisabled("Nœud : %s", n ? n->name.c_str() : "aucun");
    }
    u64 hash = 0;
    gpu::Texture* src = nullptr;
    if (what == 1 && hasMask) {
        if (Surface* s = d.GetSurface(l->mask)) { src = s->tex.get(); hash = s->version; }
    } else {
        src = ActiveContent(*v, hash);
    }
    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (!src || avail.x < 8 || avail.y < 8) {
        ImGui::TextDisabled(d.tiled ? "Aperçu indisponible (grand document)" : "Rien à afficher");
        ImGui::End();
        return;
    }
    float aspect = (float)src->Width() / src->Height();
    float w = avail.x, h = w / aspect;
    if (h > avail.y) { h = avail.y; w = h * aspect; }
    u32 tw = (u32)std::max(1.0f, w), th = (u32)std::max(1.0f, h);
    gpu::Texture* t = v->renderer.Thumbnail(*ctx, kLayerPreviewKey, src, hash ^ ((u64)tw << 40) ^ ((u64)th << 20) ^ (u64)what, tw, th);
    ImVec2 p = ImGui::GetCursorScreenPos();
    p.x += (avail.x - w) * 0.5f;
    ImGui::SetCursorScreenPos(p);
    ImGui::Image((ImTextureID)gpu::Dev().UiTextureId(t), ImVec2(w, h));
    ImGui::End();
}

}  // namespace sx
