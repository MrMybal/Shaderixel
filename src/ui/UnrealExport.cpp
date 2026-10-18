// Paint 3D project -> Unreal: the model (OBJ, original units), the textures
// (BaseColor, Normal, ORM, Emissive) and a material description (.sxmat.json)
// that the Shaderixel Link plugin reads to import everything at once: textures
// with the right settings, a material instance and the static mesh with it.
// The settings travel with the project (.sx3d meta "unreal").
#include "ui/App.h"
#include "ui/Filter.h"
#include "ui/Theme.h"

#include <imgui_stdlib.h>
#include <filesystem>
#include <fstream>

namespace sx {

namespace {
std::string AssetSafe(std::string s) {
    for (auto& c : s)
        if (!std::isalnum((unsigned char)c) && c != '_' && c != '-') c = '_';
    return s.empty() ? std::string("Shaderixel") : s;
}
}  // namespace

Json App::UnrealSettings(Document& d) {
    Json u;
    if (d.unrealSettings.empty() || !Json::Parse(d.unrealSettings, u) || !u.IsObject()) u = Json::Object();
    const Json& c = u;
    std::string stem = d.path.empty() ? d.Name() : Narrow(Utf8Path(d.path).stem().wstring());
    if (auto p = stem.find(" (3D)"); p != std::string::npos) stem.erase(p);
    if (!c.Has("name")) u["name"] = AssetSafe(stem);
    if (!c.Has("contentFolder")) u["contentFolder"] = std::string("/Game/Shaderixel/") + c["name"].S();
    if (!c.Has("importScale")) u["importScale"] = 100.0f;
    if (!c.Has("twoSided")) u["twoSided"] = false;
    if (!c.Has("emissiveStrength")) u["emissiveStrength"] = 1.0f;
    return u;
}

std::string App::ExportForUnreal(DocView& v, const std::string& folder) {
    Document& d = *v.doc;
    if (!v.mesh || d.kind != 1) { Status("Export Unreal : ouvrez un projet Paint 3D"); return ""; }
    Json u = UnrealSettings(d);
    const std::string name = AssetSafe(u["name"].S());
    std::error_code ec;
    std::filesystem::create_directories(Utf8Path(folder), ec);
    std::string err;
    const std::string obj = name + ".obj";
    if (!SaveObj(folder + "/" + obj, v.mesh->data, true, &err)) { Status("Export Unreal : " + err); return ""; }
    // Textures: the Unreal template (ORM packed), whatever the project's own export choice.
    const int keep = d.exportTemplate;
    d.exportTemplate = 1;
    ExportProjectTextures(v, folder + "/T_" + name + ".png");
    d.exportTemplate = keep;
    const bool emissive = (d.pbrChannels & (1u << (int)PbrChannel::Emissive)) != 0;
    // The description read by the plugin.
    Json m = Json::Object();
    m["format"] = "shaderixel-material";
    m["version"] = 2;
    m["name"] = name;
    m["contentFolder"] = u["contentFolder"].S();
    m["source"] = d.path;
    Json mesh = Json::Object();
    mesh["file"] = obj;
    mesh["asset"] = "SM_" + name;
    mesh["axes"] = "obj-y-up";            // Unreal X = -Z, Y = X, Z = Y
    mesh["importScale"] = u["importScale"].F(100);  // file units -> centimeters
    m["mesh"] = mesh;
    Json mat = Json::Object();
    mat["asset"] = "MI_" + name;
    mat["parent"] = u["parent"].S();      // empty: the plugin's master material (M_ShaderixelPBR2)
    mat["twoSided"] = u["twoSided"].B();
    Json scalars = Json::Object();
    scalars["EmissiveStrength"] = u["emissiveStrength"].F(1);
    mat["scalars"] = scalars;
    m["material"] = mat;
    Json texs = Json::Array();
    auto add = [&](const char* role, bool srgb, const char* compression, const char* param) {
        Json t = Json::Object();
        t["role"] = role;
        t["file"] = "T_" + name + "_" + role + ".png";
        t["asset"] = "T_" + name + "_" + role;
        t["parameter"] = param;
        t["srgb"] = srgb;
        t["compression"] = compression;
        if (std::string(role) == "ORM") {
            Json ch = Json::Object();
            ch["R"] = "AmbientOcclusion";
            ch["G"] = "Roughness";
            ch["B"] = "Metallic";
            t["channels"] = ch;
        }
        texs.Push(t);
    };
    add("BaseColor", true, "Default", "BaseColor");
    add("Normal", false, "Normalmap", "Normal");
    add("ORM", false, "Masks", "ORM");
    if (emissive) add("Emissive", true, "Default", "Emissive");
    m["textures"] = texs;
    const std::string desc = folder + "/" + name + ".sxmat.json";
    std::ofstream(Utf8Path(desc), std::ios::binary) << m.Dump(1);
    // Kept with the project.
    d.unrealSettings = u.Dump(0);
    Status("Export Unreal : " + desc + " (modèle, textures, matériau) - dans Unreal : Outils > Importer un modèle Shaderixel");
    return desc;
}

void App::DrawUnrealExportDialog() {
    if (!unrealDialog) return;
    DocView* v = Active();
    if (!v || v->doc->kind != 1) { unrealDialog = false; return; }
    Document& d = *v->doc;
    ImGui::SetNextWindowSize(ImVec2(520 * dpi, 0), ImGuiCond_Appearing);
    if (!ImGui::Begin("Exporter pour Unreal", &unrealDialog, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize)) { ImGui::End(); return; }
    ui::FilterBox("Filtrer (Ctrl+F)...", 320.0f * dpi);
    Json u = UnrealSettings(d);
    bool changed = false;
    ImGui::TextWrapped("Le modèle, ses textures (couleur, normale, ORM, émissif) et la description du matériau, "
                       "importés d'un coup dans Unreal par le plugin Shaderixel Link : textures réglées (sRGB, compression), "
                       "instance de matériau et maillage statique avec ce matériau.");
    ImGui::Separator();
    ImGui::PushItemWidth(300 * dpi);
    std::string name = u["name"].S();
    if (ImGui::InputText("Nom", &name)) { u["name"] = name; changed = true; }
    ui::HelpTooltip("Les assets s'appelleront SM_<nom>, MI_<nom>, T_<nom>_BaseColor...");
    std::string folder = u["contentFolder"].S();
    if (ImGui::InputText("Dossier Unreal", &folder)) { u["contentFolder"] = folder; changed = true; }
    ui::HelpTooltip("Dossier du Content Browser proposé à l'import (le plugin peut en choisir un autre).");
    float scale = u["importScale"].F(100);
    if (ImGui::DragFloat("Échelle d'import", &scale, 0.5f, 0.001f, 10000, "x %.3g")) { u["importScale"] = scale; changed = true; }
    ui::HelpTooltip("Unités du fichier d'origine vers les centimètres d'Unreal : 100 pour un modèle en mètres, 1 en centimètres.");
    float em = u["emissiveStrength"].F(1);
    if (ImGui::DragFloat("Intensité émissive", &em, 0.05f, 0, 100, "%.2f")) { u["emissiveStrength"] = em; changed = true; }
    bool two = u["twoSided"].B();
    if (ImGui::Checkbox("Double face", &two)) { u["twoSided"] = two; changed = true; }
    std::string parent = u["parent"].S();
    if (ImGui::InputTextWithHint("Matériau parent", "matériau du plugin (M_ShaderixelPBR2)", &parent)) { u["parent"] = parent; changed = true; }
    ui::HelpTooltip("Chemin d'un matériau de votre projet (ex. /Game/Materials/M_Master) ayant les paramètres\nBaseColor, Normal, ORM, Emissive (textures) et EmissiveStrength ; vide : celui du plugin.");
    ImGui::PopItemWidth();
    if (changed) {
        d.unrealSettings = u.Dump(0);
        d.dirty = true;
    }
    ImGui::Separator();
    if (ImGui::Button("Exporter...", ImVec2(140 * dpi, 0))) {
        std::string f = platform::SaveFileDialog(hwnd, L"Description de matériau Shaderixel (*.sxmat.json)\0*.sxmat.json\0", L"sxmat.json");
        if (!f.empty()) {
            ExportForUnreal(*v, Narrow(Utf8Path(f).parent_path().wstring()));
            unrealDialog = false;
        }
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Réglages enregistrés avec le projet (.sx3d)");
    ImGui::TextWrapped("Dans Unreal : Outils > Importer un modèle Shaderixel... (le .sxmat.json, ou directement le .sx3d). "
                       "Un .sxmat.json posé à côté d'un FBX / OBJ de même nom est aussi appliqué à l'import normal du modèle.");
    ImGui::End();
}

}  // namespace sx
