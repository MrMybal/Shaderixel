// 3D viewport: preview the document on a mesh and paint directly on it.
#include "ui/App.h"
#include "ui/Theme.h"
#include "render/MeshPaint.h"

namespace sx {

void App::AttachMesh(DocView& v, MeshData data) {
    if (!v.mesh) v.mesh = std::make_unique<MeshScene>();
    v.mesh->data = std::move(data);
    v.mesh->gpuDirty = true;
    v.mesh->bakedW = 0;
    Status("Maillage : " + v.mesh->data.name + " (" + std::to_string(v.mesh->data.indices.size() / 3) + " triangles)");
}

void App::DrawView3D() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    bool open = ImGui::Begin("Vue 3D", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    DocView* v = Active();
    if (!open || !v) { ImGui::End(); return; }
    Document& doc = *v->doc;

    // Header bar
    ImGui::SetCursorPos(ImVec2(8 * dpi, 6 * dpi));
    if (ImGui::SmallButton("Sphère")) AttachMesh(*v, MakeSphere());
    ImGui::SameLine();
    if (ImGui::SmallButton("Cube")) AttachMesh(*v, MakeCube());
    ImGui::SameLine();
    if (ImGui::SmallButton("Plan")) AttachMesh(*v, MakePlane());
    ImGui::SameLine();
    if (ImGui::SmallButton("Ouvrir un maillage...")) {
        std::string path = platform::OpenFileDialog(hwnd, L"Maillages (*.obj, *.gltf, *.glb)\0*.obj;*.gltf;*.glb\0");
        if (!path.empty()) {
            MeshData m;
            std::string err;
            if (LoadMesh(path, m, &err)) AttachMesh(*v, std::move(m));
            else Status("Maillage : " + err);
        }
    }
    if (v->mesh) {
        ImGui::SameLine();
        if (!FindChannelGroup(doc, PbrChannel::Roughness)) {
            if (ImGui::SmallButton("Créer les canaux PBR")) CreatePbrChannels(*v);
        } else if (ImGui::SmallButton("Exporter les textures PBR...")) {
            ExportPbrTextures(*v, platform::SaveFileDialog(hwnd, L"Nom de base des textures (*.png)\0*.png\0", L"png"));
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Recentrer")) {
            v->mesh->yaw = 0.7f;
            v->mesh->pitch = 0.35f;
            v->mesh->distance = 2.8f;
            v->mesh->target = {};
        }
        ImGui::SameLine();
        ImGui::TextDisabled("%s - %zu triangles   clic droit / Alt : orbite, molette : zoom, clic molette : déplacer", v->mesh->data.name.c_str(),
                            v->mesh->data.indices.size() / 3);
    }
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4 * dpi);
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (!v->mesh) {
        ImGui::SetCursorPos(ImVec2(16 * dpi, 48 * dpi));
        ImGui::TextWrapped("Choisissez une primitive ou ouvrez un fichier OBJ (avec coordonnées UV) : le document devient sa texture et vous pouvez peindre directement sur le modèle.");
        ImGui::End();
        return;
    }
    MeshScene& s = *v->mesh;
    u32 w = (u32)std::max(1.0f, avail.x), h = (u32)std::max(1.0f, avail.y);
    ImGui::InvisibleButton("##view3d", avail, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
    bool hovered = ImGui::IsItemHovered();
    ImGuiIO& io = ImGui::GetIO();

    // Camera
    bool orbit = ImGui::IsItemActive() && (ImGui::IsMouseDown(ImGuiMouseButton_Right) || (io.KeyAlt && ImGui::IsMouseDown(ImGuiMouseButton_Left)));
    if (orbit) {
        s.yaw -= io.MouseDelta.x * 0.008f;
        s.pitch += io.MouseDelta.y * 0.008f;
    }
    if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
        float3 fwd = Normalize(s.target - s.camPos);
        float3 right = Normalize(Cross({0, 1, 0}, fwd));
        float3 up = Cross(fwd, right);
        float k = s.distance * 0.0015f;
        s.target = s.target - right * (io.MouseDelta.x * k) + up * (io.MouseDelta.y * k);
    }
    if (hovered && io.MouseWheel != 0) s.distance *= std::pow(0.88f, io.MouseWheel);

    MeshPrepare3D(*v);

    // Painting
    std::vector<PointerSample> samples = platform::TakeSamples();
    Id target = doc.PaintTarget();
    bool paintTool = tool == Tool::Brush || tool == Tool::Eraser;
    auto local = [&](float x, float y) { return float2{x - origin.x, y - origin.y}; };
    if (paintTool && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.KeyAlt) {
        if (!target) Status("Sélectionnez un calque de pixels pour peindre en 3D");
        else if (v->renderer.BeginStroke(doc, *ctx, target)) {
            meshStroking = true;
            float2 p = local(io.MousePos.x, io.MousePos.y);
            meshLastDab = p;
            meshDabRemainder = 0;
            const BrushSettings& b = tool == Tool::Eraser ? eraser : brush;
            float pr = platform::PenActive() ? platform::LastPressure() : 1.0f;
            v->renderer.MeshStrokeDabs(doc, *ctx, s, {{p.x, p.y, b.size * 0.5f * (b.pressureSize ? std::max(pr, 0.05f) : 1.0f), b.flow}}, b,
                                       LinearColor(fg), tool == Tool::Eraser);
        }
    }
    if (meshStroking) {
        const BrushSettings& b = tool == Tool::Eraser ? eraser : brush;
        if (samples.empty()) samples.push_back({io.MousePos.x, io.MousePos.y, platform::PenActive() ? platform::LastPressure() : 1.0f, platform::PenActive(), false});
        std::vector<Dab> dabs;
        float spacing = std::max(1.0f, b.size * std::max(b.spacing, 0.02f));
        for (auto& sm : samples) {
            float2 p = local(sm.x, sm.y);
            float pr = sm.pen ? sm.pressure : 1.0f;
            float2 d = p - meshLastDab;
            float len = Length(d);
            float t = spacing - meshDabRemainder;
            while (t <= len) {
                float2 q = meshLastDab + d * (t / std::max(len, 1e-4f));
                dabs.push_back({q.x, q.y, b.size * 0.5f * (b.pressureSize ? std::max(pr, 0.05f) : 1.0f), b.flow * (b.pressureOpacity ? pr : 1.0f)});
                t += spacing;
            }
            meshDabRemainder = std::max(0.0f, len - (t - spacing));
            if (len > 0) meshLastDab = p;
        }
        // Render first so the occlusion test uses this frame's depth.
        v->renderer.MeshRender(*ctx, s, GatherMaterial(*v), w, h);
        if (!dabs.empty()) v->renderer.MeshStrokeDabs(doc, *ctx, s, dabs, b, LinearColor(fg), tool == Tool::Eraser);
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            v->renderer.EndStroke(doc, *ctx, v->history, "Peinture 3D");
            meshStroking = false;
        }
    }

    // Re-evaluate the document (cached) so the mesh shows the latest paint, then render.
    v->lastImage = v->renderer.Evaluate(doc, *ctx);
    gpu::Texture* img = v->renderer.MeshRender(*ctx, s, GatherMaterial(*v), w, h);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (img) dl->AddImage((ImTextureID)gpu::Dev().UiTextureId(img), origin, ImVec2(origin.x + w, origin.y + h));
    if (hovered && paintTool && !orbit) {
        const BrushSettings& b = tool == Tool::Eraser ? eraser : brush;
        float r = std::max(b.size * 0.5f, 1.0f);
        dl->AddCircle(io.MousePos, r + 1, IM_COL32(0, 0, 0, 150), 0, 1.5f);
        dl->AddCircle(io.MousePos, r, IM_COL32_WHITE, 0, 1.0f);
        ImGui::SetMouseCursor(ImGuiMouseCursor_None);
    }
    ImGui::End();
}

void App::MeshPrepare3D(DocView& v) {
    if (v.mesh) v.renderer.MeshPrepare(*v.doc, *ctx, *v.mesh);
}

}  // namespace sx
