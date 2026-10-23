#include "render/Renderer.h"
#include "render/Dispatch.h"
#include "doc/GraphTypes.h"

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
    u32 lutSize, gamut, scope;
    float scopeLevel;
    float4 ov0, ov1, ov2;  // overflow: document pixel -> contents UV
    u32 ovFlags, inpaint, compare;
    float split;
};
}  // namespace

u16 FloatToHalf(float f) {
    f = std::clamp(f, -65504.0f, 65504.0f);
    u32 x;
    memcpy(&x, &f, 4);
    const u32 sign = (x >> 16) & 0x8000u;
    const int e = (int)((x >> 23) & 0xFF) - 127 + 15;
    u32 m = x & 0x7FFFFFu;
    if (e <= 0) {
        if (e < -10) return (u16)sign;
        m = (m | 0x800000u) >> (1 - e);
        return (u16)(sign | ((m + 0x1000u) >> 13));
    }
    const u32 r = sign | ((u32)e << 10) | (m >> 13);
    return (u16)(r + ((m >> 12) & 1u));
}

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

void Renderer::BindLuts(gpu::CommandContext& ctx, gpu::DispatchDesc& d, const EffectDef& def, const EffectInstance& fx) {
    int k = 0;
    for (size_t i = 0; i < def.params.size() && k < kMaxLutParams; ++i) {
        if (def.params[i].type != ParamType::Lut) continue;
        const Lut3D* l = i < fx.luts.size() ? fx.luts[i].get() : nullptr;
        gpu::Texture* tex = nullptr;
        if (l) {
            auto& t = lutTextures[l->hash];
            if (!t) {
                const int n = l->size;
                t = gpu::Dev().CreateTexture((u32)(n * n), (u32)n, gpu::Format::RGBA32F, "LUT");
                std::vector<float4> px((size_t)n * n * n);
                for (int b = 0; b < n; ++b)
                    for (int g = 0; g < n; ++g)
                        for (int r = 0; r < n; ++r) {
                            const float* s = &l->rgb[((size_t)b * n * n + (size_t)g * n + r) * 3];
                            px[(size_t)g * n * n + (size_t)b * n + r] = {s[0], s[1], s[2], 1};
                        }
                if (t) ctx.Upload(t.get(), px.data(), (u32)(n * n * sizeof(float4)));
            }
            tex = t.get();
        }
        d.srv[5 + k++] = tex;
    }
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

void Renderer::BeginGraph(const Document& doc, Id liveOverride) {
    frameValues.clear();
    useFlat = GraphHasFunctions(doc.S().graph);
    if (useFlat) FlattenGraph(doc.S().graph, flatGraph, liveOverride);
    else flatGraph.nodes.clear();
}

const Graph& Renderer::EvalGraph(const Document& doc) const { return useFlat ? flatGraph : doc.S().graph; }

gpu::Texture* Renderer::Evaluate(Document& doc, gpu::CommandContext& ctx) {
    BeginGraph(doc);
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
    EvalResult r = Eval(doc, ctx, EvalGraph(doc).output, 0);
    if (r.tex != lastOut || r.hash != lastOutHash || !r.hash) {
        if (r.tex) MarkImageChanged({0, 0, (int)r.tex->Width(), (int)r.tex->Height()});
        lastOut = r.tex;
        lastOutHash = r.hash;
    }
    // Garbage-collect caches of deleted nodes.
    if (gpu::Dev().FrameNumber() % 120 == 0) {
        for (auto it = cache.begin(); it != cache.end();) {
            if (!EvalGraph(doc).Find(it->first)) it = cache.erase(it);
            else ++it;
        }
    }
    return r.tex;
}

gpu::Texture* Renderer::EvaluateComplete(Document& doc, gpu::CommandContext& ctx) {
    BeginGraph(doc);
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

Renderer::EvalResult Renderer::EvalInput(Document& doc, gpu::CommandContext& ctx, const Node& n, int i, int depth) {
    if (i >= (int)n.inputs.size()) return {};
    EvalResult src = Eval(doc, ctx, n.inputs[i], depth);
    u8 ch = LinkChannel(InputChannel(n, i));
    if (!ch || !src.tex) return src;
    auto* k = Kernel("channel_extract.slang");
    if (!k) { pending = true; return {}; }
    Hasher h;
    h.Pod(u8(7));
    h.Pod(src.hash);
    h.Pod(ch);
    NodeCache& c = channelCache[((u64)n.inputs[i] << 3) | ch];
    Ensure(c.tex, src.tex->Width(), src.tex->Height(), src.tex->GetFormat(), "Channel");
    if (c.hash != h.h || !src.hash) {
        struct { u32 w, h; i32 ox, oy; u32 ch; } root = {src.tex->Width(), src.tex->Height(), 0, 0, ch};
        gpu::DispatchDesc d;
        d.pipeline = k;
        d.srv[0] = src.tex;
        d.uav[0] = c.tex.get();
        SetConstants(d, root);
        Dispatch2D(ctx, d, root.w, root.h);
        c.hash = src.hash ? h.h : 0;
    }
    return {c.tex.get(), src.hash ? h.h : 0};
}

Renderer::EvalResult Renderer::Eval(Document& doc, gpu::CommandContext& ctx, Id id, int depth) {
    if (!id || depth > 256) return {};
    auto memo = frameEval.find(id);
    if (memo != frameEval.end()) return memo->second;
    frameEval[id] = {};  // cycle guard

    const Node* n = EvalGraph(doc).Find(id);
    if (!n) return {};
    const u32 W = doc.Width(), H = doc.Height();
    const gpu::Format fmt = SurfaceFormat(doc.GetPrecision());
    EvalResult result;

    switch (n->kind) {
        case NodeKind::Output: {
            result = n->inputs.empty() ? EvalResult{} : EvalInput(doc, ctx, *n, 0, depth + 1);
            break;
        }
        case NodeKind::Split:
        case NodeKind::Reroute:
        case NodeKind::SubOutput:
        case NodeKind::Function:  // only without contents (flattened otherwise)
            result = EvalInput(doc, ctx, *n, 0, depth + 1);
            break;
        case NodeKind::Value:
        case NodeKind::Math:
        case NodeKind::VarGet:
        case NodeKind::VarSet:
        case NodeKind::Event:
            break;  // values / execution only
        case NodeKind::Action: {
            // Its stored result, else the image it works on (live).
            Surface* s = n->surface ? doc.GetSurface(n->surface) : nullptr;
            if (s && s->tex) {
                Hasher h;
                h.Pod(n->surface);
                h.Pod(s->version);
                result = {s->tex.get(), h.h};
            } else if (auto* ad = FindExecAction(n->op); ad && ad->passthrough >= 0) {
                result = EvalInput(doc, ctx, *n, ad->passthrough, depth + 1);
            }
            break;
        }
        case NodeKind::SubInput: {
            // Function editor: the input captured when it was opened.
            Surface* s = doc.GetSurface(n->surface);
            if (s && s->tex) {
                Hasher h;
                h.Pod(n->surface);
                h.Pod(s->version);
                result = {s->tex.get(), h.h};
            }
            break;
        }
        case NodeKind::Tool: {
            // Baked: its pixels (the input chain is not evaluated); live: the input passes through.
            Surface* s = ToolBaked(*n) ? doc.GetSurface(n->surface) : nullptr;
            if (s && s->tex) {
                Hasher h;
                h.Pod(n->surface);
                h.Pod(s->version);
                result = {s->tex.get(), h.h};
            } else {
                result = EvalInput(doc, ctx, *n, 0, depth + 1);
            }
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
            for (int i = 0; i < 3 && i < (int)n->inputs.size(); ++i) in[i] = EvalInput(doc, ctx, *n, i, depth + 1);
            if (n->bypass || !in[1].tex) { result = in[0]; break; }
            Hasher h;
            h.Pod(u8(1));
            h.Pod(n->blend);
            h.Pod(n->opacity);
            h.Pod(n->clip);
            h.Pod(n->blendIf);
            h.Pod(n->channelOff);
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
                u32 flags = (in[2].tex ? 1u : 0u) | (doc.blendInGamma ? 2u : 0u) | 4u | (in[0].tex ? 8u : 0u) | (n->clip ? 16u : 0u) | (n->blendIf.Active() ? 32u : 0u) | ((u32)n->channelOff << 8);
                float4 bif[8];
                n->blendIf.Ranges(bif);
                d.cbData = bif;
                d.cbSize = sizeof(bif);
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
            for (int i = 0; i < 4 && i < (int)n->inputs.size() && (!def || i < nin); ++i) in[i] = EvalInput(doc, ctx, *n, i, depth + 1);
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
            auto packed = PackParams(*def, EffectParams(doc, *n), &n->fx.curves, &n->fx.luts);
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
                BindLuts(ctx, d, *def, n->fx);
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

bool Renderer::Display(gpu::CommandContext& ctx, gpu::Texture* image, u32 docW, u32 docH, const ViewParams& v, gpu::Texture* target) {
    auto* p = Shaders().Builtin("display.slang");
    if (!p || !target) return false;
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
    cb.lutSize = v.lut ? v.lutSize : 0u;
    cb.gamut = v.gamutWarning ? 1u : 0u;
    cb.scope = (u32)v.scope;
    cb.scopeLevel = v.scopeLevel;
    if (v.overflow) {
        cb.ov0 = {v.overflowInv[0], v.overflowInv[1], v.overflowInv[2], 0};
        cb.ov1 = {v.overflowInv[3], v.overflowInv[4], v.overflowInv[5], 0};
        cb.ov2 = {v.overflowInv[6], v.overflowInv[7], v.overflowInv[8], 0};
        cb.ovFlags = 1;
    }
    gpu::DispatchDesc d;
    d.pipeline = p;
    d.srv[0] = image;
    d.srv[1] = v.selection;
    d.srv[2] = v.lut;
    d.srv[3] = v.overflow;
    d.srv[4] = v.inpaint;
    d.srv[5] = v.compare ? v.snapshot : nullptr;
    cb.compare = v.snapshot ? (u32)v.compare : 0u;
    cb.split = v.split;
    cb.inpaint = v.inpaint ? 1u : 0u;
    d.uav[0] = target;
    d.cbData = &cb;
    d.cbSize = sizeof(cb);
    d.threadsX = v.viewW;
    d.threadsY = v.viewH;
    ctx.Dispatch(d);
    return true;
}

void Renderer::Composite(Document& doc, gpu::CommandContext& ctx, gpu::Texture* dst, gpu::Texture* base, gpu::Texture* top, gpu::Texture* mask,
                         BlendMode mode, float opacity, bool clip, const BlendIf* bif) {
    auto* p = Kernel("composite.slang");
    if (!p || !dst) return;
    const u32 W = dst->Width(), H = dst->Height();
    gpu::DispatchDesc d;
    d.pipeline = p;
    d.srv[0] = base;
    d.srv[1] = top;
    d.srv[2] = mask;
    d.uav[0] = dst;
    bool useBif = bif && bif->Active();
    u32 flags = (mask ? 1u : 0u) | (doc.blendInGamma ? 2u : 0u) | (top ? 4u : 0u) | (base ? 8u : 0u) | (clip ? 16u : 0u) | (useBif ? 32u : 0u);
    float4 ranges[8] = {};
    if (useBif) bif->Ranges(ranges);
    d.cbData = ranges;
    d.cbSize = sizeof(ranges);
    SetConstants(d, CompositeRoot{W, H, (u32)mode, opacity, flags});
    Dispatch2D(ctx, d, W, H);
}

gpu::TexturePtr Renderer::PreviewCopy(gpu::CommandContext& ctx, gpu::Texture* src, u32 w, u32 h) {
    auto* k = Kernel("preview_resample.slang");
    if (!k || !src || !w || !h) return nullptr;
    auto out = gpu::Dev().CreateTexture(w, h, gpu::Format::RGBA16F, "Preview");
    float scale = std::max((float)src->Width() / w, (float)src->Height() / h);
    struct { u32 w, h; float sx, sy, ox, oy; } root = {w, h, scale, scale, (src->Width() - w * scale) * 0.5f, (src->Height() - h * scale) * 0.5f};
    gpu::DispatchDesc d;
    d.pipeline = k;
    d.srv[0] = src;
    d.uav[0] = out.get();
    SetConstants(d, root);
    Dispatch2D(ctx, d, w, h);
    return out;
}

bool Renderer::ApplyEffectToSurface(Document& doc, gpu::CommandContext& ctx, History& hist, Id surface, const EffectInstance& fx, const std::string& label) {
    Surface* s = doc.GetSurface(surface);
    auto* k = Kernel("apply_masked.slang");
    EffectDefPtr def = fx.Def();
    if (!s || !k || !def) return false;
    // A one-shot action: compile now rather than wait for the background compile.
    if (ProgramFor(def)->state.load() == gpu::Program::State::Pending)
        programs[def.get()].program = Shaders().CompileNow(def->name, BuildEffectSource(*def));
    auto result = ApplyEffect(doc, ctx, fx, s->tex.get());
    if (!result) return false;
    const u32 W = s->tex->Width(), H = s->tex->Height();
    auto before = gpu::Dev().CreateTexture(W, H, s->tex->GetFormat(), "UndoBefore");
    ctx.CopyTexture(before.get(), s->tex.get());
    Surface* sel = doc.Selection();
    struct { u32 w, h, flags; } root = {W, H, sel ? 1u : 0u};
    gpu::DispatchDesc d;
    d.pipeline = k;
    d.srv[0] = before.get();
    d.srv[1] = result.get();
    d.srv[2] = sel ? sel->tex.get() : nullptr;
    d.uav[0] = s->tex.get();
    SetConstants(d, root);
    Dispatch2D(ctx, d, W, H);
    auto after = gpu::Dev().CreateTexture(W, H, s->tex->GetFormat(), "UndoAfter");
    ctx.CopyTexture(after.get(), s->tex.get());
    hist.PushPaint(label, surface, {0, 0, (int)W, (int)H}, before, after);
    doc.TouchSurface(surface);
    return true;
}

gpu::TexturePtr Renderer::ApplyEffect(Document& doc, gpu::CommandContext& ctx, const EffectInstance& fx, gpu::Texture* in, u32 refW, u32 refH) {
    EffectDefPtr def = fx.Def();
    if (!def || !in) return nullptr;
    return ApplyEffectInputs(doc, ctx, fx, {in, nullptr, nullptr, nullptr}, refW, refH);  // generators: size reference only
}

gpu::TexturePtr Renderer::ApplyEffectInputs(Document& doc, gpu::CommandContext& ctx, const EffectInstance& fx, std::array<gpu::Texture*, 4> in, u32 refW, u32 refH) {
    EffectDefPtr def = fx.Def();
    gpu::Texture* first = nullptr;
    for (auto* t : in) if (t && !first) first = t;
    if (!def || !first) return nullptr;
    auto prog = ProgramFor(def);
    if (prog->state.load() != gpu::Program::State::Ready) return nullptr;
    const u32 W = first->Width(), H = first->Height();
    auto out = gpu::Dev().CreateTexture(W, H, first->GetFormat(), "EffectOut");
    auto tmp = def->passes > 1 ? gpu::Dev().CreateTexture(W, H, first->GetFormat(), "EffectTmp") : nullptr;
    auto packed = PackParams(*def, fx.params, &fx.curves, &fx.luts);
    u32 mask = 0;
    for (int i = 0; i < 4; ++i) if (in[i] && i < (int)def->inputs.size()) mask |= 1u << i;
    for (int p = 0; p < def->passes; ++p) {
        bool toMain = ((def->passes - 1 - p) % 2) == 0;
        gpu::Texture* dst = toMain ? out.get() : tmp.get();
        gpu::Texture* prev = toMain ? tmp.get() : out.get();
        gpu::DispatchDesc d;
        d.pipeline = prog->pipeline.get();
        for (int i = 0; i < 4; ++i) d.srv[i] = (mask >> i) & 1 ? in[i] : nullptr;
        d.srv[4] = p > 0 ? prev : nullptr;
        BindLuts(ctx, d, *def, fx);
        d.uav[0] = dst;
        SetConstants(d, EffectRoot{W, H, 0, 0, refW ? refW : doc.Width(), refH ? refH : doc.Height(), (u32)p, mask, {0, 0, 0, 0, 0, 0, 0, 0}});
        d.cbData = packed.data();
        d.cbSize = packed.size() * sizeof(float4);
        Dispatch2D(ctx, d, W, H);
    }
    return out;
}

void Renderer::FillTexture(gpu::CommandContext& ctx, gpu::Texture* dst, float4 value) {
    gpu::DispatchDesc d;
    d.pipeline = Kernel("clear.slang");
    d.uav[0] = dst;
    SetConstants(d, ClearRoot{0, 0, dst->Width(), dst->Height(), value});
    Dispatch2D(ctx, d, dst->Width(), dst->Height());
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
        if (Display(ctx, src, (u32)docW, (u32)docH, v, t.tex.get())) {  // retry next frame if the kernel isn't ready
            t.hash = hash;
            t.frame = frame;
        }
    }
    return t.tex.get();
}

// ------------------------------------------------------------------ painting

bool Renderer::BeginStroke(Document& doc, gpu::CommandContext& ctx, Id surface) {
    // The stroke state is shared: wet liquid paint is set (recorded) before another stroke takes it over.
    if (fluid.active && !fluid.beginning && fluidInterrupt) fluidInterrupt();
    // A layer created this frame still has its initial clear pending: run it first (it would wipe the stroke).
    if (!doc.pendingClears.empty() || !doc.pendingCopies.empty()) Prepare(doc, ctx);
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
    strokeColorReady = false;
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

void Renderer::StrokeApplySlots(Document& doc, gpu::CommandContext& ctx, const BrushSettings& b, const std::vector<float4>& colors, bool erase) {
    auto* p = Shaders().Builtin("brush.slang");
    if (!p || strokeSlots.empty() || strokeRect.Empty()) return;
    static const Dab none = {0, 0, 0.5f, 0.0f};  // no new coverage: composite only
    Surface* sel = doc.Selection();
    for (size_t i = 0; i < strokeSlots.size(); ++i) {
        Surface* s = doc.GetSurface(strokeSlots[i].surface);
        if (!s) continue;
        const int W = (int)s->tex->Width(), H = (int)s->tex->Height();
        Rect r = strokeRect.Clipped(W, H);
        if (r.Empty()) continue;
        float4 color = i + 1 < colors.size() ? colors[i + 1] : colors.empty() ? float4{1, 1, 1, 1} : colors.back();
        gpu::DispatchDesc d;
        d.pipeline = p;
        d.srv[0] = strokeSlots[i].pre.get();
        d.srv[1] = sel ? sel->tex.get() : nullptr;
        d.uav[0] = s->tex.get();
        d.uav[1] = strokeMask.get();
        BrushRoot root = {r.x0, r.y0, (u32)r.Width(), (u32)r.Height(), (u32)W, (u32)H, 1u, b.hardness, b.opacity, erase ? 1u : 0u, sel ? 1u : 0u, 0.0f, color};
        SetConstants(d, root);
        d.cbData = &none;
        d.cbSize = sizeof(Dab);
        d.threadsX = r.Width();
        d.threadsY = r.Height();
        ctx.Dispatch(d);
        doc.TouchSurface(strokeSlots[i].surface, &r);
    }
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
    liqActive = false;
    if (doc.tiled) liqDisp.reset();
    if (doc.tiled) {
        // Very large documents: release the full-size stroke scratch buffers between strokes.
        strokePre.reset();
        strokeMask.reset();
        strokeColor.reset();
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

gpu::ReadbackPtr Renderer::ExportRGBA8(gpu::CommandContext& ctx, gpu::Texture* image, bool srgb) {
    auto prog = Shaders().CompileNow("export.slang", Shaders().File("export.slang"));
    if (!image || !prog->pipeline) return nullptr;
    auto tmp = gpu::Dev().CreateTexture(image->Width(), image->Height(), gpu::Format::RGBA8, "Export");
    gpu::DispatchDesc d;
    d.pipeline = prog->pipeline.get();
    d.srv[0] = image;
    d.uav[0] = tmp.get();
    SetConstants(d, ExportRoot{image->Width(), image->Height(), srgb ? 1u : 0u});
    d.threadsX = image->Width();
    d.threadsY = image->Height();
    ctx.Dispatch(d);
    return ctx.ReadbackTexture(tmp.get());
}

gpu::ReadbackPtr Renderer::Histogram(gpu::CommandContext& ctx, gpu::Texture* image) {
    auto* k = Kernel("histogram.slang");
    if (!image || !k) return nullptr;
    Ensure(histoTex, 256, 4, gpu::Format::R32U, "Histogram");
    struct { u32 w, h, step, clear; } root = {image->Width(), image->Height(), 1, 1};
    // At most ~4M samples: bounded cost and no 32-bit overflow (255 per sample).
    while ((u64)(root.w / root.step) * (root.h / root.step) > (4u << 20)) ++root.step;
    gpu::DispatchDesc c;
    c.pipeline = k;
    c.uav[0] = histoTex.get();
    SetConstants(c, root);
    Dispatch2D(ctx, c, 256, 4);
    root.clear = 0;
    gpu::DispatchDesc d;
    d.pipeline = k;
    d.srv[0] = image;
    d.uav[0] = histoTex.get();
    SetConstants(d, root);
    Dispatch2D(ctx, d, (root.w + root.step - 1) / root.step, (root.h + root.step - 1) / root.step);
    return ctx.ReadbackTexture(histoTex.get());
}

gpu::ReadbackPtr Renderer::ExportRGBA16(gpu::CommandContext& ctx, gpu::Texture* image, bool srgb) {
    auto prog = Shaders().CompileNow("export.slang", Shaders().File("export.slang"));
    if (!image || !prog->pipeline) return nullptr;
    auto tmp = gpu::Dev().CreateTexture(image->Width(), image->Height(), gpu::Format::RGBA16, "Export16");
    gpu::DispatchDesc d;
    d.pipeline = prog->pipeline.get();
    d.srv[0] = image;
    d.uav[0] = tmp.get();
    SetConstants(d, ExportRoot{image->Width(), image->Height(), srgb ? 1u : 0u});
    Dispatch2D(ctx, d, image->Width(), image->Height());
    return ctx.ReadbackTexture(tmp.get());
}

gpu::ReadbackPtr Renderer::ReadPixel(gpu::CommandContext& ctx, gpu::Texture* image, int x, int y) {
    if (!image) return nullptr;
    Rect r{x, y, x + 1, y + 1};
    return ctx.ReadbackTexture(image, &r);
}

}  // namespace sx
