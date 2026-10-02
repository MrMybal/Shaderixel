// Selection operations and transforms (GPU side).
#include "render/Renderer.h"

#include <cstring>

namespace sx {

using gpu::Shaders;

namespace {
template <class T>
void SetConstants(gpu::DispatchDesc& d, const T& v) {
    static_assert(sizeof(T) % 4 == 0 && sizeof(T) <= gpu::kRootConstants * 4, "root constants too large");
    memcpy(d.constants, &v, sizeof(T));
    d.numConstants = sizeof(T) / 4;
}

struct ShapeRoot { u32 w, h, shape, mode; float4 bounds; i32 ox, oy; u32 flags; };
struct FilterRoot { u32 w, h, op, pass; float radius; };
struct FromRoot { u32 w, h, op, mode; float4 color; float tolerance; u32 flags; };
struct ToMaskRoot { u32 w, h, invert; };
struct FillRoot { u32 w, h, mode, flags; float4 color; float opacity; };
struct BoundsRoot { u32 w, h, flags; };
struct TransformCB {
    float4 h0, h1, h2;
    float4 src;
    u32 w, h, interp, flags;
};

// Compile-on-demand: builtin kernels are tiny, compiling synchronously the first
// time keeps the operations immediate.
const gpu::ComputePipeline* Kernel(const char* name) {
    if (auto* p = Shaders().Builtin(name)) return p;
    return Shaders().CompileNow(name, Shaders().File(name))->pipeline.get();
}
}  // namespace

static void Dispatch2D(gpu::CommandContext& ctx, gpu::DispatchDesc& d, u32 w, u32 h) {
    d.threadsX = w;
    d.threadsY = h;
    ctx.Dispatch(d);
}

// Run a separable selection filter (ops 0..2) or a single-pass op (3 invert) from src into dst.
static void SelFilter(gpu::CommandContext& ctx, int op, float radius, gpu::Texture* src, gpu::Texture* tmp, gpu::Texture* dst) {
    auto* k = Kernel("select_filter.slang");
    u32 W = src->Width(), H = src->Height();
    if (op >= 3) {
        gpu::DispatchDesc d;
        d.pipeline = k;
        d.srv[0] = src;
        d.uav[0] = dst;
        SetConstants(d, FilterRoot{W, H, (u32)op, 0, radius});
        Dispatch2D(ctx, d, W, H);
        return;
    }
    for (u32 pass = 0; pass < 2; ++pass) {
        gpu::DispatchDesc d;
        d.pipeline = k;
        d.srv[0] = pass == 0 ? src : tmp;
        d.uav[0] = pass == 0 ? tmp : dst;
        SetConstants(d, FilterRoot{W, H, (u32)op, pass, radius});
        Dispatch2D(ctx, d, W, H);
    }
}

bool Renderer::SelectShape(Document& doc, gpu::CommandContext& ctx, History& hist, SelShape shape, float4 bounds, SelMode mode,
                           bool antialias, float feather, const std::string& label) {
    auto* k = Kernel("select_shape.slang");
    if (!k) return false;
    const u32 W = doc.Width(), H = doc.Height();
    DocState before = doc.S();
    Surface* prev = doc.Selection();
    gpu::Texture* prevTex = prev ? prev->tex.get() : nullptr;
    // Without a selection, "subtract"/"intersect" behave as if everything was selected... for
    // Photoshop parity, subtract from nothing is nothing, add to nothing is the shape.
    Id id = doc.CreateSurfaceRaw(gpu::Format::R8, "Selection");
    gpu::Texture* out = doc.GetSurface(id)->tex.get();
    if (feather > 0.0f && shape != SelShape::All && shape != SelShape::None) {
        Ensure(selTmp[0], W, H, gpu::Format::R8, "SelTmp0");
        Ensure(selTmp[1], W, H, gpu::Format::R8, "SelTmp1");
        gpu::DispatchDesc d;
        d.pipeline = k;
        d.uav[0] = selTmp[0].get();
        SetConstants(d, ShapeRoot{W, H, (u32)shape, 0, bounds, 0, 0, antialias ? 2u : 0u});
        Dispatch2D(ctx, d, W, H);
        SelFilter(ctx, 0, feather, selTmp[0].get(), selTmp[1].get(), selTmp[0].get());
        gpu::DispatchDesc c;
        c.pipeline = k;
        c.srv[0] = prevTex;
        c.srv[1] = selTmp[0].get();
        c.uav[0] = out;
        SetConstants(c, ShapeRoot{W, H, (u32)SelShape::Coverage, (u32)mode, bounds, 0, 0, prevTex ? 1u : 0u});
        Dispatch2D(ctx, c, W, H);
    } else {
        gpu::DispatchDesc d;
        d.pipeline = k;
        d.srv[0] = prevTex;
        d.uav[0] = out;
        SetConstants(d, ShapeRoot{W, H, (u32)shape, (u32)mode, bounds, 0, 0, (prevTex ? 1u : 0u) | (antialias ? 2u : 0u)});
        Dispatch2D(ctx, d, W, H);
    }
    doc.S().selection = id;
    doc.S().lastSelection = id;
    hist.PushState(label, before, doc.S());
    return true;
}

bool Renderer::SelectCoverage(Document& doc, gpu::CommandContext& ctx, History& hist, const std::vector<u8>& coverage, const Rect& r,
                              SelMode mode, float feather, const std::string& label) {
    if (r.Empty()) return false;
    auto* k = Kernel("select_shape.slang");
    const u32 W = doc.Width(), H = doc.Height();
    auto cov = gpu::Dev().CreateTexture(r.Width(), r.Height(), gpu::Format::R8, "Coverage");
    if (!k || !cov) return false;
    ctx.Upload(cov.get(), coverage.data(), r.Width());
    gpu::Texture* src = cov.get();
    int2 off = {r.x0, r.y0};
    if (feather > 0.0f) {
        // Expand to full size so the blur can spread beyond the polygon bounds.
        Ensure(selTmp[0], W, H, gpu::Format::R8, "SelTmp0");
        Ensure(selTmp[1], W, H, gpu::Format::R8, "SelTmp1");
        gpu::DispatchDesc d;
        d.pipeline = k;
        d.srv[1] = cov.get();
        d.uav[0] = selTmp[0].get();
        SetConstants(d, ShapeRoot{W, H, (u32)SelShape::Coverage, 0, {}, r.x0, r.y0, 0});
        Dispatch2D(ctx, d, W, H);
        SelFilter(ctx, 0, feather, selTmp[0].get(), selTmp[1].get(), selTmp[0].get());
        src = selTmp[0].get();
        off = {0, 0};
    }
    DocState before = doc.S();
    Surface* prev = doc.Selection();
    gpu::Texture* prevTex = prev ? prev->tex.get() : nullptr;
    Id id = doc.CreateSurfaceRaw(gpu::Format::R8, "Selection");
    gpu::DispatchDesc c;
    c.pipeline = k;
    c.srv[0] = prevTex;
    c.srv[1] = src;
    c.uav[0] = doc.GetSurface(id)->tex.get();
    SetConstants(c, ShapeRoot{W, H, (u32)SelShape::Coverage, (u32)mode, {}, off.x, off.y, prevTex ? 1u : 0u});
    Dispatch2D(ctx, c, W, H);
    doc.S().selection = id;
    doc.S().lastSelection = id;
    hist.PushState(label, before, doc.S());
    return true;
}

bool Renderer::SelectFrom(Document& doc, gpu::CommandContext& ctx, History& hist, gpu::Texture* image, SelSource src, SelMode mode,
                          float4 colorSrgb, float tolerance, const std::string& label) {
    auto* k = Kernel("select_from.slang");
    if (!k || !image) return false;
    const u32 W = doc.Width(), H = doc.Height();
    DocState before = doc.S();
    Surface* prev = doc.Selection();
    gpu::Texture* prevTex = prev ? prev->tex.get() : nullptr;
    Id id = doc.CreateSurfaceRaw(gpu::Format::R8, "Selection");
    gpu::DispatchDesc d;
    d.pipeline = k;
    d.srv[0] = image;
    d.srv[1] = prevTex;
    d.uav[0] = doc.GetSurface(id)->tex.get();
    SetConstants(d, FromRoot{W, H, (u32)src, (u32)mode, colorSrgb, tolerance, (prevTex ? 1u : 0u) | 2u});
    Dispatch2D(ctx, d, W, H);
    doc.S().selection = id;
    doc.S().lastSelection = id;
    hist.PushState(label, before, doc.S());
    return true;
}

void Renderer::Deselect(Document& doc, History& hist) {
    if (!doc.S().selection) return;
    DocState before = doc.S();
    doc.S().selection = 0;
    hist.PushState("Désélectionner", before, doc.S());
}

void Renderer::Reselect(Document& doc, History& hist) {
    Id last = doc.S().lastSelection;
    if (!last || doc.S().selection == last || !doc.GetSurface(last)) return;
    DocState before = doc.S();
    doc.S().selection = last;
    hist.PushState("Resélectionner", before, doc.S());
}

bool Renderer::SelectionFilter(Document& doc, gpu::CommandContext& ctx, History& hist, int op, float radius, const std::string& label) {
    Surface* prev = doc.Selection();
    if (!prev && op != 3) return false;
    const u32 W = doc.Width(), H = doc.Height();
    DocState before = doc.S();
    Id id = doc.CreateSurfaceRaw(gpu::Format::R8, "Selection");
    gpu::Texture* out = doc.GetSurface(id)->tex.get();
    if (!prev) {  // invert nothing = everything
        gpu::DispatchDesc d;
        d.pipeline = Kernel("select_shape.slang");
        d.uav[0] = out;
        SetConstants(d, ShapeRoot{W, H, (u32)SelShape::All, 0, {}, 0, 0, 0});
        Dispatch2D(ctx, d, W, H);
    } else {
        Ensure(selTmp[1], W, H, gpu::Format::R8, "SelTmp1");
        SelFilter(ctx, op, radius, prev->tex.get(), selTmp[1].get(), out);
    }
    doc.S().selection = id;
    doc.S().lastSelection = id;
    hist.PushState(label, before, doc.S());
    return true;
}

void Renderer::SelectionToMask(Document& doc, gpu::CommandContext& ctx, Id maskSurface, bool invert) {
    Surface* sel = doc.Selection();
    Surface* m = doc.GetSurface(maskSurface);
    if (!sel || !m) return;
    // The mask may still have a pending clear queued; drop it so it does not overwrite us.
    auto& pc = doc.pendingClears;
    pc.erase(std::remove_if(pc.begin(), pc.end(), [&](auto& c) { return c.first == maskSurface; }), pc.end());
    gpu::DispatchDesc d;
    d.pipeline = Kernel("sel_to_mask.slang");
    d.srv[0] = sel->tex.get();
    d.uav[0] = m->tex.get();
    SetConstants(d, ToMaskRoot{doc.Width(), doc.Height(), invert ? 1u : 0u});
    Dispatch2D(ctx, d, doc.Width(), doc.Height());
    doc.TouchSurface(maskSurface);
}

void Renderer::FillSelection(Document& doc, gpu::CommandContext& ctx, History* hist, Id surface, int mode, float4 color,
                             float opacity, const std::string& label, gpu::Texture* source) {
    Surface* s = doc.GetSurface(surface);
    if (!s) return;
    const u32 W = s->tex->Width(), H = s->tex->Height();
    auto before = gpu::Dev().CreateTexture(W, H, s->tex->GetFormat(), "UndoBefore");
    if (!before) return;
    ctx.CopyTexture(before.get(), s->tex.get());
    Surface* sel = doc.Selection();
    gpu::DispatchDesc d;
    d.pipeline = Kernel("fill_sel.slang");
    d.srv[0] = source ? source : before.get();
    d.srv[1] = sel ? sel->tex.get() : nullptr;
    d.uav[0] = s->tex.get();
    SetConstants(d, FillRoot{W, H, (u32)mode, sel ? 1u : 0u, color, opacity});
    Dispatch2D(ctx, d, W, H);
    if (hist) {
        auto after = gpu::Dev().CreateTexture(W, H, s->tex->GetFormat(), "UndoAfter");
        ctx.CopyTexture(after.get(), s->tex.get());
        hist->PushPaint(label, surface, {0, 0, (int)W, (int)H}, before, after);
    }
    doc.TouchSurface(surface);
}

void Renderer::CopySelected(Document& doc, gpu::CommandContext& ctx, gpu::Texture* src, gpu::Texture* dst) {
    Surface* sel = doc.Selection();
    gpu::DispatchDesc d;
    d.pipeline = Kernel("fill_sel.slang");
    d.srv[0] = src;
    d.srv[1] = sel ? sel->tex.get() : nullptr;
    d.uav[0] = dst;
    SetConstants(d, FillRoot{dst->Width(), dst->Height(), 2u, sel ? 1u : 0u, {}, 1.0f});
    Dispatch2D(ctx, d, dst->Width(), dst->Height());
}

gpu::ReadbackPtr Renderer::ComputeBounds(gpu::CommandContext& ctx, gpu::Texture* image, gpu::Texture* selection, bool imageIsMask) {
    if (!image) return nullptr;
    Ensure(boundsTex, 4, 1, gpu::Format::R32U, "Bounds");
    gpu::DispatchDesc init;
    init.pipeline = Kernel("bounds_init.slang");
    init.uav[0] = boundsTex.get();
    init.threadsX = 4;
    init.threadsY = 1;
    ctx.Dispatch(init);
    gpu::DispatchDesc d;
    d.pipeline = Kernel("bounds.slang");
    d.srv[0] = image;
    d.srv[1] = selection;
    d.uav[0] = boundsTex.get();
    SetConstants(d, BoundsRoot{image->Width(), image->Height(), (selection ? 1u : 0u) | (imageIsMask ? 2u : 0u)});
    Dispatch2D(ctx, d, image->Width(), image->Height());
    return ctx.ReadbackTexture(boundsTex.get());
}

void Renderer::TransformPreview(Document& doc, gpu::CommandContext& ctx, Id surface, gpu::Texture* original, gpu::Texture* selection,
                                const TransformParams& t) {
    Surface* s = doc.GetSurface(surface);
    if (!s || !original) return;
    TransformCB cb;
    cb.h0 = {t.hinv[0], t.hinv[1], t.hinv[2], 0};
    cb.h1 = {t.hinv[3], t.hinv[4], t.hinv[5], 0};
    cb.h2 = {t.hinv[6], t.hinv[7], t.hinv[8], 0};
    cb.src = {(float)t.src.x0, (float)t.src.y0, (float)t.src.x1, (float)t.src.y1};
    cb.w = s->tex->Width();
    cb.h = s->tex->Height();
    cb.interp = (u32)t.interp;
    cb.flags = selection ? 1u : 0u;
    gpu::DispatchDesc d;
    d.pipeline = Kernel("transform.slang");
    d.srv[0] = original;
    d.srv[1] = selection;
    d.uav[0] = s->tex.get();
    d.cbData = &cb;
    d.cbSize = sizeof(cb);
    Dispatch2D(ctx, d, cb.w, cb.h);
    doc.TouchSurface(surface);
}

}  // namespace sx
