// Library saving (effects, blends, brushes) with protection: the shipped items
// can never be overwritten; items saved by the user carry //@user and can be
// locked (//@locked) so nothing replaces them unless they are unlocked.
// Editing an item from a document always works on a copy kept in the document.
#include "ui/App.h"
#include "ui/Theme.h"
#include "doc/Brushes.h"
#include "gpu/ShaderSystem.h"

#include <imgui_stdlib.h>

#include <filesystem>
#include <fstream>
#include <sstream>

namespace sx {

namespace {
std::string Slug(const std::string& name) {
    std::string s;
    for (unsigned char c : name) {
        if (isalnum(c)) s += (char)tolower(c);
        else if (!s.empty() && s.back() != '_') s += '_';
    }
    while (!s.empty() && s.back() == '_') s.pop_back();
    return s.empty() ? "element" : s;
}

bool HasLine(const std::string& text, const char* tag) {
    std::istringstream in(text);
    for (std::string line; std::getline(in, line);) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if (line == tag) return true;
    }
    return false;
}

std::string ReadAll(const std::string& path) {
    std::ifstream f(Utf8Path(path), std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}
}  // namespace

std::string App::LibraryPath(LibKind k, const std::string& name) {
    std::string dir = k == LibKind::Brush ? Brushes().Dir() : gpu::Shaders().ShaderDir() + "/effects";
    std::string prefix = k == LibKind::Brush ? "user_" : k == LibKind::Blend ? "blend_user_" : "user_";
    return dir + "/" + prefix + Slug(name) + ".slang";
}

int App::LibraryState(const std::string& path) {
    // 0 free, 1 user item (replaceable), 2 protected (shipped or locked).
    if (!std::filesystem::exists(Utf8Path(path))) return 0;
    std::string text = ReadAll(path);
    if (!HasLine(text, "//@user") || HasLine(text, "//@locked")) return 2;
    return 1;
}

bool App::LibraryItemLocked(LibKind k, const std::string& id) {
    std::string dir = k == LibKind::Brush ? Brushes().Dir() : gpu::Shaders().ShaderDir() + "/effects";
    return LibraryState(dir + "/" + id + ".slang") == 2;
}

bool App::IsUserLibraryItem(LibKind k, const std::string& id) {
    std::string dir = k == LibKind::Brush ? Brushes().Dir() : gpu::Shaders().ShaderDir() + "/effects";
    return HasLine(ReadAll(dir + "/" + id + ".slang"), "//@user");
}

void App::SetLibraryLock(LibKind k, const std::string& id, bool lock) {
    std::string dir = k == LibKind::Brush ? Brushes().Dir() : gpu::Shaders().ShaderDir() + "/effects";
    std::string path = dir + "/" + id + ".slang";
    std::string text = ReadAll(path), out;
    if (!HasLine(text, "//@user")) return;  // shipped items stay protected
    std::istringstream in(text);
    for (std::string line; std::getline(in, line);)
        if (line.rfind("//@locked", 0) != 0) out += line + "\n";
    if (lock) out = "//@locked\n" + out;
    std::ofstream(Utf8Path(path), std::ios::binary) << out;
    if (k == LibKind::Brush) Brushes().Reload();
    else Effects().Reload();
}

bool App::SaveToLibrary(LibKind k, const std::string& name, const std::string& category, const std::string& text, bool lock, bool replace,
                        std::string* err) {
    std::string path = LibraryPath(k, name);
    int state = LibraryState(path);
    if (state == 2) { if (err) *err = "« " + name + " » est protégé : choisissez un autre nom (ou déverrouillez-le)."; return false; }
    if (state == 1 && !replace) { if (err) *err = "« " + name + " » existe déjà : cochez « Remplacer »."; return false; }
    // Metadata lines replaced, the rest kept.
    std::string out = "//@name " + name + "\n//@category " + category + "\n//@user\n";
    if (lock) out += "//@locked\n";
    bool hasBlend = false;
    std::istringstream in(text);
    for (std::string line; std::getline(in, line);) {
        if (line.rfind("//@name", 0) == 0 || line.rfind("//@category", 0) == 0 || line.rfind("//@user", 0) == 0 || line.rfind("//@locked", 0) == 0) continue;
        if (line.rfind("//@blend", 0) == 0) hasBlend = true;
        out += line + "\n";
    }
    if (k == LibKind::Blend && !hasBlend) out = "//@blend\n" + out;
    std::error_code ec;
    std::filesystem::create_directories(Utf8Path(path).parent_path(), ec);
    std::ofstream f(Utf8Path(path), std::ios::binary);
    f << out;
    if (!f) { if (err) *err = "Impossible d'écrire " + path; return false; }
    f.close();
    if (k == LibKind::Brush) Brushes().Reload();
    else {
        Effects().Reload();
        fxBrowser.previews.clear();
        blendBrowser.previews.clear();
    }
    Status(std::string(k == LibKind::Brush ? "Pinceau" : k == LibKind::Blend ? "Fusion" : "Effet") + " enregistré dans la bibliothèque : " + name +
           (lock ? " (verrouillé)" : ""));
    return true;
}

bool App::LibrarySavePopup(const char* popupId, LibKind k, const std::string& defName, const std::string& defCategory, const std::string& text,
                           std::string* savedId) {
    static std::string name, category, error;
    static bool lock = false, replace = false;
    bool saved = false;
    if (ImGui::BeginPopup(popupId)) {
        if (ImGui::IsWindowAppearing()) {
            name = defName;
            category = defCategory;
            error.clear();
            lock = replace = false;
        }
        ImGui::SetNextItemWidth(260 * dpi);
        ImGui::InputText("Nom", &name);
        ImGui::SetNextItemWidth(260 * dpi);
        ImGui::InputText("Catégorie", &category);
        std::string path = LibraryPath(k, name);
        int state = LibraryState(path);
        std::string file = Narrow(Utf8Path(path).filename().wstring());
        if (state == 2) ImGui::TextColored(ImVec4(1, 0.6f, 0.3f, 1), "%s : protégé, choisissez un autre nom", file.c_str());
        else if (state == 1) ImGui::Checkbox(("Remplacer " + file).c_str(), &replace);
        else ImGui::TextDisabled("%s", file.c_str());
        ImGui::Checkbox("Verrouiller", &lock);
        ui::HelpTooltip("Protège l'élément : il ne pourra plus être remplacé (ni depuis un autre projet) tant qu'il n'est pas déverrouillé.");
        ImGui::BeginDisabled(name.empty() || state == 2 || (state == 1 && !replace));
        if (ImGui::Button("Enregistrer", ImVec2(-1, 0))) {
            if (SaveToLibrary(k, name, category, text, lock, replace, &error)) {
                saved = true;
                if (savedId) *savedId = Narrow(Utf8Path(path).stem().wstring());
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndDisabled();
        if (!error.empty()) ImGui::TextColored(ImVec4(1, 0.5f, 0.4f, 1), "%s", error.c_str());
        ImGui::EndPopup();
    }
    return saved;
}

}  // namespace sx
