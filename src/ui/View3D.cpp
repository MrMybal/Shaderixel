// 3D viewport: preview the document on a mesh and paint directly on it.
#include "ui/App.h"
#include "ui/Filter.h"
#include "doc/Brushes.h"
#include "ui/Theme.h"
#include "ui/Lang.h"
#include "render/MeshPaint.h"

namespace sx {

namespace {
// A model point in view pixels (`depth` 1e30 behind the camera).
float2 ViewPoint(const Mat4& vp, float3 p, u32 w, u32 h, float* depth) {
    float4 c = vp.Transform({p.x, p.y, p.z, 1});
    if (c.w <= 1e-5f) { if (depth) *depth = 1e30f; return {-1e9f, -1e9f}; }
    if (depth) *depth = c.z / c.w;
    return {(c.x / c.w * 0.5f + 0.5f) * w, (0.5f - c.y / c.w * 0.5f) * h};
}
}  // namespace

void App::AttachMesh(DocView& v, MeshData data) {
    if (sim.view == &v) FinishParticles(true);
    if (fluidRun.active && fluidRun.view == &v) FluidFinish(true);
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
    // The simple actions as icons (their name in the tooltip); combos and sliders keep their text.
    const float ib = ImGui::GetFrameHeight();
    auto icon = [&](const char* id, int glyph, const char* tip, bool on = false) {
        flow(ib);
        return ui::IconButton(id, glyph, on, ib, tip);
    };
    auto replaceMesh = [this](DocView& target, MeshData mesh) {
        const bool noUv = !mesh.hasUvs;
        if (!noUv) PackUdims(mesh);
        AttachMesh(target, std::move(mesh));
        if (target.doc->kind == 1) target.doc->dirty = true;
        if (noUv) {
            uvTool.open = true;
            StartUvUnwrap(target);
            Status("Maillage sans UV : dépliage automatique proposé (aperçu, puis « Appliquer »)");
        }
    };
    if (icon("sphere", ui::IconSphere, "Sphère : remplace le modèle par une sphère")) replaceMesh(*v, MakeSphere());
    if (icon("cube", ui::IconCube, "Cube : remplace le modèle par un cube")) replaceMesh(*v, MakeCube());
    if (icon("plane", ui::IconPlane, "Plan : remplace le modèle par un plan")) replaceMesh(*v, MakePlane());
    if (icon("openmesh", ui::IconOpenMesh, "Ouvrir un maillage... (OBJ, FBX, glTF, GLB, uasset)")) {
        std::string path = platform::OpenFileDialog(hwnd, L"Maillages (*.obj, *.fbx, *.gltf, *.glb, *.uasset)\0*.obj;*.fbx;*.gltf;*.glb;*.uasset\0");
        if (!path.empty() && Narrow(Utf8Path(path).extension().wstring()) == ".uasset") {
            DocView* target = v;
            ConvertUAssetAsync(path, [this, target, replaceMesh](const std::string& glb) {
                MeshData m;
                std::string err;
                for (auto& d : docs)
                    if (d.get() == target && LoadMesh(glb, m, &err)) replaceMesh(*target, std::move(m));
            });
            path.clear();
        }
        if (!path.empty()) {
            MeshData m;
            std::string err;
            if (LoadMesh(path, m, &err)) replaceMesh(*v, std::move(m));
            else Status("Maillage : " + err);
        }
    }
    if (v->mesh) {
        if (icon("uvs", ui::IconUV, "UV automatiques...\nDépliage UV automatique (xatlas) avec options et aperçu avant d'appliquer.")) uvTool.open = true;
        if (v->mesh->data.materials.size() > 1 && v->mesh->data.uvTiles.empty() && v->mesh->data.udims.empty() &&
            v->mesh->data.triMaterial.size() * 3 == v->mesh->data.indices.size()) {
            if (button("Une tuile par matériau")) RetileMaterials(*v);
            ui::HelpTooltip("Chaque matériau du modèle reçoit sa propre zone de la texture (la peinture déjà faite suit) ;\n"
                            "les exports écrivent ensuite un jeu de textures par matériau.");
        }
        if (icon("recenter", ui::IconRecenter, "Recentrer la vue sur le modèle")) {
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
            if (icon("bake", ui::IconBake, "Cuire...\nBaker : normale, occlusion ambiante, courbure, épaisseur... depuis le modèle ou un modèle haute définition.")) baker.open = true;
            if (button("Unreal...")) unrealDialog = true;
            ui::HelpTooltip("Exporter pour Unreal : modèle, textures et matériau prêt à l'emploi,\nimportés d'un coup par le plugin Shaderixel Link.");
            if (icon("exporttex", ui::IconExport, "Exporter les textures...\nModèles d'export (Unreal, Unity, glTF / Blender, Godot, séparées), 8 / 16 bits, taille, normale, marge.")) {
                texExport.open = true;
                texExport.preset = doc.exportTemplate == 1 ? 0 : doc.exportTemplate == 2 ? 1 : 5;
            }
        } else {
            if (missing && button(hasChannels ? "Compléter les canaux PBR" : "Créer les canaux PBR")) CreatePbrChannels(*v);
            if (hasChannels && button("Exporter les textures PBR..."))
                ExportPbrTextures(*v, platform::SaveFileDialog(hwnd, L"Nom de base des textures (*.png)\0*.png\0", L"png"));
        }
        {
            const std::string tip = Tr("Environnement (HDRI) : intégré (studio, ciel couvert, plein soleil, coucher de soleil, néons) ou fichier HDR.") +
                                    (v->mesh->envAtlas || !v->mesh->envPixels.empty() ? "\n" + Tr("Actuel :") + " " + v->mesh->envName : std::string());
            if (icon("env", ui::IconEnvironment, tip.c_str(), v->mesh->envAtlas != nullptr)) ImGui::OpenPopup("##envmenu");
        }
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
                                       "Carte : occlusion (cuite)\0Carte : convexité\0Carte : concavité\0Carte : épaisseur\0Opacité\0Utilisateur 1\0Utilisateur 2\0Utilisateur 3\0Utilisateur 4\0";
            flow(140 * dpi);
            ImGui::SetNextItemWidth(140 * dpi);
            if (ImGui::Combo("##viewch", &v->mesh->viewChannel, modes) && v->mesh->viewChannel >= 8 && v->mesh->viewChannel <= 11 && !v->mesh->maps) StartMeshMapsBake(*v);
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
            if (icon("rendercam", ui::IconRenderSettings, "Rendu et caméra... (lumières, post-traitement, focale)")) ImGui::OpenPopup("##view3drender");
            if (icon("turntable", ui::IconTurntable, "Turntable...\nExporter une vidéo (MP4) ou des images du modèle qui tourne sur lui-même.")) turntable.open = true;
            if (ImGui::BeginPopup("##view3drender")) {
                DrawView3DRenderSettings(*v->mesh);
                ImGui::EndPopup();
            }
            if (icon("wire", ui::IconWireframe, "Fil de fer : afficher les arêtes des triangles", v->mesh->wireframe)) v->mesh->wireframe = !v->mesh->wireframe;
            if (icon("ortho", ui::IconOrtho, "Vue orthographique (projection parallèle), aussi en path tracing. Pavé numérique 5.", v->mesh->ortho))
                v->mesh->ortho = !v->mesh->ortho;
            if (icon("views", ui::IconViews, "Vues de face, de côté, de dessus (orthographiques)\nPavé numérique : 1 face, 3 droite, 7 dessus (Ctrl : opposé), 5 perspective / ortho"))
                ImGui::OpenPopup("##views3d");
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
    if (v->mesh) {
        // 3D paint bar: the tools that paint on the model, and the stroke options shown on it.
        ImGui::SetCursorPos(ImVec2(rowX, ImGui::GetCursorPosY() + 3 * dpi));
        first = true;
        const ImVec2 barMin = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(barMin.x - 6 * dpi, barMin.y - 3 * dpi),
                                                  ImVec2(barMin.x + ImGui::GetContentRegionAvail().x - 2 * dpi, barMin.y + ib + 3 * dpi),
                                                  ImGui::GetColorU32(ImGuiCol_FrameBg, 0.6f), 5 * dpi);
        struct T3 { Tool t; int icon; const char* tip; };
        static const T3 tools3d[] = {
            {Tool::Brush, ui::IconBrush, "Pinceau (B) : peint le matériau sur le modèle"},
            {Tool::Eraser, ui::IconEraser, "Gomme (E) : efface la peinture du calque"},
            {Tool::PolyFill, ui::IconPolyFill, "Remplir : triangle, face, île UV ou pièce sous le curseur"},
            {Tool::Projection, ui::IconStamp, "Décalque (P) : pose une image sur le modèle"},
            {Tool::Particles, ui::IconParticles, "Particules : gouttes, poussière, éclaboussures qui peignent où elles passent"},
            {Tool::Fluid, ui::IconFluid, "Peinture liquide : coule avec la gravité"},
            {Tool::Picker, ui::IconPicker, "Pipette (I)"},
            {Tool::Clone, ui::IconStamp, "Tampon de matière : Ctrl+clic place la source, glisser duplique"},
            {Tool::Smudge, ui::IconFinger, "Doigt : étaler les canaux cochés sur la surface"},
            {Tool::SurfaceMask, ui::IconMask, "Masque rapide : peindre protège, Ctrl+peindre libère"},
            {Tool::SurfacePath, ui::IconPen, "Tracé de surface : cliquer pour poser un point, glisser pour le déplacer"},
        };
        for (const T3& t3 : tools3d) {
            char id[16];
            snprintf(id, sizeof(id), "p3d%d", (int)t3.t);
            if (icon(id, t3.icon, TrC(t3.tip), tool == t3.t)) {
                tool = t3.t;
                viewTool[1] = t3.t;
                workView = 1;
            }
        }
        flow(8 * dpi);
        ImGui::Dummy(ImVec2(6 * dpi, ib));
        int& sym = v->renderer.meshSymmetry;
        if (icon("sym3d", ui::IconSymmetry, TrC("Symétrie : chaque trait est aussi peint de l'autre côté du modèle (clic : choisir l'axe)"), sym != 0))
            ImGui::OpenPopup("##sym3d");
        if (sym != 0) {
            static const char* axes[] = {"", "X", "Y", "Z"};
            flow(ImGui::CalcTextSize("X").x + 4 * dpi);
            ImGui::TextUnformatted(axes[std::clamp(sym, 0, 3)]);
        }
        if (ImGui::BeginPopup("##sym3d")) {
            static const char* names[] = {"Aucune", "Axe X (gauche / droite)", "Axe Y (haut / bas)", "Axe Z (avant / arrière)"};
            for (int k = 0; k < 4; ++k)
                if (ImGui::Selectable(TrC(names[k]), sym == k)) sym = k;
            ImGui::EndPopup();
        }
        if (icon("normal3d", ui::IconNormal,
                 TrC("Projection selon la normale moyenne : la touche est posée à plat sur la surface sous le pinceau\n(marques rondes sur les surfaces vues de biais)"),
                 brush3D.normalProject))
            brush3D.normalProject = !brush3D.normalProject;
        if (icon("uvwire3d", ui::IconUV, TrC("Afficher le fil des UV du modèle sur la texture"), showUvWire)) showUvWire = !showUvWire;
        {
            BrushSettings& bs = tool == Tool::Clone || tool == Tool::Smudge ? retouch : tool == Tool::Eraser ? eraser : brush;
            flow(150 * dpi);
            ImGui::SetNextItemWidth(150 * dpi);
            if ((brush3D.modelSize && (tool == Tool::Brush || tool == Tool::Eraser)) ||
                (tool == Tool::Particles && (brush3D.modelSize || particleBrush.customMaterial)))
                ImGui::SliderFloat("##size3d", &brush3D.modelDiameter, 0.005f, 1.0f, TrC("Diamètre modèle %.3f"), ImGuiSliderFlags_Logarithmic);
            else ImGui::SliderFloat("##size3d", &bs.size, 1, 600, TrC("Taille %.0f px"), ImGuiSliderFlags_Logarithmic);
            flow(110 * dpi);
            ImGui::SetNextItemWidth(110 * dpi);
            ImGui::SliderFloat("##flow3d", &bs.flow, 0.01f, 1, TrC("Flux %.2f"));
            if (tool == Tool::Clone) { flow(175*dpi); ImGui::Checkbox("Échantillonner les calques visibles", &cloneVisible3D); }
            if (tool == Tool::Smudge) { flow(110*dpi); ImGui::SetNextItemWidth(110*dpi); ImGui::SliderFloat("Force##doigt", &retouchStrength, 0.01f, 1); }
            if (tool == Tool::SurfaceMask && v->doc->Selection()) {
                flow(140*dpi);
                if (ImGui::SmallButton("Inverser la protection"))
                    v->renderer.SelectionFilter(*v->doc, *ctx, v->history, 3, 0, "Inverser la protection");
                flow(120*dpi);
                if (ImGui::SmallButton("Lever la protection")) v->renderer.Deselect(*v->doc,v->history);
            }
            if (tool == Tool::SurfacePath) SurfacePathOptions(*v);
        }
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
    UpdateProjections(*v);
    UpdateSmartMasks(*v);
    s.UpdateCamera((float)w / h);
    bool refreshedDepth = false;
    auto refreshDepth = [&] {
        if (!refreshedDepth) {
            v->renderer.MeshRender(*ctx, s, GatherMaterial(*v), w, h);
            refreshedDepth = true;
        }
    };

    if (sim.view && tool != Tool::Particles) CheckParticleSession();
    // Painting
    std::vector<PointerSample> samples = frameSamples;
    Id target = doc.PaintTarget();
    bool paintTool = tool == Tool::Brush || tool == Tool::Eraser;
    auto local = [&](float x, float y) { return float2{x - origin.x, y - origin.y}; };
    // Layers painted together (3D project paint layer: one per channel) and their values.
    std::vector<float4> meshColors;
    std::vector<Id> meshTargets = Paint3DTargets(*v, meshColors);
    // Smart mask corrections: the eraser paints black (removes the material) instead of erasing them.
    const PointerSample currentPen = platform::PenActive() ? platform::LastPenSample() : PointerSample{};
    const bool eraserEnd = currentPen.pen && currentPen.eraser;
    const bool physicalErase = meshStroking ? meshStrokeEraser : tool == Tool::Eraser || eraserEnd;
    bool eraseStroke = physicalErase;
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
    // The paint follows the cursor: a continuous brush lays a dab every screen pixel (a fraction of a large diameter
    // left millimetres unpainted while moving slowly), each dab's flow lowered so the stroke builds up as it did
    // with the brush's own spacing. Stamp brushes (spacing over 0.35: stitches, rivets, splatter...) keep theirs.
    float flowExponent = 1;  // dab flow f -> 1 - (1 - f)^exponent
    auto brushDiameter = [&](const BrushSettings& b, float2 point, bool forceModelSize = false) {
        if (!brush3D.modelSize && !forceModelSize) return b.size;
        if (s.ortho) return std::max(1.f, brush3D.modelDiameter * h / (2 * s.OrthoHalfHeight()));
        float3 originRay, direction, hit;
        CameraRay(s, point, w, h, originRay, direction);
        float depth = s.distance;
        if (s.Rays().Ray(originRay, direction, 100.f, &hit, nullptr))
            depth = std::max(0.02f, Dot(hit - s.camPos, Normalize(s.target - s.camPos)));
        return std::max(1.f, brush3D.modelDiameter * h / (2 * std::tan(s.fov * 0.5f) * depth));
    };
    auto spacingOf = [&](const BrushSettings& b) {
        float sp = b.spacing;
        if (brush3D.tip)
            if (auto td = TipDef(b); td && td->spacing > 0) sp = td->spacing;
        const float px = std::max(1.0f, brushDiameter(b, local(io.MousePos.x, io.MousePos.y)) * std::max(sp, 0.02f));
        const float used = sp <= 0.35f ? std::min(px, 1.0f) : px;
        flowExponent = used / px;
        return used;
    };
    std::vector<float4> dabEx;
    auto rnd = [&] {
        meshRng = meshRng * 1664525u + 1013904223u;
        return (meshRng >> 8) * (1.0f / 16777216.0f);
    };
    auto tipAngle = [&](const PointerSample& sample) {
        float a = brush3D.angle * 0.0174533f;
        if (sample.pen && brush3D.followTilt && std::abs(sample.tiltX) + std::abs(sample.tiltY) > 1)
            a += std::atan2(sample.tiltY, sample.tiltX);
        if (sample.pen && brush3D.followTwist) a += sample.twist * 0.0174533f;
        return a;
    };
    auto addDab = [&](std::vector<Dab>& out, float2 q, float pr, float2 dir, const BrushSettings& b, const PointerSample& sample) {
        const auto& B = brush3D;
        const float diameter = brushDiameter(b, q);
        float rad = diameter * 0.5f * (b.pressureSize ? std::max(pr, 0.05f) : 1.0f);
        rad *= std::max(0.05f, 1.0f + B.jitterSize * (rnd() * 2 - 1));
        if (B.scatter > 0) {
            const float ang = rnd() * 6.2831853f, dist = std::sqrt(rnd()) * B.scatter * diameter;
            q = q + float2{std::cos(ang) * dist, std::sin(ang) * dist};
        }
        float angle = tipAngle(sample) + B.jitterAngle * 3.14159265f * (rnd() * 2 - 1);
        if (B.follow && Length(dir) > 1e-4f) angle += std::atan2(dir.y, dir.x);
        float alpha = b.flow * (b.pressureOpacity ? pr : 1.0f) * (1.0f - B.jitterFlow * rnd());
        if (flowExponent < 1) alpha = 1.0f - std::pow(std::max(0.0f, 1.0f - alpha), flowExponent);  // (denser dabs)
        out.push_back({q.x, q.y, rad, alpha});
        dabEx.push_back({std::cos(angle), std::sin(angle), (float)((int)(rnd() * v->renderer.meshTipVariants) % v->renderer.meshTipVariants), 0});
    };
    if (paintTool && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.KeyAlt && !stencilMoving) {
        if (!target || meshTargets.empty()) Status("Sélectionnez un calque de pixels pour peindre en 3D");
        else if (v->renderer.BeginStrokeMulti(doc, *ctx, meshTargets)) {
            meshStrokeEraser = physicalErase;
            refreshDepth();
            v->renderer.meshNormalMode = brush3D.normalProject;
            const BrushSettings& b = meshStrokeEraser ? eraser : brush;
            setupTip(b);
            spacingOf(b);  // (the dabs' flow for this brush, the first dab included)
            float2 p = local(io.MousePos.x, io.MousePos.y);
            // Limited to the material under the brush: its UV tile.
            v->renderer.meshClip = {};
            if (brush3D.limitMaterial && !s.data.udims.empty()) {
                // UDIM: the tile under the brush (its cell of the texture).
                const int tri = PickMeshTriangle(s, p, w, h);
                if (tri >= 0) {
                    float2 c = {0, 0};
                    for (int k = 0; k < 3; ++k) c = c + s.data.vertices[s.data.indices[tri * 3 + k]].uv;
                    if (const int cell = UdimCellAt(s.data, c * (1.0f / 3.0f)); cell >= 0) {
                        const float4 t = UdimRect(s.data, cell);
                        v->renderer.meshClip = {(int)std::floor(t.x * doc.Width()), (int)std::floor(t.y * doc.Height()),
                                                (int)std::ceil(t.z * doc.Width()), (int)std::ceil(t.w * doc.Height())};
                    }
                }
            } else if (brush3D.limitMaterial && !s.data.uvTiles.empty() && s.data.triMaterial.size() * 3 == s.data.indices.size()) {
                const int tri = PickMeshTriangle(s, p, w, h);
                if (tri >= 0) {
                    const u16 mi = s.data.triMaterial[tri];
                    if (mi < s.data.uvTiles.size()) {
                        const float4 t = s.data.uvTiles[mi];
                        v->renderer.meshClip = {(int)std::floor(t.x * doc.Width()), (int)std::floor(t.y * doc.Height()), (int)std::ceil(t.z * doc.Width()),
                                                (int)std::ceil(t.w * doc.Height())};
                    }
                }
            }
            float pr = platform::PenActive() ? PenPressure(platform::LastPressure()) : 1.0f;
            std::vector<Dab> first;
            if (io.KeyShift && meshLineValid) {
                // Straight line from the end of the last stroke (in the view: the camera has to stay put).
                float2 d = p - meshLineEnd;
                float len = Length(d);
                const float sp = spacingOf(b);
                for (float t = 0; t <= len; t += sp) addDab(first, meshLineEnd + d * (t / std::max(len, 1e-4f)), pr, d, b, currentPen);
            } else {
                addDab(first, p, pr, {0, 0}, b, currentPen);
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
            refreshDepth();
            float2 p = local(io.MousePos.x, io.MousePos.y);
            Layer* al = doc.ActiveLayer();
            bool imageColor = decal.imageColor && al && al->material && IsPaintLayer3D(doc, al->material) && brush3D.use[(int)PbrChannel::BaseColor];
            v->renderer.MeshDecalMulti(doc, *ctx, s, decal.tex.get(), p, {decalW, decalH}, decal.rotation * 0.0174533f, decal.opacity, meshColors, imageColor);
            v->renderer.EndStrokeMulti(doc, *ctx, v->history, "Projection");
        }
    }
    if (meshStroking) {
        const BrushSettings& b = meshStrokeEraser ? eraser : brush;
        if (brush3D.tip && !v->renderer.meshTipOn) setupTip(b);  // the tip may still be compiling at the first dab
        if (samples.empty()) {
            samples.push_back(currentPen);
            samples.back().x = io.MousePos.x;
            samples.back().y = io.MousePos.y;
        }
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
                addDab(dabs, meshLastDab + d * (t / std::max(len, 1e-4f)), pr, d, b, sm);
                t += spacing;
            }
            meshDabRemainder = std::max(0.0f, len - (t - spacing));
            if (len > 0) meshLastDab = p;
        }
        // Render first so the occlusion test uses this frame's depth.
        v->renderer.meshNormalMode = brush3D.normalProject;
        if (!dabs.empty()) {
            refreshDepth();
            v->renderer.MeshStrokeDabsMulti(doc, *ctx, s, dabs, b, meshColors, eraseStroke, &dabEx);
        }
        dabEx.clear();
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) || io.KeyShift) {
            v->renderer.EndStrokeMulti(doc, *ctx, v->history, "Peinture 3D");
            v->renderer.meshClip = {};
            meshStroking = false;
            meshLineEnd = meshLastDab;
            meshLineValid = true;
        }
    }
    if (tool == Tool::Particles && demoMode != 114) {
        const bool canEmit = hovered && !io.KeyAlt && !orbit;
        UpdateSimulation(*v, canEmit && ImGui::IsMouseClicked(ImGuiMouseButton_Left),
                         canEmit && ImGui::IsMouseDown(ImGuiMouseButton_Left), local(io.MousePos.x, io.MousePos.y), w, h);
    }
    if (tool == Tool::Fluid) FluidInput3D(*v, local(io.MousePos.x, io.MousePos.y), w, h, hovered, orbit, origin);
    if (tool == Tool::Plugin) PluginTool3DInput(*v, local(io.MousePos.x, io.MousePos.y), w, h, hovered);
    SurfaceToolInput(*v, local(io.MousePos.x, io.MousePos.y), w, h, hovered && !orbit);
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
    if (tool == Tool::SurfaceMask && !tt) {
        auto* mask = v->surfaceTools.coverage ? v->surfaceTools.coverage.get() : doc.Selection() ? doc.Selection()->tex.get() : nullptr;
        if (mask) img = v->renderer.SurfaceMaskOverlay(*ctx,img,v->renderer.SurfaceUvView(*ctx,s,w,h),mask);
    }
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
    if (tool == Tool::SurfacePath && !tt) SurfacePathInput(*v,local(io.MousePos.x,io.MousePos.y),w,h,hovered && !orbit,origin);
    if ((tool == Tool::Clone || tool == Tool::Smudge || tool == Tool::SurfaceMask) && hovered && !orbit) {
        dl->PushClipRect(origin,ImVec2(origin.x+w,origin.y+h),true);
        const auto& b = tool == Tool::SurfaceMask ? brush : retouch;
        dl->AddCircle(io.MousePos,b.size*0.5f,IM_COL32(30,30,30,240),48,3);
        dl->AddCircle(io.MousePos,b.size*0.5f,IM_COL32(245,245,245,225),48,1);
        if (tool == Tool::Clone && v->surfaceTools.hasSource && v->surfaceTools.sourceRevision == s.revision) {
            auto p = v->surfaceTools.source;
            float4 c = s.viewProj.Transform({p.x,p.y,p.z,1});
            if (c.w>0) {
                ImVec2 q(origin.x + (c.x/c.w*0.5f+0.5f)*w, origin.y + (0.5f-c.y/c.w*0.5f)*h);
                if (v->surfaceTools.stroke == Tool::Clone) q=ImVec2(io.MousePos.x+v->surfaceTools.cloneOffset.x,io.MousePos.y+v->surfaceTools.cloneOffset.y);
                dl->AddLine(ImVec2(q.x-6*dpi,q.y),ImVec2(q.x+6*dpi,q.y),IM_COL32(255,191,70,255),2);
                dl->AddLine(ImVec2(q.x,q.y-6*dpi),ImVec2(q.x,q.y+6*dpi),IM_COL32(255,191,70,255),2);
            }
        }
        dl->PopClipRect();
    }
    if (const int sym = v->renderer.meshSymmetry; sym >= 1 && sym <= 3) {
        // The mirror plane of the symmetry (object axis through the origin), across the model's box.
        const float3 lo = s.data.boundsMin * 1.08f, hi = s.data.boundsMax * 1.08f;
        float3 c[4];
        if (sym == 1) c[0] = {0, lo.y, lo.z}, c[1] = {0, hi.y, lo.z}, c[2] = {0, hi.y, hi.z}, c[3] = {0, lo.y, hi.z};
        else if (sym == 2) c[0] = {lo.x, 0, lo.z}, c[1] = {hi.x, 0, lo.z}, c[2] = {hi.x, 0, hi.z}, c[3] = {lo.x, 0, hi.z};
        else c[0] = {lo.x, lo.y, 0}, c[1] = {hi.x, lo.y, 0}, c[2] = {hi.x, hi.y, 0}, c[3] = {lo.x, hi.y, 0};
        ImVec2 q[4];
        bool ok = true;
        for (int i = 0; i < 4 && ok; ++i) {
            float z = 0;
            const float2 p = ViewPoint(s.viewProj, c[i], w, h, &z);
            ok = z < 1e29f;
            q[i] = ImVec2(origin.x + p.x, origin.y + p.y);
        }
        if (ok) {
            const ImU32 col = sym == 1 ? IM_COL32(255, 90, 90, 255) : sym == 2 ? IM_COL32(110, 230, 110, 255) : IM_COL32(100, 150, 255, 255);
            dl->PushClipRect(origin, ImVec2(origin.x + w, origin.y + h), true);
            dl->AddQuadFilled(q[0], q[1], q[2], q[3], (col & 0x00FFFFFF) | 0x18000000);
            dl->AddPolyline(q, 4, (col & 0x00FFFFFF) | 0xB0000000, ImDrawFlags_Closed, 1.5f);
            dl->PopClipRect();
        }
    }
    // The brush hovering the texture (2D view): where it paints on the model.
    if (!hovered && cursor2D.view == v && cursor2D.frame >= ImGui::GetFrameCount() - 1) {
        const float4 col = cursor2D.eraser ? float4{1, 1, 1, 0.5f} : float4{fg.x, fg.y, fg.z, 0.6f};
        if (gpu::Texture* ov = v->renderer.MeshViewCursor(*ctx, s, v->doc->Width(), v->doc->Height(), cursor2D.p, cursor2D.r,
                                                          cursor2D.hardness, col))
            dl->AddImage((ImTextureID)gpu::Dev().UiTextureId(ov), origin, ImVec2(origin.x + w, origin.y + h));
    }
    if (hovered && tool == Tool::Particles && particleBrush.source == 0 && !orbit) {
        const float r = std::max(brushDiameter(brush, local(io.MousePos.x, io.MousePos.y), particleBrush.customMaterial) * 0.5f, 2.0f);
        dl->AddCircle(io.MousePos, r + 1, IM_COL32(0, 0, 0, 150), 0, 1.5f);
        dl->AddCircle(io.MousePos, r, IM_COL32_WHITE, 0, 1.0f);
        dl->AddCircleFilled(io.MousePos, 1.5f, IM_COL32_WHITE);
    }
    if (hovered && paintTool && !orbit) {
        const BrushSettings& b = physicalErase ? eraser : brush;
        float r = std::max(brushDiameter(b, local(io.MousePos.x, io.MousePos.y)) * 0.5f, 1.0f);
        // The dab drawn on the model under the cursor: the tip's real shape in the paint color, laid on the surface
        // (along the average normal when that mode is on), before painting.
        if (!meshStroking) setupTip(b);
        cursor3D = {ImGui::GetFrameCount(), v, local(io.MousePos.x, io.MousePos.y), r, b.hardness, tipAngle(currentPen),
                    physicalErase, brush3D.normalProject};
        {
            const float4 col = physicalErase ? float4{1, 1, 1, 0.45f} : float4{fg.x, fg.y, fg.z, 0.55f};
            if (gpu::Texture* ov = v->renderer.MeshCursorPreview(*ctx, s, local(io.MousePos.x, io.MousePos.y), r, b.hardness,
                                                                 tipAngle(currentPen), col, brush3D.normalProject))
                dl->AddImage((ImTextureID)gpu::Dev().UiTextureId(ov), origin, ImVec2(origin.x + w, origin.y + h));
        }
        bool oriented = false;
        if (brush3D.normalProject) {
            // The plane under the cursor (read back a frame later): the brush disc drawn on it, and its normal.
            auto& F = brush3D;
            if (F.cursorFrame && F.cursorFrame->ready && F.cursorFrame->data.size() >= 64) {
                memcpy(F.cursorFrameData, F.cursorFrame->data.data(), 64);
                F.cursorFrameValid = F.cursorFrameData[3] > 0;
                F.cursorFrame.reset();
            }
            if (!F.cursorFrame) F.cursorFrame = v->renderer.MeshCursorFrame(*ctx, s, local(io.MousePos.x, io.MousePos.y), r);
            if (F.cursorFrameValid) {
                const float* f = F.cursorFrameData;
                const float3 c{f[0], f[1], f[2]}, n{f[4], f[5], f[6]}, t{f[8], f[9], f[10]}, u{f[12], f[13], f[14]};
                const float R = r / f[3];
                ImVec2 ring[48];
                bool ok = true;
                for (int i = 0; i < 48 && ok; ++i) {
                    const float a = 6.2831853f * i / 48;
                    float z = 0;
                    const float2 q = ViewPoint(s.viewProj, c + t * (std::cos(a) * R) + u * (std::sin(a) * R), w, h, &z);
                    ok = z < 1e29f;
                    ring[i] = ImVec2(origin.x + q.x, origin.y + q.y);
                }
                float zc = 0, zn = 0;
                const float2 sc = ViewPoint(s.viewProj, c, w, h, &zc), sn = ViewPoint(s.viewProj, c + n * (R * 0.6f), w, h, &zn);
                if (ok && zc < 1e29f && zn < 1e29f) {
                    dl->AddPolyline(ring, 48, IM_COL32(0, 0, 0, 150), ImDrawFlags_Closed, 2.5f);
                    dl->AddPolyline(ring, 48, IM_COL32(120, 220, 255, 255), ImDrawFlags_Closed, 1.2f);
                    dl->AddLine(ImVec2(origin.x + sc.x, origin.y + sc.y), ImVec2(origin.x + sn.x, origin.y + sn.y), IM_COL32(120, 220, 255, 255), 1.5f);
                    oriented = true;
                }
            }
        }
        if (!oriented) {
            dl->AddCircle(io.MousePos, r + 1, IM_COL32(0, 0, 0, 150), 0, 1.5f);
            dl->AddCircle(io.MousePos, r, IM_COL32_WHITE, 0, 1.0f);
        }
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
    ui::FilterBox("Filtrer (Ctrl+F)...", 360.0f * dpi);
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
    ui::HelpTooltip("Rendu qualité : l'ombre de la lumière principale et une ombre de contact sur un sol invisible sous le modèle.");
    ImGui::SliderFloat("Occlusion d'écran", &s.ssao, 0, 2, s.ssao > 0 ? "%.2f" : "désactivée");
    ui::HelpTooltip("Rendu qualité : assombrit les creux et les contacts en temps réel (SSAO), sans cuisson.");
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
