#include "ui/App.h"
#include "render/Dispatch.h"
#include "ai/Net.h"
#include "core/Deflate.h"
#include <stb_image.h>

namespace sx {
namespace {
enum PathMode { Stroke, Ribbon, Eraser, Filled, PathModeCount };

// Store a portable stencil alongside the anchors. Changing the original image or moving
// the project will not change the appearance of an edited ribbon.
bool ImportRibbon(const std::string& file, Json& out) {
    FILE* fp = nullptr;
    if (_wfopen_s(&fp, Widen(file).c_str(), L"rb") || !fp) return false;
    int w = 0, h = 0, channels = 0;
    auto image = stbi_load_from_file(fp, &w, &h, &channels, 4);
    fclose(fp);
    if (!image) return false;
    const float scale = std::min(1.f, 512.f / (float)std::max(w, h));
    const int nx = std::max(1, (int)std::round(w * scale)), ny = std::max(1, (int)std::round(h * scale));
    std::vector<u8> mask((size_t)nx * ny);
    for (int y = 0; y < ny; ++y)
        for (int x = 0; x < nx; ++x) {
            double sum = 0;
            const int x0 = (int)((int64_t)x * w / nx), x1 = (int)((int64_t)(x + 1) * w / nx);
            const int y0 = (int)((int64_t)y * h / ny), y1 = (int)((int64_t)(y + 1) * h / ny);
            for (int j = y0; j < y1; ++j)
                for (int i = x0; i < x1; ++i) {
                    const auto* p = image + 4 * ((size_t)j * w + i);
                    sum += (0.2126 * p[0] + 0.7152 * p[1] + 0.0722 * p[2]) * p[3] / 255.;
                }
            mask[(size_t)y * nx + x] = (u8)std::lround(sum / ((x1 - x0) * (double)(y1 - y0)));
        }
    stbi_image_free(image);
    const auto packed = deflate::ZlibCompress(mask.data(), mask.size());
    out["w"] = nx;
    out["h"] = ny;
    out["mask"] = net::Base64Encode(packed.data(), packed.size());
    return true;
}

gpu::TexturePtr RibbonTexture(gpu::CommandContext& ctx, const Json& stencil) {
    const int w = stencil["w"].I(), h = stencil["h"].I();
    if (w < 1 || h < 1 || w > 512 || h > 512) return {};
    auto compressed = net::Base64Decode(stencil["mask"].S());
    std::vector<u8> data((size_t)w * h);
    if (stbi_zlib_decode_buffer((char*)data.data(), (int)data.size(), (const char*)compressed.data(), (int)compressed.size()) !=
        (int)data.size())
        return {};
    auto tex = gpu::Dev().CreateTexture(w, h, gpu::Format::R8, "SurfaceRibbonStencil");
    if (tex) ctx.Upload(tex.get(), data.data(), w);
    return tex;
}

bool ReadAnchors(const Json& path, MeshScene& scene, std::vector<SurfaceAnchor>& points) {
    for (const Json& j : path["points"].Elements()) {
        if (!j.IsArray() || (j.Size() != 4 && j.Size() != 10)) return false;
        u32 tri = j.At(0).U();
        float3 b{j.At(1).F(), j.At(2).F(), j.At(3).F()};
        if (!std::isfinite(b.x + b.y + b.z) || std::abs(b.x + b.y + b.z - 1) > 0.002f) return false;
        bool exists = tri < scene.data.indices.size() / 3;
        if (j.Size() == 10) {
            // Repacking or unwrapping UVs can renumber triangles while preserving the physical surface.
            float3 pos{j.At(4).F(), j.At(5).F(), j.At(6).F()};
            if (!exists || Length(SurfacePathTopology::Position(scene.data, {tri, b}) - pos) > 0.0001f) {
                float3 n = Normalize({j.At(7).F(), j.At(8).F(), j.At(9).F()});
                float3 hit;
                if (!scene.Rays().Ray(pos + n * 0.001f, n * -1, 0.002f, &hit, nullptr, nullptr, &tri, &b) || Length(hit - pos) > 0.0002f)
                    return false;
                exists = true;
            }
        }
        if (!exists) return false;
        points.push_back({tri, b});
    }
    return true;
}
void WriteAnchors(Json& path, const std::vector<SurfaceAnchor>& points, const MeshData& mesh) {
    Json list = Json::Array();
    for (const auto& p : points) {
        Json j = Json::Array();
        j.Push(p.triangle);
        j.Push(p.weights.x);
        j.Push(p.weights.y);
        j.Push(p.weights.z);
        const float3 pos = SurfacePathTopology::Position(mesh, p);
        const auto* ix = &mesh.indices[(size_t)p.triangle * 3];
        const float3 n = Normalize(mesh.vertices[ix[0]].normal * p.weights.x + mesh.vertices[ix[1]].normal * p.weights.y +
                                   mesh.vertices[ix[2]].normal * p.weights.z);
        for (float f : {pos.x, pos.y, pos.z, n.x, n.y, n.z})
            j.Push(f);
        list.Push(std::move(j));
    }
    path["points"] = std::move(list);
}
} // namespace

bool App::ParticlePathOrigins(DocView& v, std::vector<ParticleOrigin>& origins) {
    auto& scene = *v.mesh;
    const auto* layer = v.doc->FindLayer(v.particlePathSource);
    Json path;
    std::vector<SurfaceAnchor> anchors;
    if (!layer || !Json::Parse(layer->surfacePath, path) || !ReadAnchors(path, scene, anchors) || anchors.empty()) return false;
    if (v.surfaceTools.pathMeshRevision != scene.revision) {
        v.surfaceTools.pathTopology.Build(scene.data);
        v.surfaceTools.pathMeshRevision = scene.revision;
    }
    std::vector<SurfaceLineSegment> segments;
    float length = 0;
    size_t count = std::max<size_t>(1, anchors.size() - 1);
    if (path["closed"].B() && anchors.size() > 2) ++count;
    for (size_t i = 0; i < count; ++i) {
        if (!v.surfaceTools.pathTopology.Connect(scene.data, anchors[i % anchors.size()], anchors[(i + 1) % anchors.size()], length, segments))
            return false;
    }
    const int samples = std::clamp((int)std::ceil(length / std::max(particleBrush.size, 0.002f)), 1, 2048);
    size_t segment = 0;
    for (int i = 0; i < samples && !segments.empty(); ++i) {
        const float at = length * (i + 0.5f) / samples;
        while (segment + 1 < segments.size() && at > segments[segment].b.w) ++segment;
        const auto& s = segments[segment];
        const float t = std::clamp((at - s.a.w) / std::max(1e-6f, s.b.w - s.a.w), 0.0f, 1.0f);
        const float3 a{s.a.x,s.a.y,s.a.z}, b{s.b.x,s.b.y,s.b.z}, n{s.normal.x,s.normal.y,s.normal.z};
        const float3 point = a * (1-t) + b * t;
        origins.push_back({point, n, point + n * 2, n * -1, particleBrush.size * 0.5f, true});
    }
    return !origins.empty();
}

bool App::WriteSurfacePath(DocView& v, Id material, const Json& path, const char* label) {
    auto& d = *v.doc;
    auto& scene = *v.mesh;
    auto& t = v.surfaceTools;
    if (!v.renderer.MeshPrepare(d, *ctx, scene)) return false;
    auto* maskKernel = Kernel("mesh_surface_path.slang");
    auto* channelKernel = Kernel("mesh_path_channel.slang");
    if (!maskKernel || !channelKernel) return false;
    std::vector<SurfaceAnchor> anchors;
    if (!ReadAnchors(path, scene, anchors) || anchors.empty()) return false;
    if (t.pathMeshRevision != scene.revision) {
        t.pathTopology.Build(scene.data);
        t.pathMeshRevision = scene.revision;
    }
    std::vector<SurfaceLineSegment> segments;
    std::vector<u32> owners;
    const int mode = std::clamp(path["mode"].I(), 0, (int)PathModeCount - 1);
    const bool fill = mode == Filled && anchors.size() >= 3;
    float length = 0;
    size_t links = anchors.size() == 1 ? 1 : anchors.size() - 1;
    if ((fill || path["closed"].B()) && anchors.size() > 2) ++links;
    for (size_t k = 0; k < links; ++k)
        if (!t.pathTopology.Connect(scene.data, anchors[k % anchors.size()], anchors[(k + 1) % anchors.size()], length, segments,
                                    fill ? &owners : nullptr)) {
            Status("Les points du tracé doivent appartenir à une pièce continue");
            return false;
        }
    const u32 W = d.Width(), H = d.Height();
    struct ChannelImage {
        PbrChannel ch;
        gpu::TexturePtr image;
        float4 color;
    };
    std::vector<ChannelImage> channels;
    for (int ci = 0; ci < (int)PbrChannel::Count; ++ci) {
        const auto ch = (PbrChannel)ci;
        const auto& color = path["colors"][PbrChannelSuffix(ch)];
        if (!color.IsArray() || color.Size() != 4 || !FindChannelGroup(d, ch)) continue;
        auto* existing = material ? MaterialChannelLayer(d, material, ch) : nullptr;
        if (material && (!existing || existing->locked)) return false;
        auto output = gpu::Dev().CreateTexture(W, H, SurfaceFormat(d.GetPrecision()), "SurfacePathChannel");
        if (!output) return false;
        channels.push_back({ch, std::move(output), Float4FromJson(color)});
    }
    if (channels.empty()) {
        Status("Cochez au moins un canal disponible pour le tracé");
        return false;
    }
    auto mask = gpu::Dev().CreateTexture(W, H, gpu::Format::R8, "SurfacePathCoverage");
    if (!mask) return false;
    const float radius = std::clamp(path["radius"].F(0.02f), 0.0001f, 1.0f);
    const float spacing = std::clamp(path["spacing"].F(), 0.0f, 2.0f);
    gpu::TexturePtr ribbon;
    if (mode == Ribbon && path["ribbon"].IsObject()) {
        ribbon = RibbonTexture(*ctx, path["ribbon"]);
        if (!ribbon) {
            Status("Impossible de lire le motif du ruban");
            return false;
        }
    }
    if (fill) {
        std::vector<u8> pixels;
        if (!t.pathTopology.Fill(scene.data, segments, owners, (int)W, (int)H, pixels)) {
            Status("Ce contour ne délimite pas de région fermée sur la surface");
            return false;
        }
        ctx->Upload(mask.get(), pixels.data(), W);
    } else
        for (size_t begin = 0; begin < segments.size(); begin += 128) {
            gpu::DispatchDesc draw;
            draw.pipeline = maskKernel;
            draw.srv[0] = scene.posMap.get();
            draw.srv[1] = scene.nrmMap.get();
            draw.srv[2] = ribbon.get();
            draw.uav[0] = mask.get();
            u32 count = (u32)std::min<size_t>(128, segments.size() - begin);
            struct {
                u32 w, h, count;
                float radius, spacing;
                u32 start, style, hasStencil;
                float length;
            } root{W, H, count, radius, spacing, begin == 0 ? 1u : 0u, (u32)mode, ribbon ? 1u : 0u, length};
            SetConstants(draw, root);
            draw.cbData = segments.data() + begin;
            draw.cbSize = count * sizeof(SurfaceLineSegment);
            Dispatch2D(*ctx, draw, W, H);
        }
    for (auto& channel : channels) {
        gpu::DispatchDesc draw;
        draw.pipeline = channelKernel;
        draw.srv[0] = mask.get();
        draw.uav[0] = channel.image.get();
        struct {
            u32 w, h, pad[2];
            float4 color;
        } root{W, H, {}, channel.color};
        SetConstants(draw, root);
        Dispatch2D(*ctx, draw, W, H);
    }
    Edit edit(d, v.history, label);
    const bool create = material == 0;
    if (create) material = d.NewId();
    Id active = 0;
    const auto settings = path.Dump(0);
    for (auto& channel : channels) {
        Layer* layer = MaterialChannelLayer(d, material, channel.ch);
        if (create) {
            Layer l;
            l.id = d.NewId();
            l.kind = LayerKind::Pixel;
            l.name = "Tracé de surface";
            l.material = material;
            l.parent = NewLayerParent3D(d, channel.ch, material);
            l.surface = d.NewId();
            d.S().surfaces[l.surface] = Surface{};
            auto& layers = d.S().layers;
            auto pos = layers.insert(layers.begin() + d.LayerIndex(l.parent), std::move(l));
            layer = &*pos;
        }
        layer->surfacePath = settings;
        if (mode == Eraser)
            layer->blend = BlendMode::Erase;
        else if (layer->blend == BlendMode::Erase)
            layer->blend = BlendMode::Normal;
        d.GetSurface(layer->surface)->tex = std::move(channel.image);
        d.TouchSurface(layer->surface);
        if (!active) active = layer->id;
    }
    d.S().activeLayer = active;
    d.S().editMask = false;
    d.SyncGraph();
    edit.Commit();
    materialSel = material;
    t.newPath = false;
    return true;
}

void App::SurfacePathOptions(DocView& v) {
    auto& t = v.surfaceTools;
    if (ImGui::SmallButton("Nouveau tracé")) {
        t.newPath = true;
        t.dragPoint = -1;
    }
    Layer* l = v.doc->ActiveLayer();
    const bool exists = !t.newPath && l && !l->surfacePath.empty();
    Json p;
    if (exists && !Json::Parse(l->surfacePath, p)) return;
    int mode = exists ? p["mode"].I() : t.pathMode;
    ImGui::SameLine();
    ImGui::SetNextItemWidth(105 * dpi);
    bool modeChanged = ImGui::Combo("##surfacePathMode", &mode, "Trait\0Ruban\0Gomme\0Remplissage\0");
    t.pathMode = mode;
    if (!exists) return;
    if (modeChanged) p["mode"] = mode;
    const Id material = l->material;
    float radius = p["radius"].F() * 100, spacing = p["spacing"].F() * 100;
    bool closed = p["closed"].B(), changed = modeChanged;
    if (mode != Filled) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(68 * dpi);
        if (ImGui::InputFloat("Rayon (%)", &radius, 0, 0, "%.2f", ImGuiInputTextFlags_EnterReturnsTrue)) {
            p["radius"] = std::clamp(radius * 0.01f, 0.0001f, 1.0f);
            changed = true;
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(68 * dpi);
        if (ImGui::InputFloat("Pas (%)", &spacing, 0, 0, "%.2f", ImGuiInputTextFlags_EnterReturnsTrue)) {
            p["spacing"] = std::clamp(spacing * 0.01f, 0.0f, 2.0f);
            changed = true;
        }
        ImGui::SameLine();
        if (ImGui::Checkbox("Fermé", &closed)) {
            p["closed"] = closed;
            changed = true;
        }
    }
    if (mode == Ribbon) {
        ImGui::SameLine();
        if (ImGui::SmallButton("Motif…")) {
            auto file = platform::OpenFileDialog(hwnd, L"Motif du ruban\0*.png;*.jpg;*.jpeg;*.bmp;*.tga\0");
            if (!file.empty()) {
                Json stencil;
                if (ImportRibbon(file, stencil)) {
                    p["ribbon"] = std::move(stencil);
                    changed = true;
                } else
                    Status("Impossible de lire cette image");
            }
        }
        if (p["ribbon"].IsObject()) {
            ImGui::SameLine();
            if (ImGui::SmallButton("Uni")) {
                p.Erase("ribbon");
                changed = true;
            }
        }
    }
    if (changed) WriteSurfacePath(v, material, p, "Modifier le tracé de surface");
}

void App::SurfacePathInput(DocView& v, float2 mouse, u32 w, u32 h, bool hovered, ImVec2 origin) {
    auto& s = *v.mesh;
    auto& d = *v.doc;
    auto& t = v.surfaceTools;
    if (d.kind != 1) return;
    Json path;
    Id material = 0;
    std::vector<SurfaceAnchor> points;
    if (auto* layer = d.ActiveLayer(); !t.newPath && layer && !layer->surfacePath.empty() && Json::Parse(layer->surfacePath, path)) {
        material = layer->material;
        if (!ReadAnchors(path, s, points)) return;
    }
    bool click = hovered && ImGui::IsMouseClicked(0) && !ImGui::GetIO().KeyAlt;
    int nearest = -1;
    float threshold = 9 * dpi;
    auto* dl = ImGui::GetWindowDrawList();
    for (size_t i = 0; i < points.size(); ++i) {
        float3 p = SurfacePathTopology::Position(s.data, points[i]);
        float4 c = s.viewProj.Transform({p.x, p.y, p.z, 1});
        if (c.w <= 0) continue;
        float2 q{(c.x / c.w * 0.5f + 0.5f) * w, (0.5f - c.y / c.w * 0.5f) * h};
        if (q.x < 0 || q.y < 0 || q.x >= w || q.y >= h) continue;
        u32 frontTriangle;
        float3 frontWeights;
        if (!PickMeshHit(s, q, w, h, frontTriangle, frontWeights) ||
            Length(SurfacePathTopology::Position(s.data, {frontTriangle, frontWeights}) - p) > 0.002f)
            continue;
        const float distance = Length(q - mouse);
        if (distance < threshold) {
            nearest = (int)i;
            threshold = distance;
        }
        ImVec2 at{origin.x + q.x, origin.y + q.y};
        dl->AddCircleFilled(at, 4 * dpi, IM_COL32(248, 183, 53, 235));
        dl->AddCircle(at, 5 * dpi, IM_COL32(42, 31, 12, 255));
    }
    if (t.dragPoint >= 0 && (material != t.dragMaterial || (size_t)t.dragPoint >= points.size())) t.dragPoint = -1;
    if (click && nearest >= 0) {
        if (ImGui::GetIO().KeyCtrl && points.size() > 1) {
            points.erase(points.begin() + nearest);
            WriteAnchors(path, points, s.data);
            WriteSurfacePath(v, material, path, "Retirer un point du tracé");
        } else {
            t.dragPoint = nearest;
            t.dragMaterial = material;
        }
        return;
    }
    if (t.dragPoint >= 0 && ImGui::IsMouseDown(0)) {
        dl->AddCircle(ImVec2(origin.x + mouse.x, origin.y + mouse.y), 6 * dpi, IM_COL32(255, 220, 120, 255));
        return;
    }
    const bool move = t.dragPoint >= 0;
    const int point = t.dragPoint;
    if (move) t.dragPoint = -1;
    if (!click && !move) return;
    u32 tri;
    float3 bary;
    if (!hovered || !PickMeshHit(s, mouse, w, h, tri, bary)) return;
    if (!material) {
        path = Json::Object();
        path["mode"] = t.pathMode;
        const float3 p = SurfacePathTopology::Position(s.data, {tri, bary});
        const float depth = s.ortho ? s.distance : Dot(p - s.camPos, Normalize(s.target - s.camPos));
        path["radius"] = std::clamp(brush.size * depth * std::tan(s.fov * 0.5f) / h, 0.0001f, 1.0f);
        path["spacing"] = 0;
        path["closed"] = false;
        for (int i = 0; i < (int)PbrChannel::Count; ++i) {
            const auto ch = (PbrChannel)i;
            if (brush3D.use[i] && FindChannelGroup(d, ch))
                path["colors"][PbrChannelSuffix(ch)] = ToJson(BrushChannelValue(ch));
        }
    }
    if (move) {
        if (points[point].triangle == tri && Length(points[point].weights - bary) < 0.0001f) return;
        points[point] = {tri, bary};
    } else
        points.push_back({tri, bary});
    WriteAnchors(path, points, s.data);
    WriteSurfacePath(v, material, path, move ? "Déplacer un point du tracé" : "Ajouter un point du tracé");
}
} // namespace sx
