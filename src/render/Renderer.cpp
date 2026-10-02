#include "render/Renderer.h"
#include "render/Dispatch.h"

#include <cstring>

namespace sx {

using gpu::Shaders;

namespace {

struct CompositeRoot { u32 w, h, mode; float opacity; u32 flags, pad = 0; i32 ox = 0, oy = 0, bx = 0, by = 0, tx = 0, ty = 0, mx = 0, my = 0; };
struct EffectRoot { u32 w, h; i32 ox, oy; u32 dw, dh, pass, inputMask; i32 in[8]; };
struct ClearRoot { i32 x, y; u32 w, h; float4 value; };
struct ClearRRoot { u32 w, h; float value; };
struct BrushRoot { i32 rx, ry; u32 rw, rh, dw, dh, count; float hardness, opacity; u32 mode, flags; float pad; float4 color; };
struct GradientRoot { u32 w, h; float2 a, b; u32 shape; float opacity; float4 ca, cb; };
struct ImportRoot { u32 sw, sh; i32 ox, oy; u32 dw, dh, flags; };
struct ExportRoot { u32 w, h, flags; };
struct DisplayCB {
    float2 origin;
    float invZoom;
    u32 flags;
    u32 viewW, viewH, docW, docH;
    float4 outside;
    float checker, exposure, time;
    u32 selFlags;
};
}  // namespace

float HalfToFloat(u16 h) {
    u32 sign = (h >> 15) & 1, exp = (h >> 10) & 0x1f, mant = h & 0x3ff;
    u32 f;
    if (exp == 0) {
        if (mant == 0) f = sign << 31;
        else {
            exp = 127 - 15 + 1;
            while (!(mant & 0x400)) { mant <<= 1; --exp; }
            mant &= 0x3ff;
            f = (sign << 31) | (exp << 23) | (mant << 13);
        }
    } else if (exp == 31) f = (sign << 31) | 0x7f800000 | (mant << 13);
    else f = (sign << 31) | ((exp + 127 - 15) << 23) | (mant << 13);
    float out;
    memcpy(&out, &f, 4);
    return out;
}

gpu::Texture* Renderer::Ensure(gpu::TexturePtr& t, u32 w, u32 h, gpu::Format f, const char* name) {
    if (!t || t->Width() != w || t->Height() != h || t->GetFormat() != f) t = gpu::Dev().CreateTexture(w, h, f, name);
    return t.get();
}

void Renderer::Reset() {
    cache.clear();
    frameEval.clear();
    thumbs.clear();
    failed.clear();
    strokeSurface = 0;
    tiledOut.reset();
    pool.clear();
    tileDirty.clear();
    tiledVersions.clear();
    tiledStruct = 0;
    tilesX = tilesY = tilesPending = 0;
    reduced.reset();
    reducedFor = lastOut = nullptr;
}

void Renderer::Prepare(Document& doc, gpu::CommandContext& ctx) {
    if (!doc.pendingClears.empty()) {
        auto* p = Shaders().Builtin("clear.slang");
        if (!p) return;  // wait until compiled
        for (auto& [id, value] : doc.pendingClears) {
            Surface* s = doc.GetSurface(id);
            if (!s || !s->tex) continue;
            gpu::DispatchDesc d;
            d.pipeline = p;
            d.uav[0] = s->tex.get();
            if (value.x == 0 && value.y == 0 && value.z == 0 && value.w == 0) d.uavNoMap = 1;  // sparse: stays unbacked
            SetConstants(d, ClearRoot{0, 0, s->tex->Width(), s->tex->Height(), value});
            d.threadsX = s->tex->Width();
            d.threadsY = s->tex->Height();
            ctx.Dispatch(d);
            doc.TouchSurface(id);
        }
        doc.pendingClears.clear();
    }
    for (auto& [dst, src] : doc.pendingCopies) {
        Surface* a = doc.GetSurface(dst);
        Surface* b = doc.GetSurface(src);
        if (a && b && a->tex && b->tex) {
            ctx.CopyTexture(a->tex.get(), b->tex.get());
            doc.TouchSurface(dst);
        }
    }
    doc.pendingCopies.clear();
}

gpu::ProgramPtr Renderer::ProgramFor(const EffectDefPtr& def) {
    if (Effects().Generation() != effectGeneration) {
        programs.clear();
        effectGeneration = Effects().Generation();
    }
    auto it = programs.find(def.get());
    if (it != programs.end() && it->second.def == def) return it->second.program;
    ProgramEntry e;
    e.def = def;
    e.program = Shaders().Request(def->name, BuildEffectSource(*def));
    programs[def.get()] = e;
    return e.program;
}

gpu::Texture* Renderer::Evaluate(Document& doc, gpu::CommandContext& ctx) {
    doc.tiled = doc.forceTiled || (u64)doc.Width() * doc.Height() > 6000ull * 6000ull;
    if (doc.tiled) {
        if (!wasTiled) cache.clear();  // full-size node caches are what tiling avoids
        wasTiled = true;
        gpu::Texture* t = EvaluateTiled(doc, ctx, hasVisible ? &visibleRect : nullptr, 16);
        hasVisible = false;
        return t;
    }
    wasTiled = false;
    tiledOut.reset();
    pool.clear();
    doc.dirtyRects.clear();
    frameEval.clear();
    pending = false;
    EvalResult r = Eval(doc, ctx, doc.S().graph.output, 0);
    if (r.tex != lastOut || r.hash != lastOutHash || !r.hash) {
        if (r.tex) MarkImageChanged({0, 0, (int)r.tex->Width(), (int)r.tex->Height()});
        lastOut = r.tex;
        lastOutHash = r.hash;
    }
    // Garbage-collect caches of deleted nodes.
    if (gpu::Dev().FrameNumber() % 120 == 0) {
        for (auto it = cache.begin(); it != cache.end();) {
            if (!doc.S().graph.Find(it->first)) it = cache.erase(it);
            else ++it;
        }
    }
    return r.tex;
}

gpu::Texture* Renderer::EvaluateComplete(Document& doc, gpu::CommandContext& ctx) {
    doc.tiled = doc.forceTiled || (u64)doc.Width() * doc.Height() > 6000ull * 6000ull;
    if (!doc.tiled) return Evaluate(doc, ctx);
    if (!wasTiled) cache.clear();
    wasTiled = true;
    return EvaluateTiled(doc, ctx, nullptr, 1 << 30);
}

gpu::Texture* Renderer::NodeOutput(Id node) const {
    auto it = frameEval.find(node);
    if (it != frameEval.end()) return it->second.tex;
    auto c = cache.find(node);
    return c != cache.end() ? c->second.tex.get() : nullptr;
}

u64 Renderer::NodeHash(Id node) const {
    auto it = frameEval.find(node);
    return it != frameEval.end() ? it->second.hash : 0;
}

bool Renderer::NodeFailed(Id node, std::string* errors) const {
    auto it = failed.find(node);
    if (it == failed.end()) return false;
    if (errors) *errors = it->second;
    return true;
}

Renderer::EvalResult Renderer::Eval(Document& doc, gpu::CommandContext& ctx, Id id, int depth) {
    if (!id || depth > 256) return {};
    auto memo = frameEval.find(id);
    if (memo != frameEval.end()) return memo->second;
    frameEval[id] = {};  // cycle guard

    Node* n = doc.S().graph.Find(id);
    if (!n) return {};
    const u32 W = doc.Width(), H = doc.Height();
    const gpu::Format fmt = SurfaceFormat(doc.GetPrecision());
    EvalResult result;

    switch (n->kind) {
        case NodeKind::Output: {
            result = n->inputs.empty() ? EvalResult{} : Eval(doc, ctx, n->inputs[0], depth + 1);
            break;
        }
        case NodeKind::Surface: {
            Surface* s = doc.GetSurface(n->surface);
            if (s && s->tex) {
                Hasher h;
                h.Pod(n->surface);
                h.Pod(s->version);
                result = {s->tex.get(), h.h};
            }
            break;
        }
        case NodeKind::Blend: {
            EvalResult in[3];
            for (int i = 0; i < 3 && i < (int)n->inputs.size(); ++i) in[i] = Eval(doc, ctx, n->inputs[i], depth + 1);
            if (n->bypass || !in[1].tex) { result = in[0]; break; }
            Hasher h;
            h.Pod(u8(1));
            h.Pod(n->blend);
            h.Pod(n->opacity);
            h.Pod(n->clip);
            h.Pod(doc.blendInGamma);
            for (auto& r : in) h.Pod(r.hash);
            h.Pod(W);
            h.Pod(H);
            h.Pod(fmt);
            NodeCache& c = cache[id];
            Ensure(c.tex, W, H, fmt, "Blend");
            // A connected input with hash 0 is still pending: recompute next frame too.
            bool inputsReady = true;
            for (int i = 0; i < 3 && i < (int)n->inputs.size(); ++i) if (n->inputs[i] && !in[i].hash) inputsReady = false;
            if (c.hash != h.h || !inputsReady) {
                auto* p = Shaders().Builtin("composite.slang");
                if (!p) { pending = true; break; }
                gpu::DispatchDesc d;
                d.pipeline = p;
                d.srv[0] = in[0].tex;
                d.srv[1] = in[1].tex;
                d.srv[2] = in[2].tex;
                d.uav[0] = c.tex.get();
                u32 flags = (in[2].tex ? 1u : 0u) | (doc.blendInGamma ? 2u : 0u) | 4u | (in[0].tex ? 8u : 0u) | (n->clip ? 16u : 0u);
                SetConstants(d, CompositeRoot{W, H, (u32)n->blend, n->opacity, flags});
                d.threadsX = W;
                d.threadsY = H;
                ctx.Dispatch(d);
                c.hash = inputsReady ? h.h : 0;
            }
            result = {c.tex.get(), inputsReady ? h.h : 0};
            break;
        }
        case NodeKind::Effect: {
            EffectDefPtr def = n->fx.Def();
            int nin = def ? (int)def->inputs.size() : 0;
            EvalResult in[4];
            for (int i = 0; i < 4 && i < (int)n->inputs.size(); ++i) in[i] = Eval(doc, ctx, n->inputs[i], depth + 1);
            if (n->bypass || !def) { result = in[0]; break; }
            auto prog = ProgramFor(def);
            auto st = prog->state.load();
            if (st == gpu::Program::State::Failed) {
                failed[id] = prog->errors;
                result = in[0];
                break;
            }
            failed.erase(id);
            Hasher h;
            h.Pod(u8(2));
            h.Pod(prog->hash);
            auto packed = PackParams(*def, n->fx.params, &n->fx.curves);
            h.Bytes(packed.data(), packed.size() * sizeof(float4));
            for (int i = 0; i < nin && i < 4; ++i) h.Pod(in[i].hash);
            h.Pod(W);
            h.Pod(H);
            h.Pod(fmt);
            NodeCache& c = cache[id];
            if (st != gpu::Program::State::Ready) {
                pending = true;
                result = c.tex ? EvalResult{c.tex.get(), c.hash} : in[0];
                break;
            }
            Ensure(c.tex, W, H, fmt, "Effect");
            bool inputsReady = true;
            for (int i = 0; i < nin && i < 4; ++i) if (n->inputs.size() > (size_t)i && n->inputs[i] && !in[i].hash) inputsReady = false;
            if (c.hash != h.h || !inputsReady) {
                if (def->passes > 1) Ensure(c.tmp, W, H, fmt, "EffectTmp");
                u32 mask = 0;
                for (int i = 0; i < 4; ++i) if (in[i].tex && i < nin) mask |= 1u << i;
                for (int p = 0; p < def->passes; ++p) {
                    bool toMain = ((def->passes - 1 - p) % 2) == 0;
                    gpu::Texture* dst = toMain ? c.tex.get() : c.tmp.get();
                    gpu::Texture* prev = toMain ? c.tmp.get() : c.tex.get();
                    gpu::DispatchDesc d;
                    d.pipeline = prog->pipeline.get();
                    for (int i = 0; i < 4; ++i) d.srv[i] = i < nin ? in[i].tex : nullptr;
                    d.srv[4] = p > 0 ? prev : nullptr;
                    d.uav[0] = dst;
                    SetConstants(d, EffectRoot{W, H, 0, 0, W, H, (u32)p, mask, {0, 0, 0, 0, 0, 0, 0, 0}});
                    d.cbData = packed.data();
                    d.cbSize = packed.size() * sizeof(float4);
                    d.threadsX = W;
                    d.threadsY = H;
                    ctx.Dispatch(d);
                }
                c.hash = inputsReady ? h.h : 0;
            }
            result = {c.tex.get(), inputsReady ? h.h : 0};
            break;
        }
    }
    frameEval[id] = result;
    return result;
}

void Renderer::Display(gpu::CommandContext& ctx, gpu::Texture* image, u32 docW, u32 docH, const ViewParams& v, gpu::Texture* target) {
    auto* p = Shaders().Builtin("display.slang");
    if (!p || !target) return;
    DisplayCB cb = {};
    cb.origin = v.origin;
    cb.invZoom = v.invZoom;
    cb.flags = (image ? 1u : 0u) | (v.nearest ? 2u : 0u);
    cb.viewW = v.viewW;
    cb.viewH = v.viewH;
    cb.docW = docW;
    cb.docH = docH;
    cb.outside = v.outside;
    cb.checker = v.checker;
    cb.exposure = v.exposure;
    cb.time = v.time;
    cb.selFlags = v.selection ? (1u | (v.quickMask ? 2u : 0u)) : 0u;
    gpu::DispatchDesc d;
    d.pipeline = p;
    d.srv[0] = image;
    d.srv[1] = v.selection;
    d.uav[0] = target;
    d.cbData = &cb;
    d.cbSize = sizeof(cb);
    d.threadsX = v.viewW;
    d.threadsY = v.viewH;
    ctx.Dispatch(d);
}

gpu::Texture* Renderer::DisplaySource(gpu::CommandContext& ctx, gpu::Texture* image, float invZoom) {
    if (!image) return image;
    // The display shader averages up to 4x4 taps per view pixel; reduce beyond that.
    u32 factor = 1;
    while (invZoom / factor > 4.0f && factor < 64) factor *= 2;
    if (factor == 1) return image;
    auto* k = Kernel("downsample.slang");
    if (!k) return image;
    u32 w = (image->Width() + factor - 1) / factor, h = (image->Height() + factor - 1) / factor;
    Rect dirty;
    if (!reduced || reducedFactor != factor || reducedFor != image || reduced->Width() != w || reduced->Height() != h) {
        Ensure(reduced, w, h, image->GetFormat(), "DisplayReduced");
        reducedFactor = factor;
        reducedFor = image;
        dirty = {0, 0, (int)w, (int)h};
    } else if (reducedVersion != imageVersion) {
        int f = (int)factor;
        dirty = Rect{imageDirty.x0 / f, imageDirty.y0 / f, (imageDirty.x1 + f - 1) / f, (imageDirty.y1 + f - 1) / f}.Clipped((int)w, (int)h);
    }
    reducedVersion = imageVersion;
    imageDirty = {};
    if (!dirty.Empty()) {
        struct { u32 sw, sh; i32 ox, oy; u32 w, h, f; } root = {image->Width(), image->Height(), dirty.x0, dirty.y0,
                                                                 (u32)dirty.Width(), (u32)dirty.Height(), factor};
        gpu::DispatchDesc d;
        d.pipeline = k;
        d.srv[0] = image;
        d.uav[0] = reduced.get();
        SetConstants(d, root);
        Dispatch2D(ctx, d, (u32)dirty.Width(), (u32)dirty.Height());
    }
    return reduced.get();
}

gpu::Texture* Renderer::Thumbnail(gpu::CommandContext& ctx, u64 key, gpu::Texture* src, u64 hash, u32 w, u32 h) {
    ThumbCache& t = thumbs[key];
    Ensure(t.tex, w, h, gpu::Format::RGBA8, "Thumb");
    u64 frame = gpu::Dev().FrameNumber();
    bool stale = t.hash != hash;
    // Throttle refreshes while content changes every frame (painting).
    if (stale && (t.hash == 0 || frame - t.frame >= 6)) {
        float docW = src ? (float)src->Width() : 1.0f, docH = src ? (float)src->Height() : 1.0f;
        float inv = std::max(docW / w, docH / h);
        ViewParams v;
        v.invZoom = inv;
        v.viewW = w;
        v.viewH = h;
        v.origin = {(docW - w * inv) * 0.5f, (docH - h * inv) * 0.5f};
        v.nearest = false;
        v.checker = 4;
        v.outside = {0.16f, 0.16f, 0.18f, 1};
        Display(ctx, src, (u32)docW, (u32)docH, v, t.tex.get());
        t.hash = hash;
        t.frame = frame;
    }
    return t.tex.get();
}

// ------------------------------------------------------------------ painting

bool Renderer::BeginStroke(Document& doc, gpu::CommandContext& ctx, Id surface) {
    Surface* s = doc.GetSurface(surface);
    auto* clearR = Shaders().Builtin("clear_r.slang");
    if (!s || !s->tex || !clearR || !Shaders().Builtin("brush.slang")) return false;
    Ensure(strokePre, s->tex->Width(), s->tex->Height(), s->tex->GetFormat(), "StrokePre");
    Ensure(strokeMask, s->tex->Width(), s->tex->Height(), gpu::Format::R32F, "StrokeMask");
    ctx.CopyTexture(strokePre.get(), s->tex.get());
    gpu::DispatchDesc d;
    d.pipeline = clearR;
    d.uav[0] = strokeMask.get();
    d.uavNoMap = 1;
    SetConstants(d, ClearRRoot{strokeMask->Width(), strokeMask->Height(), 0.0f});
    d.threadsX = strokeMask->Width();
    d.threadsY = strokeMask->Height();
    ctx.Dispatch(d);
    strokeSurface = surface;
    strokeRect = {};
    haveSmudge = false;
    return true;
}

void Renderer::StrokeDabs(Document& doc, gpu::CommandContext& ctx, const std::vector<Dab>& dabs, const BrushSettings& b, float4 color, bool erase) {
    if (!strokeSurface || dabs.empty()) return;
    Surface* s = doc.GetSurface(strokeSurface);
    auto* p = Shaders().Builtin("brush.slang");
    if (!s || !p) return;
    const int W = (int)s->tex->Width(), H = (int)s->tex->Height();
    Rect touched;
    for (size_t start = 0; start < dabs.size(); start += 4096) {
        size_t count = std::min<size_t>(4096, dabs.size() - start);
        Rect r;
        for (size_t i = start; i < start + count; ++i) {
            const Dab& dab = dabs[i];
            Rect dr{(int)std::floor(dab.x - dab.radius - 2), (int)std::floor(dab.y - dab.radius - 2),
                    (int)std::ceil(dab.x + dab.radius + 2), (int)std::ceil(dab.y + dab.radius + 2)};
            r.Merge(dr);
        }
        r = r.Clipped(W, H);
        if (r.Empty()) continue;
        strokeRect.Merge(r);
        touched.Merge(r);
        gpu::DispatchDesc d;
        d.pipeline = p;
        d.srv[0] = strokePre.get();
        d.uav[0] = s->tex.get();
        d.uav[1] = strokeMask.get();
        if (erase) d.uavNoMap = 1;  // erasing never needs new memory
        MapDabs(d, 3, (const float4*)&dabs[start], count, sizeof(Dab), 2);
        Surface* sel = doc.Selection();
        d.srv[1] = sel ? sel->tex.get() : nullptr;
        BrushRoot root = {r.x0, r.y0, (u32)r.Width(), (u32)r.Height(), (u32)W, (u32)H, (u32)count,
                          b.hardness, b.opacity, erase ? 1u : 0u, sel ? 1u : 0u, 0.0f, color};
        SetConstants(d, root);
        d.cbData = &dabs[start];
        d.cbSize = count * sizeof(Dab);
        d.threadsX = r.Width();
        d.threadsY = r.Height();
        ctx.Dispatch(d);
    }
    doc.TouchSurface(strokeSurface, &touched);
}

void Renderer::EndStroke(Document& doc, gpu::CommandContext& ctx, History& hist, const std::string& label) {
    if (!strokeSurface) return;
    Surface* s = doc.GetSurface(strokeSurface);
    if (s && !strokeRect.Empty()) {
        auto fmt = s->tex->GetFormat();
        auto before = gpu::Dev().CreateTexture(strokeRect.Width(), strokeRect.Height(), fmt, "UndoBefore");
        auto after = gpu::Dev().CreateTexture(strokeRect.Width(), strokeRect.Height(), fmt, "UndoAfter");
        if (before && after) {
            ctx.CopyRegion(before.get(), 0, 0, strokePre.get(), strokeRect);
            ctx.CopyRegion(after.get(), 0, 0, s->tex.get(), strokeRect);
            hist.PushPaint(label, strokeSurface, strokeRect, before, after);
        }
    }
    strokeSurface = 0;
    if (doc.tiled) {
        // Very large documents: release the full-size stroke scratch buffers between strokes.
        strokePre.reset();
        strokeMask.reset();
        smudgeSnap.reset();
    }
}

void Renderer::ApplyGradient(Document& doc, gpu::CommandContext& ctx, History& hist, Id surface, float2 a, float2 b,
                             int shape, float4 colorA, float4 colorB, float opacity, const std::string& label) {
    Surface* s = doc.GetSurface(surface);
    auto* p = Shaders().Builtin("gradient.slang");
    if (!s || !p) return;
    const u32 W = s->tex->Width(), H = s->tex->Height();
    auto before = gpu::Dev().CreateTexture(W, H, s->tex->GetFormat(), "UndoBefore");
    auto after = gpu::Dev().CreateTexture(W, H, s->tex->GetFormat(), "UndoAfter");
    if (!before || !after) return;
    ctx.CopyTexture(before.get(), s->tex.get());
    gpu::DispatchDesc d;
    d.pipeline = p;
    d.srv[0] = before.get();
    Surface* sel = doc.Selection();
    d.srv[1] = sel ? sel->tex.get() : nullptr;
    d.uav[0] = s->tex.get();
    SetConstants(d, GradientRoot{W, H, a, b, (u32)shape | (sel ? 0x100u : 0u), opacity, colorA, colorB});
    d.threadsX = W;
    d.threadsY = H;
    ctx.Dispatch(d);
    ctx.CopyTexture(after.get(), s->tex.get());
    hist.PushPaint(label, surface, {0, 0, (int)W, (int)H}, before, after);
    doc.TouchSurface(surface);
}

void Renderer::Import(Document& doc, gpu::CommandContext& ctx, Id surface, const void* pixels, u32 w, u32 h, bool isFloat, int2 offset) {
    Surface* s = doc.GetSurface(surface);
    auto* p = Shaders().CompileNow("import.slang", Shaders().File("import.slang"))->pipeline.get();
    if (!s || !p) return;
    auto tmp = gpu::Dev().CreateTexture(w, h, isFloat ? gpu::Format::RGBA32F : gpu::Format::RGBA8, "ImportSrc");
    if (!tmp) return;
    ctx.Upload(tmp.get(), pixels, w * (isFloat ? 16 : 4));
    gpu::DispatchDesc d;
    d.pipeline = p;
    d.srv[0] = tmp.get();
    d.uav[0] = s->tex.get();
    d.uavRect[0] = {offset.x, offset.y, offset.x + (int)w, offset.y + (int)h};
    SetConstants(d, ImportRoot{w, h, offset.x, offset.y, s->tex->Width(), s->tex->Height(), isFloat ? 0u : 1u});
    d.threadsX = w;
    d.threadsY = h;
    ctx.Dispatch(d);
    Rect r{offset.x, offset.y, offset.x + (int)w, offset.y + (int)h};
    doc.TouchSurface(surface, &r);
}

gpu::ReadbackPtr Renderer::ExportRGBA8(gpu::CommandContext& ctx, gpu::Texture* image) {
    auto prog = Shaders().CompileNow("export.slang", Shaders().File("export.slang"));
    if (!image || !prog->pipeline) return nullptr;
    auto tmp = gpu::Dev().CreateTexture(image->Width(), image->Height(), gpu::Format::RGBA8, "Export");
    gpu::DispatchDesc d;
    d.pipeline = prog->pipeline.get();
    d.srv[0] = image;
    d.uav[0] = tmp.get();
    SetConstants(d, ExportRoot{image->Width(), image->Height(), 1u});
    d.threadsX = image->Width();
    d.threadsY = image->Height();
    ctx.Dispatch(d);
    return ctx.ReadbackTexture(tmp.get());
}

gpu::ReadbackPtr Renderer::ReadPixel(gpu::CommandContext& ctx, gpu::Texture* image, int x, int y) {
    if (!image) return nullptr;
    Rect r{x, y, x + 1, y + 1};
    return ctx.ReadbackTexture(image, &r);
}

}  // namespace sx
