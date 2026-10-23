// Liquid paint: a film of paint simulated on the GPU in texture space (shaders/fluid_paint.slang),
// composited live into the layers of a stroke (one per material channel). The stroke stays open
// while the paint moves; ending it records one undo step for every layer.
#include "render/Renderer.h"
#include "render/Dispatch.h"
#include "render/MeshPaint.h"
#include "doc/History.h"

#include <unordered_map>

namespace sx {

namespace {

struct FluidRoot {
    u32 w, h, docW, docH;
    u32 pass, kind, flags;
    float dt, seed;
    float pad[3];
};

struct FluidCB {
    float4 gravity, phys, phys2, look, look2, color, mat, emissive;
    float4 dabs[512];
    float4 dabsEx[512];
};

void FillCB(FluidCB& cb, const Renderer::FluidParams& p) {
    cb.gravity = {p.gravity.x, p.gravity.y, p.gravity.z, p.strength};
    cb.phys = {p.speed, p.retention, p.spread, p.setTime};
    cb.phys2 = {p.noise, p.noiseScale, p.absorb, p.damping};
    cb.look = {p.density, p.relief, p.heightBase, p.wetGloss};
    cb.look2 = {p.bed, 1.0f, p.dripOff, p.hardness};
    cb.color = p.color;
    cb.mat = {p.rough, p.metal, p.cohesion, p.coat};
    cb.emissive = p.emissive;
}

}  // namespace

bool Renderer::FluidBegin(Document& doc, gpu::CommandContext& ctx, const std::vector<Id>& surfaces, const std::vector<int>& kinds, MeshScene* mesh,
                          u32 simW, u32 simH, gpu::Texture* selection, gpu::Texture* bed) {
    if (!Kernel("fluid_paint.slang")) return false;
    if (mesh && (!MeshPrepare(doc, ctx, *mesh) || !mesh->posMap)) return false;
    fluid.beginning = true;
    const bool begun = BeginStrokeMulti(doc, ctx, surfaces);
    fluid.beginning = false;
    if (!begun) return false;
    FluidState& F = fluid;
    F.kinds = kinds;
    F.mesh = mesh;
    // Own copies: the selection and the relief may change while the paint runs.
    F.selection.reset();
    F.bed.reset();
    if (selection) {
        F.selection = gpu::Dev().CreateTexture(selection->Width(), selection->Height(), selection->GetFormat(), "FluidSelection");
        ctx.CopyTexture(F.selection.get(), selection);
    }
    if (bed) {
        F.bed = gpu::Dev().CreateTexture(bed->Width(), bed->Height(), bed->GetFormat(), "FluidBed");
        ctx.CopyTexture(F.bed.get(), bed);
    }
    F.w = std::max(simW, 8u);
    F.h = std::max(simH, 8u);
    const char* names[4][2] = {{"FluidF0a", "FluidF0b"}, {"FluidF1a", "FluidF1b"}, {"FluidD0a", "FluidD0b"}, {"FluidD1a", "FluidD1b"}};
    for (int k = 0; k < 2; ++k) {
        Ensure(F.f0[k], F.w, F.h, gpu::Format::RGBA32F, names[0][k]);
        Ensure(F.f1[k], F.w, F.h, gpu::Format::RGBA32F, names[1][k]);  // age: half floats stop counting after a few seconds
        Ensure(F.d0[k], F.w, F.h, gpu::Format::RGBA16F, names[2][k]);
        Ensure(F.d1[k], F.w, F.h, gpu::Format::RGBA16F, names[3][k]);
        Ensure(F.flux[k], F.w, F.h, gpu::Format::RGBA32F, k ? "FluidFluxB" : "FluidFluxA");
    }
    Ensure(F.grav, F.w, F.h, gpu::Format::RGBA16F, "FluidGravity");
    F.cur = 0;
    F.fluxCur = 0;
    // World size of a simulation texel (mean over the model): converts slopes to texels.
    F.texelWorld = 1.0f;
    if (mesh) {
        const auto& V = mesh->data.vertices;
        const auto& I = mesh->data.indices;
        double a3 = 0, auv = 0;
        for (size_t t = 0; t + 2 < I.size(); t += 3) {
            const MeshVertex &A = V[I[t]], &B = V[I[t + 1]], &C = V[I[t + 2]];
            a3 += Length(Cross(B.pos - A.pos, C.pos - A.pos)) * 0.5;
            const float2 e1 = B.uv - A.uv, e2 = C.uv - A.uv;
            auv += std::fabs(e1.x * e2.y - e1.y * e2.x) * 0.5;
        }
        if (auv > 1e-12 && a3 > 0) F.texelWorld = (float)(std::sqrt(a3 / auv) / std::sqrt((double)F.w * F.h));
    }
    // Clear the state.
    gpu::DispatchDesc d;
    d.pipeline = Kernel("fluid_paint.slang");
    for (int k = 0; k < 2; ++k) {
        d.uav[0] = F.f0[k].get();
        d.uav[1] = F.f1[k].get();
        d.uav[2] = F.d0[k].get();
        d.uav[3] = F.d1[k].get();
        SetConstants(d, FluidRoot{F.w, F.h, doc.Width(), doc.Height(), 5, 0, 0, 0, 0, {0, 0, 0}});
        Dispatch2D(ctx, d, F.w, F.h);
        d.uav[0] = F.flux[k].get();
        d.uav[1] = F.grav.get();
        d.uav[2] = F.d0[1 - k].get();
        d.uav[3] = F.d1[1 - k].get();
        Dispatch2D(ctx, d, F.w, F.h);
    }
    F.docW = doc.Width();
    F.docH = doc.Height();
    F.active = true;
    F.time = 0;
    return true;
}

void Renderer::FluidDispatch(gpu::CommandContext& ctx, u32 pass, u32 kind, float dt, const FluidParams& p, const std::vector<float4>* dabs,
                             const std::vector<float4>* ex, u32 extraFlags) {
    FluidState& F = fluid;
    auto* k = Kernel("fluid_paint.slang");
    if (!k) return;
    static FluidCB cb;
    FillCB(cb, p);
    cb.look2.y = F.texelWorld;
    size_t n = 0;
    if (dabs) {
        n = std::min<size_t>(dabs->size(), 512);
        for (size_t i = 0; i < n; ++i) {
            cb.dabs[i] = (*dabs)[i];
            cb.dabsEx[i] = ex && i < ex->size() ? (*ex)[i] : float4{1, 0, 0, 0};
        }
        kind = (u32)n;
    }
    u32 flags = extraFlags;
    if (F.mesh) flags |= 1;
    if (!F.mesh && F.selection) flags |= 4;
    if (p.offEdges) flags |= 8;
    if (F.bed && p.bed > 0) flags |= 16;
    if (F.mesh && F.portalOut && F.portalKey && F.portalKey == F.portalPending) flags |= 64;
    const int c = F.cur, o = 1 - F.cur;
    gpu::DispatchDesc d;
    d.pipeline = k;
    d.srv[0] = F.f0[c].get();
    d.srv[1] = F.f1[c].get();
    d.srv[2] = F.d0[c].get();
    d.srv[3] = F.d1[c].get();
    d.srv[4] = F.flux[F.fluxCur].get();
    d.srv[5] = F.grav.get();
    d.srv[6] = F.bed.get();
    d.srv[7] = F.mesh ? F.mesh->posMap.get() : nullptr;
    d.srv[8] = F.mesh ? F.mesh->nrmMap.get() : nullptr;
    d.srv[9] = pass == 0 ? F.selection.get() : nullptr;
    if (flags & 64) {
        d.srv[10] = F.portalOut.get();
        d.srv[11] = F.portalIn.get();
    }
    if (pass == 0) {
        d.srv[5] = nullptr;
        d.uav[0] = F.grav.get();
    } else if (pass == 2) {
        d.uav[0] = F.flux[1 - F.fluxCur].get();
    } else {
        d.uav[0] = F.f0[o].get();
        d.uav[1] = F.f1[o].get();
        d.uav[2] = F.d0[o].get();
        d.uav[3] = F.d1[o].get();
    }
    SetConstants(d, FluidRoot{F.w, F.h, F.docW, F.docH, pass, kind, flags, dt, p.seed, {0, 0, 0}});
    d.cbData = &cb;
    d.cbSize = n ? sizeof(FluidCB) : offsetof(FluidCB, dabs);  // the extras follow the full dab array
    Dispatch2D(ctx, d, F.w, F.h);
    if (pass == 2) F.fluxCur = 1 - F.fluxCur;
    else if (pass != 0) F.cur = o;
}

std::vector<Id> Renderer::FluidSurfaces() const {
    std::vector<Id> out;
    if (!fluid.active || !strokeSurface) return out;
    out.push_back(strokeSurface);
    for (auto& s : strokeSlots) out.push_back(s.surface);
    return out;
}

void Renderer::FluidGravity(gpu::CommandContext& ctx, const FluidParams& p) {
    if (!fluid.active) return;
    FluidDispatch(ctx, 0, 0, 0, p, nullptr, nullptr, 0);
}

void Renderer::FluidDeposit(gpu::CommandContext& ctx, const std::vector<float4>& dabs, const std::vector<float4>& ex, bool world, bool erase,
                            const FluidParams& p) {
    if (!fluid.active || dabs.empty()) return;
    for (size_t i = 0; i < dabs.size(); i += 512) {
        std::vector<float4> d(dabs.begin() + i, dabs.begin() + std::min(dabs.size(), i + 512));
        std::vector<float4> e(ex.begin() + std::min(ex.size(), i), ex.begin() + std::min(ex.size(), i + 512));
        FluidDispatch(ctx, 1, 0, 0, p, &d, &e, (world ? 2u : 0u) | (erase ? 32u : 0u));
    }
}

void Renderer::FluidPortals(gpu::CommandContext& ctx) {
    FluidState& F = fluid;
    if (!F.mesh || !F.mesh->posMap || !F.mesh->nrmMap) return;
    const u64 key = ((u64)F.mesh->revision * 1000003ull) ^ ((u64)F.w << 40) ^ ((u64)F.h << 20) ^ (u64)F.docW * 7919ull ^ 0x51ED;
    if (F.portalKey == key && F.portalPending == key) return;  // built
    if (F.portalPending != key) {
        // Position and normal maps to the CPU (once per model and size).
        F.rbPos = ctx.ReadbackTexture(F.mesh->posMap.get());
        F.rbNrm = ctx.ReadbackTexture(F.mesh->nrmMap.get());
        F.portalPending = key;
        F.portalKey = 0;
        return;
    }
    if (!F.rbPos || !F.rbNrm || !F.rbPos->ready || !F.rbNrm->ready) return;
    const u32 W = F.w, H = F.h, DW = F.rbPos->width, DH = F.rbPos->height;
    const float* pos = (const float*)F.rbPos->data.data();
    const float* nrm = (const float*)F.rbNrm->data.data();
    if (F.rbPos->data.size() < (size_t)DW * DH * 16 || F.rbNrm->data.size() < (size_t)DW * DH * 16) return;
    // Simulation texel -> document pixel, as the shader (DocPixel).
    auto docIndex = [&](int x, int y) {
        const int dx = std::clamp((int)((x + 0.5f) / W * DW), 0, (int)DW - 1), dy = std::clamp((int)((y + 0.5f) / H * DH), 0, (int)DH - 1);
        return (size_t)dy * DW + dx;
    };
    const size_t N = (size_t)W * H;
    std::vector<u8> valid(N, 0);
    for (u32 y = 0; y < H; ++y)
        for (u32 x = 0; x < W; ++x) valid[(size_t)y * W + x] = pos[docIndex(x, y) * 4 + 3] > 0.75f ? 1 : 0;  // not the padding
    auto P = [&](size_t i) { const float* q = pos + docIndex((int)(i % W), (int)(i / W)) * 4; return float3{q[0], q[1], q[2]}; };
    auto Nn = [&](size_t i) { const float* q = nrm + docIndex((int)(i % W), (int)(i / W)) * 4; return Normalize(float3{q[0], q[1], q[2]}); };
    const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    auto isBorder = [&](size_t i) {
        if (!valid[i]) return false;
        const int x = (int)(i % W), y = (int)(i / W);
        for (int k = 0; k < 4; ++k) {
            const int nx = x + dx[k], ny = y + dy[k];
            if (nx < 0 || ny < 0 || nx >= (int)W || ny >= (int)H || !valid[(size_t)ny * W + nx]) return true;
        }
        return false;
    };
    // Island border texels in a hash grid of their world positions (cell: about two texels).
    std::vector<size_t> border;
    for (size_t i = 0; i < N; ++i) if (isBorder(i)) border.push_back(i);
    const float cell = std::max(F.texelWorld * 2.0f, 1e-6f);
    auto cellKey = [&](float3 p) {
        const long long cx = (long long)std::floor(p.x / cell), cy = (long long)std::floor(p.y / cell), cz = (long long)std::floor(p.z / cell);
        return (u64)((cx * 73856093ll) ^ (cy * 19349663ll) ^ (cz * 83492791ll));
    };
    std::unordered_map<u64, std::vector<u32>> grid;
    grid.reserve(border.size());
    for (size_t b = 0; b < border.size(); ++b) grid[cellKey(P(border[b]))].push_back((u32)b);
    std::vector<float> out(N * 4, -1.0f), in(N * 4, -1.0f);
    int made = 0;
    for (size_t i : border) {
        const int x = (int)(i % W), y = (int)(i / W);
        const float3 pi = P(i), ni = Nn(i);
        for (int k = 0; k < 4; ++k) {
            const int nx = x + dx[k], ny = y + dy[k];
            if (nx >= 0 && ny >= 0 && nx < (int)W && ny < (int)H && valid[(size_t)ny * W + nx]) continue;
            // Where the step would land on the model: one texel further along the surface.
            const int bx = x - dx[k], by = y - dy[k];
            float3 step = {0, 0, 0};
            if (bx >= 0 && by >= 0 && bx < (int)W && by < (int)H && valid[(size_t)by * W + bx]) step = pi - P((size_t)by * W + bx);
            if (Length(step) < 1e-9f) continue;
            const float3 target = pi + step;
            const float reach = std::max(Length(step) * 1.6f, F.texelWorld * 1.5f);
            float best = reach;
            long long bestT = -1;
            const long long cx = (long long)std::floor(target.x / cell), cy = (long long)std::floor(target.y / cell), cz = (long long)std::floor(target.z / cell);
            const int rr = (int)std::ceil(reach / cell);
            for (int ox = -rr; ox <= rr; ++ox)
                for (int oy = -rr; oy <= rr; ++oy)
                    for (int oz = -rr; oz <= rr; ++oz) {
                        const u64 key2 = (u64)(((cx + ox) * 73856093ll) ^ ((cy + oy) * 19349663ll) ^ ((cz + oz) * 83492791ll));
                        auto it = grid.find(key2);
                        if (it == grid.end()) continue;
                        for (u32 b : it->second) {
                            const size_t t = border[b];
                            const int tx = (int)(t % W), ty = (int)(t / W);
                            if (std::abs(tx - x) <= 3 && std::abs(ty - y) <= 3) continue;  // same side of the seam
                            if (Dot(Nn(t), ni) < -0.2f) continue;                         // a thin shell's back
                            const float dd = Length(P(t) - target);
                            if (dd < best) { best = dd; bestT = (long long)t; }
                        }
                    }
            if (bestT < 0) continue;
            // The target lists its senders (two at most): the paint is gathered there.
            float* slot = &in[(size_t)bestT * 4];
            const int s = slot[0] < 0 ? 0 : slot[2] < 0 ? 1 : -1;
            if (s < 0) continue;
            slot[s * 2] = (float)i;
            slot[s * 2 + 1] = (float)k;
            out[i * 4 + k] = (float)bestT;
            ++made;
        }
    }
    Ensure(F.portalOut, W, H, gpu::Format::RGBA32F, "FluidPortalOut");
    Ensure(F.portalIn, W, H, gpu::Format::RGBA32F, "FluidPortalIn");
    ctx.Upload(F.portalOut.get(), out.data(), W * 16);
    ctx.Upload(F.portalIn.get(), in.data(), W * 16);
    F.portals = made;
    Log("[fluid] seams: %zu island border texels, %d crossings, texel %.5f, maps %ux%u (%zu bytes)", border.size(), made, F.texelWorld, DW, DH, F.rbPos->data.size());
    F.portalKey = key;
    F.rbPos.reset();
    F.rbNrm.reset();
}

int Renderer::FluidStep(gpu::CommandContext& ctx, float dt, const FluidParams& p) {
    if (!fluid.active || dt <= 0) return 0;
    if (fluid.mesh) FluidPortals(ctx);
    // At most one texel per substep: enough substeps for the fastest paint.
    const float fastest = p.speed * std::max(p.strength, 0.05f) * 1.6f;
    const int n = std::clamp((int)std::ceil(fastest * dt) + 1, 2, 32);
    const float h = dt / n;
    for (int i = 0; i < n; ++i) {
        FluidDispatch(ctx, 2, 0, h, p, nullptr, nullptr, 0);
        FluidDispatch(ctx, 3, 0, h, p, nullptr, nullptr, 0);
    }
    fluid.time += dt;
    return n;
}

void Renderer::FluidComposite(Document& doc, gpu::CommandContext& ctx, const FluidParams& p) {
    FluidState& F = fluid;
    auto* k = Kernel("fluid_paint.slang");
    if (!F.active || !strokeSurface || !k) return;
    static FluidCB cb;
    FillCB(cb, p);
    auto one = [&](Id surface, gpu::Texture* pre, int kind) {
        Surface* s = doc.GetSurface(surface);
        if (!s || !pre) return;
        gpu::DispatchDesc d;
        d.pipeline = k;
        d.srv[0] = F.f0[F.cur].get();
        d.srv[1] = F.f1[F.cur].get();
        d.srv[2] = F.d0[F.cur].get();
        d.srv[3] = F.d1[F.cur].get();
        d.srv[5] = F.grav.get();
        d.srv[9] = pre;
        d.uav[0] = s->tex.get();
        SetConstants(d, FluidRoot{F.w, F.h, s->tex->Width(), s->tex->Height(), 4, (u32)kind, F.mesh ? 1u : 0u, 0, p.seed, {0, 0, 0}});
        d.cbData = &cb;
        d.cbSize = offsetof(FluidCB, dabs);
        Dispatch2D(ctx, d, s->tex->Width(), s->tex->Height());
        doc.TouchSurface(surface);
    };
    one(strokeSurface, strokePre.get(), F.kinds.empty() ? 0 : F.kinds[0]);
    for (size_t i = 0; i < strokeSlots.size(); ++i) one(strokeSlots[i].surface, strokeSlots[i].pre.get(), i + 1 < F.kinds.size() ? F.kinds[i + 1] : 0);
    strokeRect = {0, 0, (int)F.docW, (int)F.docH};
}

void Renderer::FluidEnd(Document& doc, gpu::CommandContext& ctx, History& hist, const std::string& label, bool keep) {
    FluidState& F = fluid;
    if (!F.active) return;
    F.active = false;
    if (strokeSurface) {
        if (keep) {
            EndStrokeMulti(doc, ctx, hist, label);
        } else {
            // Discarded: every layer back to its state before the paint.
            if (Surface* s = doc.GetSurface(strokeSurface)) { ctx.CopyTexture(s->tex.get(), strokePre.get()); doc.TouchSurface(strokeSurface); }
            for (auto& slot : strokeSlots)
                if (Surface* s = doc.GetSurface(slot.surface)) { ctx.CopyTexture(s->tex.get(), slot.pre.get()); doc.TouchSurface(slot.surface); }
            strokeSlots.clear();
            strokeSurface = 0;
        }
    }
    // The simulation memory goes back to the pool.
    for (int k = 0; k < 2; ++k) {
        F.f0[k].reset();
        F.f1[k].reset();
        F.d0[k].reset();
        F.d1[k].reset();
        F.flux[k].reset();
    }
    F.grav.reset();
    F.mesh = nullptr;
    F.selection.reset();
    F.bed.reset();
}

}  // namespace sx
