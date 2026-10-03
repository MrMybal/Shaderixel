// Programmable brushes on the GPU (see doc/Brushes.h and shaders/brush_common.slang).
#include "render/Renderer.h"
#include "render/Dispatch.h"
#include "doc/Brushes.h"

#include <cmath>
#include <cstring>

namespace sx {

using gpu::Shaders;

namespace {
struct BrushProgRoot {
    i32 rx, ry;
    u32 rw, rh, dw, dh, count, base, mode, flags;
};
struct ClearRootB { i32 x, y; u32 w, h; float4 value; };
struct ClearRRootB { u32 w, h; float value; };
}  // namespace

const gpu::ComputePipeline* Renderer::BrushPipeline(const BrushDef& b, std::string* errors) {
    std::string src = BuildBrushSource(b);
    Hasher h;
    h.Bytes(src.data(), src.size());
    auto it = brushPrograms.find(h.h);
    gpu::ProgramPtr prog;
    if (it != brushPrograms.end()) prog = it->second;
    else {
        prog = Shaders().Request(b.def ? b.def->name : b.id, src);
        brushPrograms[h.h] = prog;
    }
    auto st = prog->state.load();
    if (st == gpu::Program::State::Failed) {
        if (errors) *errors = prog->errors;
        return nullptr;
    }
    if (errors) errors->clear();
    return st == gpu::Program::State::Ready ? prog->pipeline.get() : nullptr;
}

void Renderer::RunBrush(gpu::CommandContext& ctx, const gpu::ComputePipeline* pipe, const BrushDef& b, const EffectInstance& tip,
                        const std::vector<Dab>& dabs, const std::vector<float4>& extra, u32 base, const BrushSettings& s, float4 color,
                        bool erase, const BrushTargets& t, Rect* touched) {
    // cbuffer: color, settings, parameters, then the dabs.
    auto params = PackParams(*b.def, tip.params);
    params.insert(params.begin(), {color, float4{s.opacity, s.hardness, s.flow, 0.0f}});
    const float reach = b.extent * 1.4143f;  // rotated square of `extent` radii
    for (size_t start = 0; start < dabs.size(); start += kMaxBrushDabs) {
        size_t count = std::min<size_t>(kMaxBrushDabs, dabs.size() - start);
        std::vector<float4> cb = params;
        cb.resize(params.size() + kMaxBrushDabs * 3);
        std::vector<Dab> bounds;
        Rect r;
        const bool pen = !dabs.empty() && extra.size() >= dabs.size() * 2;  // interleaved (stroke, pen) per dab
        for (size_t i = 0; i < count; ++i) {
            const Dab& d = dabs[start + i];
            size_t k = start + i;
            cb[params.size() + i * 3] = {d.x, d.y, d.radius, d.alpha};
            cb[params.size() + i * 3 + 1] = pen ? extra[k * 2] : k < extra.size() ? extra[k] : float4{1, 0, 0, 0};
            cb[params.size() + i * 3 + 2] = pen ? extra[k * 2 + 1] : float4{0, 0, 0, 0};
            float rr = d.radius * reach + 2;
            bounds.push_back({d.x, d.y, rr, 0});
            r.Merge(Rect{(int)std::floor(d.x - rr), (int)std::floor(d.y - rr), (int)std::ceil(d.x + rr), (int)std::ceil(d.y + rr)});
        }
        r = r.Clipped((int)t.w, (int)t.h);
        if (r.Empty()) continue;
        if (touched) touched->Merge(r);
        gpu::DispatchDesc d;
        d.pipeline = pipe;
        d.srv[0] = t.pre;
        d.srv[1] = t.sel;
        d.uav[0] = t.layer;
        d.uav[1] = t.mask;
        d.uav[2] = t.color;
        u32 texBits = 0;
        for (int k = 0; k < 4; ++k)
            if (s.tipTexBound[k]) {
                d.srv[2 + k] = s.tipTexBound[k];
                texBits |= 4u << k;
            }
        if (erase) d.uavNoMap |= 1;
        MapDabs(d, 7, (const float4*)bounds.data(), bounds.size(), sizeof(Dab), 0);
        BrushProgRoot root = {r.x0, r.y0, (u32)r.Width(), (u32)r.Height(), t.w, t.h, (u32)count, base + (u32)start,
                              erase ? 1u : 0u, (t.sel ? 1u : 0u) | (t.preview ? 2u : 0u) | texBits};
        SetConstants(d, root);
        d.cbData = cb.data();
        d.cbSize = (params.size() + count * 3) * sizeof(float4);
        Dispatch2D(ctx, d, (u32)r.Width(), (u32)r.Height());
    }
}

bool Renderer::StrokeProgram(Document& doc, gpu::CommandContext& ctx, const std::vector<Dab>& dabs, const std::vector<float4>& extra,
                             const BrushDef& b, const EffectInstance& tip, const BrushSettings& s, float4 color, bool erase, u32 base) {
    if (!strokeSurface || dabs.empty() || !b.def) return false;
    Surface* surf = doc.GetSurface(strokeSurface);
    const gpu::ComputePipeline* pipe = BrushPipeline(b);
    auto* clear = Kernel("clear.slang");
    if (!surf || !pipe || !clear) return false;
    const u32 W = surf->tex->Width(), H = surf->tex->Height();
    // Stroke color buffer (tips can vary the color), cleared once per stroke.
    if (!strokeColorReady) {
        Ensure(strokeColor, W, H, gpu::Format::RGBA16F, "StrokeColor");
        gpu::DispatchDesc c;
        c.pipeline = clear;
        c.uav[0] = strokeColor.get();
        c.uavNoMap = 1;
        SetConstants(c, ClearRootB{0, 0, W, H, {0, 0, 0, 0}});
        Dispatch2D(ctx, c, W, H);
        strokeColorReady = true;
    }
    Surface* sel = doc.Selection();
    BrushTargets t{strokePre.get(), sel ? sel->tex.get() : nullptr, surf->tex.get(), strokeMask.get(), strokeColor.get(), W, H, false};
    Rect touched;
    RunBrush(ctx, pipe, b, tip, dabs, extra, base, s, color, erase, t, &touched);
    strokeRect.Merge(touched);
    doc.TouchSurface(strokeSurface, &touched);
    return true;
}

gpu::Texture* Renderer::BrushPreview(gpu::CommandContext& ctx, const BrushDef& b, const EffectInstance& tip, const BrushSettings& s,
                                     float4 color, u32 w, u32 h, std::string* errors) {
    const gpu::ComputePipeline* pipe = BrushPipeline(b, errors);
    auto* clear = Kernel("clear.slang");
    auto* clearR = Kernel("clear_r.slang");
    if (!pipe || !clear || !clearR) return previewLayer ? previewLayer.get() : nullptr;
    Hasher hh;
    hh.Bytes(b.text.data(), b.text.size());
    auto packed = PackParams(*b.def, tip.params);
    hh.Bytes(packed.data(), packed.size() * sizeof(float4));
    hh.Pod(s.size);
    hh.Pod(s.hardness);
    hh.Pod(s.flow);
    hh.Pod(s.spacing);
    hh.Pod(s.opacity);
    hh.Pod(color);
    hh.Pod(w);
    hh.Pod(h);
    for (auto* tx : s.tipTexBound) hh.Pod(tx);
    if (previewHash == hh.h && previewLayer) return previewLayer.get();
    Ensure(previewLayer, w, h, gpu::Format::RGBA16F, "BrushPreview");
    Ensure(previewMask, w, h, gpu::Format::R32F, "BrushPreviewMask");
    Ensure(previewColor, w, h, gpu::Format::RGBA16F, "BrushPreviewColor");
    for (gpu::Texture* t : {previewLayer.get(), previewColor.get()}) {
        gpu::DispatchDesc c;
        c.pipeline = clear;
        c.uav[0] = t;
        SetConstants(c, ClearRootB{0, 0, w, h, {0, 0, 0, 0}});
        Dispatch2D(ctx, c, w, h);
    }
    {
        gpu::DispatchDesc c;
        c.pipeline = clearR;
        c.uav[0] = previewMask.get();
        SetConstants(c, ClearRRootB{w, h, 0.0f});
        Dispatch2D(ctx, c, w, h);
    }
    // An S-shaped stroke with a pressure ramp (thin - full - thin).
    const float size = std::min(s.size, h * 0.42f);
    const float spacing = std::max(1.0f, size * std::max(b.spacing > 0 ? b.spacing : s.spacing, 0.01f));
    std::vector<Dab> dabs;
    std::vector<float4> extra;
    auto curve = [&](float u) { return float2{size * 0.6f + u * (w - size * 1.2f), h * 0.5f + std::sin(u * 6.2831853f) * (h * 0.5f - size * 0.55f) * 0.7f}; };
    float2 prev = curve(0);
    float dist = 0, next = 0;
    const int steps = (int)w * 2;
    for (int i = 0; i <= steps; ++i) {
        float u = (float)i / steps;
        float2 p = curve(u);
        dist += Length(p - prev);
        if (dist < next) { prev = p; continue; }
        next += spacing;
        float pressure = std::sin(u * 3.14159265f) * 0.85f + 0.15f;
        float rad = size * 0.5f * (s.pressureSize ? std::max(pressure, 0.05f) : 1.0f);
        float alpha = s.flow * (s.pressureOpacity ? pressure : 1.0f);
        float2 dir = p - prev;
        dabs.push_back({p.x, p.y, rad, alpha});
        extra.push_back({pressure, std::atan2(dir.y, dir.x), u * 1.5f, dist});
        extra.push_back({std::sin(u * 6.2831853f) * 40.0f, 20.0f, u * 3.14159265f, 600.0f});  // preview: some tilt and rotation
        prev = p;
    }
    BrushTargets t{nullptr, nullptr, previewLayer.get(), previewMask.get(), previewColor.get(), w, h, true};
    RunBrush(ctx, pipe, b, tip, dabs, extra, 0, s, color, false, t, nullptr);
    previewHash = hh.h;
    return previewLayer.get();
}

// ------------------------------------------------------------------ test pad

void Renderer::PadClear(gpu::CommandContext& ctx, u32 w, u32 h) {
    Ensure(padLayer, w, h, gpu::Format::RGBA16F, "BrushPad");
    Ensure(padPre, w, h, gpu::Format::RGBA16F, "BrushPadPre");
    Ensure(padMask, w, h, gpu::Format::R32F, "BrushPadMask");
    Ensure(padColor, w, h, gpu::Format::RGBA16F, "BrushPadColor");
    auto* clear = Kernel("clear.slang");
    if (!clear) return;
    for (gpu::Texture* t : {padLayer.get(), padPre.get()}) {
        gpu::DispatchDesc c;
        c.pipeline = clear;
        c.uav[0] = t;
        SetConstants(c, ClearRootB{0, 0, w, h, {0, 0, 0, 0}});
        Dispatch2D(ctx, c, w, h);
    }
}

void Renderer::PadBeginStroke(gpu::CommandContext& ctx) {
    if (!padLayer) return;
    const u32 w = padLayer->Width(), h = padLayer->Height();
    ctx.CopyTexture(padPre.get(), padLayer.get());
    auto* clear = Kernel("clear.slang");
    auto* clearR = Kernel("clear_r.slang");
    if (!clear || !clearR) return;
    gpu::DispatchDesc c;
    c.pipeline = clear;
    c.uav[0] = padColor.get();
    SetConstants(c, ClearRootB{0, 0, w, h, {0, 0, 0, 0}});
    Dispatch2D(ctx, c, w, h);
    gpu::DispatchDesc m;
    m.pipeline = clearR;
    m.uav[0] = padMask.get();
    SetConstants(m, ClearRRootB{w, h, 0.0f});
    Dispatch2D(ctx, m, w, h);
}

bool Renderer::PadStroke(gpu::CommandContext& ctx, const BrushDef& b, const EffectInstance& tip, const BrushSettings& s, float4 color,
                         const std::vector<Dab>& dabs, const std::vector<float4>& extra, bool erase, u32 dabBase) {
    const gpu::ComputePipeline* pipe = BrushPipeline(b);
    if (!pipe || !padLayer || dabs.empty()) return false;
    BrushTargets t{padPre.get(), nullptr, padLayer.get(), padMask.get(), padColor.get(), padLayer->Width(), padLayer->Height(), false};
    RunBrush(ctx, pipe, b, tip, dabs, extra, dabBase, s, color, erase, t, nullptr);
    return true;
}

}  // namespace sx
