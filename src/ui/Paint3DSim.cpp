// Paint 3D simulation (Marmoset / Substance particle brushes): particles emitted
// from the brush fall, slide and run over the model and paint where they go -
// drips, thick runs, falling dust, splashes, spreading stains, rising soot.
// Painting uses model-space dabs (mesh_brush.slang, world dabs): no camera, so the
// particles also paint the parts that are not visible.
#include "ui/App.h"
#include "ui/Theme.h"
#include "render/MeshPaint.h"

namespace sx {

namespace {

struct SimPreset {
    const char* name;
    const char* help;
    int emit;          // 0 on the surface under the brush, 1 above it (falls), 2 from the camera (thrown)
    float gravity;     // multiplier (negative: upwards)
    float adhesion;    // friction on the surface
    float sizeK, lifeK;
    float shrink;      // per second
    bool stick;        // landing stops the particle (dust, splash)
    float wander;      // random walk on the surface (stains)
};

const SimPreset kPresets[] = {
    {"Coulures (pluie, peinture)", "Gouttes qui coulent vers le bas en laissant une traînée.", 0, 1.0f, 0.6f, 1.0f, 1.0f, 0.25f, false, 0.0f},
    {"Ruissellement épais", "Coulées larges et lentes (rouille, boue, cire).", 0, 0.45f, 1.2f, 1.8f, 1.6f, 0.15f, false, 0.0f},
    {"Poussière qui tombe", "Particules lâchées au-dessus du pinceau : elles se posent sur les faces tournées vers le haut.", 1, 1.0f, 3.0f, 0.7f, 2.0f, 0.0f, true, 0.0f},
    {"Éclaboussures", "Gouttes projetées depuis la caméra : impacts et petites coulures.", 2, 1.0f, 0.8f, 1.1f, 0.8f, 0.6f, false, 0.0f},
    {"Propagation (taches, brûlure)", "Taches qui avancent au hasard sur la surface.", 0, 0.0f, 0.5f, 1.3f, 2.5f, 0.1f, false, 1.0f},
    {"Suie qui monte (fumée)", "Traînées qui remontent le long du modèle.", 0, -0.6f, 0.7f, 1.2f, 1.6f, 0.2f, false, 0.25f},
};
constexpr int kPresetCount = (int)(sizeof(kPresets) / sizeof(kPresets[0]));

float3 Perp(float3 n) { return Normalize(Cross(std::fabs(n.y) < 0.9f ? float3{0, 1, 0} : float3{1, 0, 0}, n)); }

}  // namespace

int SimPresetCount() { return kPresetCount; }
const char* SimPresetName(int i) { return kPresets[std::clamp(i, 0, kPresetCount - 1)].name; }
const char* SimPresetHelp(int i) { return kPresets[std::clamp(i, 0, kPresetCount - 1)].help; }

bool App::CameraRay(MeshScene& s, float2 local, u32 w, u32 h, float3& o, float3& d) {
    // Same basis as the view (left-handed look-at, vertical fov).
    const float3 fwd = Normalize(s.target - s.camPos);
    const float3 right = Normalize(Cross(float3{0, 1, 0}, fwd));
    const float3 up = Cross(fwd, right);
    const float ty = std::tan(s.fov * 0.5f), tx = ty * (float)w / std::max(h, 1u);
    const float nx = (local.x + 0.5f) / w * 2.0f - 1.0f, ny = 1.0f - (local.y + 0.5f) / h * 2.0f;
    o = s.camPos;
    d = Normalize(fwd + right * (nx * tx) + up * (ny * ty));
    return true;
}

float App::Rand01() {
    sim.rng = sim.rng * 1664525u + 1013904223u;
    return (sim.rng >> 8) * (1.0f / 16777216.0f);
}

void App::SimEmit(DocView& v, float2 local, u32 w, u32 h, float dt) {
    MeshScene& s = *v.mesh;
    if (sim.rc.Empty() || sim.scene != &s || sim.rev != s.revision) {
        sim.rc.Build(s.data);
        sim.scene = &s;
        sim.rev = s.revision;
    }
    const auto& B = brush3D;
    const SimPreset& P = kPresets[std::clamp(B.simPreset, 0, kPresetCount - 1)];
    float3 o, d, hit, n;
    CameraRay(s, local, w, h, o, d);
    float t = 0;
    bool onModel = sim.rc.Ray(o, d, 100.0f, &hit, &n, &t);
    if (!onModel && P.emit == 1) {
        // Falling dust: also from beside the model, at the camera's target depth.
        t = s.distance;
        hit = o + d * t;
        n = {0, 1, 0};
    } else if (P.emit != 2 && !onModel) {
        return;
    }
    // Brush radius in model units at the hit depth.
    const float pixel = 2.0f * std::tan(s.fov * 0.5f) * (onModel ? t : s.distance) / std::max(h, 1u);
    const float radius = std::max(brush.size * 0.5f * pixel, 1e-4f);
    sim.emitAcc += B.simRate * dt;
    int count = (int)sim.emitAcc;
    sim.emitAcc -= count;
    const float3 down = {0, -1, 0};
    for (int i = 0; i < count && sim.parts.size() < 4000; ++i) {
        SimParticle p;
        p.size = B.simSize * P.sizeK * (0.6f + 0.8f * Rand01());
        p.life = B.simLife * P.lifeK * (0.6f + 0.8f * Rand01());
        p.alpha = brush.flow;
        const float a = Rand01() * 6.2831853f, r = std::sqrt(Rand01()) * radius;
        if (P.emit == 0) {
            // On the surface around the hit: re-cast from the camera through the offset point.
            const float3 tx = Perp(n), ty = Cross(n, tx);
            const float3 q = hit + (tx * std::cos(a) + ty * std::sin(a)) * r;
            float3 ph, nh;
            if (!sim.rc.Ray(q + n * radius, n * -1.0f, radius * 3.0f, &ph, &nh)) continue;
            p.p = ph;
            p.n = nh;
            p.onSurface = true;
            p.v = Perp(nh) * ((Rand01() - 0.5f) * 0.02f);
        } else if (P.emit == 1) {
            // Above the brush: falls on what is below.
            const float3 tx = {1, 0, 0}, tz = {0, 0, 1};
            p.p = hit + (tx * std::cos(a) + tz * std::sin(a)) * r + float3{0, 0.8f, 0};
            p.v = down * 0.2f;
            p.onSurface = false;
        } else {
            // Thrown from the camera through the brush, inside a cone.
            float3 dd = d;
            const float3 tx = Perp(d), ty = Cross(d, tx);
            const float spread = B.simSpread * 0.15f;
            dd = Normalize(dd + (tx * std::cos(a) + ty * std::sin(a)) * (std::sqrt(Rand01()) * spread));
            p.p = o + d * std::max(t - 0.6f, 0.05f);
            p.v = dd * 6.0f;
            p.onSurface = false;
        }
        p.alive = true;
        sim.parts.push_back(p);
    }
}

void App::SimStep(DocView& v, float dt, std::vector<float4>& dabs, std::vector<float4>& ex) {
    MeshScene& s = *v.mesh;
    const auto& B = brush3D;
    const SimPreset& P = kPresets[std::clamp(B.simPreset, 0, kPresetCount - 1)];
    const float3 g = float3{0, -1, 0} * (2.5f * B.simGravity * P.gravity);
    const float minY = s.data.boundsMin.y - 0.5f;
    const int sub = 4;
    const float h = dt / sub;
    for (auto& p : sim.parts) {
        if (!p.alive) continue;
        for (int k = 0; k < sub && p.alive; ++k) {
            const float3 prev = p.p;
            if (p.onSurface) {
                // Slide: gravity along the surface, friction, random walk; then back onto the surface.
                float3 gt = g - p.n * Dot(g, p.n);
                p.v = p.v + gt * h;
                if (P.wander > 0) {
                    const float3 tx = Perp(p.n), ty = Cross(p.n, tx);
                    const float a = Rand01() * 6.2831853f;
                    p.v = p.v + (tx * std::cos(a) + ty * std::sin(a)) * (P.wander * 0.6f * h * 60.0f * 0.02f);
                }
                p.v = p.v - p.n * Dot(p.v, p.n);
                p.v = p.v * std::max(0.0f, 1.0f - B.simAdhesion * P.adhesion * 4.0f * h);
                const float sp = Length(p.v), maxSp = 1.5f;
                if (sp > maxSp) p.v = p.v * (maxSp / sp);
                const float3 q = p.p + p.v * h;
                const float lift = std::max(p.size * 2.0f, 0.01f);
                float3 ph, nh;
                if (sim.rc.Ray(q + p.n * lift, p.n * -1.0f, lift * 2.5f, &ph, &nh) && Dot(nh, p.n) > -0.2f) {
                    p.p = ph;
                    p.n = nh;
                } else {
                    p.onSurface = false;  // over an edge: falls
                    p.p = q;
                }
            } else {
                // Free fall (or thrown): until the next surface.
                p.v = p.v + g * h;
                const float3 seg = p.v * h;
                const float len = Length(seg);
                float3 ph, nh;
                if (len > 1e-6f && sim.rc.Ray(p.p, seg * (1.0f / len), len, &ph, &nh)) {
                    p.p = ph + nh * 1e-4f;
                    p.n = nh;
                    // Impact: a splat.
                    dabs.push_back({ph.x, ph.y, ph.z, p.size * 1.6f});
                    ex.push_back({p.alpha, nh.x, nh.y, nh.z});
                    if (P.stick && Dot(nh, float3{0, 1, 0}) > 0.25f) { p.alive = false; break; }
                    p.onSurface = true;
                    p.v = (p.v - nh * Dot(p.v, nh)) * 0.3f;
                } else {
                    p.p = p.p + seg;
                }
                if (p.p.y < minY) p.alive = false;
            }
            if (p.onSurface && p.alive) {
                // Trail: a dab every fraction of the size.
                p.trail += Length(p.p - prev);
                if (p.trail >= p.size * 0.35f || k == 0) {
                    p.trail = 0;
                    dabs.push_back({p.p.x, p.p.y, p.p.z, p.size});
                    ex.push_back({p.alpha * 0.8f, p.n.x, p.n.y, p.n.z});
                }
            }
        }
        p.life -= dt;
        p.size *= std::max(0.0f, 1.0f - P.shrink * dt);
        if (p.life <= 0 || p.size < 1e-4f) p.alive = false;
    }
    sim.parts.erase(std::remove_if(sim.parts.begin(), sim.parts.end(), [](const SimParticle& p) { return !p.alive; }), sim.parts.end());
}

void App::UpdateSimulation(DocView& v, bool emitting, float2 local, u32 w, u32 h, const std::vector<Id>& targets, const std::vector<float4>& colors) {
    if (!v.mesh) return;
    const double now = ImGui::GetTime();
    const float dt = std::clamp((float)(now - sim.last), 1.0f / 240, 1.0f / 20);
    sim.last = now;
    if (emitting) {
        if (sim.view != &v) { sim.parts.clear(); sim.view = &v; }
        SimEmit(v, local, w, h, dt);
    }
    if (sim.parts.empty()) {
        if (sim.stroking && !emitting) {
            v.renderer.EndStrokeMulti(*v.doc, *ctx, v.history, "Simulation de peinture");
            sim.stroking = false;
        }
        return;
    }
    if (!sim.stroking) {
        if (targets.empty() || !v.renderer.BeginStrokeMulti(*v.doc, *ctx, targets)) { sim.parts.clear(); return; }
        sim.stroking = true;
        v.renderer.meshTipOn = false;
    }
    std::vector<float4> dabs, ex;
    SimStep(v, dt, dabs, ex);
    BrushSettings b = brush;
    b.hardness = 0.35f;
    b.opacity = brush.opacity;
    for (size_t i = 0; i < dabs.size(); i += 1024) {
        std::vector<float4> d(dabs.begin() + i, dabs.begin() + std::min(dabs.size(), i + 1024)), e(ex.begin() + i, ex.begin() + std::min(ex.size(), i + 1024));
        v.renderer.MeshWorldDabsMulti(*v.doc, *ctx, *v.mesh, d, e, b, colors, false);
    }
}

}  // namespace sx
