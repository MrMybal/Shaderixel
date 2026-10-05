// Tiled evaluation for very large documents.
//
// Instead of caching every node output at full resolution (2 GB per node at
// 16K in half float), the graph is evaluated tile by tile with small transient
// textures; only the final image is kept. Spatial effects request their inputs
// on the tile grown by their halo, so results match full-image rendering.
// Effects with a global halo (`//@halo full`) get their inputs from the
// regular full-resolution cached path.
//
// Only tiles touched by an edit (surface dirty rectangles grown by the graph's
// total halo) are recomputed; structural/parameter changes invalidate all
// tiles. Work is spread over frames (visible tiles first) to keep the UI fluid.

#include "render/Renderer.h"
#include "render/Dispatch.h"

#include <algorithm>
#include <cstring>

namespace sx {

namespace {
struct CompositeRootT { u32 w, h, mode; float opacity; u32 flags, pad; i32 ox, oy, bx, by, tx, ty, mx, my; };
struct EffectRootT { u32 w, h; i32 ox, oy; u32 dw, dh, pass, inputMask; i32 in[8]; };
struct ClearRootT { i32 x, y; u32 w, h; float4 value; };

Rect Grow(const Rect& r, int h, int W, int H) { return Rect{r.x0 - h, r.y0 - h, r.x1 + h, r.y1 + h}.Clipped(W, H); }
}  // namespace

gpu::Texture* Renderer::PoolGet(u32 w, u32 h, gpu::Format f) {
    // Exact sizes: shaders clamp reads to the texture extent, which must be
    // the valid area. Interior tiles share sizes, so reuse is still high.
    u32 rw = w, rh = h;
    for (auto& e : pool)
        if (!e.used && e.tex->Width() == rw && e.tex->Height() == rh && e.tex->GetFormat() == f) {
            e.used = true;
            return e.tex.get();
        }
    PoolEntry e;
    e.tex = gpu::Dev().CreateTexture(rw, rh, f, "TileTmp");
    e.used = true;
    pool.push_back(e);
    return e.tex.get();
}

void Renderer::PoolReset(bool trim) {
    for (auto& e : pool) e.used = false;
    if (trim && pool.size() > 48) pool.erase(pool.begin() + 48, pool.end());
}

u64 Renderer::StructHash(Document& doc, Id id, std::unordered_map<Id, u64>& memo, int& maxHalo, bool& compiling) {
    auto it = memo.find(id);
    if (it != memo.end()) return it->second;
    memo[id] = 0;
    const Node* n = doc.S().graph.Find(id);
    if (!n) return 0;
    Hasher h;
    h.Pod(n->kind);
    h.Pod(n->bypass);
    switch (n->kind) {
        case NodeKind::Surface: h.Pod(n->surface); break;
        case NodeKind::Tool:
            if (ToolBaked(*n)) {
                h.Pod(n->surface);
                memo[id] = h.h;
                return h.h;  // the inputs are not used
            }
            break;
        case NodeKind::Blend: h.Pod(n->blend); h.Pod(n->opacity); h.Pod(n->clip); h.Pod(n->blendIf); h.Pod(n->channelOff); h.Pod(doc.blendInGamma); break;
        case NodeKind::Effect: {
            EffectDefPtr def = n->fx.Def();
            if (def) {
                auto prog = ProgramFor(def);
                if (prog->state.load() == gpu::Program::State::Pending) compiling = true;
                h.Pod(prog->hash);
                auto packed = PackParams(*def, n->fx.params, &n->fx.curves, &n->fx.luts);
                h.Bytes(packed.data(), packed.size() * sizeof(float4));
                if (!n->bypass) {
                    int halo = EffectHalo(*def, n->fx.params);
                    if (halo < 0 || maxHalo < 0) maxHalo = -1;
                    else maxHalo += halo;
                }
            }
            break;
        }
        default: break;
    }
    for (Id in : n->inputs) h.Pod(StructHash(doc, in, memo, maxHalo, compiling));
    for (u8 c : n->inputChannel) h.Pod(c);
    memo[id] = h.h;
    return h.h;
}

Renderer::TileRes Renderer::EvalTileInput(Document& doc, gpu::CommandContext& ctx, const Node& n, int i, const Rect& need, int depth) {
    if (i >= (int)n.inputs.size()) return {};
    TileRes src = EvalTile(doc, ctx, n.inputs[i], need, depth);
    u8 ch = InputChannel(n, i);
    if (!ch || !src.tex) return src;
    auto* k = Kernel("channel_extract.slang");
    if (!k) { pending = true; return {}; }
    gpu::Texture* out = PoolGet((u32)need.Width(), (u32)need.Height(), src.tex->GetFormat());
    struct { u32 w, h; i32 ox, oy; u32 ch; } root = {(u32)need.Width(), (u32)need.Height(), need.x0 - src.origin.x, need.y0 - src.origin.y, ch};
    gpu::DispatchDesc d;
    d.pipeline = k;
    d.srv[0] = src.tex;
    d.uav[0] = out;
    SetConstants(d, root);
    Dispatch2D(ctx, d, root.w, root.h);
    return {out, {need.x0, need.y0}};
}

Renderer::TileRes Renderer::EvalTile(Document& doc, gpu::CommandContext& ctx, Id id, const Rect& need, int depth) {
    if (!id || depth > 256 || need.Empty()) return {};
    u64 key = ((u64)id << 32) ^ ((u64)need.x0 * 73856093u) ^ ((u64)need.y0 * 19349663u) ^ ((u64)need.x1 * 83492791u) ^ ((u64)need.y1 * 2654435761u);
    auto memo = tileMemo.find(key);
    if (memo != tileMemo.end()) return memo->second;
    const Node* n = doc.S().graph.Find(id);
    if (!n) return {};
    const int W = (int)doc.Width(), H = (int)doc.Height();
    const gpu::Format fmt = SurfaceFormat(doc.GetPrecision());
    TileRes res;
    switch (n->kind) {
        case NodeKind::Output:
            res = n->inputs.empty() ? TileRes{} : EvalTileInput(doc, ctx, *n, 0, need, depth + 1);
            break;
        case NodeKind::Split:
            res = EvalTileInput(doc, ctx, *n, 0, need, depth + 1);
            break;
        case NodeKind::Surface:
            if (Surface* s = doc.GetSurface(n->surface)) res = {s->tex.get(), {0, 0}};
            break;
        case NodeKind::Tool:
            if (Surface* s = ToolBaked(*n) ? doc.GetSurface(n->surface) : nullptr) res = {s->tex.get(), {0, 0}};
            else res = EvalTileInput(doc, ctx, *n, 0, need, depth + 1);
            break;
        case NodeKind::Blend: {
            TileRes in[3];
            for (int i = 0; i < 3 && i < (int)n->inputs.size(); ++i) in[i] = EvalTileInput(doc, ctx, *n, i, need, depth + 1);
            if (n->bypass || !in[1].tex) { res = in[0]; break; }
            auto* p = Kernel("composite.slang");
            gpu::Texture* out = PoolGet((u32)need.Width(), (u32)need.Height(), fmt);
            gpu::DispatchDesc d;
            d.pipeline = p;
            d.srv[0] = in[0].tex;
            d.srv[1] = in[1].tex;
            d.srv[2] = in[2].tex;
            d.uav[0] = out;
            u32 flags = (in[2].tex ? 1u : 0u) | (doc.blendInGamma ? 2u : 0u) | 4u | (in[0].tex ? 8u : 0u) | (n->clip ? 16u : 0u) | (n->blendIf.Active() ? 32u : 0u) | ((u32)n->channelOff << 8);
                float4 bif[8];
                n->blendIf.Ranges(bif);
                d.cbData = bif;
                d.cbSize = sizeof(bif);
            SetConstants(d, CompositeRootT{(u32)need.Width(), (u32)need.Height(), (u32)n->blend, n->opacity, flags, 0, need.x0, need.y0,
                                           in[0].origin.x, in[0].origin.y, in[1].origin.x, in[1].origin.y, in[2].origin.x, in[2].origin.y});
            Dispatch2D(ctx, d, (u32)need.Width(), (u32)need.Height());
            res = {out, {need.x0, need.y0}};
            break;
        }
        case NodeKind::Effect: {
            EffectDefPtr def = n->fx.Def();
            int nin = def ? (int)def->inputs.size() : 0;
            if (n->bypass || !def) {
                res = n->inputs.empty() ? TileRes{} : EvalTileInput(doc, ctx, *n, 0, need, depth + 1);
                break;
            }
            auto prog = ProgramFor(def);
            if (prog->state.load() != gpu::Program::State::Ready) {
                pending = prog->state.load() == gpu::Program::State::Pending;
                res = n->inputs.empty() ? TileRes{} : EvalTileInput(doc, ctx, *n, 0, need, depth + 1);
                break;
            }
            int halo = EffectHalo(*def, n->fx.params);
            Rect area = halo < 0 ? need : Grow(need, halo, W, H);
            TileRes in[4];
            for (int i = 0; i < 4 && i < nin && i < (int)n->inputs.size(); ++i) {
                if (!n->inputs[i]) continue;
                if (halo < 0) {
                    // Global effect: full-resolution input from the cached path.
                    EvalResult full = EvalInput(doc, ctx, *n, i, depth + 1);
                    in[i] = {full.tex, {0, 0}};
                } else {
                    in[i] = EvalTileInput(doc, ctx, *n, i, area, depth + 1);
                }
            }
            auto packed = PackParams(*def, n->fx.params, &n->fx.curves, &n->fx.luts);
            u32 mask = 0;
            for (int i = 0; i < 4; ++i) if (in[i].tex) mask |= 1u << i;
            gpu::Texture* a = PoolGet((u32)area.Width(), (u32)area.Height(), fmt);
            gpu::Texture* b = def->passes > 1 ? PoolGet((u32)area.Width(), (u32)area.Height(), fmt) : nullptr;
            gpu::Texture* last = a;
            for (int p = 0; p < def->passes; ++p) {
                gpu::Texture* dst = (p % 2 == 0) ? a : b;
                gpu::Texture* prev = (p % 2 == 0) ? b : a;
                gpu::DispatchDesc d;
                d.pipeline = prog->pipeline.get();
                for (int i = 0; i < 4; ++i) d.srv[i] = in[i].tex;
                d.srv[4] = p > 0 ? prev : nullptr;
                BindLuts(ctx, d, *def, n->fx);
                d.uav[0] = dst;
                EffectRootT root = {(u32)area.Width(), (u32)area.Height(), area.x0, area.y0, (u32)W, (u32)H, (u32)p, mask,
                                    {in[0].origin.x, in[0].origin.y, in[1].origin.x, in[1].origin.y, in[2].origin.x, in[2].origin.y, in[3].origin.x, in[3].origin.y}};
                SetConstants(d, root);
                d.cbData = packed.data();
                d.cbSize = packed.size() * sizeof(float4);
                Dispatch2D(ctx, d, (u32)area.Width(), (u32)area.Height());
                last = dst;
            }
            res = {last, {area.x0, area.y0}};
            break;
        }
    }
    tileMemo[key] = res;
    return res;
}

gpu::Texture* Renderer::EvaluateTiled(Document& doc, gpu::CommandContext& ctx, const Rect* visible, int budget) {
    const u32 W = doc.Width(), H = doc.Height();
    const gpu::Format fmt = SurfaceFormat(doc.GetPrecision());
    const int T = kTileSize;
    pending = false;
    frameEval.clear();
    // (Re)allocate the output and the tile grid.
    if (!tiledOut || tiledOut->Width() != W || tiledOut->Height() != H || tiledOut->GetFormat() != fmt) {
        tiledOut = gpu::Dev().CreateTexture(W, H, fmt, "TiledOutput");
        tilesX = ((int)W + T - 1) / T;
        tilesY = ((int)H + T - 1) / T;
        tileDirty.assign((size_t)tilesX * tilesY, 1);
        tiledStruct = 0;
        tiledVersions.clear();
    }
    // Structural / parameter changes: everything.
    std::unordered_map<Id, u64> memo;
    int maxHalo = 0;
    bool compiling = false;
    u64 sh = StructHash(doc, doc.S().graph.output, memo, maxHalo, compiling);
    sh ^= ((u64)W << 20) ^ H;
    if (sh != tiledStruct) {
        std::fill(tileDirty.begin(), tileDirty.end(), 1);
        tiledStruct = sh;
    }
    // Pixel edits: dirty rectangles grown by the total halo downstream.
    auto markRect = [&](const Rect& r0) {
        Rect r = maxHalo < 0 ? Rect{0, 0, (int)W, (int)H} : Grow(r0, maxHalo, (int)W, (int)H);
        if (r.Empty()) return;
        for (int ty = r.y0 / T; ty <= (r.y1 - 1) / T && ty < tilesY; ++ty)
            for (int tx = r.x0 / T; tx <= (r.x1 - 1) / T && tx < tilesX; ++tx) tileDirty[(size_t)ty * tilesX + tx] = 1;
    };
    for (auto& [sid, s] : doc.S().surfaces) {
        auto it = tiledVersions.find(sid);
        if (it != tiledVersions.end() && it->second == s.version) continue;
        bool known = it != tiledVersions.end();
        tiledVersions[sid] = s.version;
        auto dr = doc.dirtyRects.find(sid);
        if (!known) continue;  // new surface: structure hash already covers it
        if (dr != doc.dirtyRects.end()) markRect(dr->second);
        else markRect({0, 0, (int)W, (int)H});
    }
    doc.dirtyRects.clear();
    if (compiling) {
        pending = true;
        return tiledOut.get();
    }

    // Order dirty tiles: visible first.
    std::vector<int> order;
    for (int i = 0; i < (int)tileDirty.size(); ++i) if (tileDirty[i]) order.push_back(i);
    tilesPending = (int)order.size();
    if (order.empty()) return tiledOut.get();
    if (visible) {
        std::stable_partition(order.begin(), order.end(), [&](int i) {
            Rect tr{(i % tilesX) * T, (i / tilesX) * T, (i % tilesX + 1) * T, (i / tilesX + 1) * T};
            return !(tr.x1 <= visible->x0 || tr.x0 >= visible->x1 || tr.y1 <= visible->y0 || tr.y0 >= visible->y1);
        });
    }
    int done = 0;
    for (int i : order) {
        if (done >= budget) break;
        Rect tile = Rect{(i % tilesX) * T, (i / tilesX) * T, (i % tilesX + 1) * T, (i / tilesX + 1) * T}.Clipped((int)W, (int)H);
        tileMemo.clear();
        PoolReset(false);
        TileRes r = EvalTile(doc, ctx, doc.S().graph.output, tile, 0);
        if (r.tex) {
            ctx.CopyRegion(tiledOut.get(), tile.x0, tile.y0, r.tex, {tile.x0 - r.origin.x, tile.y0 - r.origin.y, tile.x1 - r.origin.x, tile.y1 - r.origin.y});
        } else {
            gpu::DispatchDesc d;
            d.pipeline = Kernel("clear.slang");
            d.uav[0] = tiledOut.get();
            SetConstants(d, ClearRootT{tile.x0, tile.y0, (u32)tile.Width(), (u32)tile.Height(), {0, 0, 0, 0}});
            Dispatch2D(ctx, d, (u32)tile.Width(), (u32)tile.Height());
        }
        tileDirty[i] = 0;
        MarkImageChanged(tile);
        ++done;
    }
    tileMemo.clear();
    PoolReset(true);
    tilesPending -= done;
    if (tilesPending > 0) pending = true;
    return tiledOut.get();
}

}  // namespace sx
