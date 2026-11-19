// Paint 3D simulation (Marmoset / Substance particle brushes): particles emitted
// from the brush fall, slide and run over the model and paint where they go -
// drips, thick runs, falling dust, splashes, spreading stains, rising soot.
// Painting uses model-space dabs (mesh_brush.slang, world dabs): no camera, so the
// particles also paint the parts that are not visible.
#include "ui/App.h"
#include "ui/CodeEdit.h"
#include "ui/Lang.h"
#include "ui/Theme.h"
#include "render/MeshPaint.h"
#include "render/Dispatch.h"
#include <imgui_stdlib.h>
#include <fstream>

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
    {"Pluie tombante", "Des gouttes tombent au-dessus du pinceau puis ruissellent sur le modèle.", 1, 1.25f, 0.50f, 0.85f, 1.0f, 0.32f, false, 0.0f},
};
constexpr double kParticleStep = 1.0 / 120.0;
// Keep the original preset indices, including Rust, stable for existing demos.
constexpr int kRustBrush = 6;
constexpr int kRainBehavior = 6;
constexpr int kBehaviorCount = (int)std::size(kPresets);

struct MaterialParticlePreset {
    const char* name;
    const char* help;
    int behavior;
    u32 seed;
    float rate, size, gravity, adhesion, life, flow, hardness;
    // sRGB colors and independent deposit strengths for the trail and the impact.
    struct Deposit {
        PbrChannel channel;
        float3 trail, impact;
        float2 strength;
    };
    std::initializer_list<Deposit> deposits;
};

const MaterialParticlePreset kMaterialPresets[] = {
    {"Rouille sous un rivet", "Un impact chargé en relief et une traînée de rouille qui suit la surface.",
     0, 8041, 22, 0.023f, 1, 0.5f, 1.65f, 0.65f, 0.35f,
     {{PbrChannel::BaseColor, {0.49f,0.19f,0.075f}, {0.26f,0.075f,0.024f}, {0.8f,1}},
      {PbrChannel::Roughness, {0.83f,0.83f,0.83f}, {0.95f,0.95f,0.95f}, {0.9f,1}},
      {PbrChannel::Height, {0.55f,0.55f,0.55f}, {0.76f,0.76f,0.76f}, {0.1f,1}}}},
    {"Boue projetée", "Des impacts foncés et épais, suivis de traînées de terre plus mates.",
     3, 1819, 44, 0.028f, 0.72f, 1.20f, 1.15f, 0.68f, 0.52f,
     {{PbrChannel::BaseColor, {0.36f,0.24f,0.14f}, {0.22f,0.14f,0.075f}, {0.85f,1}},
      {PbrChannel::Roughness, {0.71f,0.71f,0.71f}, {0.34f,0.34f,0.34f}, {0.8f,1}},
      {PbrChannel::Height, {0.63f,0.63f,0.63f}, {0.84f,0.84f,0.84f}, {0.55f,1}}}},
    {"Pluie sur la surface", "Des gouttes tombent, brillent à l’impact et laissent un film légèrement en relief.",
     kRainBehavior, 5261, 75, 0.013f, 1, 0.35f, 1.55f, 0.42f, 0.25f,
     {{PbrChannel::Roughness, {0.23f,0.23f,0.23f}, {0.11f,0.11f,0.11f}, {0.75f,1}},
      {PbrChannel::Height, {0.535f,0.535f,0.535f}, {0.59f,0.59f,0.59f}, {0.22f,0.65f}}}},
    {"Poussière déposée", "Un dépôt clair et mat qui se pose surtout sur les surfaces orientées vers le haut.",
     2, 3947, 100, 0.019f, 0.70f, 0.85f, 2.05f, 0.36f, 0.18f,
     {{PbrChannel::BaseColor, {0.66f,0.60f,0.51f}, {0.73f,0.68f,0.59f}, {0.35f,0.85f}},
      {PbrChannel::Roughness, {0.80f,0.80f,0.80f}, {0.96f,0.96f,0.96f}, {0.42f,0.92f}},
      {PbrChannel::Height, {0.51f,0.51f,0.51f}, {0.56f,0.56f,0.56f}, {0.1f,0.35f}}}},
};
constexpr int kScriptBrush = kRustBrush + (int)std::size(kMaterialPresets);
constexpr int kScriptVersion = 1;

float3 Perp(float3 n) { return Normalize(Cross(std::fabs(n.y) < 0.9f ? float3{0, 1, 0} : float3{1, 0, 0}, n)); }

}  // namespace

int SimPresetCount() { return kScriptBrush + 1; }
const char* SimPresetName(int i) {
    if (i == kScriptBrush) return "Gouttes vagabondes (script)";
    return i >= kRustBrush && i < kScriptBrush ? kMaterialPresets[i-kRustBrush].name : kPresets[std::clamp(i, 0, kRustBrush-1)].name;
}

bool App::CameraRay(MeshScene& s, float2 local, u32 w, u32 h, float3& o, float3& d) {
    // Same basis as the view (left-handed look-at, vertical fov).
    const float3 fwd = Normalize(s.target - s.camPos);
    const float3 right = Normalize(Cross(float3{0, 1, 0}, fwd));
    const float3 up = Cross(fwd, right);
    const float ty = s.ortho ? s.OrthoHalfHeight() : std::tan(s.fov * 0.5f), tx = ty * (float)w / std::max(h, 1u);
    const float nx = (local.x + 0.5f) / w * 2.0f - 1.0f, ny = 1.0f - (local.y + 0.5f) / h * 2.0f;
    if (s.ortho) {
        // Parallel rays starting well in front of the model.
        o = s.camPos + right * (nx * tx) + up * (ny * ty) - fwd * 100.0f;
        d = fwd;
        return true;
    }
    o = s.camPos;
    d = Normalize(fwd + right * (nx * tx) + up * (ny * ty));
    return true;
}

namespace {
float SampleRandom(u32& rng) {
    rng = rng * 1664525u + 1013904223u;
    return (rng >> 8) * (1.0f / 16777216.0f);
}
}

float App::Rand01() { return SampleRandom(fluidRng); }

App::ParticleOrigin App::ParticleOriginAt(MeshScene& s, float2 local, u32 w, u32 h) {
    ParticleOrigin origin;
    if (!w || !h) return origin;
    auto& rc = s.Rays();
    CameraRay(s, local, w, h, origin.rayOrigin, origin.rayDirection);
    float t = 0;
    const bool hit = rc.Ray(origin.rayOrigin, origin.rayDirection, 100.0f, &origin.point, &origin.normal, &t);
    const auto& B = sim.playing ? sim.settings : particleBrush;
    const auto& P = kPresets[B.behavior];
    if (!hit) {
        if (P.emit == 0) return origin;
        t = s.distance + (s.ortho ? 100.0f : 0.0f);
        origin.point = origin.rayOrigin + origin.rayDirection * t;
        origin.normal = {0, 1, 0};
    }
    const float unitsPerPixel = s.ortho ? 2.0f * s.OrthoHalfHeight() / h : 2.0f * std::tan(s.fov * 0.5f) * t / h;
    origin.radius = (brush3D.modelSize || B.customMaterial) ? brush3D.modelDiameter * 0.5f : brush.size * unitsPerPixel * 0.5f;
    origin.valid = true;
    return origin;
}

void App::SimEmit(MeshScene& s, const ParticleOrigin& origin, int count) {
    if (!origin.valid) return;
    const auto& B = sim.settings;
    const auto& P = kPresets[B.behavior];
    auto& rays = s.Rays();
    auto random = [&] { return SampleRandom(sim.rng); };
    const float3 hit = origin.point, n = origin.normal, o = origin.rayOrigin, d = origin.rayDirection;
    const float t = Length(hit - o);
    const float radius = std::max(origin.radius, 1e-4f);
    const float3 down = {0, -1, 0};
    for (int i = 0; i < count && sim.parts.size() < 4000; ++i) {
        SimParticle p;
        p.size = B.size * P.sizeK * (0.6f + 0.8f * random());
        p.life = B.life * P.lifeK * (0.6f + 0.8f * random());
        p.alpha = B.flow;
        const float a = random() * 6.2831853f, r = std::sqrt(random()) * radius;
        if (P.emit == 0) {
            // On the surface around the hit: re-cast from the camera through the offset point.
            const float3 tx = Perp(n), ty = Cross(n, tx);
            const float3 q = hit + (tx * std::cos(a) + ty * std::sin(a)) * r;
            float3 ph, nh;
            if (!rays.Ray(q + n * radius, n * -1.0f, radius * 3.0f, &ph, &nh)) continue;
            p.p = ph;
            p.n = nh;
            p.onSurface = true;
            p.justPlaced = true;
            p.v = Perp(nh) * ((random() - 0.5f) * 0.02f);
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
            const float spread = B.spread * 0.15f;
            dd = Normalize(dd + (tx * std::cos(a) + ty * std::sin(a)) * (std::sqrt(random()) * spread));
            p.p = o + d * std::max(t - 0.6f, 0.05f);
            p.v = dd * 6.0f;
            p.onSurface = false;
        }
        if (sim.settings.program && sim.settings.program->Has(particles::Stage::Emit)) {
            using namespace particles;
            auto state = ParticleScriptState(p);
            if (!p.onSurface) { state[NX] = n.x; state[NY] = n.y; state[NZ] = n.z; }
            sim.settings.program->Run(Stage::Emit,state,sim.scriptRng,sim.settings.scriptValues);
            const auto offset = [](float result, float original) { return original + std::clamp(result-original,-2.f,2.f); };
            p.p = {offset(state[X],p.p.x),offset(state[Y],p.p.y),offset(state[Z],p.p.z)};
            p.v = {std::clamp(state[VX],-20.f,20.f),std::clamp(state[VY],-20.f,20.f),std::clamp(state[VZ],-20.f,20.f)};
            p.size = std::clamp(state[Radius],1e-4f,0.25f);
            p.life = std::clamp(state[Life],0.f,15.f);
            p.alpha = std::clamp(state[Alpha],0.f,1.f);
            p.onSurface = state[OnSurface] != 0;
            p.justPlaced = p.onSurface;
            p.alive = state[Alive] != 0 && p.life > 0;
        }
        if (!p.alive) continue;
        sim.parts.push_back(p);
    }
}

particles::State App::ParticleScriptState(const SimParticle& p) const {
    using namespace particles;
    State state{};
    state[X] = p.p.x; state[Y] = p.p.y; state[Z] = p.p.z;
    state[VX] = p.v.x; state[VY] = p.v.y; state[VZ] = p.v.z;
    state[NX] = p.n.x; state[NY] = p.n.y; state[NZ] = p.n.z;
    state[Radius] = p.size;
    state[Life] = p.life;
    state[Alpha] = p.alpha;
    state[Age] = p.age;
    state[Time] = (float)(sim.tick * kParticleStep);
    state[Step] = (float)kParticleStep;
    state[Speed] = Length(p.v);
    state[OnSurface] = p.onSurface ? 1.f : 0.f;
    state[Alive] = p.alive ? 1.f : 0.f;
    state[Gravity] = 1;
    state[ImpactRadius] = p.size * 1.6f;
    state[Strength] = p.alpha;
    state[Restitution] = 0.35f;
    return state;
}

void App::SimStep(MeshScene& s, std::vector<float4>& dabs, std::vector<float4>& ex) {
    const auto& B = sim.settings;
    const auto& P = kPresets[B.behavior];
    auto& rays = s.Rays();
    auto random = [&] { return SampleRandom(sim.rng); };
    const float dt = (float)kParticleStep;
    const float3 g = float3{0, -1, 0} * (2.5f * B.gravity * P.gravity);
    const float minY = s.data.boundsMin.y - 0.5f;
    const int sub = 2;
    const float h = dt / sub;
    using namespace particles;
    const bool scriptedUpdate = B.program && B.program->Has(Stage::Update);
    const bool scriptedImpact = B.program && B.program->Has(Stage::Impact);
    const auto deposit = [&](const SimParticle& p, float3 point, float& restitution) {
        float radius = p.size * 1.6f, strength = p.alpha;
        auto response = Native;
        if (scriptedImpact) {
            auto state = ParticleScriptState(p);
            state[X] = point.x; state[Y] = point.y; state[Z] = point.z;
            B.program->Run(Stage::Impact,state,sim.scriptRng,B.scriptValues);
            radius = std::clamp(state[ImpactRadius],0.f,0.4f);
            strength = std::clamp(state[Strength],0.f,1.f);
            response = (ImpactResponse)(int)std::clamp(state[Response],(float)Native,(float)Bounce);
            restitution = std::clamp(state[Restitution],0.f,1.f);
        }
        if (!scriptedImpact || (radius > 0 && strength > 0)) {
            dabs.push_back({point.x,point.y,point.z,radius});
            ex.push_back({-strength,p.n.x,p.n.y,p.n.z});
        }
        return response;
    };
    for (auto& p : sim.parts) {
        if (!p.alive) continue;
        if (p.justPlaced) {
            float restitution = 0;
            const auto response = deposit(p,p.p,restitution);
            p.justPlaced = false;
            if (response == Stop) { p.alive = false; continue; }
            if (response == Bounce) {
                p.onSurface = false;
                p.p = p.p + p.n * 1e-4f;
                p.v = p.v + p.n * restitution * 0.1f;
            }
        }
        float3 acceleration = g;
        if (scriptedUpdate) {
            auto state = ParticleScriptState(p);
            B.program->Run(Stage::Update,state,sim.scriptRng,B.scriptValues);
            if (state[Alive] == 0 || state[Life] <= 0) { p.alive = false; continue; }
            p.life = std::clamp(state[Life],0.f,15.f);
            p.size = std::clamp(state[Radius],1e-4f,0.25f);
            p.alpha = std::clamp(state[Alpha],0.f,1.f);
            p.v = {std::clamp(state[VX],-20.f,20.f),std::clamp(state[VY],-20.f,20.f),std::clamp(state[VZ],-20.f,20.f)};
            acceleration = g * std::clamp(state[Gravity],-4.f,4.f) +
                float3{std::clamp(state[AX],-20.f,20.f),std::clamp(state[AY],-20.f,20.f),std::clamp(state[AZ],-20.f,20.f)};
        }
        for (int k = 0; k < sub && p.alive; ++k) {
            const float3 prev = p.p;
            if (p.onSurface) {
                // Slide: gravity along the surface, friction, random walk; then back onto the surface.
                float3 gt = acceleration - p.n * Dot(acceleration, p.n);
                p.v = p.v + gt * h;
                if (P.wander > 0) {
                    const float3 tx = Perp(p.n), ty = Cross(p.n, tx);
                    const float a = random() * 6.2831853f;
                    p.v = p.v + (tx * std::cos(a) + ty * std::sin(a)) * (P.wander * 0.6f * h * 60.0f * 0.02f);
                }
                p.v = p.v - p.n * Dot(p.v, p.n);
                p.v = p.v * std::max(0.0f, 1.0f - B.adhesion * P.adhesion * 4.0f * h);
                const float sp = Length(p.v), maxSp = 1.5f;
                if (sp > maxSp) p.v = p.v * (maxSp / sp);
                const float3 q = p.p + p.v * h;
                const float lift = std::max(p.size * 2.0f, 0.01f);
                float3 ph, nh;
                if (rays.Ray(q + p.n * lift, p.n * -1.0f, lift * 2.5f, &ph, &nh) && Dot(nh, p.n) > -0.2f) {
                    p.p = ph;
                    p.n = nh;
                } else {
                    p.onSurface = false;  // over an edge: falls
                    p.p = q;
                }
            } else {
                // Free fall (or thrown): until the next surface.
                p.v = p.v + acceleration * h;
                const float3 seg = p.v * h;
                const float len = Length(seg);
                float3 ph, nh;
                if (len > 1e-6f && rays.Ray(p.p, seg * (1.0f / len), len, &ph, &nh)) {
                    p.p = ph + nh * 1e-4f;
                    p.n = nh;
                    // Impact: a splat.
                    float restitution = 0;
                    const auto response = deposit(p,ph,restitution);
                    if (response == Stop || (response == Native && P.stick && Dot(nh, float3{0, 1, 0}) > 0.25f)) {
                        p.alive = false;
                        break;
                    }
                    p.onSurface = response != Bounce;
                    const float normalSpeed = Dot(p.v,nh);
                    const float3 tangent = p.v - nh * normalSpeed;
                    p.v = p.onSurface ? tangent * 0.3f : tangent * 0.75f + nh * (std::max(-normalSpeed,0.f)*restitution);
                } else {
                    p.p = p.p + seg;
                }
                if (p.p.y < minY) p.alive = false;
            }
            if (p.onSurface && p.alive) {
                // Trail: a dab every fraction of the size.
                p.trail += Length(p.p - prev);
                if (p.trail >= p.size * 0.22f) {
                    p.trail = 0;
                    dabs.push_back({p.p.x, p.p.y, p.p.z, p.size});
                    ex.push_back({p.alpha * 0.8f, p.n.x, p.n.y, p.n.z});
                }
            }
        }
        p.life -= dt;
        if (B.program) p.age += dt;
        p.size *= std::max(0.0f, 1.0f - P.shrink * dt);
        if (p.life <= 0 || p.size < 1e-4f || p.age >= 15.0f) p.alive = false;
    }
    sim.parts.erase(std::remove_if(sim.parts.begin(), sim.parts.end(), [](const SimParticle& p) { return !p.alive; }), sim.parts.end());
}


App::ParticleBrush App::DefaultParticleBrush(int preset) {
    if (preset == kScriptBrush) {
        auto example = DefaultParticleBrush(kRustBrush);
        example.name = SimPresetName(preset);
        example.seed = 92131;
        example.rate = 35;
        example.colors[(int)PbrChannel::BaseColor] = {0.10f,0.65f,0.77f,1};
        example.impactColors[(int)PbrChannel::BaseColor] = {0.13f,0.38f,0.83f,1};
        particles::Diagnostic diagnostic;
        if (!example.SetScript(std::string(particles::ExampleProgram()),diagnostic))
            Log("Example particle script failed at line %d: %s",diagnostic.line+1,diagnostic.message.c_str());
        return example;
    }
    ParticleBrush value;
    value.name = SimPresetName(preset);
    value.behavior = std::clamp(preset, 0, kRustBrush - 1);
    for (auto& strength : value.deposits) strength = {1, 1};
    if (preset < kRustBrush || preset >= SimPresetCount()) return value;
    const auto& material = kMaterialPresets[preset-kRustBrush];
    value.behavior = material.behavior;
    value.seed = material.seed;
    value.rate = material.rate;
    value.size = material.size;
    value.gravity = material.gravity;
    value.adhesion = material.adhesion;
    value.life = material.life;
    value.flow = material.flow;
    value.hardness = material.hardness;
    value.adjustable = value.customMaterial = true;
    for (const auto& d : material.deposits) {
        const auto i = (size_t)d.channel;
        value.channels |= 1u << i;
        value.colors[i] = {d.trail.x,d.trail.y,d.trail.z,1};
        value.impactColors[i] = {d.impact.x,d.impact.y,d.impact.z,1};
        value.deposits[i] = d.strength;
    }
    return value;
}

void App::SetParticleBrush(int preset) {
    particleBrush = DefaultParticleBrush(preset);
    if (!particleBrush.customMaterial) {
        particleBrush.flow = brush.flow;
        particleBrush.opacity = brush.opacity;
    }
}

bool App::ParticleBrush::SetScript(std::string text, particles::Diagnostic& error) {
    auto compiled = particles::Program::Compile(text,error);
    if (!compiled) return false;
    particles::ControlValues values{};
    const auto& controls = compiled->Controls();
    for (size_t i = 0; i < controls.size(); ++i) {
        const auto& control = controls[i];
        values[i] = control.initial;
        if (program) {
            const auto& previous = program->Controls();
            const auto match = std::ranges::find(previous,control.id,&particles::Control::id);
            if (match != previous.end() && match->kind == control.kind)
                values[i] = control.Restore(scriptValues[match - previous.begin()]);
        }
    }
    scriptValues = values;
    decltype(scriptGroupOpen) groupOpen{};
    if (program) {
        const auto& previous = program->Groups();
        const auto& groups = compiled->Groups();
        for (size_t i = 0; i < groups.size(); ++i) {
            const auto match = std::ranges::find(previous,groups[i].id,&particles::ControlGroup::id);
            if (match != previous.end()) groupOpen[i] = scriptGroupOpen[match - previous.begin()];
        }
    }
    scriptGroupOpen = groupOpen;
    program = std::move(compiled);
    script = std::move(text);
    scriptVersion = kScriptVersion;
    return true;
}

Json App::ParticleBrush::ToJson(bool includeGroupState) const {
    Json j = Json::Object();
    j["name"] = name;
    j["behavior"] = behavior;
    j["source"] = source;
    j["seed"] = seed;
    j["rate"] = rate;
    j["size"] = size;
    j["gravity"] = gravity;
    j["adhesion"] = adhesion;
    j["life"] = life;
    j["spread"] = spread;
    j["hardness"] = hardness;
    j["flow"] = flow;
    j["opacity"] = opacity;
    j["adjustable"] = adjustable;
    j["wet"] = wet;
    j["customMaterial"] = customMaterial;
    j["channels"] = channels;
    if (!script.empty()) {
        j["particleScript"]["version"] = scriptVersion;
        j["particleScript"]["source"] = script;
        if (program) {
            const auto& controls = program->Controls();
            for (size_t i = 0; i < controls.size(); ++i) {
                const auto& control = controls[i];
                j["particleScript"]["values"][control.id] = control.kind == particles::ControlKind::Checkbox ?
                    Json(scriptValues[i] != 0) : Json(scriptValues[i]);
            }
            if (includeGroupState) {
                const auto& groups = program->Groups();
                for (size_t i = 0; i < groups.size(); ++i)
                    j["particleScript"]["groupOpen"][groups[i].id] = scriptGroupOpen[i].value_or(groups[i].defaultOpen);
            }
        }
    }
    for (int i = 0; i < (int)PbrChannel::Count; ++i) {
        Json row = Json::Array();
        for (float f : {colors[i].x, colors[i].y, colors[i].z, impactColors[i].x, impactColors[i].y,
                        impactColors[i].z, deposits[i].x, deposits[i].y}) row.Push(f);
        j["deposits"].Push(std::move(row));
    }
    return j;
}

App::ParticleBrush App::ParticleBrush::FromJson(const Json& j) {
    const auto bounded = [&](const char* key, u32 fallback, u32 maximum) {
        return (u32)std::clamp(j[key].Num(fallback), 0.0, (double)maximum);
    };
    auto brush = DefaultParticleBrush(0);
    brush.behavior = (int)bounded("behavior", 0, kBehaviorCount - 1);
    brush.name = j["name"].S();
    brush.source = (int)bounded("source", 0, 2);
    brush.seed = bounded("seed", 777, UINT32_MAX);
    brush.rate = std::clamp(j["rate"].F(60), 1.0f, 400.0f);
    brush.size = std::clamp(j["size"].F(0.015f), 0.002f, 0.1f);
    brush.gravity = std::clamp(j["gravity"].F(1), 0.0f, 3.0f);
    brush.adhesion = std::clamp(j["adhesion"].F(0.5f), 0.0f, 2.0f);
    brush.life = std::clamp(j["life"].F(2.5f), 0.2f, 10.0f);
    brush.spread = std::clamp(j["spread"].F(0.5f), 0.0f, 2.0f);
    brush.hardness = std::clamp(j["hardness"].F(0.35f), 0.0f, 0.99f);
    brush.flow = std::clamp(j["flow"].F(0.25f), 0.0f, 1.0f);
    brush.opacity = std::clamp(j["opacity"].F(1), 0.0f, 1.0f);
    brush.adjustable = j["adjustable"].B();
    brush.wet = j["wet"].B();
    brush.customMaterial = j["customMaterial"].B();
    brush.channels = bounded("channels", 0, (1u << (int)PbrChannel::Count)-1u);
    const auto& script = j["particleScript"];
    brush.script = script["source"].S();
    if (!brush.script.empty()) {
        brush.scriptVersion = script["version"].Num() == kScriptVersion ? kScriptVersion : 0;
        particles::Diagnostic error;
        if (brush.scriptVersion == kScriptVersion && brush.SetScript(brush.script,error)) {
            const auto& controls = brush.program->Controls();
            for (size_t i = 0; i < controls.size(); ++i) {
                const auto& control = controls[i];
                const auto& saved = script["values"][control.id];
                const auto type = control.kind == particles::ControlKind::Checkbox ? Json::Type::Bool : Json::Type::Number;
                if (saved.GetType() == type) brush.scriptValues[i] = control.Restore(saved.Num());
            }
            const auto& groups = brush.program->Groups();
            for (size_t i = 0; i < groups.size(); ++i) {
                const auto& saved = script["groupOpen"][groups[i].id];
                if (saved.GetType() == Json::Type::Bool) brush.scriptGroupOpen[i] = saved.B();
            }
        }
    }
    for (int i = 0; i < (int)PbrChannel::Count && (size_t)i < j["deposits"].Elements().size(); ++i) {
        const Json& row = j["deposits"].At(i);
        if (!row.IsArray() || row.Size() != 8) continue;
        const auto at = [&](int k) { return std::clamp(row.At(k).F(), 0.0f, 1.0f); };
        brush.colors[i] = {at(0), at(1), at(2), 1};
        brush.impactColors[i] = {at(3), at(4), at(5), 1};
        brush.deposits[i] = {at(6), at(7)};
    }
    return brush;
}

bool App::BeginParticleStroke(DocView& v, bool replay) {
    if (!particleBrush.script.empty() && !particleBrush.program) {
        Status("Ouvrez l’éditeur pour corriger le script de particules");
        return false;
    }
    auto inputs = replay ? std::move(sim.inputs) : std::vector<ParticleInput>{};
    auto origins = replay ? std::move(sim.origins) : std::vector<ParticleOrigin>{};
    auto savedSource = sim.settings.source;
    FinishParticles(false);
    if (!v.mesh || !v.renderer.MeshPrepare(*v.doc, *ctx, *v.mesh)) return false;
    auto& d = *v.doc;
    const auto* active = d.ActiveLayer();
    if (!active) return false;
    sim.view = &v;
    sim.rev = v.mesh->revision;
    sim.settings = particleBrush;
    if (replay) sim.settings.source = savedSource;
    sim.inputs = std::move(inputs);
    sim.origins = std::move(origins);
    sim.activeLayer = active->id;
    sim.selection = d.S().selection;
    sim.wasDirty = d.dirty;
    sim.lastRevision = d.dirty.Revision();
    sim.rng = sim.settings.seed;
    sim.scriptRng = sim.settings.seed ^ 0xa50863d5u;
    sim.replay = replay;
    const bool custom = sim.settings.customMaterial && active->material && IsPaintLayer3D(d, active->material) &&
                        !d.S().editMask && !d.S().editInpaint;
    bool enabled[(int)PbrChannel::Count];
    memcpy(enabled, brush3D.use, sizeof(enabled));
    if (custom) for (int i = 0; i < (int)PbrChannel::Count; ++i) brush3D.use[i] = (sim.settings.channels & (1u << i)) != 0;
    std::vector<float4> colors;
    sim.targets = Paint3DTargets(v, colors);
    memcpy(brush3D.use, enabled, sizeof(enabled));
    if (sim.targets.empty()) {
        sim = {};
        Status("Sélectionnez un calque de peinture avec les canaux de la brosse");
        return false;
    }
    sim.disabledRegions = v.renderer.disabledPaintRegions;
    sim.deposits.reserve(colors.size());
    for (size_t i = 0; i < colors.size(); ++i) sim.deposits.push_back({colors[i], colors[i], {1, 1}});
    if (custom) for (int c = 0; c < (int)PbrChannel::Count; ++c) {
        const auto* layer = MaterialChannelLayer(d, active->material, (PbrChannel)c);
        if (!layer) continue;
        const auto found = std::find(sim.targets.begin(), sim.targets.end(), layer->surface);
        if (found == sim.targets.end()) continue;
        auto& deposit = sim.deposits[found - sim.targets.begin()];
        deposit = {LinearColor(sim.settings.colors[c]), LinearColor(sim.settings.impactColors[c]), sim.settings.deposits[c]};
    }
    sim.playing = true;
    sim.burst = true;
    return true;
}

void App::AdvanceParticles(double dt, ParticleOrigin origin, bool held) {
    if (!sim.playing || !ViewAlive(sim.view) || !sim.view->mesh) return;
    auto& v = *sim.view;
    auto& d = *v.doc;
    auto& mesh = *v.mesh;
    if (!sim.settings.wet && !sim.stroking) {
        if (!v.renderer.BeginParticlePaint(d, *ctx, sim.targets)) { FinishParticles(false); return; }
        sim.stroking = true;
    }
    sim.elapsed += std::max(dt, 0.0);
    BrushSettings painting = brush;
    painting.hardness = sim.settings.hardness;
    painting.opacity = sim.settings.opacity;
    auto& regions = v.renderer.disabledPaintRegions;
    std::swap(regions, sim.disabledRegions);
    const Rect clip = v.renderer.meshClip;
    v.renderer.meshClip = {};
    std::vector<float4> dabs, extra;
    // Feed the solver fixed ticks. Render batches can contain any number of ticks; the coverage fields
    // store the same ordered sequence of updates independently of the frame rate.
    while (sim.elapsed + 1e-10 >= kParticleStep) {
        sim.elapsed = std::max(0.0, sim.elapsed - kParticleStep);
        const ParticleOrigin* input = nullptr;
        ParticleOrigin interpolated;
        if (sim.replay) {
            if (sim.replayIndex < sim.inputs.size() && sim.inputs[sim.replayIndex].tick == sim.tick)
                input = &sim.inputs[sim.replayIndex++].origin;
        } else if (held || sim.held || (sim.burst && (origin.valid || !sim.origins.empty()))) {
            interpolated = !sim.origins.empty() ? sim.origins.front() : origin.valid ? origin : sim.previous;
            if (origin.valid && sim.previous.valid && held && sim.held && dt > 0 && sim.settings.source == 0) {
                const float t = std::clamp((float)(1.0 - sim.elapsed / dt), 0.0f, 1.0f);
                interpolated.point = sim.previous.point + (origin.point - sim.previous.point) * t;
                interpolated.normal = Normalize(sim.previous.normal + (origin.normal - sim.previous.normal) * t);
                interpolated.rayOrigin = sim.previous.rayOrigin + (origin.rayOrigin - sim.previous.rayOrigin) * t;
                interpolated.rayDirection = Normalize(sim.previous.rayDirection + (origin.rayDirection - sim.previous.rayDirection) * t);
                interpolated.radius = sim.previous.radius + (origin.radius - sim.previous.radius) * t;
            }
            if (interpolated.valid) {
                sim.inputs.push_back({sim.tick, interpolated});
                input = &sim.inputs.back().origin;
            }
        }
        if (input) {
            sim.emitAcc += sim.settings.rate * kParticleStep;
            int count = (int)sim.emitAcc;
            sim.emitAcc -= count;
            if (sim.burst) {
                count += sim.origins.empty() ? 1 : std::clamp((int)std::lround(sim.settings.rate * 0.5f), 1, 200);
                sim.burst = false;
            }
            if (sim.origins.empty()) SimEmit(mesh, *input, count);
            else for (int i = 0; i < count; ++i) {
                const size_t source = std::min((size_t)(SampleRandom(sim.rng) * sim.origins.size()), sim.origins.size()-1);
                SimEmit(mesh, sim.origins[source], 1);
            }
        }
        SimStep(mesh, dabs, extra);
        ++sim.tick;
        const bool more = sim.replay ? sim.replayIndex < sim.inputs.size() : held || sim.held;
        if (!more && sim.parts.empty() && !sim.burst) { sim.playing = false; break; }
        // An emitter may miss the surface entirely.
        if (!more && sim.parts.empty() && !input) { sim.playing = false; break; }
    }
    if (sim.settings.wet) {
        for (size_t i = 0; i < dabs.size(); ++i) if (extra[i].x < 0) {
            sim.wetDabs.push_back(dabs[i]);
            auto deposit = extra[i];
            deposit.x *= -sim.settings.opacity;
            sim.wetExtra.push_back(deposit);
        }
    } else if (!dabs.empty()) {
        if (d.dirty.Revision() != sim.lastRevision) sim.wasDirty = true;
        v.renderer.ParticleDabs(d, *ctx, mesh, dabs, extra, painting, sim.deposits);
        sim.lastRevision = d.dirty.Revision();
        sim.versions.clear();
        for (Id id : sim.targets) if (auto* sf = d.GetSurface(id)) sim.versions.push_back({id, sf->version});
    }
    std::swap(regions, sim.disabledRegions);
    v.renderer.meshClip = clip;
    if (held && origin.valid) sim.previous = origin;
    sim.held = held;
    if (!sim.playing && (!sim.settings.adjustable || sim.settings.wet)) FinishParticles(true);
}

void App::FinishParticles(bool keep) {
    auto state = std::move(sim);
    sim = {};
    if (!ViewAlive(state.view)) return;
    auto& v = *state.view;
    auto& d = *v.doc;
    if (state.stroking) {
        if (keep) v.renderer.EndStrokeMulti(d, *ctx, v.history, state.settings.name);
        else {
            const bool clean = !state.wasDirty && d.dirty.Revision() == state.lastRevision;
            v.renderer.CancelParticlePaint(d, *ctx);
            if (clean) d.dirty.MarkSaved(d.dirty.Revision());
        }
    } else if (keep && !state.wetDabs.empty() && d.S().activeLayer == state.activeLayer && FluidStart(v, &state.settings)) {
        fluidRun.world = true;
        fluidRun.dabs = std::move(state.wetDabs);
        fluidRun.ex = std::move(state.wetExtra);
        if (tool == Tool::Particles && Active() == &v) {
            tool = viewTool[1] = Tool::Fluid;
            Status("Impacts déposés dans la peinture humide");
        } else FluidFinish(true);
    }
}

void App::CheckParticleSession() {
    if (!sim.view) return;
    if (!ViewAlive(sim.view)) { sim = {}; return; }
    auto& v = *sim.view;
    const auto& d = *v.doc;
    for (auto [id, version] : sim.versions) {
        const auto* sf = v.doc->GetSurface(id);
        if (!sf || sf->version != version) {
            v.renderer.CancelParticlePaint(*v.doc, *ctx, false);
            sim = {};
            return;
        }
    }
    if (!v.mesh || v.mesh->revision != sim.rev || Active() != &v || tool != Tool::Particles ||
        d.S().activeLayer != sim.activeLayer || d.S().selection != sim.selection) FinishParticles(true);
}

void App::ParticleSelectionOrigins(DocView& v) {
    auto* selection = v.doc->Selection();
    auto* kernel = Kernel("mesh_particle_sources.slang");
    if (!selection || !kernel) return;
    struct { u32 w, h, step; } root{v.doc->Width(), v.doc->Height(), 1};
    root.step = std::max(1u, (std::max(root.w, root.h) + 127) / 128);
    const u32 w = (root.w + root.step - 1) / root.step, h = (root.h + root.step - 1) / root.step;
    auto map = gpu::Dev().CreateTexture(w * 2, h, gpu::Format::RGBA32F, "ParticleSelectionSources");
    if (!map) return;
    gpu::DispatchDesc dispatch;
    dispatch.pipeline = kernel;
    dispatch.srv[0] = v.mesh->posMap.get();
    dispatch.srv[1] = v.mesh->nrmMap.get();
    dispatch.srv[2] = selection->tex.get();
    dispatch.uav[0] = map.get();
    SetConstants(dispatch, root);
    Dispatch2D(*ctx, dispatch, w, h);
    sim.sourceMask = ctx->ReadbackTexture(map.get());
}

namespace {
std::string ParticleMeshSignature(const MeshData& mesh) {
    Hasher hash;
    for (const auto& vertex : mesh.vertices) {
        hash.Pod(vertex.pos);
        hash.Pod(vertex.normal);
    }
    hash.Bytes(mesh.indices.data(), mesh.indices.size() * sizeof(u32));
    return std::to_string(hash.h);
}
}

Json App::ParticleGestureJson(const MeshData& mesh) const {
    const auto pack = [](const ParticleOrigin& origin) {
        Json row = Json::Array();
        for (const auto& p : {origin.point,origin.normal,origin.rayOrigin,origin.rayDirection})
            for (float value : {p.x,p.y,p.z}) row.Push(value);
        row.Push(origin.radius);
        return row;
    };
    Json j = Json::Object();
    j["version"] = sim.settings.script.empty() ? 1 : 2;
    j["mesh"] = ParticleMeshSignature(mesh);
    j["brush"] = sim.settings.ToJson(/*includeGroupState=*/false);
    for (const auto& entry : sim.inputs) {
        Json input = Json::Array();
        input.Push(entry.tick);
        input.Push(pack(entry.origin));
        j["inputs"].Push(std::move(input));
    }
    for (const auto& origin : sim.origins) j["origins"].Push(pack(origin));
    return j;
}

bool App::LoadParticleGesture(DocView& v, const Json& j) {
    const double version = j["version"].Num();
    if (!v.mesh || (version != 1 && version != 2) || j["mesh"].S() != ParticleMeshSignature(v.mesh->data) ||
        !j["brush"].IsObject() || j["inputs"].Elements().empty()) return false;
    const auto unpack = [](const Json& row, ParticleOrigin& out) {
        if (!row.IsArray() || row.Size() != 13) return false;
        for (const Json& item : row.Elements())
            if (item.GetType() != Json::Type::Number || !std::isfinite(item.F())) return false;
        auto vec = [&](int i) { return float3{row.At(i).F(),row.At(i+1).F(),row.At(i+2).F()}; };
        out = {vec(0),vec(3),vec(6),vec(9),row.At(12).F(),true};
        return out.radius > 0 && out.radius < 100 && Length(out.normal) > 0.1f;
    };
    std::vector<ParticleInput> inputs;
    std::vector<ParticleOrigin> origins;
    for (const auto& row : j["inputs"].Elements()) {
        if (!row.IsArray() || row.Size() != 2) return false;
        const double tick = row.At(0).Num(-1);
        if (tick < 0 || tick >= UINT32_MAX || std::floor(tick) != tick || (!inputs.empty() && tick <= inputs.back().tick)) return false;
        ParticleOrigin origin;
        if (!unpack(row.At(1),origin)) return false;
        inputs.push_back({(u32)tick,origin});
    }
    for (const auto& row : j["origins"].Elements()) {
        ParticleOrigin origin;
        if (!unpack(row,origin)) return false;
        origins.push_back(origin);
    }
    auto loaded = ParticleBrush::FromJson(j["brush"]);
    if (version == 2 && (loaded.script.empty() || !loaded.program)) return false;
    if (version == 1 && !loaded.script.empty()) return false;
    FinishParticles(false);
    particleBrush = std::move(loaded);
    tool = viewTool[1] = Tool::Particles;
    workView = 1;
    sim.inputs = std::move(inputs);
    sim.origins = std::move(origins);
    sim.settings.source = particleBrush.source;
    return BeginParticleStroke(v, true);
}

void App::UpdateSimulation(DocView& v, bool clicked, bool held, float2 pos, u32 w, u32 h) {
    CheckParticleSession();
    if (clicked && particleBrush.source == 0) {
        if (sim.view == &v) FinishParticles(true);
        if (tool != Tool::Particles) return; // wet impacts can hand off to Fluid
        const auto origin = ParticleOriginAt(*v.mesh, pos, w, h);
        if (!origin.valid || !BeginParticleStroke(v)) return;
        sim.previous = origin;
        sim.held = true;
        return;
    }
    if (sim.view != &v) return;
    if (sim.sourceMask) {
        if (!sim.sourceMask->ready) return;
        const auto& data = sim.sourceMask->data;
        for (size_t pos = 0; pos + sizeof(float4) * 2 <= data.size(); pos += sizeof(float4) * 2) {
            float4 pair[2];
            memcpy(pair, data.data() + pos, sizeof(pair));
            if (pair[0].w <= 0) continue;
            const float3 p{pair[0].x,pair[0].y,pair[0].z}, n = Normalize({pair[1].x,pair[1].y,pair[1].z});
            sim.origins.push_back({p, n, p + n * 2, n * -1, sim.settings.size * 0.5f, true});
        }
        sim.sourceMask.reset();
        if (sim.origins.empty()) {
            FinishParticles(false);
            Status("La sélection ne contient aucune surface à émettre");
            return;
        }
        sim.queued = true;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape) && !ImGui::GetIO().WantTextInput) { FinishParticles(false); return; }
    if (!sim.playing) return;
    const double dt = std::clamp(ImGui::GetIO().DeltaTime, 0.0f, 0.1f);
    if (sim.queued) {
        sim.queued = false;
        AdvanceParticles(dt, sim.origins.front(), false);
    } else {
        const bool emit = !sim.replay && sim.settings.source == 0 && held;
        const auto origin = emit ? ParticleOriginAt(*v.mesh, pos, w, h) : ParticleOrigin{};
        AdvanceParticles(dt, origin, emit);
    }
}

void App::DrawParticleOptions(DocView& v) {
    auto& b = particleBrush;
    if (ImGui::Button("Activer les particules")) {
        tool = viewTool[1] = Tool::Particles;
        workView = 1;
    }
    if (ImGui::BeginCombo("Brosse à particules", b.name.c_str())) {
        for (int i = 0; i < SimPresetCount(); ++i) {
            ImGui::PushID(i);
            if (ImGui::Selectable(SimPresetName(i))) SetParticleBrush(i);
            if (i >= kRustBrush && i < kScriptBrush && ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", kMaterialPresets[i-kRustBrush].help);
            ImGui::PopID();
        }
        if (!particlePresets.empty()) ImGui::Separator();
        for (size_t i = 0; i < particlePresets.size(); ++i) {
            ImGui::PushID((int)i + SimPresetCount());
            if (ImGui::Selectable(particlePresets[i].name.c_str())) b = particlePresets[i];
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    ImGui::InputText("Nom de la brosse", &b.name);
    ImGui::SameLine();
    if (ImGui::SmallButton("Enregistrer la brosse") && !b.name.empty()) {
        const auto previous = particlePresets;
        auto found = std::find_if(particlePresets.begin(), particlePresets.end(), [&](const auto& p) { return p.name == b.name; });
        if (found == particlePresets.end()) particlePresets.push_back(b);
        else *found = b;
        if (SaveAiSettings()) Status("Brosse à particules enregistrée");
        else { particlePresets = previous; Status("Impossible d’enregistrer la brosse"); }
    }
    DrawParticleCodeEditor();
    if (b.program && !b.program->Controls().empty()) {
        ImGui::SeparatorText("Réglages du script");
        const auto& controls = b.program->Controls();
        const auto resetValues = [&](size_t first, size_t end) {
            for (size_t i = first; i < end; ++i) b.scriptValues[i] = controls[i].initial;
        };
        const auto resetAll = Tr("Tout réinitialiser") + "###script-reset-all";
        if (ImGui::SmallButton(resetAll.c_str())) {
            resetValues(0,controls.size());
            b.scriptGroupOpen = {};
        }
        const auto drawControl = [&](size_t i) {
            using namespace particles;
            if (!b.program->IsControlVisible(i,b.scriptValues)) return;
            const auto& control = controls[i];
            auto& value = b.scriptValues[i];
            const auto label = Tr(control.label);
            ImGui::PushID(control.id.c_str());
            if (control.kind == ControlKind::Checkbox) {
                bool checked = value != 0;
                const auto caption = label + "###script-checkbox";
                if (ImGui::Checkbox(caption.c_str(),&checked)) value = checked ? 1.f : 0.f;
            } else {
                if (control.kind == ControlKind::Slider) {
                    ImGui::SliderFloat("##script-slider",&value,control.min,control.max,"%.4g",
                                       ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_NoRoundToFormat);
                } else {
                    const auto current = std::ranges::find(control.options,value,&ChoiceOption::value);
                    const char* preview = current != control.options.end() ? TrC(current->label.c_str()) : "";
                    if (ImGui::BeginCombo("##script-choice",preview)) {
                        for (size_t j = 0; j < control.options.size(); ++j) {
                            const auto& option = control.options[j];
                            const bool selected = option.value == value;
                            ImGui::PushID((int)j);
                            const auto caption = Tr(option.label) + "###script-option";
                            if (ImGui::Selectable(caption.c_str(),selected)) value = option.value;
                            if (selected) ImGui::SetItemDefaultFocus();
                            ImGui::PopID();
                        }
                        ImGui::EndCombo();
                    }
                }
                ImGui::SameLine();
                ImGui::TextUnformatted(label.c_str());
            }
            ImGui::PopID();
        };
        size_t next = 0;
        const auto drawUntil = [&](size_t end) {
            while (next < end) drawControl(next++);
        };
        const auto& groups = b.program->Groups();
        for (size_t i = 0; i < groups.size(); ++i) {
            const auto& group = groups[i];
            drawUntil(group.firstControl);
            if (b.program->IsGroupVisible(i,b.scriptValues)) {
                const auto title = Tr(group.label) + "###script-group-" + group.id;
                const auto reset = Tr("Réinitialiser") + "###script-group-reset-" + group.id;
                const auto& style = ImGui::GetStyle();
                const float gap = style.ItemSpacing.x + style.FramePadding.x + std::floor(style.WindowPadding.x * 0.5f);
                const float headerWidth = ImGui::CalcTextSize(title.c_str(),nullptr,true).x +
                    ImGui::GetFontSize() + style.FramePadding.x * 2;
                const float resetWidth = ImGui::CalcTextSize(reset.c_str(),nullptr,true).x + style.FramePadding.x * 2;
                const bool fitsInline = headerWidth + gap + resetWidth <= ImGui::GetContentRegionAvail().x;
                const ImGuiID id = ImGui::GetID(title.c_str());
                ImGui::GetStateStorage()->SetBool(id,b.scriptGroupOpen[i].value_or(group.defaultOpen));
                bool open = ImGui::CollapsingHeader(title.c_str(),fitsInline ? ImGuiTreeNodeFlags_SpanLabelWidth : 0);
                const bool headerSubmitted = ImGui::GetItemID() == id;
                if (headerSubmitted && ImGui::IsItemToggledOpen()) b.scriptGroupOpen[i] = open;
                if (fitsInline && headerSubmitted) ImGui::SameLine(0,gap);
                if (ImGui::Button(reset.c_str())) {
                    resetValues(group.firstControl,group.endControl);
                    b.scriptGroupOpen[i].reset();
                    open = group.defaultOpen;
                    ImGui::GetStateStorage()->SetBool(id,open);
                }
                if (open) {
                    ImGui::Indent();
                    drawUntil(group.endControl);
                    ImGui::Unindent();
                }
            }
            next = group.endControl;
        }
        drawUntil(controls.size());
    }
    if (ImGui::BeginCombo("Comportement", kPresets[b.behavior].name)) {
        for (int i = 0; i < kBehaviorCount; ++i) {
            if (ImGui::Selectable(kPresets[i].name, i == b.behavior)) b.behavior = i;
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", kPresets[i].help);
        }
        ImGui::EndCombo();
    }
    ImGui::Combo("Source", &b.source, "Sous le pinceau\0Sélection UV\0Tracé de surface\0");
    if (b.source == 2) {
        const auto* selected = v.doc->FindLayer(v.particlePathSource);
        if (ImGui::BeginCombo("Tracé d’émission", selected ? selected->name.c_str() : "Choisir un tracé")) {
            std::set<Id> listed;
            for (const auto& layer : v.doc->S().layers) if (!layer.surfacePath.empty() && listed.insert(layer.material).second) {
                ImGui::PushID((int)layer.id);
                if (ImGui::Selectable(layer.name.c_str(), layer.id == v.particlePathSource)) v.particlePathSource = layer.id;
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
    }
    ImGui::SliderFloat("Particules / s", &b.rate, 1, 400, "%.0f", ImGuiSliderFlags_Logarithmic);
    float size = b.size * 100;
    if (ImGui::SliderFloat("Taille des gouttes", &size, 0.2f, 10, "%.1f %%", ImGuiSliderFlags_Logarithmic)) b.size = size / 100;
    ImGui::SliderFloat("Gravité", &b.gravity, 0, 3);
    ImGui::SliderFloat("Adhérence", &b.adhesion, 0, 2);
    ImGui::SliderFloat("Durée de vie", &b.life, 0.2f, 10, "%.1f s");
    if (b.behavior == 3) ImGui::SliderFloat("Dispersion", &b.spread, 0, 2);
    ImGui::SliderFloat("Dureté des particules", &b.hardness, 0, 0.99f);
    ImGui::SliderFloat("Débit des particules", &b.flow, 0, 1);
    ImGui::SliderFloat("Opacité des particules", &b.opacity, 0, 1);
    ImGui::InputScalar("Graine", ImGuiDataType_U32, &b.seed);
    ImGui::Checkbox("Aperçu rejouable", &b.adjustable);
    ImGui::Checkbox("Déposer les impacts en peinture humide", &b.wet);
    if (ImGui::Checkbox("Matière propre à la brosse", &b.customMaterial) && b.customMaterial && !b.channels) {
        for (int c = 0; c < (int)PbrChannel::Count; ++c) {
            if (brush3D.use[c]) b.channels |= 1u << c;
            auto linear = BrushChannelValue((PbrChannel)c);
            b.colors[c] = b.impactColors[c] = {LinearToSrgb(linear.x), LinearToSrgb(linear.y), LinearToSrgb(linear.z), 1};
        }
    }
    if (b.customMaterial && ImGui::TreeNode("Matière à l’impact et sur la traînée")) {
        for (int c = 0; c < (int)PbrChannel::Count; ++c) {
            ImGui::PushID(c);
            const u32 bit = 1u << c;
            bool enabled = (b.channels & bit) != 0;
            if (ImGui::Checkbox(PbrChannelName((PbrChannel)c), &enabled)) b.channels = enabled ? b.channels | bit : b.channels & ~bit;
            if (enabled) {
                ImGui::ColorEdit3("Matière de la traînée", &b.colors[c].x);
                ImGui::ColorEdit3("Matière à l’impact", &b.impactColors[c].x);
                ImGui::SliderFloat2("Intensité (traînée / impact)", &b.deposits[c].x, 0, 1);
            }
            ImGui::PopID();
        }
        ImGui::TreePop();
    }
    if (b.source != 0 && ImGui::Button("Émettre depuis la source")) {
        FinishParticles(true);
        tool = viewTool[1] = Tool::Particles;
        workView = 1;
        if (BeginParticleStroke(v)) {
            if (b.source == 1) ParticleSelectionOrigins(v);
            else sim.queued = ParticlePathOrigins(v, sim.origins);
            if (!sim.sourceMask && !sim.queued) {
                FinishParticles(false);
                Status("Choisissez une sélection UV ou un tracé valide");
            }
        }
    }
    if (sim.view == &v && !sim.inputs.empty()) {
        if (ImGui::Button("Rejouer le geste")) BeginParticleStroke(v, true);
        ImGui::SameLine();
        if (ImGui::Button("Figer en peinture")) FinishParticles(true);
        ImGui::SameLine();
        if (ImGui::Button("Annuler l’aperçu")) FinishParticles(false);
        if (sim.view && ImGui::Button("Enregistrer le geste…")) {
            const auto path = platform::SaveFileDialog(hwnd, L"Geste de particules Shaderixel (*.sxstroke)\0*.sxstroke\0", L"sxstroke");
            if (!path.empty()) {
                std::ofstream file(Utf8Path(path), std::ios::binary);
                file << ParticleGestureJson(v.mesh->data).Dump(0);
                file.close();
                Status(file ? "Geste enregistré" : "Impossible d’écrire le geste");
            }
        }
    }
    if (ImGui::Button("Charger un geste…")) {
        const auto path = platform::OpenFileDialog(hwnd, L"Geste de particules Shaderixel (*.sxstroke)\0*.sxstroke\0");
        if (!path.empty()) {
            std::error_code ec;
            const auto size = std::filesystem::file_size(Utf8Path(path), ec);
            std::string text;
            Json j;
            if (ec || size > (64u << 20) || !ReadTextFile(path, text) || !Json::Parse(text, j) || !LoadParticleGesture(v,j))
                Status("Geste illisible ou géométrie différente du geste enregistré");
        }
    }
}

void App::DrawParticleCodeEditor() {
    constexpr const char* popup = "Éditeur de particules##particle-code";
    if (ImGui::SmallButton("Éditer le script…")) {
        particleCode.text = particleBrush.script.empty() ? "emit {\n}\n\nupdate {\n}\n\nimpact {\n}\n" : particleBrush.script;
        particleCode.diagnostic = {};
        ImGui::OpenPopup(popup);
    }
    if (!particleBrush.script.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("%s",TrC(particleBrush.program ? "Script actif" : "Script à corriger"));
    }
    ImGui::SetNextWindowSize(ImVec2(730*dpi,610*dpi),ImGuiCond_FirstUseEver);
    if (!ImGui::BeginPopupModal(popup,nullptr,ImGuiWindowFlags_NoSavedSettings)) return;
    const bool loadExample = ImGui::Button("Charger l’exemple");
    if (loadExample) {
        particleCode.text = particles::ExampleProgram();
        particleCode.diagnostic = {};
    }
    if (ImGui::CollapsingHeader("Référence du script")) {
        ImGui::TextWrapped("emit — position x/y/z, vitesse vx/vy/vz, radius, life, alpha, alive et on_surface.");
        ImGui::TextWrapped("update — vitesse, ax/ay/az, gravity, radius, life, alpha et alive à chaque pas de 1/120 s.");
        ImGui::TextWrapped("impact — impact_radius, strength, restitution et response (NATIVE, SLIDE, STOP, BOUNCE).");
        ImGui::TextWrapped("En lecture dans les trois blocs : nx/ny/nz, speed, age, time et dt. Expressions, let, if/else et random() dans chaque bloc.");
        ImGui::TextWrapped("Fonctions : min, max, clamp, mix, abs, floor, sqrt, sin, cos. life et age sont en secondes.");
        ImGui::TextWrapped("Déclarez les réglages avant les blocs et utilisez leurs identifiants en lecture dans le script.");
        ImGui::TextUnformatted("slider gain \"Gain\" = 1 [0, 2];");
        ImGui::TextUnformatted("checkbox paint \"Paint\" = true;");
        ImGui::TextUnformatted("choice mode \"Mode\" = SLIDE [SLIDE \"Slide\", STOP \"Stop\"];");
        ImGui::TextWrapped("Ajoutez when après un réglage pour l’afficher selon les valeurs des réglages déclarés plus haut.");
        ImGui::TextUnformatted("slider stop_gain \"Gain\" = 1 [0, 2] when mode == STOP;");
        ImGui::TextWrapped("Placez des réglages dans un bloc group pour les replier ensemble. Une condition when peut aussi piloter le groupe.");
        ImGui::TextWrapped("Indiquez open ou collapsed après le libellé du groupe pour choisir son état initial.");
        ImGui::TextUnformatted("group extra \"Extra\" collapsed when mode == STOP {\n    checkbox deposit \"Deposit\" = true;\n}");
    }
    std::vector<ui::CodeMark> marks;
    if (!particleCode.diagnostic.message.empty()) marks.push_back({particleCode.diagnostic.line,particleCode.diagnostic.message});
    if (ImGui::IsWindowAppearing() || loadExample) ImGui::SetNextWindowFocus();
    if (ui::CodeEditor("##particle-source",&particleCode.text,
                       ImVec2(-1,std::max(100.0f*dpi,ImGui::GetContentRegionAvail().y - 72*dpi)),false,nullptr,&marks))
        particleCode.diagnostic = {};
    if (!particleCode.diagnostic.message.empty())
        ImGui::TextWrapped("%s %d — %s",TrC("Ligne"),particleCode.diagnostic.line+1,TrC(particleCode.diagnostic.message.c_str()));
    if (ImGui::Button("Appliquer le script")) {
        if (particleBrush.SetScript(particleCode.text,particleCode.diagnostic)) {
            particleBrush.adjustable = true;
            Status("Script de particules appliqué");
            ImGui::CloseCurrentPopup();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Désactiver le script")) {
        particleBrush.program.reset();
        particleBrush.script.clear();
        particleBrush.scriptValues = {};
        particleBrush.scriptGroupOpen = {};
        particleBrush.scriptVersion = kScriptVersion;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Annuler")) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

}  // namespace sx
