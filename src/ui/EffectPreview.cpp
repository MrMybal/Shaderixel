// Effect previews: simulated inputs (reference images, the active layer, a file) run through an effect,
// for the effect editor (output or a node) and its library page.
#include "ui/App.h"
#include "ui/Theme.h"

#include <imgui_internal.h>
#include <stb_image.h>

namespace sx {

namespace {
constexpr int kRefSize = 512;
// Combo order of the sources.
const char* kSourceNames = "Logo sur fond\0Mire de couleurs\0Dégradé\0Damier\0Blanc\0Noir\0Calque actif\0Image entière\0Image (fichier)...\0";
enum { kSrcLogo, kSrcChart, kSrcGradient, kSrcChecker, kSrcWhite, kSrcBlack, kSrcLayer, kSrcImage, kSrcFile, kSrcCount };

u64 Mix(u64 h, u64 v) { return (h ^ v) * 1099511628211ull; }
}  // namespace

gpu::Texture* App::PreviewSource(DocView& v, int input, u64& hash) {
    auto& P = efPv;
    input = std::clamp(input, 0, 3);
    const int kind = std::clamp(P.src[input], 0, kSrcCount - 1);
    const gpu::Format fmt = v.doc->GetPrecision() == Precision::Float32 ? gpu::Format::RGBA32F : gpu::Format::RGBA16F;
    if (kind == kSrcLayer || kind == kSrcImage) {
        // Live document content, reduced (refreshed when it changes, at most every few frames).
        u64 h = 0;
        gpu::Texture* t = kind == kSrcLayer ? ActiveLayerContent(v, h) : v.lastImage;
        if (kind == kSrcImage) h = v.renderer.ImageVersion() ^ 0x5bd1e995;
        if (!t) { hash = 0; return nullptr; }
        auto& live = P.live[input];
        const u64 frame = gpu::Dev().FrameNumber();
        if (!live.tex || (live.hash != h && frame - live.frame >= 10)) {
            const u32 w = kRefSize, hgt = std::max<u32>(1, (u32)((u64)kRefSize * t->Height() / std::max<u32>(1, t->Width())));
            if (auto copy = v.renderer.PreviewCopy(*ctx, t, w, hgt)) {
                live.tex = copy;
                live.hash = h;
                live.frame = frame;
            }
        }
        hash = Mix(live.hash, (u64)kind);
        return live.tex.get();
    }
    std::string key = std::to_string(kind) + (kind == kSrcFile ? "|" + P.path[input] : std::string());
    auto it = P.refs.find(key);
    if (it == P.refs.end()) {
        gpu::TexturePtr tex;
        if (kind == kSrcFile && !P.path[input].empty()) {
            int w = 0, h = 0;
            if (stbi_uc* px = LoadImagePixels(P.path[input], w, h)) {
                tex = v.renderer.ImportTexture(*ctx, px, (u32)w, (u32)h, false, fmt);
                stbi_image_free(px);
            }
        }
        if (!tex) {
            static const int refKind[] = {0, 1, 2, 3, 4, 5};
            std::vector<u8> px = ReferencePixels(kind < 6 ? refKind[kind] : 0, kRefSize);
            tex = v.renderer.ImportTexture(*ctx, px.data(), kRefSize, kRefSize, false, fmt);
        }
        it = P.refs.emplace(key, tex).first;
    }
    hash = std::hash<std::string>()(key) | 1;
    return it->second.get();
}

gpu::Texture* App::RenderEffectPreview(DocView& v, const EffectInstance& fx, int inputs, u64 slot, bool* waiting) {
    auto& P = efPv;
    auto& S = P.slots[slot];
    inputs = std::clamp(inputs, 0, 4);
    std::array<gpu::Texture*, 4> in{};
    u64 h = 1469598103934665603ull;
    for (int i = 0; i < std::max(1, inputs); ++i) {
        u64 sh = 0;
        in[i] = PreviewSource(v, i, sh);
        h = Mix(h, sh);
    }
    h = Mix(h, std::hash<std::string>()(fx.graph.empty() ? fx.effectId + "|" + fx.customCode : fx.graph));
    for (const float4& p : fx.params) {
        u64 bits[2];
        memcpy(bits, &p, sizeof(bits));
        h = Mix(Mix(h, bits[0]), bits[1]);
    }
    h = Mix(h, (u64)inputs);
    if (!in[0]) {
        if (waiting) *waiting = false;
        return S.out.get();
    }
    if (S.hash != h) {
        gpu::TexturePtr out;
        const u32 rw = in[0]->Width(), rh = in[0]->Height();
        if (inputs == 0) out = v.renderer.ApplyEffect(*v.doc, *ctx, fx, in[0], rw, rh);  // generator: the size only
        else out = v.renderer.ApplyEffectInputs(*v.doc, *ctx, fx, in, rw, rh);
        if (out) {
            S.out = out;
            S.hash = h;
        }
    }
    if (waiting) *waiting = S.hash != h;
    return S.out.get();
}

void App::DrawPreviewSources(DocView& v, int inputs, bool blend) {
    auto& P = efPv;
    if (inputs == 0) {
        ImGui::TextDisabled("Générateur : pas d'image d'entrée.");
        return;
    }
    static const char* blendNames[] = {"A (dessous)", "B (calque)", "Masque", "4"};
    for (int i = 0; i < std::min(inputs, 4); ++i) {
        ImGui::PushID(i);
        char label[48];
        if (blend) snprintf(label, sizeof(label), "%s", blendNames[i]);
        else snprintf(label, sizeof(label), "Entrée %d", i + 1);
        ImGui::SetNextItemWidth(-ImGui::CalcTextSize(label).x - ImGui::GetStyle().ItemInnerSpacing.x - 4);
        int k = P.src[i];
        if (ImGui::Combo(label, &k, kSourceNames)) {
            if (k == kSrcFile) {
                std::string p = platform::OpenFileDialog(hwnd, L"Images\0*.png;*.jpg;*.jpeg;*.tga;*.bmp;*.psd;*.hdr;*.tif;*.tiff\0");
                if (!p.empty()) {
                    P.path[i] = p;
                    P.src[i] = k;
                }
            } else {
                P.src[i] = k;
            }
        }
        if (P.src[i] == kSrcFile && !P.path[i].empty() && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", P.path[i].c_str());
        ImGui::PopID();
    }
    (void)v;
}

void App::DrawPreviewImage(DocView& v, gpu::Texture* t, u64 key, u64 hash, float width, float maxHeight) {
    const float aspect = t && t->Width() ? (float)t->Height() / t->Width() : 1.0f;
    float w = width, h = width * aspect;
    if (maxHeight > 0 && h > maxHeight) {
        h = maxHeight;
        w = h / aspect;
    }
    ImVec2 p = ImGui::GetCursorScreenPos();
    p.x += std::max(0.0f, (width - w) * 0.5f);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    // Checker behind: transparency shows.
    const float cell = 8.0f;
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), IM_COL32(60, 60, 66, 255));
    dl->PushClipRect(p, ImVec2(p.x + w, p.y + h), true);
    for (float y = 0; y < h; y += cell)
        for (float x = ((int)(y / cell) & 1) ? cell : 0; x < w; x += cell * 2)
            dl->AddRectFilled(ImVec2(p.x + x, p.y + y), ImVec2(p.x + x + cell, p.y + y + cell), IM_COL32(90, 90, 98, 255));
    dl->PopClipRect();
    if (t) {
        gpu::Texture* shown = v.renderer.Thumbnail(*ctx, key, t, hash ^ ((u64)w << 20), (u32)std::max(1.0f, w), (u32)std::max(1.0f, h));
        if (shown) dl->AddImage((ImTextureID)gpu::Dev().UiTextureId(shown), p, ImVec2(p.x + w, p.y + h));
    }
    dl->AddRect(p, ImVec2(p.x + w, p.y + h), ui::col::Border);
    ImGui::Dummy(ImVec2(width, h));
}

}  // namespace sx
