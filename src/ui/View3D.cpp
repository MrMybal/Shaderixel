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

void App::SetView3DPreset(MeshScene& s, int k) {
    // 0 front, 1 back, 2 left, 3 right, 4 top, 5 bottom; orthographic like CAD / Blender views.
    static const float yaws[] = {0.0f, 3.14159265f, -1.5707963f, 1.5707963f, 0.0f, 0.0f};
    static const float pitches[] = {0.0f, 0.0f, 0.0f, 0.0f, 1.5695f, -1.5695f};
    k = std::clamp(k, 0, 5);
    s.yaw = yaws[k];
    s.pitch = pitches[k];
    s.ortho = true;
    s.target = (s.data.boundsMin + s.data.boundsMax) * 0.5f;
}

void App::DrawView3D() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    bool open = ImGui::Begin("Vue 3D", PanelFlag("Vue 3D"), ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    if (open) {
        view3dShownFrame = ImGui::GetFrameCount();
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && (ImGui::IsMouseClicked(0) || ImGui::IsMouseClicked(1) || ImGui::IsMouseClicked(2))) workView = 1;
    }
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
            if (button("Cuire...")) baker.open = true;
            ui::HelpTooltip("Baker : normale, occlusion ambiante, courbure, épaisseur... depuis le modèle ou un modèle haute définition.");
            if (button("Unreal...")) unrealDialog = true;
            ui::HelpTooltip("Exporter pour Unreal : modèle, textures et matériau prêt à l'emploi,\nimportés d'un coup par le plugin Shaderixel Link.");
            if (button("Exporter les textures...")) {
                texExport.open = true;
                texExport.preset = doc.exportTemplate == 1 ? 0 : doc.exportTemplate == 2 ? 1 : 5;
            }
            ui::HelpTooltip("Modèles d'export (Unreal, Unity, glTF / Blender, Godot, séparées), 8 / 16 bits, taille, normale, marge.");
        } else {
            if (missing && button(hasChannels ? "Compléter les canaux PBR" : "Créer les canaux PBR")) CreatePbrChannels(*v);
            if (hasChannels && button("Exporter les textures PBR..."))
                ExportPbrTextures(*v, platform::SaveFileDialog(hwnd, L"Nom de base des textures (*.png)\0*.png\0", L"png"));
        }
        const std::string envLabel = v->mesh->envAtlas || !v->mesh->envPixels.empty() ? v->mesh->envName + " \xE2\x96\xBE" : std::string("HDRI \xE2\x96\xBE");
        if (button(envLabel.c_str())) ImGui::OpenPopup("##envmenu");
        ui::HelpTooltip("Environnement : intégré (studio, ciel couvert, plein soleil, coucher de soleil, néons) ou fichier HDR.");
        if (ImGui::BeginPopup("##envmenu")) {
            for (auto& [id, label] : ProceduralEnvironments())
                if (ImGui::Selectable(label.c_str(), v->mesh->envAtlas && v->mesh->envPath == id)) {
                    std::string err;
                    if (!LoadEnvironment(*v->mesh, id, &err)) Status("HDRI : " + err);
                }
            ImGui::Separator();
            if (ImGui::Selectable("Fichier HDR...")) {
                std::string path = platform::OpenFileDialog(hwnd, L"Environnement HDR (*.hdr)\0*.hdr\0Images\0*.png;*.jpg;*.exr\0");
                std::string err;
                if (!path.empty() && !LoadEnvironment(*v->mesh, path, &err)) Status("HDRI : " + err);
            }
            ImGui::EndPopup();
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
            static const char* modes = "Rendu PBR\0Couleur de base\0Rugosité\0Métal\0Normale\0Émissif\0Occlusion\0Hauteur\0"
                                       "Carte : occlusion (cuite)\0Carte : convexité\0Carte : concavité\0Carte : épaisseur\0";
            flow(140 * dpi);
            ImGui::SetNextItemWidth(140 * dpi);
            if (ImGui::Combo("##viewch", &v->mesh->viewChannel, modes) && v->mesh->viewChannel >= 8 && !v->mesh->maps) StartMeshMapsBake(*v);
            ui::HelpTooltip("Afficher un canal seul (tel que peint), ou une carte du maillage cuite (masques intelligents)");
            flow(100 * dpi);
            ImGui::SetNextItemWidth(100 * dpi);
            ImGui::SliderFloat("##expo", &v->mesh->post.exposure, -4, 4, "Expo %.1f EV");
            ui::HelpTooltip("Exposition de la vue (double-clic pour saisir)");
            flow(110 * dpi);
            ImGui::SetNextItemWidth(110 * dpi);
            ImGui::Combo("##quality", &v->mesh->quality, "Rendu rapide\0Rendu qualité\0Path tracing\0");
            ui::HelpTooltip("Rendu rapide : rastérisation simple.\nRendu qualité : ombre de la lumière principale, occlusion, sol.\nPath tracing : le moteur du mode Rendu dans la vue (reflets, ombres douces, lumière indirecte), affiné quand rien ne bouge.");
            if (v->mesh->quality == 2 && v->viewRT) {
                char rt[48];
                snprintf(rt, sizeof(rt), "%d / %d éch.", RenderSamples(*v->viewRT), v->mesh->rtSamples);
                flow(ImGui::CalcTextSize(rt).x + 8 * dpi);
                ImGui::TextDisabled("%s", rt);
            }
            if (button("Rendu et caméra...")) ImGui::OpenPopup("##view3drender");
            if (button("Turntable...")) turntable.open = true;
            ui::HelpTooltip("Exporter une vidéo (MP4) ou des images du modèle qui tourne sur lui-même.");
            if (ImGui::BeginPopup("##view3drender")) {
                DrawView3DRenderSettings(*v->mesh);
                ImGui::EndPopup();
            }
            flow(ImGui::GetFrameHeight() + ImGui::CalcTextSize("Fil de fer").x + 12 * dpi);
            ImGui::Checkbox("Fil de fer", &v->mesh->wireframe);
            flow(ImGui::GetFrameHeight() + ImGui::CalcTextSize("Ortho").x + 12 * dpi);
            ImGui::Checkbox("Ortho", &v->mesh->ortho);
            ui::HelpTooltip("Vue orthographique (projection parallèle), aussi en path tracing. Pavé numérique 5.");
            if (button("Vues \xE2\x96\xBE")) ImGui::OpenPopup("##views3d");
            ui::HelpTooltip("Vues de face, de côté, de dessus (orthographiques)\nPavé numérique : 1 face, 3 droite, 7 dessus (Ctrl : opposé), 5 perspective / ortho");
            if (ImGui::BeginPopup("##views3d")) {
                static const char* names[] = {"Face", "Arrière", "Gauche", "Droite", "Dessus", "Dessous"};
                for (int k = 0; k < 6; ++k)
                    if (ImGui::Selectable(names[k])) SetView3DPreset(*v->mesh, k);
                ImGui::Separator();
                if (ImGui::Selectable("Perspective", !v->mesh->ortho)) v->mesh->ortho = false;
                if (ImGui::Selectable("Orthographique", v->mesh->ortho)) v->mesh->ortho = true;
                ImGui::EndPopup();
            }
        }
        char info[160];
        snprintf(info, sizeof(info), "%s - %zu triangles", v->mesh->data.name.c_str(), v->mesh->data.indices.size() / 3);
        flow(ImGui::CalcTextSize(info).x + 20 * dpi);
        ImGui::TextDisabled("%s", info);
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        ui::HelpTooltip("Clic droit / Alt+clic : orbite   Molette : zoom   Clic molette : déplacer\nMaj + clic droit : tourner l'éclairage (HDRI et lumières)\nPinceau / gomme : peindre sur le modèle");
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
    // Turntable export: the frames are rendered at their own size, nothing is painted meanwhile.
    const bool tt = turntable.running && turntable.view == v;
    bool hovered = ImGui::IsItemHovered() && !tt;
    ImGuiIO& io = ImGui::GetIO();

    // Camera
    bool orbit = ImGui::IsItemActive() && (ImGui::IsMouseDown(ImGuiMouseButton_Right) || (io.KeyAlt && ImGui::IsMouseDown(ImGuiMouseButton_Left)));
    if (orbit && io.KeyShift && ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
        // Shift + right drag (Substance-like): the lighting turns around the model, not the camera.
        const float a = io.MouseDelta.x * 0.01f;
        if (a != 0) {
            if (s.lightFollow) {
                // The key light leaves the camera where it is, then turns.
                const float3 l = s.KeyLightDir();
                s.lightYaw = std::atan2(l.x, -l.z);
                s.lightPitch = std::asin(std::clamp(l.y, -1.0f, 1.0f));
                s.lightFollow = false;
            }
            s.lightYaw += a;
            s.envRotation += a;
            for (auto& x : s.lights)
                if (!x.follow) x.yaw += a;
            char b[64];
            snprintf(b, sizeof(b), "Éclairage : %.0f°", std::remainder(s.lightYaw * 57.29578f, 360.0f));
            statusMsg = b;
        }
    } else if (orbit) {
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
    // Projection tool / stencil: Ctrl+wheel resizes, Shift+wheel turns the image; otherwise the wheel zooms.
    const bool stencilActive = decal.stencil && decal.tex && (tool == Tool::Brush || tool == Tool::Eraser);
    if (hovered && io.MouseWheel != 0 && (tool == Tool::Projection || stencilActive) && (io.KeyCtrl || io.KeyShift)) {
        if (io.KeyCtrl) decal.size = std::clamp(decal.size * std::pow(1.1f, io.MouseWheel), 0.02f, 3.0f);
        else decal.rotation = std::fmod(decal.rotation + io.MouseWheel * 5.0f + 540.0f, 360.0f) - 180.0f;
    } else if (hovered && io.MouseWheel != 0) s.distance *= std::pow(0.88f, io.MouseWheel);
    // Numeric keypad (Blender-like): views, perspective / orthographic.
    if (hovered && !io.WantTextInput) {
        if (ImGui::IsKeyPressed(ImGuiKey_Keypad5, false)) s.ortho = !s.ortho;
        if (ImGui::IsKeyPressed(ImGuiKey_Keypad1, false)) SetView3DPreset(s, io.KeyCtrl ? 1 : 0);
        if (ImGui::IsKeyPressed(ImGuiKey_Keypad3, false)) SetView3DPreset(s, io.KeyCtrl ? 2 : 3);
        if (ImGui::IsKeyPressed(ImGuiKey_Keypad7, false)) SetView3DPreset(s, io.KeyCtrl ? 5 : 4);
    }
    UpdateCameraAnimation(s);
    if (tt) TurntableFrame(*v, w, h);
    // Depth of field: a click on the model chooses the sharp distance.
    if (focusPick && hovered) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.KeyAlt) {
            if (PickFocus(s, float2{io.MousePos.x - origin.x, io.MousePos.y - origin.y}, w, h)) {
                char b[64];
                snprintf(b, sizeof(b), "Mise au point : %.2f", s.dofFocus);
                Status(b);
            } else {
                Status("Mise au point : cliquez sur le modèle");
            }
            focusPick = false;
            io.MouseClicked[ImGuiMouseButton_Left] = false;  // not a paint stroke
        }
    }

    MeshPrepare3D(*v);
    UpdateSmartMasks(*v);
    UpdateProjections(*v);

    // Painting
    std::vector<PointerSample> samples = frameSamples;
    Id target = doc.PaintTarget();
    bool paintTool = tool == Tool::Brush || tool == Tool::Eraser;
    auto local = [&](float x, float y) { return float2{x - origin.x, y - origin.y}; };
    // Layers painted together (3D project paint layer: one per channel) and their values.
    std::vector<float4> meshColors;
    std::vector<Id> meshTargets = Paint3DTargets(*v, meshColors);
    // Smart mask corrections: the eraser paints black (removes the material) instead of erasing them.
    bool eraseStroke = tool == Tool::Eraser;
    if (doc.PaintsMaskEdits()) {
        for (auto& c : meshColors) c = eraseStroke ? float4{0, 0, 0, 1} : float4{1, 1, 1, 1};
        eraseStroke = false;
    }
    // Stencil: placed at the view center at first; S + drag moves it (no painting meanwhile).
    const float stH = decal.size * (float)h * 0.5f, stW = decal.h > 0 ? stH * decal.w / decal.h : stH;
    bool stencilMoving = false;
    if (stencilActive) {
        if (!decal.stencilPlaced) {
            decal.stencilPos = {w * 0.5f, h * 0.5f};
            decal.stencilPlaced = true;
        }
        if (hovered && ImGui::IsKeyDown(ImGuiKey_S) && !io.WantTextInput) {
            stencilMoving = true;
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) decal.stencilPos = decal.stencilPos + float2{io.MouseDelta.x, io.MouseDelta.y};
        }
        v->renderer.meshStencilOn = true;
        v->renderer.meshStencilTex = decal.tex.get();
        v->renderer.meshStencil = {decal.stencilPos.x, decal.stencilPos.y, stW, stH};
        const float a = decal.rotation * 0.0174533f;
        v->renderer.meshStencil2 = {std::cos(a), std::sin(a), decal.stencilInvert ? 1.0f : 0.0f, decal.stencilLuminance ? 1.0f : 0.0f};
        v->renderer.meshStencil3 = {decal.stencilThreshold, decal.stencilContrast, 0, 0};
    } else {
        v->renderer.meshStencilOn = false;
    }
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
    if (paintTool && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.KeyAlt && !stencilMoving) {
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
            v->renderer.MeshStrokeDabsMulti(doc, *ctx, s, first, b, meshColors, eraseStroke, &dabEx);
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
        if (!dabs.empty()) v->renderer.MeshStrokeDabsMulti(doc, *ctx, s, dabs, b, meshColors, eraseStroke, &dabEx);
        dabEx.clear();
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) || io.KeyShift) {
            v->renderer.EndStrokeMulti(doc, *ctx, v->history, "Peinture 3D");
            meshStroking = false;
            meshLineEnd = meshLastDab;
            meshLineValid = true;
        }
    }
    // Paint simulation: particles emitted while the button is held, simulated until they stop.
    {
        const bool emitting = tool == Tool::Particles && hovered && ImGui::IsMouseDown(ImGuiMouseButton_Left) && !io.KeyAlt && !orbit;
        if (emitting || !sim.parts.empty() || sim.stroking) {
            if (emitting && meshTargets.empty()) Status("Sélectionnez un calque 3D pour la simulation");
            v->renderer.MeshRender(*ctx, s, GatherMaterial(*v), w, h);  // depth for the brush kernel
            UpdateSimulation(*v, emitting && !meshTargets.empty(), local(io.MousePos.x, io.MousePos.y), w, h, meshTargets, meshColors);
        }
    }
    if (tool == Tool::Plugin) PluginTool3DInput(*v, local(io.MousePos.x, io.MousePos.y), w, h, hovered);
    // Eyedropper: the material under the cursor goes to the brush material (color, roughness, metal, height).
    if (tool == Tool::Picker && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.KeyAlt)
        StartMaterialPick(*v, local(io.MousePos.x, io.MousePos.y), w, h);
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
    if (s.quality == 2 && s.viewChannel == 0)
        if (gpu::Texture* rt = UpdateViewRayTrace(*v, w, h)) img = rt;  // ray-traced view (the raster render keeps the brush depth)
    if (gpu::Texture* pv = UvPreviewRender(*v, w, h)) img = pv;  // UV tool: checker on the new layout
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (img) {
        ImVec2 a = origin, b(origin.x + w, origin.y + h);
        if (tt) {
            // Turntable frame (its own size) fitted in the view.
            const float k = std::min(avail.x / w, avail.y / h);
            a = ImVec2(origin.x + (avail.x - w * k) * 0.5f, origin.y + (avail.y - h * k) * 0.5f);
            b = ImVec2(a.x + w * k, a.y + h * k);
            dl->AddRectFilled(origin, ImVec2(origin.x + avail.x, origin.y + avail.y), IM_COL32(16, 16, 18, 255));
        }
        dl->AddImage((ImTextureID)gpu::Dev().UiTextureId(img), a, b);
    }
    if (tt) TurntableCapture(*v, img);
    if (hovered && tool == Tool::Projection && !orbit && decal.tex) {
        // Decal preview at the cursor (rotated quad).
        float c = std::cos(decal.rotation * 0.0174533f), sn = std::sin(decal.rotation * 0.0174533f);
        auto corner = [&](float x, float y) { return ImVec2(io.MousePos.x + c * x - sn * y, io.MousePos.y + sn * x + c * y); };
        ImVec2 q[4] = {corner(-decalW, -decalH), corner(decalW, -decalH), corner(decalW, decalH), corner(-decalW, decalH)};
        dl->AddImageQuad((ImTextureID)gpu::Dev().UiTextureId(decal.tex.get()), q[0], q[1], q[2], q[3], ImVec2(0, 0), ImVec2(1, 0), ImVec2(1, 1), ImVec2(0, 1),
                         IM_COL32(255, 255, 255, 150));
        dl->AddPolyline(q, 4, IM_COL32(255, 140, 0, 220), ImDrawFlags_Closed, 1.5f);
    }
    if (s.wireframe && !tt) {
        // Edges of the front-facing triangles (capped for heavy models).
        const auto& V = s.data.vertices;
        const auto& I = s.data.indices;
        const size_t maxTris = 60000;
        const size_t step = std::max<size_t>(1, I.size() / 3 / maxTris);
        auto proj = [&](float3 p, bool& ok) {
            float4 c = s.viewProj.Transform({p.x, p.y, p.z, 1});
            ok = c.w > 1e-5f;
            return ImVec2(origin.x + (c.x / c.w * 0.5f + 0.5f) * w, origin.y + (0.5f - c.y / c.w * 0.5f) * h);
        };
        for (size_t t = 0; t + 2 < I.size(); t += 3 * step) {
            bool o0, o1, o2;
            ImVec2 a = proj(V[I[t]].pos, o0), b = proj(V[I[t + 1]].pos, o1), c = proj(V[I[t + 2]].pos, o2);
            if (!o0 || !o1 || !o2) continue;
            if ((b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x) > 0) continue;  // back face
            ImVec2 tri[3] = {a, b, c};
            dl->AddPolyline(tri, 3, IM_COL32(120, 200, 255, 90), ImDrawFlags_Closed, 1.0f);
        }
    }
    if (stencilActive) {
        // The stencil on screen (where it paints, semi-transparent).
        float c = std::cos(decal.rotation * 0.0174533f), sn = std::sin(decal.rotation * 0.0174533f);
        ImVec2 ctr(origin.x + decal.stencilPos.x, origin.y + decal.stencilPos.y);
        auto corner = [&](float x, float y) { return ImVec2(ctr.x + c * x - sn * y, ctr.y + sn * x + c * y); };
        ImVec2 q[4] = {corner(-stW, -stH), corner(stW, -stH), corner(stW, stH), corner(-stW, stH)};
        dl->PushClipRect(origin, ImVec2(origin.x + w, origin.y + h), true);
        dl->AddImageQuad((ImTextureID)gpu::Dev().UiTextureId(decal.tex.get()), q[0], q[1], q[2], q[3], ImVec2(0, 0), ImVec2(1, 0), ImVec2(1, 1), ImVec2(0, 1),
                         IM_COL32(255, 255, 255, stencilMoving ? 150 : 55));
        dl->AddPolyline(q, 4, stencilMoving ? IM_COL32(255, 200, 80, 230) : IM_COL32(120, 200, 255, 160), ImDrawFlags_Closed, 1.5f);
        dl->PopClipRect();
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

void App::DrawView3DRenderSettings(MeshScene& s) {
    ImGui::PushItemWidth(220 * dpi);
    ImGui::SeparatorText("Lumière principale");
    ImGui::Checkbox("Suit la caméra", &s.lightFollow);
    if (!s.lightFollow) {
        float yaw = s.lightYaw * 57.29578f, pitch = s.lightPitch * 57.29578f;
        if (ImGui::SliderFloat("Direction", &yaw, -180, 180, "%.0f°")) s.lightYaw = yaw / 57.29578f;
        if (ImGui::SliderFloat("Hauteur", &pitch, 5, 89, "%.0f°")) s.lightPitch = pitch / 57.29578f;
    }
    DrawLightingSettings(s);
    if (s.quality != 2) {
        ImGui::SeparatorText("Path tracing");
        ImGui::TextDisabled("Échantillons, rebonds, débruitage et profondeur de champ :");
        if (ImGui::SmallButton("Passer en Path tracing")) s.quality = 2;
    } else {
        ImGui::SeparatorText("Path tracing");
        ImGui::SliderInt("Échantillons", &s.rtSamples, 16, 4096, "%d", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderInt("Rebonds", &s.rtBounces, 1, 16);
        ImGui::Checkbox("Sol (ombres portées)", &s.rtFloor);
        ImGui::SliderFloat("Débruitage", &s.rtDenoise, 0, 1, s.rtDenoise > 0 ? "%.2f" : "désactivé");
        ImGui::SeparatorText("Profondeur de champ");
        ImGui::SliderFloat("Ouverture", &s.dofAperture, 0, 0.15f, s.dofAperture > 0 ? "%.3f" : "désactivée", ImGuiSliderFlags_Logarithmic);
        ui::HelpTooltip("Taille de l'objectif : plus elle est grande, plus ce qui est hors de la distance nette est flou.\nUn grand flou demande plus d'échantillons (et un débruitage plus fort) pour être lisse.");
        if (s.dofAperture > 0) {
            ImGui::SliderFloat("Distance nette", &s.dofFocus, 0.05f, 30, "%.2f", ImGuiSliderFlags_Logarithmic);
            if (ImGui::SmallButton("Choisir sur le modèle")) {
                focusPick = true;
                ImGui::CloseCurrentPopup();
            }
            ui::HelpTooltip("Puis cliquez sur le point du modèle qui doit être net.");
        }
    }
    ImGui::SeparatorText("Lumière");
    if (s.envAtlas) ImGui::SliderFloat("Soleil (avec HDRI)", &s.sunIntensity, 0, 8, "%.2f");
    else ImGui::SliderFloat("Intensité", &s.lightIntensity, 0, 8, "%.2f");
    ImGui::SliderFloat("Ombre", &s.shadowStrength, 0, 1, "%.2f");
    ImGui::Checkbox("Ombre au sol", &s.groundShadow);
    ImGui::SliderFloat("Occlusion d'écran", &s.ssao, 0, 2, s.ssao > 0 ? "%.2f" : "désactivée");
    ui::HelpTooltip("Rendu qualité : assombrit les creux et les contacts en temps réel (SSAO), sans cuisson.");
    ui::HelpTooltip("Rendu qualité : l'ombre de la lumière principale et une ombre de contact sur un sol invisible sous le modèle.");
    ImGui::SliderFloat("Occlusion cuite", &s.aoStrength, 0, 1, "%.2f");
    ui::HelpTooltip("Occlusion ambiante cuite du modèle (Cuisson ou masques intelligents) dans la vue, en Rendu qualité.");
    ImGui::PopItemWidth();
    DrawCameraBookmarks(s);
    DrawPostSettings(s.post, true);
    ImGui::TextDisabled("Rendu par lancer de rayons : onglet Rendu.");
}

bool App::DrawPostSettings(PostSettings& p, bool withExposure) {
    bool changed = false;
    ImGui::PushID(&p);
    ImGui::PushItemWidth(220 * dpi);
    ImGui::SeparatorText("Post-process");
    changed |= ImGui::Checkbox("Effets de caméra", &p.enabled);
    if (withExposure) changed |= ImGui::SliderFloat("Exposition", &p.exposure, -6, 6, "%.2f EV");
    changed |= ImGui::Combo("Courbe", &p.tonemap, "Linéaire\0ACES (film)\0Reinhard\0PBR Neutral (couleurs fidèles)\0AgX (film, naturel)\0Doux\0");
    ImGui::BeginDisabled(!p.enabled);
    changed |= ImGui::SliderFloat("Bloom", &p.bloom, 0, 2, "%.2f");
    if (p.bloom > 0) {
        changed |= ImGui::SliderFloat("Seuil du bloom", &p.bloomThreshold, 0, 4, "%.2f");
        changed |= ImGui::SliderFloat("Étendue du bloom", &p.bloomRadius, 0.2f, 1.2f, "%.2f");
    }
    changed |= ImGui::SliderFloat("Netteté", &p.sharpen, 0, 1, "%.2f");
    changed |= ImGui::SliderFloat("Aberration chromatique", &p.chroma, 0, 1, "%.2f");
    changed |= ImGui::SliderFloat("Température", &p.temperature, -1, 1, "%.2f");
    changed |= ImGui::SliderFloat("Teinte", &p.tint, -1, 1, "%.2f");
    changed |= ImGui::SliderFloat("Contraste", &p.contrast, -1, 1, "%.2f");
    changed |= ImGui::SliderFloat("Saturation", &p.saturation, 0, 2, "%.2f");
    changed |= ImGui::SliderFloat("Vignettage", &p.vignette, 0, 1, "%.2f");
    if (p.vignette > 0) changed |= ImGui::SliderFloat("Douceur du vignettage", &p.vignetteSoft, 0, 1, "%.2f");
    changed |= ImGui::SliderFloat("Grain", &p.grain, 0, 1, "%.2f");
    ImGui::EndDisabled();
    static const char* presets[] = {"Neutre", "Studio produit", "Cinéma", "Jeu vidéo", "Vintage"};
    if (ImGui::BeginCombo("Préréglage", "Choisir...")) {
        for (int i = 0; i < 5; ++i)
            if (ImGui::Selectable(presets[i])) {
                const float e = p.exposure;
                p = PostSettings{};
                p.exposure = e;
                if (i == 1) { p.tonemap = 3; p.sharpen = 0.25f; p.contrast = 0.08f; p.vignette = 0.15f; }
                if (i == 2) { p.tonemap = 4; p.bloom = 0.35f; p.contrast = 0.15f; p.saturation = 0.9f; p.vignette = 0.45f; p.grain = 0.25f; p.temperature = 0.1f; }
                if (i == 3) { p.tonemap = 1; p.bloom = 0.5f; p.bloomThreshold = 0.9f; p.sharpen = 0.3f; p.saturation = 1.1f; p.vignette = 0.25f; p.chroma = 0.15f; }
                if (i == 4) { p.tonemap = 2; p.contrast = -0.1f; p.saturation = 0.7f; p.temperature = 0.35f; p.vignette = 0.6f; p.grain = 0.5f; p.chroma = 0.3f; }
                changed = true;
            }
        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();
    ImGui::PopID();
    return changed;
}

void App::MeshPrepare3D(DocView& v) {
    if (v.mesh) v.renderer.MeshPrepare(*v.doc, *ctx, *v.mesh);
}

}  // namespace sx
