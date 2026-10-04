// Automatic UVs (xatlas): options and presets, preview of the current and the
// new layout (charts colored), checker preview on the model, then apply: the
// mesh gets the new UVs and the painted textures are carried over (every texel
// of a new island samples the old texture at the same surface point).
#include "ui/App.h"
#include "ui/Theme.h"
#include "mesh/UvUnwrap.h"
#include "render/MeshPaint.h"

#include <imgui_internal.h>
#include <future>

namespace sx {

namespace {

ImU32 ChartColor(u32 c) {
    // Stable hue per chart.
    float h = std::fmod(c * 0.61803398875f, 1.0f), r, g, b;
    ImGui::ColorConvertHSVtoRGB(h, 0.55f, 0.95f, r, g, b);
    return ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, 0.55f));
}

// UV layout in a square: filled triangles (chart colors when known) and their edges.
void DrawLayout(ImDrawList* dl, ImVec2 a, float size, const MeshData& m, const std::vector<u32>* charts) {
    dl->AddRectFilled(a, ImVec2(a.x + size, a.y + size), IM_COL32(28, 28, 32, 255));
    // Checker of 8 cells (texel scale reference).
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x)
            if ((x + y) & 1) dl->AddRectFilled(ImVec2(a.x + size * x / 8, a.y + size * y / 8), ImVec2(a.x + size * (x + 1) / 8, a.y + size * (y + 1) / 8), IM_COL32(34, 34, 40, 255));
    const size_t tris = m.indices.size() / 3;
    const size_t step = std::max<size_t>(1, tris / 60000);  // very dense meshes: a subset of triangles
    auto P = [&](u32 i) { const float2& uv = m.vertices[i].uv; return ImVec2(a.x + uv.x * size, a.y + uv.y * size); };
    for (size_t t = 0; t < tris; t += step) {
        u32 i0 = m.indices[t * 3], i1 = m.indices[t * 3 + 1], i2 = m.indices[t * 3 + 2];
        ImU32 col = charts && i0 < charts->size() ? ChartColor((*charts)[i0]) : IM_COL32(120, 160, 255, 90);
        dl->AddTriangleFilled(P(i0), P(i1), P(i2), col);
    }
    if (tris < 30000)
        for (size_t t = 0; t < tris; ++t) {
            u32 i0 = m.indices[t * 3], i1 = m.indices[t * 3 + 1], i2 = m.indices[t * 3 + 2];
            dl->AddTriangle(P(i0), P(i1), P(i2), IM_COL32(255, 255, 255, 50), 1.0f);
        }
    dl->AddRect(a, ImVec2(a.x + size, a.y + size), ui::col::Border);
}

}  // namespace

void App::StartUvUnwrap(DocView& v) {
    if (!v.mesh) return;
    if (uvJob && uvJob->work.valid() && uvJob->work.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
    auto job = std::make_shared<UvJob>();
    job->options = uvTool.options;
    job->input = v.mesh->data;
    uvJob = job;
    uvTool.hasResult = false;
    job->work = std::async(std::launch::async, [job] { return UnwrapMesh(job->input, job->options, job->result, &job->progress, &job->cancel); });
}

void App::ApplyUvUnwrap(DocView& v) {
    if (!uvJob || !uvTool.hasResult || !v.mesh) return;
    Document& d = *v.doc;
    const UnwrapResult& r = uvJob->result;
    const MeshData& old = v.mesh->data;
    if (uvTool.transfer) {
        // Old UV of each new vertex (same surface point), in document pixels.
        const float W = (float)d.Width(), H = (float)d.Height();
        std::vector<float2> dst(r.mesh.vertices.size()), src(r.mesh.vertices.size());
        for (size_t i = 0; i < r.mesh.vertices.size(); ++i) {
            dst[i] = {r.mesh.vertices[i].uv.x * W, r.mesh.vertices[i].uv.y * H};
            const float2 ou = r.sourceVertex[i] < old.vertices.size() ? old.vertices[r.sourceVertex[i]].uv : float2{0, 0};
            src[i] = {ou.x * W, ou.y * H};
        }
        // Every painted surface (layers, masks, surface nodes).
        std::vector<Id> surfaces;
        for (auto& l : d.S().layers) {
            if (l.surface && (l.kind == LayerKind::Pixel)) surfaces.push_back(l.surface);
            if (l.mask) surfaces.push_back(l.mask);
        }
        for (auto& n : d.S().graph.nodes) if (n.kind == NodeKind::Surface && n.surface) surfaces.push_back(n.surface);
        std::sort(surfaces.begin(), surfaces.end());
        surfaces.erase(std::unique(surfaces.begin(), surfaces.end()), surfaces.end());
        v.history.BeginGroup();
        for (Id id : surfaces) {
            Surface* s = d.GetSurface(id);
            if (!s) continue;
            auto before = gpu::Dev().CreateTexture(s->tex->Width(), s->tex->Height(), s->tex->GetFormat(), "UvBefore");
            ctx->CopyTexture(before.get(), s->tex.get());
            v.renderer.WarpTriangles(d, *ctx, id, before.get(), nullptr, Rect{0, 0, (int)W, (int)H}, dst, src, r.mesh.indices, false);
            auto after = gpu::Dev().CreateTexture(s->tex->Width(), s->tex->Height(), s->tex->GetFormat(), "UvAfter");
            ctx->CopyTexture(after.get(), s->tex.get());
            v.history.PushPaint("UV automatiques", id, {0, 0, (int)W, (int)H}, before, after);
        }
        v.history.EndGroup();
    }
    uvTool.previousMesh = std::make_shared<MeshData>(old);
    AttachMesh(v, MeshData(r.mesh));
    uvTool.preview = false;
    uvTool.hasResult = false;
    d.dirty = true;
    Status("UV automatiques appliquées : " + std::to_string(r.charts) + " îlots" + (uvTool.transfer ? ", peinture reportée (Ctrl+Z annule le report)" : ""));
}

void App::DrawUvToolWindow() {
    auto& T = uvTool;
    if (!T.open) return;
    DocView* v = Active();
    ImGui::SetNextWindowSize(ImVec2(820 * dpi, 640 * dpi), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("UV automatiques (xatlas)", &T.open, ImGuiWindowFlags_NoDocking)) { ImGui::End(); return; }
    if (!v || !v->mesh) {
        ImGui::TextWrapped("Ouvrez un maillage (Vue 3D) ou un projet Paint 3D.");
        ImGui::End();
        return;
    }
    // Job state.
    bool running = uvJob && uvJob->work.valid() && uvJob->work.wait_for(std::chrono::seconds(0)) != std::future_status::ready;
    if (uvJob && uvJob->work.valid() && !running) {
        bool ok = uvJob->work.get();
        T.hasResult = ok;
        T.message = ok ? uvJob->result.error : "Échec : " + uvJob->result.error;
        if (ok) T.preview = true;
    }
    UnwrapOptions& o = T.options;
    if (o.resolution <= 0) o.resolution = (int)v->doc->Width();

    // Left: options.
    ImGui::BeginChild("##uvopts", ImVec2(300 * dpi, 0), ImGuiChildFlags_Borders);
    ImGui::PushItemWidth(-110 * dpi);
    static const char* presets[] = {"Équilibré", "Peu d'îlots (plus de distorsion)", "Faible distorsion (plus d'îlots)", "Surface dure (arêtes vives)",
                                    "Conserver les îlots (réempaqueter)"};
    if (ImGui::Combo("Préréglage", &T.preset, presets, 5)) {
        UnwrapOptions p;
        p.resolution = o.resolution;
        p.padding = o.padding;
        if (T.preset == 1) { p.maxCost = 8.0f; p.normalDeviationWeight = 1.0f; p.maxIterations = 2; }
        if (T.preset == 2) { p.maxCost = 0.7f; p.normalDeviationWeight = 4.0f; p.maxIterations = 2; }
        if (T.preset == 3) { p.normalSeamWeight = 2000.0f; p.maxCost = 2.0f; }
        if (T.preset == 4) p.useInputUvs = true;
        o = p;
    }
    ImGui::SeparatorText("Îlots");
    ImGui::BeginDisabled(o.useInputUvs);
    ImGui::SliderFloat("Coût max.", &o.maxCost, 0.1f, 20.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
    ui::HelpTooltip("Plus bas : plus d'îlots, moins déformés. Plus haut : moins d'îlots, plus de distorsion.");
    ImGui::SliderFloat("Écart de normale", &o.normalDeviationWeight, 0.0f, 10.0f, "%.2f");
    ui::HelpTooltip("Poids de l'angle entre une face et la normale moyenne de son îlot.");
    ImGui::SliderFloat("Rondeur", &o.roundnessWeight, 0.0f, 1.0f, "%.3f");
    ImGui::SliderFloat("Bords droits", &o.straightnessWeight, 0.0f, 20.0f, "%.1f");
    ImGui::SliderFloat("Coutures (normales)", &o.normalSeamWeight, 0.0f, 2000.0f, "%.1f", ImGuiSliderFlags_Logarithmic);
    ui::HelpTooltip("Au-delà de 1000, les arêtes vives (normales séparées) sont toujours coupées.");
    ImGui::SliderFloat("Coutures (UV)", &o.textureSeamWeight, 0.0f, 10.0f, "%.2f");
    ImGui::SliderInt("Itérations", &o.maxIterations, 1, 8);
    ui::HelpTooltip("Plus d'itérations : de meilleurs îlots, plus lent.");
    ImGui::EndDisabled();
    ImGui::Checkbox("Conserver les îlots existants", &o.useInputUvs);
    ImGui::SeparatorText("Empaquetage");
    ImGui::InputInt("Résolution", &o.resolution, 256, 1024);
    o.resolution = std::clamp(o.resolution, 128, 8192);
    ImGui::SliderInt("Marge (texels)", &o.padding, 0, 32);
    ui::HelpTooltip("Espace entre les îlots : évite les bavures entre îlots au filtrage et aux mipmaps.");
    ImGui::Checkbox("Rotation des îlots", &o.rotateCharts);
    ImGui::Checkbox("Recherche exhaustive (lent)", &o.bruteForce);
    ImGui::Checkbox("Aligner sur des blocs 4x4", &o.blockAlign);
    ImGui::PopItemWidth();
    ImGui::Separator();
    if (running) {
        ImGui::ProgressBar(uvJob->progress.load(), ImVec2(-1, 0));
        if (ImGui::Button("Interrompre", ImVec2(-1, 0))) uvJob->cancel = true;
    } else if (ImGui::Button("Calculer l'aperçu", ImVec2(-1, 0))) {
        StartUvUnwrap(*v);
    }
    if (!T.message.empty()) ImGui::TextColored(ImVec4(1, 0.65f, 0.3f, 1), "%s", T.message.c_str());
    ImGui::EndChild();

    // Right: current | new layout, stats, apply.
    ImGui::SameLine();
    ImGui::BeginChild("##uvprev", ImVec2(0, 0));
    const MeshData& cur = v->mesh->data;
    bool have = T.hasResult && uvJob;
    float avail = ImGui::GetContentRegionAvail().x;
    float side = std::min((avail - 12 * dpi) * 0.5f, ImGui::GetContentRegionAvail().y - 140 * dpi);
    side = std::max(side, 120 * dpi);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGui::TextDisabled("UV actuelles");
    ImGui::SameLine(side + 12 * dpi);
    ImGui::TextDisabled(have ? "Aperçu (couleur = îlot)" : "Aperçu : « Calculer l'aperçu »");
    ImVec2 p = ImGui::GetCursorScreenPos();
    DrawLayout(dl, p, side, cur, nullptr);
    if (have) DrawLayout(dl, ImVec2(p.x + side + 12 * dpi, p.y), side, uvJob->result.mesh, &uvJob->result.chartOfVertex);
    else dl->AddRect(ImVec2(p.x + side + 12 * dpi, p.y), ImVec2(p.x + 2 * side + 12 * dpi, p.y + side), ui::col::Border);
    ImGui::Dummy(ImVec2(2 * side + 12 * dpi, side));
    ImGui::Text("Actuel : %zu sommets, %zu triangles", cur.vertices.size(), cur.indices.size() / 3);
    if (have) {
        const UnwrapResult& r = uvJob->result;
        ImGui::Text("Nouveau : %u îlots, %zu sommets (coutures), occupation %.0f %%, atlas %u x %u", r.charts, r.mesh.vertices.size(),
                    r.utilization * 100, r.width, r.height);
        ImGui::Checkbox("Aperçu sur le modèle (damier)", &T.preview);
        ui::HelpTooltip("La Vue 3D montre les nouvelles UV avec un damier : carreaux réguliers = peu de distorsion.");
        ImGui::Checkbox("Reporter la peinture existante sur les nouvelles UV", &T.transfer);
        if (ImGui::Button("Appliquer", ImVec2(160 * dpi, 0))) ApplyUvUnwrap(*v);
        ImGui::SameLine();
        if (ImGui::Button("Abandonner l'aperçu", ImVec2(160 * dpi, 0))) {
            T.hasResult = false;
            T.preview = false;
        }
    }
    if (T.previousMesh) {
        if (ImGui::SmallButton("Revenir aux UV précédentes")) {
            AttachMesh(*v, MeshData(*T.previousMesh));
            T.previousMesh.reset();
            Status("UV précédentes rétablies (annulez aussi le report de peinture avec Ctrl+Z)");
        }
    }
    ImGui::EndChild();
    ImGui::End();
}

// Checker preview of the computed UVs in the 3D view (instead of the document's material).
gpu::Texture* App::UvPreviewRender(DocView& v, u32 w, u32 h) {
    auto& T = uvTool;
    if (!T.open || !T.preview || !T.hasResult || !uvJob || !v.mesh) return nullptr;
    if (!T.scene || T.sceneFor != uvJob.get()) {
        T.scene = std::make_unique<MeshScene>();
        T.scene->data = uvJob->result.mesh;
        T.scene->gpuDirty = true;
        ++T.scene->revision;
        T.sceneFor = uvJob.get();
    }
    if (!T.checker) {
        const int n = 512, cells = 16;
        std::vector<u8> px((size_t)n * n * 4);
        for (int y = 0; y < n; ++y)
            for (int x = 0; x < n; ++x) {
                bool c = ((x * cells / n) + (y * cells / n)) & 1;
                u8* q = &px[((size_t)y * n + x) * 4];
                // Colored rows / columns so stretching and rotation show.
                q[0] = c ? 230 : (u8)(60 + 120 * x / n);
                q[1] = c ? 230 : 70;
                q[2] = c ? 230 : (u8)(60 + 120 * y / n);
                q[3] = 255;
            }
        T.checker = gpu::Dev().CreateTexture(n, n, gpu::Format::RGBA8, "UvChecker");
        ctx->Upload(T.checker.get(), px.data(), n * 4);
    }
    // Same camera as the model's scene.
    MeshScene& s = *T.scene;
    s.yaw = v.mesh->yaw;
    s.pitch = v.mesh->pitch;
    s.distance = v.mesh->distance;
    s.target = v.mesh->target;
    v.renderer.MeshPrepare(*v.doc, *ctx, s);
    MeshMaterial m;
    m.tex[(int)PbrChannel::BaseColor] = T.checker.get();
    return v.renderer.MeshRender(*ctx, s, m, w, h);
}

}  // namespace sx
