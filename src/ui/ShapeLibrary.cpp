// Custom shapes: picker (built-in and user shapes), user shapes made from a path or an SVG file,
// saved in %LOCALAPPDATA%\Shaderixel\shapes.
#include "ui/App.h"
#include "ui/Theme.h"
#include "core/Json.h"

#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <filesystem>
#include <fstream>

namespace sx {

namespace {
std::string ShapeDir() {
    const char* la = getenv("LOCALAPPDATA");
    return la ? std::string(la) + "\\Shaderixel\\shapes" : std::string();
}

Json PathToJson(const std::vector<SubPath>& path) {
    Json cs = Json::Array();
    for (auto& sp : path) {
        Json e = Json::Array(), pts = Json::Array();
        e.Push(sp.closed);
        for (auto& a : sp.pts)
            for (float f : {a.p.x, a.p.y, a.in.x, a.in.y, a.out.x, a.out.y}) pts.Push(f);
        e.Push(pts);
        cs.Push(e);
    }
    return cs;
}

std::vector<SubPath> PathFromJson(const Json& j) {
    std::vector<SubPath> out;
    for (auto& e : j.Elements()) {
        SubPath sp;
        sp.closed = e.Size() > 0 && e.At(0).B(true);
        if (e.Size() > 1) {
            const auto& v = e.At(1).Elements();
            for (size_t k = 0; k + 5 < v.size(); k += 6)
                sp.pts.push_back({{v[k].F(), v[k + 1].F()}, {v[k + 2].F(), v[k + 3].F()}, {v[k + 4].F(), v[k + 5].F()}});
        }
        out.push_back(sp);
    }
    return out;
}

constexpr int kIcon = 64;  // icon size in the atlas

// White coverage of a shape (even-odd, antialiased) in an icon of the atlas.
void RasterIcon(const std::vector<SubPath>& path, u8* atlas, int atlasW, int x0) {
    VectorPath vp;
    vp.subpaths = path;
    const float pad = 3, s = kIcon - pad * 2;
    for (auto& sp : vp.subpaths)
        for (auto& p : sp.pts)
            for (float2* q : {&p.p, &p.in, &p.out}) *q = {pad + q->x * s, pad + q->y * s};
    Rect r;
    std::vector<u8> cov;
    RasterizeContours(FlattenPath(vp, 0.2f), kIcon, kIcon, true, r, cov);
    for (int y = std::max(0, r.y0); y < std::min(kIcon, r.y1); ++y)
        for (int x = std::max(0, r.x0); x < std::min(kIcon, r.x1); ++x) {
            u8* o = atlas + ((size_t)y * atlasW + x0 + x) * 4;
            o[0] = o[1] = o[2] = 235;
            o[3] = cov[(size_t)(y - r.y0) * r.Width() + (x - r.x0)];
        }
}
}  // namespace

std::vector<CustomShape>& App::UserShapes() {
    if (!userShapesLoaded) {
        userShapesLoaded = true;
        std::error_code ec;
        const std::string dir = ShapeDir();
        if (!dir.empty())
            for (auto& e : std::filesystem::directory_iterator(Utf8Path(dir), ec)) {
                if (e.path().extension() != ".json") continue;
                std::string text;
                Json j;
                if (!ReadTextFile(Narrow(e.path().wstring()), text) || !Json::Parse(text, j)) continue;
                CustomShape s;
                s.name = j["name"].S(Narrow(e.path().stem().wstring()));
                s.path = PathFromJson(j["path"]);
                s.user = true;
                if (!s.path.empty()) userShapes.push_back(std::move(s));
            }
    }
    return userShapes;
}

bool App::SaveUserShape(const std::string& name, std::vector<SubPath> path) {
    NormalizeShapePath(path);
    const std::string dir = ShapeDir();
    if (dir.empty() || path.empty()) return false;
    std::error_code ec;
    std::filesystem::create_directories(Utf8Path(dir), ec);
    std::string file;
    for (char c : name) file += (std::isalnum((unsigned char)c) || (unsigned char)c >= 0x80 || c == ' ' || c == '-') ? c : '_';
    if (file.empty()) file = "forme";
    Json j = Json::Object();
    j["name"] = name;
    j["path"] = PathToJson(path);
    std::ofstream f(Utf8Path(dir + "\\" + file + ".json"), std::ios::binary);
    if (!f) return false;
    f << j.Dump(1);
    auto& shapes = UserShapes();
    shapes.erase(std::remove_if(shapes.begin(), shapes.end(), [&](const CustomShape& s) { return s.name == name; }), shapes.end());
    CustomShape s;
    s.name = name;
    s.path = path;
    s.user = true;
    shapes.push_back(std::move(s));
    return true;
}

bool App::ShapePicker(ShapeStyle& s, DocView* v) {
    bool changed = false;
    auto& user = UserShapes();
    std::vector<const CustomShape*> all;
    for (auto& b : BuiltinShapes()) all.push_back(&b);
    for (auto& u : user) all.push_back(&u);
    if (s.custom.empty() && !all.empty()) {
        s.custom = all.front()->path;
        s.customName = all.front()->name;
        changed = true;
    }
    // Icons: one atlas texture (RGBA8, shown as is), rebuilt when the list changes.
    u64 listKey = all.size();
    for (auto* c : all) listKey = listKey * 1099511628211ull ^ std::hash<std::string>()(c->name);
    if (!shapeAtlas || shapeAtlasKey != listKey) {
        const int aw = kIcon * (int)std::max<size_t>(1, all.size());
        std::vector<u8> px((size_t)aw * kIcon * 4, 0);
        for (size_t i = 0; i < all.size(); ++i) RasterIcon(all[i]->path, px.data(), aw, (int)i * kIcon);
        shapeAtlas = gpu::Dev().CreateTexture((u32)aw, kIcon, gpu::Format::RGBA8, "ShapeIcons");
        ctx->Upload(shapeAtlas.get(), px.data(), (u32)aw * 4);
        shapeAtlasKey = listKey;
    }
    const float cell = 40 * dpi, gap = ImGui::GetStyle().ItemSpacing.x * 0.5f;
    const int cols = std::max(1, (int)((ImGui::GetContentRegionAvail().x + gap) / (cell + gap)));
    ImDrawList* dl = ImGui::GetWindowDrawList();
    std::string removeName;
    for (size_t i = 0; i < all.size(); ++i) {
        const CustomShape& cs = *all[i];
        if (i % cols) ImGui::SameLine(0, gap);
        ImGui::PushID((int)i);
        const ImVec2 a = ImGui::GetCursorScreenPos(), b(a.x + cell, a.y + cell);
        const bool on = s.customName == cs.name;
        if (ImGui::InvisibleButton("##shape", ImVec2(cell, cell))) {
            s.custom = cs.path;
            s.customName = cs.name;
            changed = true;
        }
        const bool hov = ImGui::IsItemHovered();
        dl->AddRectFilled(a, b, on ? IM_COL32(70, 60, 130, 255) : hov ? IM_COL32(50, 52, 62, 255) : IM_COL32(36, 37, 44, 255), 4);
        if (on) dl->AddRect(a, b, ui::col::Accent, 4, 0, 2.0f);
        const float u0 = (float)i / (float)all.size(), u1 = (float)(i + 1) / (float)all.size();
        dl->AddImage((ImTextureID)gpu::Dev().UiTextureId(shapeAtlas.get()), ImVec2(a.x + 3, a.y + 3), ImVec2(b.x - 3, b.y - 3), ImVec2(u0, 0), ImVec2(u1, 1));
        if (cs.user) dl->AddCircleFilled(ImVec2(b.x - 5, a.y + 5), 2.5f, ui::col::Accent);
        if (hov) ImGui::SetTooltip("%s%s", cs.name.c_str(), cs.user ? "\n(clic droit : supprimer)" : "");
        if (cs.user && ImGui::BeginPopupContextItem("##ushape")) {
            if (ImGui::MenuItem("Supprimer cette forme")) removeName = cs.name;
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }
    if (!removeName.empty()) {
        std::error_code ec;
        for (auto& e : std::filesystem::directory_iterator(Utf8Path(ShapeDir()), ec)) {
            std::string text;
            Json j;
            if (ReadTextFile(Narrow(e.path().wstring()), text) && Json::Parse(text, j) && j["name"].S("") == removeName) std::filesystem::remove(e.path(), ec);
        }
        user.erase(std::remove_if(user.begin(), user.end(), [&](const CustomShape& c) { return c.name == removeName; }), user.end());
    }
    // New shapes: from the active path, from an SVG file.
    const VectorPath* path = nullptr;
    if (v)
        for (auto& p : v->doc->S().paths)
            if (p.id == v->doc->S().activePath && !p.Empty()) path = &p;
    ImGui::BeginDisabled(!path);
    if (ImGui::Button("Depuis le tracé actif")) {
        shapeNewName = path->name.empty() ? "Ma forme" : path->name;
        ImGui::OpenPopup("##newshape");
    }
    ImGui::EndDisabled();
    if (!path) ImGui::SetItemTooltip("Tracez un chemin à la plume (P) : il devient une forme réutilisable.");
    ImGui::SameLine();
    if (ImGui::Button("Importer un SVG...")) {
        std::string f = platform::OpenFileDialog(hwnd, L"SVG (*.svg)\0*.svg\0");
        std::string text;
        if (!f.empty() && ReadTextFile(f, text)) {
            auto sp = ParseSvgFile(text);
            const std::string name = Narrow(Utf8Path(f).stem().wstring());
            if (sp.empty()) Status("Aucun tracé lisible dans " + name);
            else if (SaveUserShape(name, sp)) {
                s.custom = UserShapes().back().path;
                s.customName = name;
                changed = true;
                Status("Forme « " + name + " » ajoutée");
            }
        }
    }
    if (ImGui::BeginPopup("##newshape")) {
        ImGui::TextUnformatted("Nom de la forme");
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        const bool enter = ImGui::InputText("##n", &shapeNewName, ImGuiInputTextFlags_EnterReturnsTrue);
        if ((enter || ImGui::Button("Enregistrer")) && path && !shapeNewName.empty()) {
            if (SaveUserShape(shapeNewName, path->subpaths)) {
                s.custom = UserShapes().back().path;
                s.customName = shapeNewName;
                changed = true;
                Status("Forme « " + shapeNewName + " » ajoutée à la bibliothèque");
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    return changed;
}

}  // namespace sx
