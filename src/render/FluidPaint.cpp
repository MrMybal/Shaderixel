// Liquid paint: a film of paint simulated on the GPU in texture space (shaders/fluid_paint.slang),
// composited live into the layers of a stroke (one per material channel). The stroke stays open
// while the paint moves; ending it records one undo step for every layer.
#include "render/Renderer.h"
#include "render/Dispatch.h"
#include "render/MeshPaint.h"
#include "doc/History.h"

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
    if (!BeginStrokeMulti(doc, ctx, surfaces)) return false;
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
        Ensure(F.f1[k], F.w, F.h, gpu::Format::RGBA16F, names[1][k]);
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

int Renderer::FluidStep(gpu::CommandContext& ctx, float dt, const FluidParams& p) {
    if (!fluid.active || dt <= 0) return 0;
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
        d.srv[9] = pre;
        d.uav[0] = s->tex.get();
        SetConstants(d, FluidRoot{F.w, F.h, s->tex->Width(), s->tex->Height(), 4, (u32)kind, 0, 0, p.seed, {0, 0, 0}});
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
