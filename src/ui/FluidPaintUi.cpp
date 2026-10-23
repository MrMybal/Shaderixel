// Liquid paint tool: the brush lays a film of paint that carries the whole brush material
// (base color, roughness, metal, relief, emissive) and runs with gravity. On a 3D project the
// slope comes from the model (each texel of the UV layout knows its surface), so the paint runs
// down the model, around its relief, and drops from the overhangs; on an image the support is a
// plane that can be tilted (wall, table, ceiling) and turned. The simulation runs on the GPU
// (render/FluidPaint.cpp) while the paint is wet, then the whole session is one undo step.
#include "ui/App.h"
#include "ui/Theme.h"
#include "render/MeshPaint.h"
#include "platform/Platform.h"

#include <imgui_internal.h>

namespace sx {

namespace {

struct FluidPreset {
    const char* name;
    const char* help;
    float3 color;  // sRGB
    bool useColor, useRough, useMetal, useHeight, useEmissive;
    float rough, metal, height;
    float3 emissive;
    float flow, viscosity, retention, spread, setTime, noise, noiseScale, absorb, density, relief, wetGloss, dripOff;
};

const FluidPreset kFluidPresets[] = {
    {"Peinture qui coule", "Peinture fraîche : coulures qui s'allongent puis se figent, brillantes tant qu'elles sont humides.",
     {0.72f, 0.07f, 0.05f}, true, true, true, true, false, 0.35f, 0.0f, 0.56f, {0, 0, 0},
     4.0f, 1.0f, 0.3f, 0.25f, 8, 0.5f, 6, 0.03f, 3.0f, 1.0f, 0.8f, 0.5f},
    {"Peinture épaisse (empâtement)", "Pâte lourde : coule peu, garde le relief du pinceau.",
     {0.9f, 0.75f, 0.2f}, true, true, true, true, false, 0.5f, 0.0f, 0.62f, {0, 0, 0},
     6.0f, 3.0f, 0.9f, 0.1f, 4, 0.4f, 5, 0.0f, 4.0f, 2.5f, 0.5f, 0.2f},
    {"Encre (s'étale et tache)", "Encre fluide : s'étale, coule vite et tache le support.",
     {0.05f, 0.07f, 0.16f}, true, true, true, false, false, 0.5f, 0.0f, 0.5f, {0, 0, 0},
     3.0f, 0.5f, 0.12f, 0.9f, 5, 0.7f, 10, 0.5f, 2.0f, 0.1f, 0.5f, 0.3f},
    {"Huile / goudron", "Noir brillant et visqueux, reste humide longtemps.",
     {0.02f, 0.02f, 0.02f}, true, true, true, true, false, 0.08f, 0.0f, 0.55f, {0, 0, 0},
     4.0f, 2.5f, 0.45f, 0.4f, 15, 0.4f, 8, 0.0f, 4.0f, 1.2f, 1.0f, 0.4f},
    {"Rouille qui coule", "Traînées orangées et mates qui tachent le métal.",
     {0.42f, 0.17f, 0.06f}, true, true, true, true, false, 0.85f, 0.0f, 0.52f, {0, 0, 0},
     3.0f, 1.2f, 0.18f, 0.15f, 6, 0.8f, 4, 0.6f, 1.6f, 0.15f, 0.3f, 0.3f},
    {"Eau / humidité", "Mouille la surface : seulement la rugosité (et un peu plus sombre), sans couleur.",
     {0.5f, 0.5f, 0.5f}, false, true, false, false, false, 0.05f, 0.0f, 0.5f, {0, 0, 0},
     3.0f, 0.4f, 0.1f, 0.6f, 10, 0.6f, 8, 0.2f, 2.0f, 0.0f, 1.0f, 0.6f},
    {"Métal liquide (chrome)", "Coulures de métal poli, en relief.",
     {0.86f, 0.86f, 0.88f}, true, true, true, true, false, 0.08f, 1.0f, 0.6f, {0, 0, 0},
     4.0f, 1.5f, 0.4f, 0.5f, 8, 0.3f, 6, 0.0f, 4.0f, 1.5f, 0.0f, 0.4f},
    {"Cire de bougie", "Coule lentement et se fige vite, en gouttes épaisses.",
     {0.93f, 0.9f, 0.82f}, true, true, true, true, false, 0.45f, 0.0f, 0.65f, {0, 0, 0},
     5.0f, 3.5f, 0.6f, 0.15f, 2.5f, 0.5f, 5, 0.0f, 5.0f, 2.0f, 0.4f, 0.0f},
    {"Sang", "Rouge sombre et brillant, coule et tache un peu.",
     {0.33f, 0.01f, 0.02f}, true, true, true, true, false, 0.2f, 0.0f, 0.53f, {0, 0, 0},
     4.0f, 0.9f, 0.25f, 0.3f, 10, 0.5f, 6, 0.15f, 3.0f, 0.6f, 0.9f, 0.5f},
    {"Lave (émissive)", "Coulée épaisse, sombre en surface et lumineuse.",
     {0.1f, 0.03f, 0.02f}, true, true, true, true, true, 0.7f, 0.0f, 0.64f, {1.0f, 0.35f, 0.05f},
     5.0f, 4.0f, 0.7f, 0.2f, 6, 0.6f, 5, 0.0f, 5.0f, 2.0f, 0.2f, 0.2f},
};
constexpr int kFluidPresetCount = (int)(sizeof(kFluidPresets) / sizeof(kFluidPresets[0]));
// Load of the brush (coat / retained film) of each preset: above 1 the excess runs.
const float kFluidPresetCoat[kFluidPresetCount] = {1.9f, 1.1f, 2.2f, 1.5f, 1.8f, 2.0f, 1.5f, 1.25f, 1.9f, 1.3f};

constexpr float kDeg = 0.0174532925f;

// Channels painted by the liquid, in the order of the stroke layers.
const PbrChannel kFluidChannels[] = {PbrChannel::BaseColor, PbrChannel::Roughness, PbrChannel::Metallic, PbrChannel::Height, PbrChannel::Emissive};

}  // namespace

void App::ApplyFluidPreset(int i) {
    const FluidPreset& P = kFluidPresets[std::clamp(i, 0, kFluidPresetCount - 1)];
    auto& F = fluidSet;
    F.preset = i;
    F.flow = P.flow * 0.5f;
    F.coat = kFluidPresetCoat[std::clamp(i, 0, kFluidPresetCount - 1)];
    F.viscosity = P.viscosity;
    F.retention = P.retention;
    F.spread = P.spread;
    F.setTime = P.setTime;
    F.noise = P.noise;
    F.noiseScale = P.noiseScale;
    F.absorb = P.absorb;
    F.density = P.density;
    F.relief = P.relief;
    F.wetGloss = P.wetGloss;
    F.dripOff = P.dripOff;
    // The material carried by the paint (the brush material of the 3D projects).
    if (P.useColor) fg = {P.color.x, P.color.y, P.color.z, 1};
    auto& B = brush3D;
    B.use[(int)PbrChannel::BaseColor] = P.useColor;
    B.use[(int)PbrChannel::Roughness] = P.useRough;
    B.use[(int)PbrChannel::Metallic] = P.useMetal;
    B.use[(int)PbrChannel::Height] = P.useHeight;
    B.use[(int)PbrChannel::Emissive] = P.useEmissive;
    B.rough = P.rough;
    B.metal = P.metal;
    B.height = P.height;
    if (P.useEmissive) B.emissive = {P.emissive.x, P.emissive.y, P.emissive.z, 1};
}

Renderer::FluidParams App::FluidParamsNow(DocView& v) {
    const auto& F = fluidSet;
    Renderer::FluidParams p;
    const u32 simW = std::max(fluidRun.simW, 64u);
    p.speed = 0.2f * simW / std::max(F.viscosity, 0.05f);
    p.strength = F.gravity;
    p.retention = F.retention;
    p.spread = F.spread;
    p.cohesion = F.cohesion;
    p.coat = F.coat * F.retention;
    p.setTime = F.setTime;
    p.noise = F.noise;
    p.noiseScale = F.noiseScale * simW / 512.0f;
    p.absorb = F.absorb;
    p.damping = 4.0f + 3.0f * F.viscosity;
    p.density = F.density * 1.2f;  // squared in the shader: the retained film covers
    p.relief = F.relief;
    p.heightBase = brush3D.height;
    p.wetGloss = F.wetGloss;
    p.bed = F.bed * 6.0f;
    p.dripOff = F.dripOff;
    p.hardness = F.hardness;
    p.seed = fluidRun.seed;
    p.offEdges = F.offEdges;
    const float4 c = fluidRun.useOverride ? fluidRun.overrideColor : LinearColor(fg);
    p.color = {c.x, c.y, c.z, 1};
    p.rough = brush3D.rough;
    p.metal = brush3D.metal;
    p.emissive = {SrgbToLinear(brush3D.emissive.x), SrgbToLinear(brush3D.emissive.y), SrgbToLinear(brush3D.emissive.z), 1};
    if (fluidRun.active ? fluidRun.mesh : (v.doc->kind == 1 && v.mesh != nullptr)) {
        // World gravity: straight down, the bottom of the view, or any direction.
        MeshScene& s = *v.mesh;
        float3 g = {0, -1, 0};
        if (F.gravMode == 1) {
            const float3 fwd = Normalize(s.target - s.camPos);
            const float3 right = Normalize(Cross(float3{0, 1, 0}, fwd));
            const float3 up = Cross(fwd, right);
            g = up * -1.0f;
        } else if (F.gravMode == 2) {
            const float t = F.tilt * kDeg, a = F.azimuth * kDeg;
            g = {std::sin(t) * std::cos(a), -std::cos(t), std::sin(t) * std::sin(a)};
        }
        g = Normalize(g);
        p.gravity = {g.x, g.y, g.z, 0};
    } else {
        // Image: a plane tilted by tilt2D (0 flat on a table, 90 a wall, 180 a ceiling), the slope toward dir2D.
        const float t = F.tilt2D * kDeg, a = F.dir2D * kDeg;
        const float slope = std::sin(t);
        p.gravity = {-std::sin(a) * slope, std::cos(a) * slope, -std::cos(t), 0};
    }
    return p;
}

bool App::FluidStart(DocView& v) {
    if (fluidRun.active) FluidFinish(true);
    Document& d = *v.doc;
    if (d.tiled) { Status("Peinture liquide : pas sur les très grands documents (tuilés)"); return false; }
    // Particles still running hold the stroke: they end here.
    if (sim.stroking && sim.view == &v) {
        v.renderer.EndStrokeMulti(d, *ctx, v.history, "Simulation de peinture");
        sim.stroking = false;
        sim.parts.clear();
    }
    std::vector<Id> targets;
    std::vector<int> kinds;
    fluidRun.useOverride = false;
    const bool project = d.kind == 1 && v.mesh;
    if (project) {
        Layer* al = d.ActiveLayer();
        if (al && al->material && IsPaintLayer3D(d, al->material) && !d.S().editMask) {
            for (PbrChannel c : kFluidChannels) {
                if (!brush3D.use[(int)c]) continue;
                Layer* l = MaterialChannelLayer(d, al->material, c);
                if (!l || !l->surface) continue;
                targets.push_back(l->surface);
                kinds.push_back(c == PbrChannel::BaseColor ? 0 : c == PbrChannel::Roughness ? 1 : c == PbrChannel::Metallic ? 2 : c == PbrChannel::Height ? 3 : 4);
            }
        }
    }
    if (targets.empty()) {
        // A plain layer, a mask or a fill material's mask: the paint color (white / gray on masks).
        Id t = d.PaintTarget();
        if (!t) { Status("Peinture liquide : sélectionnez un calque de pixels"); return false; }
        targets.push_back(t);
        kinds.push_back(0);
        fluidRun.useOverride = true;
        fluidRun.overrideColor = PaintColor(d);
    }
    // Simulation size: the document's, at most 2048 (auto) or a fraction.
    const u32 W = d.Width(), H = d.Height();
    float k = 1.0f;
    switch (fluidSet.simRes) {
        case 0: k = std::min(1.0f, 2048.0f / std::max(W, H)); break;
        case 2: k = 0.5f; break;
        case 3: k = 0.25f; break;
        default: break;
    }
    const u32 sw = std::max(16u, (u32)std::lround(W * k)), sh = std::max(16u, (u32)std::lround(H * k));
    gpu::Texture* bed = project ? ChannelTexture(v, PbrChannel::Height) : nullptr;
    Surface* sel = d.Selection();
    if (!v.renderer.FluidBegin(d, *ctx, targets, kinds, project ? v.mesh.get() : nullptr, sw, sh, sel && !project ? sel->tex.get() : nullptr, bed)) {
        Status("Peinture liquide : impossible de démarrer la simulation");
        return false;
    }
    fluidRun.view = &v;
    fluidRun.active = true;
    v.renderer.fluidInterrupt = [this] { FluidFinish(true); };
    fluidRun.versions.clear();
    fluidRun.mesh = project;
    fluidRun.simW = sw;
    fluidRun.simH = sh;
    fluidRun.idle = 0;
    fluidRun.gravKey = 0;
    fluidRun.seed = (float)(Rand01() * 997.0);
    fluidRun.dabs.clear();
    fluidRun.ex.clear();
    return true;
}

void App::FluidFinish(bool keep) {
    if (!fluidRun.active) return;
    DocView* v = fluidRun.view;
    fluidRun.active = false;
    fluidRun.painting = false;
    fluidRun.haveDoc = fluidRun.haveHit = false;
    fluidRun.dabs.clear();
    fluidRun.ex.clear();
    bool alive = false;
    for (auto& d : docs) alive |= d.get() == v;
    if (!alive || !v) return;
    // The last state of the paint, then one undo step for every layer.
    v->renderer.FluidComposite(*v->doc, *ctx, FluidParamsNow(*v));
    v->renderer.FluidEnd(*v->doc, *ctx, v->history, "Peinture liquide", keep);
    Status(keep ? "Peinture liquide : figée" : "Peinture liquide : annulée");
}

void App::FluidInput2D(DocView& v, float2 dp, float2 canvasMin, bool hovered) {
    ImGuiIO& io = ImGui::GetIO();
    const BrushSettings& b = brush;
    const float rad = std::max(b.size * 0.5f, 0.5f);
    // Brush outline (document pixels to screen).
    if (hovered) {
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        dl->AddCircle(io.MousePos, rad * v.zoom, IM_COL32(0, 0, 0, 160), 0, 2.5f);
        dl->AddCircle(io.MousePos, rad * v.zoom, IM_COL32(120, 200, 255, 230), 0, 1.2f);
    }
    const bool down = ImGui::IsMouseDown(ImGuiMouseButton_Left) && !io.KeyAlt;
    if (hovered && ImGui::IsKeyPressed(ImGuiKey_Escape) && fluidRun.active) { FluidFinish(false); return; }
    if (hovered && ImGui::IsKeyPressed(ImGuiKey_Enter) && fluidRun.active) { FluidFinish(true); return; }
    const bool starting = hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.KeyAlt;
    if (!down || (!fluidRun.painting && !starting)) {
        fluidRun.painting = false;
        fluidRun.haveDoc = false;
        return;
    }
    if ((!fluidRun.active || fluidRun.view != &v) && !FluidStart(v)) return;
    fluidRun.erase = io.KeyCtrl;
    const float pressure = platform::PenActive() ? PenPressure(platform::LastPressure()) : 1.0f;
    std::vector<float2> pts;
    if (!fluidRun.haveDoc) {
        pts.push_back(dp);
    } else {
        const float2 dd = dp - fluidRun.lastDoc;
        const float len = Length(dd), step = std::max(rad * 0.3f, 0.75f);
        const int n = std::min(256, (int)std::floor(len / step));
        for (int i = 1; i <= n; ++i) pts.push_back(fluidRun.lastDoc + dd * (i * step / std::max(len, 1e-4f)));
        if (pts.empty()) pts.push_back(dp);
    }
    // Paint per second, shared by the dabs of this frame: a fast stroke lays a thin coat, a still brush a pool.
    const float dt = std::clamp(io.DeltaTime, 1.0f / 240, 1.0f / 15);
    const float amount = fluidSet.flow * dt * pressure * (b.pressureSize ? 1.0f : 1.0f) / (float)pts.size();
    for (auto& q : pts) {
        fluidRun.dabs.push_back({q.x, q.y, rad * (b.pressureSize ? std::max(pressure, 0.2f) : 1.0f), 0});
        fluidRun.ex.push_back({amount * 10.0f, 0, 0, 0});
    }
    fluidRun.world = false;
    fluidRun.lastDoc = dp;
    fluidRun.haveDoc = true;
    fluidRun.painting = true;
    fluidRun.idle = 0;
    (void)canvasMin;
}

void App::FluidInput3D(DocView& v, float2 local, u32 w, u32 h, bool hovered, bool orbit, ImVec2 origin) {
    ImGuiIO& io = ImGui::GetIO();
    MeshScene& s = *v.mesh;
    const BrushSettings& b = brush;
    if (hovered && !orbit) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddCircle(io.MousePos, b.size * 0.5f, IM_COL32(0, 0, 0, 160), 0, 2.5f);
        dl->AddCircle(io.MousePos, b.size * 0.5f, IM_COL32(120, 200, 255, 230), 0, 1.2f);
    }
    // Gravity gizmo (bottom left): where the paint runs, seen from the camera.
    {
        Renderer::FluidParams p = FluidParamsNow(v);
        const float3 g = {p.gravity.x, p.gravity.y, p.gravity.z};
        const float3 fwd = Normalize(s.target - s.camPos);
        const float3 right = Normalize(Cross(float3{0, 1, 0}, fwd));
        const float3 up = Cross(fwd, right);
        const float2 sc = {Dot(g, right), -Dot(g, up)};
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 c(origin.x + 46, origin.y + h - 46);
        dl->AddCircleFilled(c, 30, IM_COL32(20, 22, 26, 170));
        dl->AddCircle(c, 30, IM_COL32(255, 255, 255, 60));
        const ImVec2 e(c.x + sc.x * 24, c.y + sc.y * 24);
        dl->AddLine(c, e, IM_COL32(120, 200, 255, 255), 3);
        dl->AddCircleFilled(e, 4.5f, IM_COL32(120, 200, 255, 255));
        dl->AddText(ImVec2(c.x - 24, c.y + 32), IM_COL32(200, 200, 200, 200), "Gravité");
    }
    const bool down = hovered && ImGui::IsMouseDown(ImGuiMouseButton_Left) && !io.KeyAlt && !orbit;
    if (hovered && ImGui::IsKeyPressed(ImGuiKey_Escape) && fluidRun.active) { FluidFinish(false); return; }
    if (hovered && ImGui::IsKeyPressed(ImGuiKey_Enter) && fluidRun.active) { FluidFinish(true); return; }
    if (!down) {
        fluidRun.painting = false;
        fluidRun.haveHit = false;
        return;
    }
    if (sim.rc.Empty() || sim.scene != &s || sim.rev != s.revision) {
        sim.rc.Build(s.data);
        sim.scene = &s;
        sim.rev = s.revision;
    }
    float3 o, d, hit, n;
    CameraRay(s, local, w, h, o, d);
    float t = 0;
    if (!sim.rc.Ray(o, d, 100.0f, &hit, &n, &t)) { fluidRun.haveHit = false; return; }
    if ((!fluidRun.active || fluidRun.view != &v) && !FluidStart(v)) return;
    fluidRun.erase = io.KeyCtrl;
    const float pixel = s.ortho ? 2.0f * s.OrthoHalfHeight() / std::max(h, 1u) : 2.0f * std::tan(s.fov * 0.5f) * t / std::max(h, 1u);
    const float pressure = platform::PenActive() ? PenPressure(platform::LastPressure()) : 1.0f;
    const float radius = std::max(b.size * 0.5f * pixel * (b.pressureSize ? std::max(pressure, 0.2f) : 1.0f), 1e-4f);
    std::vector<float3> pts;
    if (fluidRun.haveHit && Length(hit - fluidRun.lastHit) < radius * 12) {
        const float3 dd = hit - fluidRun.lastHit;
        const float len = Length(dd), step = radius * 0.3f;
        const int cnt = std::min(256, (int)std::floor(len / step));
        for (int i = 1; i <= cnt; ++i) pts.push_back(fluidRun.lastHit + dd * (i * step / std::max(len, 1e-6f)));
    }
    if (pts.empty()) pts.push_back(hit);
    const float dt = std::clamp(io.DeltaTime, 1.0f / 240, 1.0f / 15);
    const float amount = fluidSet.flow * dt * pressure / (float)pts.size();
    for (auto& q : pts) {
        fluidRun.dabs.push_back({q.x, q.y, q.z, radius});
        fluidRun.ex.push_back({amount * 10.0f, n.x, n.y, n.z});
    }
    fluidRun.world = true;
    fluidRun.lastHit = hit;
    fluidRun.haveHit = true;
    fluidRun.painting = true;
    fluidRun.idle = 0;
}

void App::FluidTick() {
    if (!fluidRun.active) return;
    DocView* v = fluidRun.view;
    bool alive = false;
    for (auto& d : docs) alive |= d.get() == v;
    if (!alive) {
        fluidRun.active = false;  // the document was closed with its renderer
        return;
    }
    // Another tool or another document: the paint sets where it is.
    if (tool != Tool::Fluid || Active() != v) { FluidFinish(true); return; }
    // Another edit of the painted layers (a filter, a fill...): the paint was set before it, as it was.
    {
        const auto ids = v->renderer.FluidSurfaces();
        bool changed = ids.size() != fluidRun.versions.size() && !fluidRun.versions.empty();
        for (size_t i = 0; i < ids.size() && i < fluidRun.versions.size() && !changed; ++i) {
            const Surface* s = v->doc->GetSurface(ids[i]);
            changed = !s || s->version != fluidRun.versions[i];
        }
        if (changed) {
            fluidRun.active = false;
            v->renderer.FluidEnd(*v->doc, *ctx, v->history, "Peinture liquide", true);
            Status("Peinture liquide : figée (calque modifié)");
            return;
        }
    }
    // A control of another panel is used (a menu, a filter, a button): the paint sets before its action runs.
    {
        ImGuiContext& g = *GImGui;
        if (g.ActiveId && g.ActiveIdWindow) {
            const char* root = g.ActiveIdWindow->RootWindow ? g.ActiveIdWindow->RootWindow->Name : g.ActiveIdWindow->Name;
            const std::string r = root ? root : "";
            const bool ours = r == "Canevas" || r == "Vue 3D" || r == "Options" || r == "Matériau du pinceau" || r == "Outils" ||
                              r.find("DockSpace") != std::string::npos || r.find("WindowOverViewport") != std::string::npos;
            if (!ours) { FluidFinish(true); return; }
        }
    }
    Renderer::FluidParams p = FluidParamsNow(*v);
    u64 key = 1469598103934665603ull;
    for (float f : {p.gravity.x, p.gravity.y, p.gravity.z, p.strength}) {
        u32 b;
        memcpy(&b, &f, 4);
        key = (key ^ b) * 1099511628211ull;
    }
    if (key != fluidRun.gravKey) {
        v->renderer.FluidGravity(*ctx, p);
        fluidRun.gravKey = key;
    }
    if (!fluidRun.dabs.empty()) {
        v->renderer.FluidDeposit(*ctx, fluidRun.dabs, fluidRun.ex, fluidRun.world, fluidRun.erase, p);
        fluidRun.dabs.clear();
        fluidRun.ex.clear();
    }
    const float dt = (demoMode ? 1.0f / 60 : std::clamp(ImGui::GetIO().DeltaTime, 1.0f / 240, 1.0f / 20)) * fluidSet.timeScale;
    if (!fluidSet.paused) {
        fluidRun.substeps = v->renderer.FluidStep(*ctx, dt, p);
        if (!fluidRun.painting) fluidRun.idle += dt;
    }
    v->renderer.FluidComposite(*v->doc, *ctx, p);
    fluidRun.versions.clear();
    for (Id id : v->renderer.FluidSurfaces())
        if (const Surface* s = v->doc->GetSurface(id)) fluidRun.versions.push_back(s->version);
    // Set: nothing runs any more.
    if (!fluidRun.painting && !fluidSet.paused && fluidRun.idle > fluidSet.setTime * 1.5f + 0.5f) FluidFinish(true);
}

void App::DrawFluidOptions(bool in3D) {
    auto& F = fluidSet;
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##fluidpreset", kFluidPresets[std::clamp(F.preset, 0, kFluidPresetCount - 1)].name, ImGuiComboFlags_HeightLarge)) {
        for (int i = 0; i < kFluidPresetCount; ++i) {
            if (ImGui::Selectable(kFluidPresets[i].name, i == F.preset)) ApplyFluidPreset(i);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", kFluidPresets[i].help);
        }
        ImGui::EndCombo();
    }
    ui::HelpTooltip("Préréglage : le matériau de la peinture (couleur, rugosité, métal, relief, émissif)\net sa façon de couler.");
    DocView* v = Active();
    const bool project = v && v->doc->kind == 1 && v->mesh;
    // State of the paint.
    if (fluidRun.active && fluidRun.view == v) {
        ImGui::TextColored(ImVec4(0.47f, 0.78f, 1.0f, 1), "Peinture humide : %.1f s", v->renderer.FluidTime());
        ImGui::SameLine();
        ImGui::TextDisabled("(%d sous-pas)", fluidRun.substeps);
        if (ImGui::Button(F.paused ? "Reprendre" : "Pause")) F.paused = !F.paused;
        ImGui::SameLine();
        if (ImGui::Button("Figer (Entrée)")) FluidFinish(true);
        ImGui::SameLine();
        if (ImGui::Button("Annuler (Échap)")) FluidFinish(false);
    } else {
        ImGui::TextDisabled(project ? "Peignez sur le modèle (ou sur ses UV) : la peinture coule." : "Peignez : la peinture coule selon l'inclinaison du support.");
    }
    ImGui::SeparatorText("Pinceau");
    ImGui::SliderFloat("Taille##fl", &brush.size, 1, 600, "%.0f px", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Charge", &F.coat, 0.2f, 4, "%.2f", ImGuiSliderFlags_Logarithmic);
    ui::HelpTooltip("Épaisseur de la couche posée par le pinceau, par rapport au film retenu :\nen dessous de 1 elle tient, au-dessus l'excédent coule (petites coulures tout le long).");
    ImGui::SliderFloat("Débit", &F.flow, 0, 20, "%.1f");
    ui::HelpTooltip("Peinture ajoutée par seconde en plus de la couche : un pinceau immobile fait une flaque\nqui coule en grosse goutte. Ctrl : enlève la peinture humide.");
    ImGui::SliderFloat("Dureté##fl", &F.hardness, 0, 1, "%.2f");
    if (project && !in3D) {
        ImGui::SeparatorText("Matériau de la peinture");
        auto& B = brush3D;
        auto row = [&](PbrChannel c, const char* label) {
            ImGui::PushID((int)c + 100);
            ImGui::Checkbox(label, &B.use[(int)c]);
            ImGui::SameLine(130 * dpi);
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::BeginDisabled(!B.use[(int)c]);
            switch (c) {
                case PbrChannel::BaseColor: ImGui::ColorEdit3("##v", &fg.x); break;
                case PbrChannel::Roughness: ImGui::SliderFloat("##v", &B.rough, 0, 1, "%.2f"); break;
                case PbrChannel::Metallic: ImGui::SliderFloat("##v", &B.metal, 0, 1, "%.2f"); break;
                case PbrChannel::Height: ImGui::SliderFloat("##v", &B.height, 0, 1, "%.2f (base)"); break;
                case PbrChannel::Emissive: ImGui::ColorEdit3("##v", &B.emissive.x); break;
                default: break;
            }
            ImGui::EndDisabled();
            ImGui::PopID();
        };
        row(PbrChannel::BaseColor, "Couleur");
        row(PbrChannel::Roughness, "Rugosité");
        row(PbrChannel::Metallic, "Métal");
        row(PbrChannel::Height, "Relief");
        row(PbrChannel::Emissive, "Émissif");
        ImGui::TextDisabled("Tous les canaux cochés sont peints ensemble ; l'épaisseur donne le relief.");
    } else if (!project) {
        ImGui::SeparatorText("Couleur");
        ImGui::ColorEdit3("Peinture", &fg.x);
    }
    ImGui::SeparatorText("Gravité");
    if (project) {
        ImGui::Combo("Direction", &F.gravMode, "Monde (vers le bas)\0Bas de la vue\0Personnalisée\0");
        ui::HelpTooltip("Monde : la peinture descend le long du modèle.\nBas de la vue : tournez la caméra pour choisir où elle coule (même en cours de route).\n"
                        "Personnalisée : comme si le modèle était tourné.");
        if (F.gravMode == 2) {
            ImGui::SliderFloat("Inclinaison", &F.tilt, 0, 180, "%.0f°");
            ui::HelpTooltip("Écart avec la verticale : 0 tout droit vers le bas, 90 à l'horizontale (un plan posé à plat coule alors), 180 vers le haut.");
            ImGui::SliderFloat("Orientation", &F.azimuth, -180, 180, "%.0f°");
        }
    } else {
        ImGui::SliderFloat("Inclinaison du support", &F.tilt2D, 0, 180, "%.0f°");
        ui::HelpTooltip("0 : à plat sur une table (la peinture s'étale), 90 : un mur (elle coule),\n180 : un plafond (elle goutte et tombe).");
        ImGui::SliderFloat("Sens de la pente", &F.dir2D, -180, 180, "%.0f°");
        ui::HelpTooltip("0 : vers le bas de l'image, 90 : vers la gauche, -90 : vers la droite.");
        ImGui::SameLine();
        // Small dial: the direction the paint runs.
        ImVec2 c = ImGui::GetCursorScreenPos();
        const float r = ImGui::GetFrameHeight() * 0.5f;
        ImGui::Dummy(ImVec2(r * 2, r * 2));
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 m(c.x + r, c.y + r);
        dl->AddCircle(m, r, ui::col::Border);
        const float slope = std::sin(F.tilt2D * kDeg);
        dl->AddLine(m, ImVec2(m.x - std::sin(F.dir2D * kDeg) * r * slope, m.y + std::cos(F.dir2D * kDeg) * r * slope), ui::col::Accent, 2);
        if (ImGui::Button("Mur")) { F.tilt2D = 90; F.dir2D = 0; }
        ImGui::SameLine();
        if (ImGui::Button("Incliné 45°")) { F.tilt2D = 45; F.dir2D = 0; }
        ImGui::SameLine();
        if (ImGui::Button("À plat")) F.tilt2D = 0;
        ImGui::SameLine();
        if (ImGui::Button("Plafond")) F.tilt2D = 180;
    }
    ImGui::SliderFloat("Force", &F.gravity, 0, 3, "%.2f");
    ImGui::SeparatorText("Liquide");
    ImGui::SliderFloat("Viscosité", &F.viscosity, 0.1f, 6, "%.2f", ImGuiSliderFlags_Logarithmic);
    ui::HelpTooltip("Élevée : coule lentement (miel, cire) ; faible : vite (encre, eau).");
    ImGui::SliderFloat("Film retenu", &F.retention, 0.02f, 2, "%.2f", ImGuiSliderFlags_Logarithmic);
    ui::HelpTooltip("Épaisseur qui reste accrochée : faible, les coulures vont loin et sont fines ;\nélevée, la peinture tient et seules les grosses gouttes coulent.");
    ImGui::SliderFloat("Étalement", &F.spread, 0, 1.5f, "%.2f");
    ui::HelpTooltip("La peinture se nivelle et s'élargit (pression) ; 0 : coulures étroites.");
    ImGui::SliderFloat("Cohésion", &F.cohesion, 0, 2, "%.2f");
    ui::HelpTooltip("Le liquide en mouvement se regroupe : des filets étroits plutôt qu'une nappe.");
    ImGui::SliderFloat("Prise", &F.setTime, 0.5f, 60, "%.1f s", ImGuiSliderFlags_Logarithmic);
    ui::HelpTooltip("Temps avant que la peinture ne coule plus (puis elle est figée et l'historique la garde).");
    ImGui::SliderFloat("Irrégularité", &F.noise, 0, 1, "%.2f");
    ImGui::SliderFloat("Échelle des irrégularités", &F.noiseScale, 1, 40, "%.1f", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Tache le support", &F.absorb, 0, 1, "%.2f");
    ui::HelpTooltip("Part de la peinture absorbée (encre, rouille) : les traînées restent même quand le liquide est passé.");
    if (project) {
        ImGui::SliderFloat("Suit le relief", &F.bed, 0, 2, "%.2f");
        ui::HelpTooltip("Le relief existant (canal Hauteur) détourne la peinture : elle contourne les bosses et remplit les creux.");
        ImGui::SliderFloat("Gouttes qui tombent", &F.dripOff, 0, 2, "%.2f");
        ui::HelpTooltip("Sous les surfaces tournées vers le bas, la peinture épaisse se détache.");
    } else {
        if (F.tilt2D > 95) ImGui::SliderFloat("Gouttes qui tombent", &F.dripOff, 0, 2, "%.2f");
    }
    ImGui::Checkbox("S'écoule hors des bords", &F.offEdges);
    ui::HelpTooltip(project ? "Coché : la peinture quitte les îles UV par leurs bords ; sinon elle s'y accumule."
                            : "Coché : la peinture quitte l'image (ou la sélection) ; sinon elle s'accumule au bord.");
    ImGui::SeparatorText("Aspect");
    ImGui::SliderFloat("Couvrance", &F.density, 0.3f, 10, "%.1f", ImGuiSliderFlags_Logarithmic);
    ui::HelpTooltip("Opacité de la peinture selon son épaisseur.");
    if (project) ImGui::SliderFloat("Relief (épaisseur)", &F.relief, 0, 4, "%.2f");
    ImGui::SliderFloat("Brillance humide", &F.wetGloss, 0, 1, "%.2f");
    ui::HelpTooltip("Tant qu'elle est humide, la peinture est plus brillante (rugosité basse) et un peu plus sombre.");
    ImGui::SeparatorText("Simulation");
    ImGui::SliderFloat("Vitesse du temps", &F.timeScale, 0.1f, 4, "%.2f x", ImGuiSliderFlags_Logarithmic);
    ImGui::Combo("Résolution", &F.simRes, "Auto (2048 max.)\0Document\0Moitié\0Quart\0");
    ui::HelpTooltip("Taille de la grille de simulation (prise en compte au prochain trait).");
}

}  // namespace sx
