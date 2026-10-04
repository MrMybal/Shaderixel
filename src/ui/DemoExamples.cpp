// Example Paint 3D projects for the package (demo "example3dx", SX_KIND = tonneau | vase | coffre):
// a procedural mesh without UVs (unwrapped by xatlas at creation), a base paint layer computed per
// texel (color, roughness, metal, relief), material layers with computed masks, a render scene,
// saved to SX_OUT.
#include "ui/App.h"

#include <cmath>
#include <functional>

namespace sx {

namespace {

float Hash1(float n) { return std::fmod(std::abs(std::sin(n * 12.9898f) * 43758.5453f), 1.0f); }
float Hash2(int x, int y) { return Hash1((float)(x * 157 + y * 113) + 0.5f); }
float Smooth(float t) { return t * t * (3 - 2 * t); }
// Value noise and a 3-octave sum (0..1).
float Noise(float x, float y) {
    int xi = (int)std::floor(x), yi = (int)std::floor(y);
    float fx = Smooth(x - xi), fy = Smooth(y - yi);
    float a = Hash2(xi, yi), b = Hash2(xi + 1, yi), c = Hash2(xi, yi + 1), d = Hash2(xi + 1, yi + 1);
    return (a + (b - a) * fx) * (1 - fy) + (c + (d - c) * fx) * fy;
}
float Fbm(float x, float y) { return Noise(x, y) * 0.55f + Noise(x * 2.1f, y * 2.1f) * 0.3f + Noise(x * 4.3f, y * 4.3f) * 0.15f; }
float Clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }
float3 Mix(float3 a, float3 b, float t) { return a + (b - a) * t; }
// 1 inside |d| < half, soft edge of `soft`.
float Band(float d, float half, float soft) { return Clamp01((half - std::abs(d)) / soft + 0.5f); }

struct Texel {
    float3 color;
    float rough = 0.6f, metal = 0, height = 0.5f;
    float masks[2] = {0, 0};  // material layer masks
};

// Wood: streaks along `along`, rings across; tone varies per plank.
float3 Wood(float along, float across, float plank, float& grainOut) {
    float g = Fbm(along * 1.5f + plank * 7.3f, across * 26.0f + plank * 3.1f);
    float rings = 0.5f + 0.5f * std::sin((across * 40.0f + g * 6.0f) * 1.3f);
    grainOut = g * 0.6f + rings * 0.4f;
    float tone = 0.82f + 0.36f * Hash1(plank * 3.7f + 1.0f);
    return Mix(float3{0.25f, 0.13f, 0.06f}, float3{0.55f, 0.34f, 0.17f}, grainOut) * tone;
}

}  // namespace

void App::DemoExample3D() {
    char kindBuf[32] = "", meshFile[512] = "", out[512] = "";
    GetEnvironmentVariableA("SX_KIND", kindBuf, sizeof(kindBuf));
    GetEnvironmentVariableA("SX_MESHFILE", meshFile, sizeof(meshFile));
    GetEnvironmentVariableA("SX_OUT", out, sizeof(out));
    const std::string kind = kindBuf;
    auto presetIndex = [](const char* name) {
        for (int i = 0; i < MaterialPresetCount(); ++i)
            if (std::string(MaterialPresetName(i)) == name) return i;
        return 0;
    };
    // Original (file) frame of each model: height and center, to undo NormalizeMesh.
    float origH = 1.0f;
    float3 origC = {0, 0.5f, 0};
    const char* layerNames[2] = {"", ""};
    const char* presets[2] = {"", ""};
    if (kind == "tonneau") { origH = 1.1f; origC = {0, 0.55f, 0}; layerNames[0] = "Cerclages"; presets[0] = "Fer brut"; layerNames[1] = "Rouille"; presets[1] = "Rouille"; }
    if (kind == "vase") { origH = 1.0f; origC = {0, 0.5f, 0}; layerNames[0] = "Or (lèvre)"; presets[0] = "Or"; }
    if (kind == "coffre") { origH = 0.94f; origC = {0, 0.47f, -0.01f}; layerNames[0] = "Ferrures"; presets[0] = "Or"; }

    if (frameCounter == 4) {
        p3dNew = {};
        p3dNew.mesh = 3;
        p3dNew.meshPath = meshFile;
        p3dNew.basePreset = presetIndex(kind == "vase" ? "Plastique blanc" : "Bois");
        NewPaint3DProject();
    }
    DocView* a = Active();
    if (!a || !a->mesh || a->doc->kind != 1) return;
    Document& d = *a->doc;
    MeshScene& s = *a->mesh;
    if (frameCounter == 8) ImGui::SetWindowFocus("Vue 3D");
    if (frameCounter != 12 && frameCounter != 20 && frameCounter != 24) return;

    float yMin = 1e9f, yMax = -1e9f;
    for (auto& v : s.data.vertices) { yMin = std::min(yMin, v.pos.y); yMax = std::max(yMax, v.pos.y); }
    const float k = origH / std::max(yMax - yMin, 1e-6f);
    const float3 center = {0, (yMin + yMax) * 0.5f, 0};
    auto toOrig = [&](float3 p) { return (p - center) * k + origC; };

    if (frameCounter == 12) {
        // Per texel shading (UV-space rasterization of every triangle with its 3D position).
        const int W = (int)d.Width();
        std::vector<Texel> img((size_t)W * W);
        std::vector<u8> covered((size_t)W * W, 0);
        std::function<Texel(float3, float3)> shade;
        const float PI = 3.14159265f;

        if (kind == "tonneau") {
            static constexpr float H = 1.1f;
            shade = [&](float3 o, float3 n) {
                Texel t;
                float r = std::sqrt(o.x * o.x + o.z * o.z);
                float g;
                if (std::abs(n.y) > 0.6f && r < 0.334f) {
                    // Heads: planks along z.
                    float u = (o.x + 0.4f) / 0.095f, plank = std::floor(u);
                    t.color = Wood(o.z * 3.0f, o.x, plank, g);
                    float gap = std::min(u - plank, plank + 1 - u);
                    t.height = 0.5f + g * 0.02f - Band(gap, 0.0f, 0.06f) * 0.08f;
                    t.color = t.color * (1.0f - Band(gap, 0.0f, 0.06f) * 0.6f);
                    t.rough = 0.8f;
                    return t;
                }
                float ang = std::atan2(o.z, o.x);
                float f = (ang / (2 * PI) + 0.5f) * 24.0f, stave = std::floor(f);
                float edge = std::min(f - stave, stave + 1 - f);
                if (std::abs(n.y) > 0.6f || r < 0.34f) {
                    // Stave ends (chimes): end grain, lighter.
                    t.color = Mix(float3{0.38f, 0.24f, 0.12f}, float3{0.6f, 0.42f, 0.24f}, Fbm(f * 3, r * 60)) ;
                    t.rough = 0.85f;
                    return t;
                }
                t.color = Wood(o.y * 2.0f, f / 24.0f * 2.0f, stave, g);
                float groove = Band(edge, 0.0f, 0.07f);
                t.color = t.color * (1.0f - groove * 0.65f);
                t.height = 0.5f + g * 0.025f - groove * 0.09f;
                t.rough = 0.72f - g * 0.1f;
                // Iron hoops (material 0) and rust patches on them (material 1).
                float y = o.y / H;
                float hoop = 0;
                for (float c : {0.09f, 0.27f, 0.73f, 0.91f}) hoop = std::max(hoop, Band(y - c, 0.026f, 0.006f));
                t.masks[0] = hoop;
                float rust = Clamp01((Fbm(f * 0.9f, o.y * 9.0f) - 0.56f) / 0.1f) * 0.85f;
                t.masks[1] = hoop * rust;
                if (hoop > 0.5f) t.height = 0.5f;
                return t;
            };
        } else if (kind == "vase") {
            // Greek key unit (6 x 6, top row first): terracotta lines on the black band.
            static const char* key[6] = {"#####.", "#...#.", "#.#.#.", "#.###.", "#.....", "######"};
            shade = [&](float3 o, float3 n) {
                Texel t;
                const float3 terra = {0.74f, 0.37f, 0.17f}, black = {0.035f, 0.03f, 0.028f};
                float y = o.y, r = std::sqrt(o.x * o.x + o.z * o.z);
                float ang = std::atan2(o.z, o.x), u = ang / (2 * PI) + 0.5f;
                bool inside = y > 0.8f && (n.x * o.x + n.z * o.z) < -0.2f * r;
                // Body radius profile (same control points as the mesh), to tell the handles apart.
                auto bodyR = [](float yy) {
                    static const float P[][2] = {{0.07f, 0.12f}, {0.15f, 0.22f}, {0.30f, 0.33f}, {0.45f, 0.37f}, {0.60f, 0.33f},
                                                 {0.72f, 0.22f}, {0.80f, 0.12f}, {0.88f, 0.10f}, {0.95f, 0.12f}, {1.0f, 0.165f}};
                    if (yy <= P[0][0]) return 0.17f;
                    for (int i = 0; i < 9; ++i)
                        if (yy <= P[i + 1][0]) {
                            float tt = (yy - P[i][0]) / (P[i + 1][0] - P[i][0]);
                            return P[i][1] + (P[i + 1][1] - P[i][1]) * tt;
                        }
                    return 0.165f;
                };
                bool handle = r > bodyR(y) * 1.12f + 0.01f;
                float glaze = 0;  // 1 = black glaze
                if (handle || y < 0.07f || y > 0.95f) glaze = 1;
                else if (y < 0.14f) {
                    // Rays pointing up.
                    float fr = u * 28.0f - std::floor(u * 28.0f), h = (y - 0.07f) / 0.07f;
                    glaze = std::abs(fr - 0.5f) < 0.5f * (1.0f - h) ? 1.0f : 0.0f;
                } else if (y < 0.20f) glaze = 1;
                else if (y < 0.215f) glaze = 0;
                else if (y < 0.23f) glaze = 1;
                else if (y < 0.40f) glaze = y < 0.33f + 0.045f * std::sin(u * 2 * PI * 9) ? 1.0f : 0.0f;  // running wave
                else if (y < 0.55f) {
                    // Row of dots.
                    float dx = (u * 18.0f - std::floor(u * 18.0f) - 0.5f) * (2 * PI * 0.37f / 18.0f);  // arc length from the cell center
                    float dy = y - 0.475f;
                    glaze = dx * dx + dy * dy < 0.022f * 0.022f ? 1.0f : 0.0f;
                } else if (y < 0.575f) glaze = 1;
                else if (y < 0.725f) {
                    // Meander band: black ground, terracotta key.
                    float cu = u * 16.0f * 6.0f, cv = (0.725f - y) / 0.15f * 6.0f;
                    int col = ((int)std::floor(cu)) % 6, row = std::clamp((int)std::floor(cv), 0, 5);
                    glaze = key[row][col] == '#' ? 0.0f : 1.0f;
                } else if (y < 0.80f) glaze = 1;
                else if (y < 0.86f) glaze = 0;
                else if (y < 0.87f) glaze = 1;
                else if (y < 0.93f) glaze = 0;
                else glaze = 1;
                if (inside) glaze = 0;
                float clay = Fbm(u * 40.0f, y * 30.0f);
                t.color = Mix(terra * (0.9f + clay * 0.2f), black, glaze);
                if (inside) t.color = terra * 0.45f;
                t.rough = glaze > 0.5f ? 0.22f : 0.68f;
                t.height = 0.5f + glaze * 0.012f + clay * 0.01f;
                // Gold on the lip.
                t.masks[0] = !handle && y > 0.975f ? 1.0f : 0.0f;
                return t;
            };
        } else {  // coffre
            shade = [&](float3 o, float3 n) {
                Texel t;
                float g;
                const bool lock = o.z < -0.384f && std::abs(o.x) < 0.075f && o.y > 0.418f && o.y < 0.602f;
                const bool lid = o.y > 0.553f && !lock;
                const bool cap = lid && std::abs(n.x) > 0.8f;
                if (cap) {
                    float rr = std::sqrt((o.y - 0.55f) * (o.y - 0.55f) + o.z * o.z);
                    t.color = Wood(rr * 3.0f, o.z, 40, g) * 0.9f;
                    t.masks[0] = Clamp01((rr - 0.345f) / 0.006f);
                } else if (lid) {
                    float th = std::atan2(o.y - 0.55f, o.z), f = th / (3.14159265f / 7.0f), plank = std::floor(f);
                    t.color = Wood(o.x * 1.2f, th * 0.39f, plank + 10, g);
                    float groove = Band(std::min(f - plank, plank + 1 - f), 0.0f, 0.06f);
                    t.color = t.color * (1.0f - groove * 0.6f);
                    t.height = 0.5f + g * 0.02f - groove * 0.08f;
                    float strap = std::max(Band(o.x - 0.40f, 0.035f, 0.004f), Band(o.x + 0.40f, 0.035f, 0.004f));
                    float rim = o.y < 0.6f ? 1.0f : 0.0f;
                    float ends = Band(std::abs(o.x) - 0.6f, 0.035f, 0.004f);
                    t.masks[0] = std::max({strap, rim, ends});
                } else {
                    // Body: horizontal planks.
                    float f = o.y / 0.1375f, plank = std::floor(f);
                    float along = std::abs(n.z) > 0.7f ? o.x : o.z;
                    t.color = Wood(along * 1.2f, o.y, plank + (std::abs(n.z) > 0.7f ? 0 : 20), g);
                    float groove = Band(std::min(f - plank, plank + 1 - f), 0.0f, 0.05f);
                    t.color = t.color * (1.0f - groove * 0.6f);
                    t.height = 0.5f + g * 0.02f - groove * 0.08f;
                    float strap = std::max(Band(o.x - 0.40f, 0.035f, 0.004f), Band(o.x + 0.40f, 0.035f, 0.004f));
                    float corner = (std::abs(o.x) > 0.555f && std::abs(o.z) > 0.335f) ? 1.0f : 0.0f;
                    float rims = (o.y < 0.05f || o.y > 0.505f) ? 1.0f : 0.0f;
                    t.masks[0] = std::max({strap * (std::abs(n.x) > 0.7f ? 0.0f : 1.0f), corner, rims});
                }
                t.rough = 0.62f - g * 0.08f;
                // Studs along the straps (relief under the gold).
                for (float sx : {-0.40f, 0.40f}) {
                    float along = lid ? std::atan2(o.y - 0.55f, o.z) * 0.39f : o.y;
                    float cell = along / 0.07f - std::floor(along / 0.07f) - 0.5f;
                    float dd = (o.x - sx) * (o.x - sx) + cell * cell * 0.07f * 0.07f;
                    if (!cap && dd < 0.012f * 0.012f) t.height = 0.5f + 0.25f * (1.0f - dd / (0.012f * 0.012f));
                }
                if (lock) {
                    t.masks[0] = 1;
                    float kx = o.x, ky = o.y - 0.535f;
                    bool hole = kx * kx + ky * ky < 0.016f * 0.016f || (std::abs(kx) < 0.007f && ky < 0 && ky > -0.045f);
                    if (hole) { t.masks[0] = 0; t.color = {0.01f, 0.01f, 0.01f}; t.height = 0.3f; t.rough = 0.9f; }
                    else t.height = 0.56f;
                }
                return t;
            };
        }

        const auto& V = s.data.vertices;
        const auto& I = s.data.indices;
        for (size_t ti = 0; ti + 2 < I.size(); ti += 3) {
            const MeshVertex &a0 = V[I[ti]], &a1 = V[I[ti + 1]], &a2 = V[I[ti + 2]];
            float2 p0 = {a0.uv.x * W, a0.uv.y * W}, p1 = {a1.uv.x * W, a1.uv.y * W}, p2 = {a2.uv.x * W, a2.uv.y * W};
            int x0 = std::max(0, (int)std::floor(std::min({p0.x, p1.x, p2.x})) - 2), x1 = std::min(W - 1, (int)std::ceil(std::max({p0.x, p1.x, p2.x})) + 2);
            int y0 = std::max(0, (int)std::floor(std::min({p0.y, p1.y, p2.y})) - 2), y1 = std::min(W - 1, (int)std::ceil(std::max({p0.y, p1.y, p2.y})) + 2);
            float den = (p1.y - p2.y) * (p0.x - p2.x) + (p2.x - p1.x) * (p0.y - p2.y);
            if (std::abs(den) < 1e-9f) continue;
            for (int y = y0; y <= y1; ++y)
                for (int x = x0; x <= x1; ++x) {
                    float px = x + 0.5f, py = y + 0.5f;
                    float l0 = ((p1.y - p2.y) * (px - p2.x) + (p2.x - p1.x) * (py - p2.y)) / den;
                    float l1 = ((p2.y - p0.y) * (px - p2.x) + (p0.x - p2.x) * (py - p2.y)) / den;
                    float l2 = 1 - l0 - l1;
                    if (l0 < -0.08f || l1 < -0.08f || l2 < -0.08f) continue;
                    const size_t i = (size_t)y * W + x;
                    const bool inTri = l0 >= 0 && l1 >= 0 && l2 >= 0;
                    if (covered[i] == 2 || (covered[i] == 1 && !inTri)) continue;  // texels inside a triangle win over padding
                    float3 pos = a0.pos * l0 + a1.pos * l1 + a2.pos * l2;
                    float3 nrm = Normalize(a0.normal * l0 + a1.normal * l1 + a2.normal * l2);
                    img[i] = shade(toOrig(pos), nrm);
                    covered[i] = inTri ? 2 : 1;
                }
        }
        // Base paint layer (every channel, full coverage where the mesh is).
        Id paint = AddPaintLayer3D(*a, kind == "vase" ? "Céramique peinte" : "Bois");
        auto upload = [&](PbrChannel c, auto value) {
            Layer* l = MaterialChannelLayer(d, paint, c);
            Surface* sf = l ? d.GetSurface(l->surface) : nullptr;
            if (!sf) return;
            a->renderer.Prepare(d, *ctx);
            std::vector<float> f((size_t)W * W * 4);
            for (size_t i = 0; i < img.size(); ++i) {
                float3 v = value(img[i]);
                float cov = covered[i] ? 1.0f : 0.0f;
                f[i * 4] = v.x * cov;
                f[i * 4 + 1] = v.y * cov;
                f[i * 4 + 2] = v.z * cov;
                f[i * 4 + 3] = cov;
            }
            if (sf->tex->GetFormat() == gpu::Format::RGBA32F) {
                ctx->Upload(sf->tex.get(), f.data(), W * 16);
            } else {
                std::vector<u16> h(f.size());
                for (size_t i = 0; i < f.size(); ++i) {
                    float x = f[i];
                    u32 b;
                    memcpy(&b, &x, 4);
                    int e = (int)((b >> 23) & 255) - 127 + 15;
                    h[i] = x <= 0 || e <= 0 ? 0 : e >= 31 ? 0x7BFF : (u16)((e << 10) | ((b >> 13) & 0x3FF));
                }
                ctx->Upload(sf->tex.get(), h.data(), W * 8);
            }
            d.CancelPendingClear(l->surface);
            d.TouchSurface(l->surface);
        };
        auto gray = [](float v) { float g = SrgbToLinear(Clamp01(v)); return float3{g, g, g}; };
        upload(PbrChannel::BaseColor, [](const Texel& t) { return float3{SrgbToLinear(t.color.x), SrgbToLinear(t.color.y), SrgbToLinear(t.color.z)}; });
        upload(PbrChannel::Roughness, [&](const Texel& t) { return gray(t.rough); });
        upload(PbrChannel::Metallic, [&](const Texel& t) { return gray(t.metal); });
        upload(PbrChannel::Height, [&](const Texel& t) { return gray(t.height); });
        // Material layers with their computed masks.
        for (int m = 0; m < 2; ++m) {
            if (!presets[m][0]) continue;
            AddMaterialLayer(*a, presetIndex(presets[m]));
            Id mat = materialSel;
            std::vector<float> mask((size_t)W * W);
            double sum = 0;
            for (size_t i = 0; i < img.size(); ++i) {
                mask[i] = covered[i] ? img[i].masks[m] : 0.0f;
                sum += mask[i];
            }
            Log("[demo] example %s: material %s, mask coverage %.2f %%", kind.c_str(), presets[m], 100.0 * sum / img.size());
            for (auto& l : d.S().layers)
                if (l.material == mat) {
                    l.name = layerNames[m];
                    if (l.mask) {
                        a->renderer.Prepare(d, *ctx);
                        WriteMaskData(d, l.mask, mask, W, W);
                        break;
                    }
                }
        }
        brush3D.use[(int)PbrChannel::Height] = false;
    }
    if (frameCounter == 20) {
        // 3D view camera and the render scene saved with the project.
        s.yaw = kind == "coffre" ? 0.55f : 0.6f;
        s.pitch = kind == "coffre" ? 0.35f : 0.25f;
        s.distance = 2.8f;
        a->render.reset();
        PtRenderState& r = RenderStateFor(*a);
        const float floor = yMin;
        r.settings.maxSamples = 1024;
        auto addObj = [&](PtObjectType type, const char* name, float3 pos, float3 rot, float scale) -> PtObject& {
            PtObject o;
            o.type = type;
            o.name = name;
            o.position = pos;
            o.rotation = rot;
            o.scale = scale;
            r.objects.push_back(o);
            return r.objects.back();
        };
        if (kind == "tonneau") {
            // A second barrel lying beside the first one.
            PtObject& o = addObj(PtObjectType::Project, "Tonneau couché", {1.35f, 0, 0.55f}, {35, 90, 0}, 1.0f);
            o.material.useProject = true;
            // Lying: its radius rests on the floor.
            float rad = 0;
            for (auto& v : s.data.vertices) rad = std::max(rad, std::sqrt(v.pos.x * v.pos.x + v.pos.z * v.pos.z));
            o.position.y = floor + rad;
            r.lights[0].intensity = 70;
            r.distance = 3.9f;
            r.target = {0.5f, -0.1f, 0.2f};
        } else if (kind == "vase") {
            // Darker backdrop, warm rim light from behind.
            r.objects[1].material.baseColor = {0.18f, 0.17f, 0.16f};
            PtLight rim;
            rim.name = "Contre-jour";
            rim.type = PtLightType::Spot;
            rim.position = {1.6f, 1.8f, 1.6f};
            rim.color = {1.0f, 0.78f, 0.55f};
            rim.intensity = 260;
            rim.size = 0.08f;
            rim.spotAngle = 45;
            float3 dd = Normalize(float3{0, 0, 0} - rim.position);
            rim.yaw = std::atan2(dd.x, dd.z) * 57.29578f;
            rim.pitch = std::asin(dd.y) * 57.29578f;
            r.lights.push_back(rim);
            r.distance = 3.3f;
        } else {
            // Gold beads in front of the chest.
            const float3 spots[] = {{-0.55f, 0, -0.95f}, {-0.35f, 0, -1.05f}, {-0.45f, 0, -1.25f}, {0.6f, 0, -1.0f}, {0.75f, 0, -1.2f}, {0.2f, 0, -1.3f}};
            for (const float3& p : spots) {
                PtObject& o = addObj(PtObjectType::Sphere, "Perle d'or", {p.x, floor + 0.07f, p.z}, {}, 0.07f);
                o.material.baseColor = {1.0f, 0.71f, 0.29f};
                o.material.metallic = 1;
                o.material.roughness = 0.18f;
            }
            r.distance = 3.4f;
            r.pitch = 0.3f;
            r.objects[0].material.coat = 0.6f;  // varnished wood
            r.objects[0].material.coatRoughness = 0.12f;
        }
        r.settings.focusDistance = r.distance;
        a->history.Clear();
    }
    if (frameCounter == 24 && out[0]) SaveDocument(*a, out);
}

}  // namespace sx
