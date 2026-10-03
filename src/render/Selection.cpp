// Selection operations and transforms (GPU side).
#include "render/Renderer.h"
#include "render/Dispatch.h"

#include <cstring>
#include <unordered_map>

namespace sx {

using gpu::Shaders;

namespace {
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

}  // namespace

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

void Renderer::DispatchSelOffset(gpu::CommandContext& ctx, gpu::Texture* src, gpu::Texture* dst, int2 offset) {
    struct { u32 w, h; i32 dx, dy; } root = {dst->Width(), dst->Height(), offset.x, offset.y};
    gpu::DispatchDesc d;
    d.pipeline = Kernel("sel_offset.slang");
    d.srv[0] = src;
    d.uav[0] = dst;
    SetConstants(d, root);
    Dispatch2D(ctx, d, dst->Width(), dst->Height());
}

bool Renderer::OffsetSelection(Document& doc, gpu::CommandContext& ctx, History& hist, int2 offset, const std::string& label) {
    Surface* prev = doc.Selection();
    if (!prev || (offset.x == 0 && offset.y == 0)) return false;
    DocState before = doc.S();
    Id id = doc.CreateSurfaceRaw(gpu::Format::R8, "Selection");
    DispatchSelOffset(ctx, prev->tex.get(), doc.GetSurface(id)->tex.get(), offset);
    doc.S().selection = id;
    doc.S().lastSelection = id;
    hist.PushState(label, before, doc.S());
    return true;
}

gpu::Texture* Renderer::OffsetSelectionPreview(Document& doc, gpu::CommandContext& ctx, int2 offset) {
    Surface* sel = doc.Selection();
    if (!sel) return nullptr;
    Ensure(selMove, doc.Width(), doc.Height(), gpu::Format::R8, "SelMove");
    DispatchSelOffset(ctx, sel->tex.get(), selMove.get(), offset);
    return selMove.get();
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
                             float opacity, const std::string& label, gpu::Texture* source, gpu::Texture* coverage, bool useSelection) {
    Surface* s = doc.GetSurface(surface);
    if (!s) return;
    const u32 W = s->tex->Width(), H = s->tex->Height();
    auto before = gpu::Dev().CreateTexture(W, H, s->tex->GetFormat(), "UndoBefore");
    if (!before) return;
    ctx.CopyTexture(before.get(), s->tex.get());
    Surface* sel = useSelection ? doc.Selection() : nullptr;
    gpu::DispatchDesc d;
    d.pipeline = Kernel("fill_sel.slang");
    d.srv[0] = source ? source : before.get();
    d.srv[1] = sel ? sel->tex.get() : nullptr;
    d.srv[2] = coverage;
    d.uav[0] = s->tex.get();
    SetConstants(d, FillRoot{W, H, (u32)mode, (sel ? 1u : 0u) | (coverage ? 2u : 0u), color, opacity});
    Dispatch2D(ctx, d, W, H);
    if (hist) {
        auto after = gpu::Dev().CreateTexture(W, H, s->tex->GetFormat(), "UndoAfter");
        ctx.CopyTexture(after.get(), s->tex.get());
        hist->PushPaint(label, surface, {0, 0, (int)W, (int)H}, before, after);
    }
    doc.TouchSurface(surface);
}

bool Renderer::StrokeSelection(Document& doc, gpu::CommandContext& ctx, History& hist, Id surface, float width, int position, float4 color,
                               float opacity) {
    Surface* sel = doc.Selection();
    if (!sel || !doc.GetSurface(surface) || width <= 0) return false;
    const u32 W = doc.Width(), H = doc.Height();
    Ensure(selTmp[0], W, H, gpu::Format::R8, "SelTmp0");
    Ensure(selTmp[1], W, H, gpu::Format::R8, "SelTmp1");
    auto outer = gpu::Dev().CreateTexture(W, H, gpu::Format::R8, "StrokeOuter");
    auto inner = gpu::Dev().CreateTexture(W, H, gpu::Format::R8, "StrokeInner");
    auto ring = gpu::Dev().CreateTexture(W, H, gpu::Format::R8, "StrokeRing");
    gpu::Texture* src = sel->tex.get();
    float outR = position == 0 ? 0 : position == 1 ? width * 0.5f : width;
    float inR = position == 0 ? width : position == 1 ? width * 0.5f : 0;
    if (outR > 0) SelFilter(ctx, 1, outR, src, selTmp[0].get(), outer.get());
    else ctx.CopyTexture(outer.get(), src);
    if (inR > 0) SelFilter(ctx, 2, inR, src, selTmp[1].get(), inner.get());
    else ctx.CopyTexture(inner.get(), src);
    gpu::DispatchDesc d;
    d.pipeline = Kernel("sel_subtract.slang");
    d.srv[0] = outer.get();
    d.srv[1] = inner.get();
    d.uav[0] = ring.get();
    struct { u32 w, h; } root = {W, H};
    SetConstants(d, root);
    Dispatch2D(ctx, d, W, H);
    FillSelection(doc, ctx, &hist, surface, 0, color, opacity, "Contour", nullptr, ring.get(), false);
    return true;
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

gpu::ReadbackPtr Renderer::ComputeBounds(gpu::CommandContext& ctx, gpu::Texture* image, gpu::Texture* selection, bool imageIsMask, bool useRed) {
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
    SetConstants(d, BoundsRoot{image->Width(), image->Height(), (selection ? 1u : 0u) | (imageIsMask ? 2u : 0u) | (useRed ? 4u : 0u)});
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

// ------------------------------------------------------------------ warp

float2 BezierPatch(const float2 g[16], float u, float v) {
    float bu[4] = {(1 - u) * (1 - u) * (1 - u), 3 * u * (1 - u) * (1 - u), 3 * u * u * (1 - u), u * u * u};
    float bv[4] = {(1 - v) * (1 - v) * (1 - v), 3 * v * (1 - v) * (1 - v), 3 * v * v * (1 - v), v * v * v};
    float2 p;
    for (int j = 0; j < 4; ++j)
        for (int i = 0; i < 4; ++i) p = p + g[j * 4 + i] * (bu[i] * bv[j]);
    return p;
}

namespace {
const gpu::GraphicsPipeline* WarpPipeline(gpu::Format f) {
    static std::unordered_map<int, gpu::GraphicsPipelinePtr> cache;
    static u64 gen = ~0ull;
    if (gen != Shaders().FileGeneration()) { cache.clear(); gen = Shaders().FileGeneration(); }
    auto it = cache.find((int)f);
    if (it != cache.end()) return it->second.get();
    auto prog = Shaders().CompileGraphicsNow("warp.slang", Shaders().File("warp.slang"));
    if (prog->state.load() != gpu::Program::State::Ready) return nullptr;
    gpu::GraphicsDesc gd;
    gd.colorFormats = {f};
    gd.useDepth = false;
    gd.depthWrite = false;
    auto p = gpu::Dev().CreateGraphicsPipeline(prog->vs.data(), prog->vs.size(), prog->ps.data(), prog->ps.size(), gd);
    cache[(int)f] = p;
    return p.get();
}
struct WarpCB { float4 src; float2 origin, target, docSize; u32 flags; u32 pad; };
struct WarpCombineRoot { u32 w, h; i32 ox, oy; u32 ww, wh, flags; };
}  // namespace

WarpMesh BezierWarpMesh(const float2 grid[16], const Rect& src, int n) {
    WarpMesh m;
    m.nx = m.ny = n;
    for (int y = 0; y <= n; ++y)
        for (int x = 0; x <= n; ++x) {
            float u = (float)x / n, v = (float)y / n;
            m.dst.push_back(BezierPatch(grid, u, v));
            m.src.push_back({src.x0 + u * src.Width(), src.y0 + v * src.Height()});
        }
    return m;
}

void Renderer::WarpPreview(Document& doc, gpu::CommandContext& ctx, Id surface, gpu::Texture* original, gpu::Texture* selection, const Rect& src,
                           const WarpMesh& mesh, bool mask) {
    const int NX = mesh.nx, NY = mesh.ny;
    if (NX <= 0 || NY <= 0 || mesh.dst.size() != (size_t)(NX + 1) * (NY + 1) || mesh.src.size() != mesh.dst.size()) return;
    std::vector<u32> idx;
    idx.reserve((size_t)NX * NY * 6);
    for (int y = 0; y < NY; ++y)
        for (int x = 0; x < NX; ++x) {
            u32 a = y * (NX + 1) + x, b = a + 1, c = a + NX + 1, d = c + 1;
            idx.insert(idx.end(), {a, b, d, a, d, c});
        }
    WarpTriangles(doc, ctx, surface, original, selection, src, mesh.dst, mesh.src, idx, mask);
}

void Renderer::WarpTriangles(Document& doc, gpu::CommandContext& ctx, Id surface, gpu::Texture* original, gpu::Texture* selection, const Rect& src,
                             const std::vector<float2>& dst, const std::vector<float2>& srcPts, const std::vector<u32>& idx, bool mask) {
    Surface* s = doc.GetSurface(surface);
    auto* combine = Kernel("warp_combine.slang");
    const gpu::GraphicsPipeline* pipe = s && original ? WarpPipeline(s->tex->GetFormat()) : nullptr;
    if (!s || !original || !combine || !pipe || src.Empty() || dst.size() != srcPts.size() || idx.empty()) return;
    const u32 W = s->tex->Width(), H = s->tex->Height();
    struct Vtx { float pos[3], nrm[3], uv[2]; };
    std::vector<Vtx> verts;
    verts.reserve(dst.size());
    float bx0 = 1e30f, by0 = 1e30f, bx1 = -1e30f, by1 = -1e30f;
    for (size_t i = 0; i < dst.size(); ++i) {
        float2 p = dst[i];
        bx0 = std::min(bx0, p.x); by0 = std::min(by0, p.y); bx1 = std::max(bx1, p.x); by1 = std::max(by1, p.y);
        verts.push_back({{p.x, p.y, 0}, {0, 0, 1}, {srcPts[i].x, srcPts[i].y}});
    }
    Rect box = Rect{(int)std::floor(bx0) - 2, (int)std::floor(by0) - 2, (int)std::ceil(bx1) + 2, (int)std::ceil(by1) + 2}.Clipped((int)W, (int)H);
    gpu::TexturePtr warped;
    if (!box.Empty()) {
        if (!warpTarget || warpTarget->Width() != (u32)box.Width() || warpTarget->Height() != (u32)box.Height() || warpTarget->GetFormat() != s->tex->GetFormat())
            warpTarget = gpu::Dev().CreateTexture((u32)box.Width(), (u32)box.Height(), s->tex->GetFormat(), "Warp", gpu::UsageRenderTarget);
        auto vb = gpu::Dev().CreateBuffer(ctx, verts.data(), verts.size() * sizeof(Vtx), "WarpVB");
        auto ib = gpu::Dev().CreateBuffer(ctx, idx.data(), idx.size() * sizeof(u32), "WarpIB");
        WarpCB cb = {{(float)src.x0, (float)src.y0, (float)src.x1, (float)src.y1}, {(float)box.x0, (float)box.y0},
                     {(float)box.Width(), (float)box.Height()}, {(float)W, (float)H}, (selection ? 1u : 0u) | (mask ? 2u : 0u), 0};
        gpu::DrawDesc d;
        d.pipeline = pipe;
        d.srv[0] = original;
        d.srv[1] = selection;
        d.rt[0] = warpTarget.get();
        d.clearColor = true;
        d.clearValue = {0, 0, 0, 0};
        d.cbData = &cb;
        d.cbSize = sizeof(cb);
        d.vertices = vb.get();
        d.vertexStride = sizeof(Vtx);
        d.indices = ib.get();
        d.count = (u32)idx.size();
        ctx.Draw(d);
        warped = warpTarget;
    }
    gpu::DispatchDesc c;
    c.pipeline = combine;
    c.srv[0] = original;
    c.srv[1] = selection;
    c.srv[2] = warped.get();
    c.uav[0] = s->tex.get();
    SetConstants(c, WarpCombineRoot{W, H, box.x0, box.y0, warped ? (u32)box.Width() : 0u, warped ? (u32)box.Height() : 0u, selection && !mask ? 1u : 0u});
    Dispatch2D(ctx, c, W, H);
    doc.TouchSurface(surface);
}

}  // namespace sx
