// 3D viewport: preview the document on a mesh and paint directly on it.
#include "ui/App.h"
#include "doc/Brushes.h"
#include "ui/Theme.h"
#include "render/MeshPaint.h"

namespace sx {

void App::AttachMesh(DocView& v, MeshData data) {
    if (!v.mesh) v.mesh = std::make_unique<MeshScene>();
    v.mesh->data = std::move(data);
    v.mesh->gpuDirty = true;
    ++v.mesh->revision;
    v.mesh->bakedW = 0;
    Status("Maillage : " + v.mesh->data.name + " (" + std::to_string(v.mesh->data.indices.size() / 3) + " triangles)");
}

void App::DrawView3D() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    bool open = ImGui::Begin("Vue 3D", PanelFlag("Vue 3D"), ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    DocView* v = Active();
    if (!open || !v) { ImGui::End(); return; }
    Document& doc = *v->doc;

    // Header bar: wraps to more lines when the view is narrow.
    const ImVec2 start = ImGui::GetCursorPos();  // below the dock tab bar
    ImGui::SetCursorPos(ImVec2(start.x + 8 * dpi, start.y + 6 * dpi));
    const float rowX = ImGui::GetCursorPosX();
    bool first = true;
    auto flow = [&](float width) {  // SameLine when the next item fits, otherwise a new row
        if (first) { first = false; return; }
        ImGui::SameLine();
        if (ImGui::GetContentRegionAvail().x < width + 8 * dpi) {
            ImGui::NewLine();
            ImGui::SetCursorPosX(rowX);
        }
    };
    auto button = [&](const char* label) {
        flow(ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2);
        return ImGui::SmallButton(label);
    };
    if (button("Sphère")) AttachMesh(*v, MakeSphere());
    if (button("Cube")) AttachMesh(*v, MakeCube());
    if (button("Plan")) AttachMesh(*v, MakePlane());
    if (button("Ouvrir un maillage...")) {
        std::string path = platform::OpenFileDialog(hwnd, L"Maillages (*.obj, *.fbx, *.gltf, *.glb, *.uasset)\0*.obj;*.fbx;*.gltf;*.glb;*.uasset\0");
        if (!path.empty() && Narrow(Utf8Path(path).extension().wstring()) == ".uasset") {
            DocView* target = v;
            ConvertUAssetAsync(path, [this, target](const std::string& glb) {
                MeshData m;
                std::string err;
                for (auto& d : docs)
                    if (d.get() == target && LoadMesh(glb, m, &err)) AttachMesh(*target, std::move(m));
            });
            path.clear();
        }
        if (!path.empty()) {
            MeshData m;
            std::string err;
            if (LoadMesh(path, m, &err)) {
                bool noUv = !m.hasUvs;
                AttachMesh(*v, std::move(m));
                if (noUv) {
                    // Nothing can be painted without UVs: straight to the unwrap tool.
                    uvTool.open = true;
                    StartUvUnwrap(*v);
                    Status("Maillage sans UV : dépliage automatique proposé (aperçu, puis « Appliquer »)");
                }
            } else Status("Maillage : " + err);
        }
    }
    if (v->mesh) {
        if (button("UV automatiques...")) uvTool.open = true;
        ui::HelpTooltip("Dépliage UV automatique (xatlas) avec options et aperçu avant d'appliquer.");
        if (button("Recentrer")) {
            v->mesh->yaw = 0.7f;
            v->mesh->pitch = 0.35f;
            v->mesh->distance = 2.8f;
            v->mesh->target = {};
        }
        bool missing = false;
        for (int c = 1; c < (int)PbrChannel::Count; ++c) if (!FindChannelGroup(doc, (PbrChannel)c)) missing = true;
        bool hasChannels = FindChannelGroup(doc, PbrChannel::Roughness) != 0;
        if (doc.kind == 1) {
            // Project: channels chosen at creation, exports by template.
            if (button("Exporter les textures...")) {
                std::string f = platform::SaveFileDialog(hwnd, L"Nom de base des textures (*.png)\0*.png\0", L"png");
                if (!f.empty()) ExportProjectTextures(*v, f);
            }
            flow(150 * dpi);
            ImGui::SetNextItemWidth(150 * dpi);
            ImGui::Combo("##exptpl", &doc.exportTemplate, "Textures séparées\0Unreal (ORM)\0Unity HDRP\0");
            ui::HelpTooltip("Modèle d'export des textures");
        } else {
            if (missing && button(hasChannels ? "Compléter les canaux PBR" : "Créer les canaux PBR")) CreatePbrChannels(*v);
            if (hasChannels && button("Exporter les textures PBR..."))
                ExportPbrTextures(*v, platform::SaveFileDialog(hwnd, L"Nom de base des textures (*.png)\0*.png\0", L"png"));
        }
        if (button(v->mesh->envAtlas || !v->mesh->envPixels.empty() ? "HDRI (actif)" : "HDRI...")) {
            std::string path = platform::OpenFileDialog(hwnd, L"Environnement HDR (*.hdr)\0*.hdr\0Images\0*.png;*.jpg;*.exr\0");
            std::string err;
            if (!path.empty() && !LoadEnvironment(*v->mesh, path, &err)) Status("HDRI : " + err);
        }
        if (v->mesh->envAtlas) {
            flow(110 * dpi);
            ImGui::SetNextItemWidth(110 * dpi);
            ImGui::SliderFloat("##envI", &v->mesh->envIntensity, 0.05f, 8.0f, "Intensité %.2f", ImGuiSliderFlags_Logarithmic);
            flow(110 * dpi);
            ImGui::SetNextItemWidth(110 * dpi);
            float deg = v->mesh->envRotation * 57.29578f;
            if (ImGui::SliderFloat("##envR", &deg, -180, 180, "Rotation %.0f°")) v->mesh->envRotation = deg / 57.29578f;
            flow(ImGui::GetFrameHeight() + ImGui::CalcTextSize("Fond").x + 12 * dpi);
            ImGui::Checkbox("Fond", &v->mesh->envBackground);
            ui::HelpTooltip("Afficher l'environnement derrière le modèle");
            if (v->mesh->envBackground) {
                flow(90 * dpi);
                ImGui::SetNextItemWidth(90 * dpi);
                ImGui::SliderFloat("##envB", &v->mesh->envBlur, 0.0f, 5.0f, "Flou %.1f");
            }
            if (button("Sans HDRI")) v->mesh->envAtlas.reset();
        }
        {
            static const char* modes = "Rendu PBR\0Couleur de base\0Rugosité\0Métal\0Normale\0Émissif\0Occlusion\0Hauteur\0";
            flow(140 * dpi);
            ImGui::SetNextItemWidth(140 * dpi);
            ImGui::Combo("##viewch", &v->mesh->viewChannel, modes);
            ui::HelpTooltip("Afficher un canal seul (tel que peint)");
        }
        char info[160];
        snprintf(info, sizeof(info), "%s - %zu triangles", v->mesh->data.name.c_str(), v->mesh->data.indices.size() / 3);
        flow(ImGui::CalcTextSize(info).x + 20 * dpi);
        ImGui::TextDisabled("%s", info);
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        ui::HelpTooltip("Clic droit / Alt+clic : orbite   Molette : zoom   Clic molette : déplacer\nPinceau / gomme : peindre sur le modèle");
    }
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4 * dpi);
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (!v->mesh) {
        ImGui::SetCursorPos(ImVec2(start.x + 16 * dpi, ImGui::GetCursorPosY() + 24 * dpi));
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
    // Projection tool: Ctrl+wheel resizes, Shift+wheel turns the decal; otherwise the wheel zooms.
    if (hovered && io.MouseWheel != 0 && tool == Tool::Projection && (io.KeyCtrl || io.KeyShift)) {
        if (io.KeyCtrl) decal.size = std::clamp(decal.size * std::pow(1.1f, io.MouseWheel), 0.02f, 3.0f);
        else decal.rotation = std::fmod(decal.rotation + io.MouseWheel * 5.0f + 540.0f, 360.0f) - 180.0f;
    } else if (hovered && io.MouseWheel != 0) s.distance *= std::pow(0.88f, io.MouseWheel);

    MeshPrepare3D(*v);

    // Painting
    std::vector<PointerSample> samples = frameSamples;
    Id target = doc.PaintTarget();
    bool paintTool = tool == Tool::Brush || tool == Tool::Eraser;
    auto local = [&](float x, float y) { return float2{x - origin.x, y - origin.y}; };
    // Layers painted together (3D project paint layer: one per channel) and their values.
    std::vector<float4> meshColors;
    std::vector<Id> meshTargets = Paint3DTargets(*v, meshColors);
    // 3D brush: the tip of the brush (atlas of variants), dynamics (jitter, scatter, angle), lazy mouse, line with Shift.
    auto setupTip = [&](const BrushSettings& b) {
        v->renderer.meshTipOn = false;
        if (!brush3D.tip) return;
        if (auto td = TipDef(b)) {
            EffectInstance tip;
            tip.params = b.tipParams;
            BrushSettings bound = b;
            BindTipTextures(bound, *td);
            v->renderer.meshTipOn = v->renderer.BuildMeshTip(*ctx, *td, tip, bound);
        }
    };
    auto spacingOf = [&](const BrushSettings& b) {
        float sp = b.spacing;
        if (brush3D.tip)
            if (auto td = TipDef(b); td && td->spacing > 0) sp = td->spacing;
        return std::max(1.0f, b.size * std::max(sp, 0.02f));
    };
    std::vector<float4> dabEx;
    auto rnd = [&] {
        meshRng = meshRng * 1664525u + 1013904223u;
        return (meshRng >> 8) * (1.0f / 16777216.0f);
    };
    auto addDab = [&](std::vector<Dab>& out, float2 q, float pr, float2 dir, const BrushSettings& b) {
        const auto& B = brush3D;
        float rad = b.size * 0.5f * (b.pressureSize ? std::max(pr, 0.05f) : 1.0f);
        rad *= std::max(0.05f, 1.0f + B.jitterSize * (rnd() * 2 - 1));
        if (B.scatter > 0) {
            const float ang = rnd() * 6.2831853f, dist = std::sqrt(rnd()) * B.scatter * b.size;
            q = q + float2{std::cos(ang) * dist, std::sin(ang) * dist};
        }
        float angle = B.angle * 0.0174533f + B.jitterAngle * 3.14159265f * (rnd() * 2 - 1);
        if (B.follow && Length(dir) > 1e-4f) angle += std::atan2(dir.y, dir.x);
        const float alpha = b.flow * (b.pressureOpacity ? pr : 1.0f) * (1.0f - B.jitterFlow * rnd());
        out.push_back({q.x, q.y, rad, alpha});
        dabEx.push_back({std::cos(angle), std::sin(angle), (float)((int)(rnd() * v->renderer.meshTipVariants) % v->renderer.meshTipVariants), 0});
    };
    if (paintTool && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.KeyAlt) {
        if (!target || meshTargets.empty()) Status("Sélectionnez un calque de pixels pour peindre en 3D");
        else if (v->renderer.BeginStrokeMulti(doc, *ctx, meshTargets)) {
            const BrushSettings& b = tool == Tool::Eraser ? eraser : brush;
            setupTip(b);
            float2 p = local(io.MousePos.x, io.MousePos.y);
            float pr = platform::PenActive() ? PenPressure(platform::LastPressure()) : 1.0f;
            std::vector<Dab> first;
            if (io.KeyShift && meshLineValid) {
                // Straight line from the end of the last stroke (in the view: the camera has to stay put).
                float2 d = p - meshLineEnd;
                float len = Length(d);
                const float sp = spacingOf(b);
                for (float t = 0; t <= len; t += sp) addDab(first, meshLineEnd + d * (t / std::max(len, 1e-4f)), pr, d, b);
            } else {
                addDab(first, p, pr, {0, 0}, b);
            }
            meshStroking = true;
            meshLastDab = p;
            meshLazy = p;
            meshDabRemainder = 0;
            v->renderer.MeshStrokeDabsMulti(doc, *ctx, s, first, b, meshColors, tool == Tool::Eraser, &dabEx);
            dabEx.clear();
        }
    }
    // Projection: the decal is placed where it is seen (occlusion and facing like the brush).
    const float decalH = decal.size * (float)h * 0.5f;
    const float decalW = decal.h > 0 ? decalH * decal.w / decal.h : decalH;
    if (tool == Tool::Projection && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.KeyAlt) {
        if (!decal.tex) Status("Projection : choisissez une image (panneau Matériau du pinceau)");
        else if (meshTargets.empty()) Status("Sélectionnez un calque pour projeter");
        else if (v->renderer.BeginStrokeMulti(doc, *ctx, meshTargets)) {
            v->renderer.MeshRender(*ctx, s, GatherMaterial(*v), w, h);  // depth of this frame
            float2 p = local(io.MousePos.x, io.MousePos.y);
            Layer* al = doc.ActiveLayer();
            bool imageColor = decal.imageColor && al && al->material && IsPaintLayer3D(doc, al->material) && brush3D.use[(int)PbrChannel::BaseColor];
            v->renderer.MeshDecalMulti(doc, *ctx, s, decal.tex.get(), p, {decalW, decalH}, decal.rotation * 0.0174533f, decal.opacity, meshColors, imageColor);
            v->renderer.EndStrokeMulti(doc, *ctx, v->history, "Projection");
        }
    }
    if (meshStroking) {
        const BrushSettings& b = tool == Tool::Eraser ? eraser : brush;
        if (brush3D.tip && !v->renderer.meshTipOn) setupTip(b);  // the tip may still be compiling at the first dab
        if (samples.empty()) samples.push_back({io.MousePos.x, io.MousePos.y, platform::PenActive() ? platform::LastPressure() : 1.0f, platform::PenActive(), false});
        std::vector<Dab> dabs;
        const float spacing = spacingOf(b);
        for (auto& sm : samples) {
            float2 p = local(sm.x, sm.y);
            if (brush3D.lazy > 0) {
                // Lazy mouse: the brush follows the cursor at the end of a string.
                float2 d = p - meshLazy;
                float l = Length(d);
                if (l <= brush3D.lazy) continue;
                meshLazy = meshLazy + d * ((l - brush3D.lazy) / l);
                p = meshLazy;
            }
            float pr = sm.pen ? PenPressure(sm.pressure) : 1.0f;
            float2 d = p - meshLastDab;
            float len = Length(d);
            float t = spacing - meshDabRemainder;
            while (t <= len) {
                addDab(dabs, meshLastDab + d * (t / std::max(len, 1e-4f)), pr, d, b);
                t += spacing;
            }
            meshDabRemainder = std::max(0.0f, len - (t - spacing));
            if (len > 0) meshLastDab = p;
        }
        // Render first so the occlusion test uses this frame's depth.
        v->renderer.MeshRender(*ctx, s, GatherMaterial(*v), w, h);
        if (!dabs.empty()) v->renderer.MeshStrokeDabsMulti(doc, *ctx, s, dabs, b, meshColors, tool == Tool::Eraser, &dabEx);
        dabEx.clear();
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) || io.KeyShift) {
            v->renderer.EndStrokeMulti(doc, *ctx, v->history, "Peinture 3D");
            meshStroking = false;
            meshLineEnd = meshLastDab;
            meshLineValid = true;
        }
    }
    // Polygon fill: the triangle / face / UV island / part under the cursor.
    if (tool == Tool::PolyFill && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.KeyAlt) {
        int tri = PickMeshTriangle(s, local(io.MousePos.x, io.MousePos.y), w, h);
        if (tri < 0) Status("Remplissage par polygone : cliquez sur le modèle");
        else PolygonFill(*v, tri, io.KeyCtrl);
    }
    if (tool == Tool::PolyFill && hovered && !orbit) {
        polyHover = PickMeshTriangle(s, local(io.MousePos.x, io.MousePos.y), w, h);
    } else {
        polyHover = -1;
    }

    // Re-evaluate the document (cached) so the mesh shows the latest paint, then render.
    v->lastImage = v->renderer.Evaluate(doc, *ctx);
    gpu::Texture* img = v->renderer.MeshRender(*ctx, s, GatherMaterial(*v), w, h);
    if (gpu::Texture* pv = UvPreviewRender(*v, w, h)) img = pv;  // UV tool: checker on the new layout
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (img) dl->AddImage((ImTextureID)gpu::Dev().UiTextureId(img), origin, ImVec2(origin.x + w, origin.y + h));
    if (hovered && tool == Tool::Projection && !orbit && decal.tex) {
        // Decal preview at the cursor (rotated quad).
        float c = std::cos(decal.rotation * 0.0174533f), sn = std::sin(decal.rotation * 0.0174533f);
        auto corner = [&](float x, float y) { return ImVec2(io.MousePos.x + c * x - sn * y, io.MousePos.y + sn * x + c * y); };
        ImVec2 q[4] = {corner(-decalW, -decalH), corner(decalW, -decalH), corner(decalW, decalH), corner(-decalW, decalH)};
        dl->AddImageQuad((ImTextureID)gpu::Dev().UiTextureId(decal.tex.get()), q[0], q[1], q[2], q[3], ImVec2(0, 0), ImVec2(1, 0), ImVec2(1, 1), ImVec2(0, 1),
                         IM_COL32(255, 255, 255, 150));
        dl->AddPolyline(q, 4, IM_COL32(255, 140, 0, 220), ImDrawFlags_Closed, 1.5f);
    }
    if (tool == Tool::PolyFill && polyHover >= 0) DrawPolyHighlight(s, polyHover, origin, w, h);
    if (hovered && paintTool && !orbit) {
        const BrushSettings& b = tool == Tool::Eraser ? eraser : brush;
        float r = std::max(b.size * 0.5f, 1.0f);
        dl->AddCircle(io.MousePos, r + 1, IM_COL32(0, 0, 0, 150), 0, 1.5f);
        dl->AddCircle(io.MousePos, r, IM_COL32_WHITE, 0, 1.0f);
        if (brush3D.lazy > 0 && meshStroking) {
            ImVec2 lz(origin.x + meshLazy.x, origin.y + meshLazy.y);
            dl->AddLine(lz, io.MousePos, IM_COL32(255, 200, 80, 200), 1.5f);
            dl->AddCircleFilled(lz, 3, IM_COL32(255, 200, 80, 255));
        }
        if (io.KeyShift && meshLineValid && !meshStroking)
            dl->AddLine(ImVec2(origin.x + meshLineEnd.x, origin.y + meshLineEnd.y), io.MousePos, IM_COL32(255, 255, 255, 160), 1.0f);
        ImGui::SetMouseCursor(ImGuiMouseCursor_None);
    }
    ImGui::End();
}

void App::MeshPrepare3D(DocView& v) {
    if (v.mesh) v.renderer.MeshPrepare(*v.doc, *ctx, *v.mesh);
}

}  // namespace sx
