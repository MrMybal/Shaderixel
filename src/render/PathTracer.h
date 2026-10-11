#pragma once
// Path tracer for the Render mode: a scene of meshes (the 3D project's mesh with
// its painted channels, backdrops, primitives, imported models), lights and an
// HDRI, rendered progressively on the GPU (compute shader, any vendor: the
// geometry and BVH live in float textures) or on the CPU (all cores), same
// integrator: GGX + Lambert + dielectric, next-event estimation with MIS.
#include "render/PostProcess.h"
#include "mesh/Mesh.h"
#include "gpu/Device.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace sx {

class Renderer;

struct PtMaterial {
    float3 baseColor = {0.8f, 0.8f, 0.8f};
    float roughness = 0.5f, metallic = 0.0f;
    float3 emission = {0, 0, 0};
    float emissionStrength = 0.0f;
    float transmission = 0.0f, ior = 1.45f;
    float coat = 0.0f, coatRoughness = 0.05f;  // clear varnish over the base (glaze, lacquer, varnished wood)
    bool useProject = false;  // base color / roughness / metal / emissive / normal from its texture set
    int texSet = 0;           // runtime: atlas tile (0 = the open project, then imported .sx3d / model textures)
    bool backdrop = false;    // runtime: floor / backdrop (left out of the coverage mask pass)
};

enum class PtObjectType : int { Project, Plane, Backdrop, Cube, Sphere, File, Sx3d };

struct PtObject {
    std::string name;
    PtObjectType type = PtObjectType::Plane;
    std::string path;           // File
    float3 position, rotation;  // rotation: degrees (yaw, pitch, roll)
    float scale = 1.0f;
    bool visible = true;
    PtMaterial material;
    std::vector<PtMaterial> parts;   // imported models: one material per part (mesh triMaterial)
    std::shared_ptr<MeshData> mesh;  // geometry (object space)
    bool UsesParts() const { return !parts.empty() && mesh && mesh->triMaterial.size() * 3 == mesh->indices.size(); }
};

enum class PtLightType : int { Point, Spot, Area, Sun };

struct PtLight {
    std::string name;
    PtLightType type = PtLightType::Area;
    float3 position = {2, 3, -2};
    float yaw = 45, pitch = -45;  // direction (degrees)
    float3 color = {1, 1, 1};
    float intensity = 20.0f;      // W-ish (point / spot / area), irradiance for the sun
    float size = 1.0f;            // area: side, point / spot: radius, sun: angular diameter (degrees)
    float spotAngle = 40.0f;      // degrees (full cone)
    bool visible = true;
    float3 Direction() const;
};

struct PtSettings {
    int device = 0;            // 0 GPU, 1 CPU
    int maxSamples = 1024;
    int maxBounces = 8;
    bool viewSize = true;      // render at the view's size; otherwise width x height
    int width = 1920, height = 1080;
    float exposure = 0.0f;     // stops
    int tonemap = 3;           // 0 linear, 1 ACES, 2 Reinhard, 3 Khronos PBR Neutral, 4 AgX
    float aperture = 0.0f;     // lens radius (scene units), 0 = pinhole
    float focusDistance = 2.8f;
    float fov = 0.8f;          // vertical, radians
    float clampIndirect = 20.0f;
    std::string hdri = "*studio";  // equirectangular .hdr / image, "*studio" built-in, "" = background color + no env light
    float envIntensity = 1.0f, envRotation = 0.0f;  // rotation: degrees
    bool envVisible = true;
    float3 background = {0.05f, 0.05f, 0.06f};
    PostSettings post;         // camera effects (exposure and tone curve come from the fields above)
    float denoise = 0.6f;      // denoiser strength (0 off)
};

struct PtCamera {
    float3 position, target;
    float3 up = {0, 1, 0};
};

// Flattened scene: world-space triangles (BVH order), materials, lights, environment.
struct PtTriangle {
    float3 p[3], n[3];
    float2 uv[3];
    int material = 0;
};
struct PtNode {
    float3 bmin, bmax;
    u32 leftFirst = 0, count = 0;  // count 0: interior (children leftFirst, leftFirst + 1)
};
struct PtEnvironment {
    int width = 0, height = 0;
    std::vector<float4> pixels;  // linear RGB
    std::vector<float> conditional, marginal;  // CDFs (row-wise, then over rows)
    float total = 0;             // sum of luminance * sin(theta)
};

struct PtScene {
    std::vector<PtTriangle> triangles;
    std::vector<PtNode> nodes;
    std::vector<PtMaterial> materials;
    std::vector<PtLight> lights;
    std::shared_ptr<PtEnvironment> env;
    u64 hash = 0;
};

// Builds the flattened scene (object transforms, BVH with binned SAH).
// tiles[set] = atlas placement of texture set `set` (offset xy, scale zw): UVs of textured materials are remapped.
void BuildPtScene(const std::vector<PtObject>& objects, const std::vector<PtLight>& lights, PtScene& out, const std::vector<float4>* tiles = nullptr);
// Materials in scene order (one per object, or one per part): triangle material indices refer to this list.
std::vector<PtMaterial> SceneMaterials(const std::vector<PtObject>& objects);
// Loads an equirectangular image and its importance sampling tables.
std::shared_ptr<PtEnvironment> LoadPtEnvironment(const std::string& path, std::string* error);
// Importance sampling tables of an environment (after filling its pixels).
void BuildEnvTables(PtEnvironment& env);
// Built-in studio environment (soft boxes over a gray gradient), used when no HDRI file is chosen.
std::shared_ptr<PtEnvironment> MakeStudioEnvironment();
// Built-in environments ("*studio", "*couvert", "*coucher", "*soleil", "*neons"): id and label.
const std::vector<std::pair<std::string, std::string>>& ProceduralEnvironments();
std::shared_ptr<PtEnvironment> MakeProceduralEnvironment(const std::string& id);  // null: unknown id
// Primitive meshes (object space).
MeshData MakeBackdrop();  // floor curving up into a wall (cyclorama)
MeshData MakeFloorPlane(float half = 6.0f);

// Project textures sampled by "useProject" materials (CPU copies for the CPU renderer).
struct PtImage {
    int width = 0, height = 0;
    std::vector<float4> pixels;
    float4 Sample(float2 uv) const;  // bilinear, wrap
};
struct PtProjectTextures {
    PtImage base, orm, emissive, normal;  // orm: roughness, metal, occlusion
};

// CPU integrator: renders samples progressively into an accumulation buffer (sum of radiance).
class PtCpuRenderer {
public:
    ~PtCpuRenderer() { Stop(); }
    void Start(std::shared_ptr<const PtScene> scene, std::shared_ptr<const PtProjectTextures> tex, const PtSettings& s, const PtCamera& cam, int w, int h);
    void Stop();
    bool Running() const { return running; }
    int Samples() const { return samples.load(); }
    void SetPaused(bool p) { paused = p; }
    void SetMaxSamples(int n) { maxSamples = n; }
    // Copies the current average (linear) into `out` (w * h); false when nothing new.
    bool Fetch(std::vector<float4>& out, int& samplesOut);
private:
    void Loop();
    std::shared_ptr<const PtScene> scene;
    std::shared_ptr<const PtProjectTextures> tex;
    PtSettings settings;
    PtCamera camera;
    int width = 0, height = 0;
    std::vector<float4> accum;
    std::mutex mutex;
    std::thread thread;
    std::atomic<bool> running{false}, stop{false}, paused{false};
    std::atomic<int> maxSamples{1024};
    std::atomic<int> samples{0};
    int fetched = -1;
};

// Shading shared by the CPU renderer (the GPU kernel mirrors it).
float3 PtTrace(const PtScene& scene, const PtProjectTextures* tex, const PtSettings& s, float3 origin, float3 dir, u32& rng);

// GPU path tracer (shaders/pathtrace.slang): progressive accumulation, a band of rows per call
// (keeps each submission short: no driver timeout on slow GPUs).
class PtGpuRenderer {
public:
    void SetScene(std::shared_ptr<const PtScene> s);
    void Reset() { samples = 0; row = 0; }
    // Texture atlas (base, ORM, emission, normal) of `size` pixels; then each set packed into its tile
    // from PBR channels (PbrChannel order, null = absent).
    void BeginAtlas(u32 size);
    void PackTile(gpu::CommandContext& ctx, gpu::Texture* const* channels, u32 x, u32 y, u32 tile, float defRough = 0.5f, float defMetal = 0.0f);
    // Traces `rows` rows of the current pass; false when the kernel is unavailable.
    bool Render(gpu::CommandContext& ctx, const std::vector<PtMaterial>& materials, const std::vector<PtLight>& lights, const PtSettings& s,
                const PtCamera& cam, u32 w, u32 h, u32 rows);
    // Linear average (CPU renderer) into the accumulation, for display / export.
    void UploadAverage(gpu::CommandContext& ctx, const std::vector<float4>& pixels, u32 w, u32 h);
    gpu::Texture* Tonemap(gpu::CommandContext& ctx, const PtSettings& s);
    int Samples() const { return samples; }
    u32 Row() const { return row; }
    gpu::TexturePtr accum, view, packed[4];  // packed: base, ORM, emission, normal
    gpu::TexturePtr guideA, guideN;          // first-hit albedo (a: hits) / normal + distance sums (denoiser guides, passes)
    // Render pass image (RGBA8) from the guides: 0 albedo, 1 normal (world), 2 depth (white near, black at `farDist`), 3 mask.
    gpu::Texture* Pass(gpu::CommandContext& ctx, int mode, float farDist);
private:
    void EnsureAccum(u32 w, u32 h);
    std::shared_ptr<const PtScene> scene;
    gpu::TexturePtr geo, envTex, envCdf;
    gpu::TexturePtr passTex[4];
    bool geoDirty = true;
    u32 nodeBase = 0;
    int samples = 0;
    u32 row = 0;
};

// Render mode state of a 3D project (scene, camera, renderers).
struct PtRenderState {
    std::vector<PtObject> objects;
    std::vector<PtLight> lights;
    PtSettings settings;
    float yaw = 0.6f, pitch = 0.22f, distance = 3.4f;
    float3 target;
    int selObject = 0, selLight = -1;
    bool paused = false;
    bool texDirty = true;             // repack the project channels
    std::string atlasKey;             // texture sets packed in the atlas
    std::vector<float4> tiles;        // atlas placement per texture set
    std::vector<gpu::TexturePtr> setTextures;  // imported model textures (alive while packed)
    std::unordered_map<std::string, std::shared_ptr<void>> hiddenProjects;  // imported .sx3d (DocView), by path
    int selPart = 0;
    std::string geometryKey, shadingKey;
    std::shared_ptr<PtScene> scene;
    std::shared_ptr<PtEnvironment> env;
    std::string envPath;              // environment loaded in `env`
    PtGpuRenderer gpu;
    PtCpuRenderer cpu;
    PostChain post;                   // display image of the accumulation (bloom, grading...)
    gpu::Texture* display = nullptr;  // last post-processed image (PNG export)
    u64 paintKey = 0;
    gpu::Texture* denoiseDepth = nullptr;  // 3D view: depth of the raster pass (same camera) guiding the denoiser                 // 3D view ray tracing: document state the packed textures belong to
    std::shared_ptr<PtProjectTextures> cpuTex;
    std::vector<gpu::ReadbackPtr> texReadbacks;
    std::vector<float4> cpuPixels;
    int cpuShown = -1;
    u32 width = 0, height = 0;        // current render size
    u32 rowsPerFrame = 32;
    double startTime = 0, doneTime = 0;
    int lastFrame = -10;              // ImGui frame of the last update (tab shown again: repack)
    bool cpuStarted = false;
    u32 projectRevision = ~0u;        // mesh revision copied into the project object
    gpu::ReadbackPtr hdrReadback;     // pending .hdr export
    std::string hdrPath;
    std::string message;              // last error (environment, import)
    PtCamera Camera() const;
};

}  // namespace sx
