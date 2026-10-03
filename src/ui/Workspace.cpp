// Workspace: dock layout, side panel tabs (Général, Outils, Effets, Fusion,
// Peinture), hideable zones (graph area, right column), extra windows that can
// be moved to other screens (ImGui viewports) and saved layouts.
#include "ui/App.h"
#include "ui/Theme.h"

#include <imgui_internal.h>
#include <imgui_stdlib.h>

#include <filesystem>
#include <fstream>
#include <sstream>

namespace sx {

namespace {
const char* kSideTabNames[App::kSideTabs] = {"Général", "Outils", "Effets", "Fusion", "Peinture"};

ImGuiID SideTabDock(int i) {
    char id[32];
    snprintf(id, sizeof(id), "##sxside%d", i);
    return ImHashStr(id);
}

std::string ExtraWindowName(int i) { return "Fenêtre " + std::to_string(i + 2); }

std::string LayoutDir() { return ExeDir() + "/layouts"; }

App* gWorkspaceApp = nullptr;

void* SettingsOpen(ImGuiContext*, ImGuiSettingsHandler*, const char* name) { return strcmp(name, "State") == 0 ? (void*)1 : nullptr; }

void SettingsReadLine(ImGuiContext*, ImGuiSettingsHandler*, void*, const char* line) {
    App& a = *gWorkspaceApp;
    int i;
    unsigned u;
    if (sscanf(line, "Tab=%d", &i) == 1) a.sideTab = a.sideTabRequest = std::clamp(i, 0, App::kSideTabs - 1);
    else if (sscanf(line, "HideBottom=%d", &i) == 1) a.hideBottom = i != 0;
    else if (sscanf(line, "HideRight=%d", &i) == 1) a.hideRight = i != 0;
    else if (sscanf(line, "Bottom=0x%08X", &u) == 1) a.bottomZone = u;
    else if (sscanf(line, "Right=0x%08X", &u) == 1) a.rightZone = u;
    else if (sscanf(line, "Windows=%d", &i) == 1) a.extraWindows.assign(std::clamp(i, 0, 8), true);
    else if (sscanf(line, "WindowOpen%d=", &i) == 1 && i >= 0 && i < (int)a.extraWindows.size()) a.extraWindows[i] = strchr(line, '=')[1] == '1';
}

void SettingsWriteAll(ImGuiContext*, ImGuiSettingsHandler* h, ImGuiTextBuffer* buf) {
    App& a = *gWorkspaceApp;
    buf->appendf("[%s][State]\n", h->TypeName);
    buf->appendf("Tab=%d\nHideBottom=%d\nHideRight=%d\n", a.sideTab, a.hideBottom ? 1 : 0, a.hideRight ? 1 : 0);
    buf->appendf("Bottom=0x%08X\nRight=0x%08X\n", a.bottomZone, a.rightZone);
    buf->appendf("Windows=%d\n", (int)a.extraWindows.size());
    for (size_t i = 0; i < a.extraWindows.size(); ++i) buf->appendf("WindowOpen%d=%d\n", (int)i, a.extraWindows[i] ? 1 : 0);
    buf->append("\n");
}
}  // namespace

void App::RegisterWorkspaceSettings() {
    gWorkspaceApp = this;
    ImGuiSettingsHandler h;
    h.TypeName = "Shaderixel";
    h.TypeHash = ImHashStr("Shaderixel");
    h.ReadOpenFn = SettingsOpen;
    h.ReadLineFn = SettingsReadLine;
    h.WriteAllFn = SettingsWriteAll;
    ImGui::AddSettingsHandler(&h);
}

float App::ToolbarWidth() const { return (38.0f + 2 * 6.0f + 2.0f) * dpi; }

void App::BuildDockLayout(ImGuiID dock) {
    ImVec2 work = ImGui::GetMainViewport()->WorkSize;
    ImGui::DockBuilderRemoveNode(dock);
    ImGui::DockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dock, work);
    ImGuiID left, rest, right, center, bottom, rightTop, rightBottom;
    ImGui::DockBuilderSplitNode(dock, ImGuiDir_Left, ToolbarWidth() / std::max(work.x, 1.0f), &left, &rest);
    ImGui::DockBuilderSplitNode(rest, ImGuiDir_Right, 0.25f, &right, &center);
    ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.32f, &bottom, &center);
    ImGui::DockBuilderSplitNode(right, ImGuiDir_Up, 0.62f, &rightTop, &rightBottom);
    ImGui::DockBuilderDockWindow("Outils", left);
    ImGui::DockBuilderDockWindow("Canevas", center);
    ImGui::DockBuilderDockWindow("Vue 3D", center);
    ImGui::DockBuilderDockWindow("Graphe", bottom);
    ImGui::DockBuilderDockWindow("Éditeur d'effets", bottom);
    ImGui::DockBuilderDockWindow("Panneaux", rightTop);
    ImGui::DockBuilderDockWindow("Calques", rightBottom);
    ImGui::DockBuilderDockWindow("Tracés", rightBottom);
    if (ImGuiDockNode* n = ImGui::DockBuilderGetNode(left)) n->LocalFlags |= ImGuiDockNodeFlags_NoTabBar | ImGuiDockNodeFlags_NoResizeX;
    if (ImGuiDockNode* n = ImGui::DockBuilderGetNode(rightTop)) n->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;
    ImGui::DockBuilderFinish(dock);
    bottomZone = bottom;
    rightZone = right;

    // One dockspace per side tab; hidden tabs keep their panels docked.
    ImVec2 sideSize(work.x * 0.25f, work.y * 0.62f - ImGui::GetFrameHeight() * 2);
    for (int i = 0; i < kSideTabs; ++i) {
        ImGuiID id = SideTabDock(i);
        ImGui::DockBuilderRemoveNode(id);
        ImGui::DockBuilderAddNode(id, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(id, sideSize);
        ImGuiID top = id, low = id;
        auto two = [&](float ratio) { ImGui::DockBuilderSplitNode(id, ImGuiDir_Up, ratio, &top, &low); };
        switch (i) {
            case 0:  // Général
                two(0.45f);
                ImGui::DockBuilderDockWindow("Aperçu du calque", top);
                ImGui::DockBuilderDockWindow("Histogramme", top);
                ImGui::DockBuilderDockWindow("Propriétés", low);
                ImGui::DockBuilderDockWindow("Historique", low);
                ImGui::DockBuilderDockWindow("Assistant IA", low);
                ImGui::DockBuilderDockWindow("Génération IA", low);
                break;
            case 1:  // Outils
                ImGui::DockBuilderDockWindow("Options", id);
                break;
            case 2:  // Effets
                ImGui::DockBuilderDockWindow("Effets", id);
                break;
            case 3:  // Fusion
                ImGui::DockBuilderDockWindow("Fusions", id);
                break;
            case 4:  // Peinture
                two(0.42f);
                ImGui::DockBuilderDockWindow("Couleur", top);
                ImGui::DockBuilderDockWindow("Pinceaux", low);
                break;
        }
        for (ImGuiID n : {top, low})
            if (ImGuiDockNode* node = ImGui::DockBuilderGetNode(n)) node->LocalFlags |= ImGuiDockNodeFlags_AutoHideTabBar;
        ImGui::DockBuilderFinish(id);
    }
    // Extra windows start empty.
    for (size_t i = 0; i < extraWindows.size(); ++i) {
        ImGuiID id = ImHashStr((ExtraWindowName((int)i) + "##dock").c_str());
        ImGui::DockBuilderRemoveNode(id);
    }
    extraWindows.clear();
}

void App::DrawSidePanel() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    bool open = ImGui::Begin("Panneaux", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoTitleBar);
    ImGui::PopStyleVar();
    if (!open) {
        // Collapsed or hidden: keep every tab's panels docked (and hidden).
        for (int i = 0; i < kSideTabs; ++i) ImGui::DockSpace(SideTabDock(i), ImVec2(0, 0), ImGuiDockNodeFlags_KeepAliveOnly);
        ImGui::End();
        return;
    }
    ImGui::SetCursorPos(ImVec2(6 * dpi, 4 * dpi));
    if (ImGui::BeginTabBar("##sidetabs", ImGuiTabBarFlags_FittingPolicyResizeDown)) {
        for (int i = 0; i < kSideTabs; ++i) {
            ImGuiTabItemFlags f = sideTabRequest == i ? ImGuiTabItemFlags_SetSelected : 0;
            if (ImGui::BeginTabItem(kSideTabNames[i], nullptr, f)) {
                if (sideTabRequest < 0 || sideTabRequest == i) sideTab = i;
                ImGui::EndTabItem();
            }
        }
        sideTabRequest = -1;
        ImGui::EndTabBar();
    }
    for (int i = 0; i < kSideTabs; ++i)
        if (i != sideTab) ImGui::DockSpace(SideTabDock(i), ImVec2(0, 0), ImGuiDockNodeFlags_KeepAliveOnly);
    ImGui::DockSpace(SideTabDock(sideTab), ImGui::GetContentRegionAvail(), ImGuiDockNodeFlags_None);
    ImGui::End();
}

void App::ShowSideTab(int tab) {
    sideTabRequest = std::clamp(tab, 0, kSideTabs - 1);
    hideRight = false;
}

void App::DrawExtraWindows() {
    for (size_t i = 0; i < extraWindows.size(); ++i) {
        std::string name = ExtraWindowName((int)i);
        ImGuiID dock = ImHashStr((name + "##dock").c_str());
        if (!extraWindows[i]) {
            ImGui::DockSpace(dock, ImVec2(0, 0), ImGuiDockNodeFlags_KeepAliveOnly);
            continue;
        }
        ImGuiViewport* mv = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2(mv->WorkPos.x + 120 * dpi + i * 40 * dpi, mv->WorkPos.y + 120 * dpi + i * 40 * dpi), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(900 * dpi, 640 * dpi), ImGuiCond_FirstUseEver);
        if (Automated()) {
            ImGuiWindowClass wc;  // scripted runs never take the focus
            wc.ViewportFlagsOverrideSet = ImGuiViewportFlags_NoFocusOnAppearing;
            ImGui::SetNextWindowClass(&wc);
        }
        bool open = true;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        bool visible = ImGui::Begin(name.c_str(), &open, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoDocking);
        ImGui::PopStyleVar();
        if (visible) {
            ImGuiDockNode* node = ImGui::DockBuilderGetNode(dock);
            if (!node || (node->Windows.Size == 0 && !node->IsSplitNode())) {
                ImVec2 c = ImGui::GetCursorScreenPos(), avail = ImGui::GetContentRegionAvail();
                const char* hint = "Glissez des panneaux ici (par leur onglet).\nCette fenêtre peut aller sur un autre écran.";
                ImVec2 ts = ImGui::CalcTextSize(hint);
                ImGui::GetWindowDrawList()->AddText(ImVec2(c.x + (avail.x - ts.x) * 0.5f, c.y + (avail.y - ts.y) * 0.5f), ui::col::TextDim, hint);
            }
            ImGui::DockSpace(dock, ImGui::GetContentRegionAvail(), ImGuiDockNodeFlags_None);
        } else {
            ImGui::DockSpace(dock, ImVec2(0, 0), ImGuiDockNodeFlags_KeepAliveOnly);
        }
        ImGui::End();
        if (!open) extraWindows[i] = false;
    }
}

void App::NewExtraWindow() {
    for (size_t i = 0; i < extraWindows.size(); ++i)
        if (!extraWindows[i]) { extraWindows[i] = true; return; }
    if (extraWindows.size() < 8) extraWindows.push_back(true);
}

bool App::Shown(const char* window) const {
    ImGuiWindow* w = ImGui::FindWindowByName(window);
    if (!w) return true;
    ImGuiDockNode* n = w->DockNode ? w->DockNode : (w->DockId ? ImGui::DockBuilderGetNode(w->DockId) : nullptr);
    // Panels of the side tabs live in their own dockspaces, inside "Panneaux":
    // panels of the other tabs are not submitted at all.
    for (int guard = 0; n && guard < 64; ++guard) {
        if (!n->ParentNode)
            for (int i = 0; i < kSideTabs; ++i)
                if (i != sideTab && n->ID == SideTabDock(i)) return false;
        if (hideBottom && n->ID == bottomZone) return false;
        if (hideRight && n->ID == rightZone) return false;
        if (n->ParentNode) n = n->ParentNode;
        else if (n->HostWindow && n->HostWindow->DockNode && n->HostWindow->DockNode != n) n = n->HostWindow->DockNode;
        else break;
    }
    return true;
}

// ------------------------------------------------------------------ saved layouts

void App::ScanLayouts() {
    savedLayouts.clear();
    std::error_code ec;
    for (auto& e : std::filesystem::directory_iterator(Utf8Path(LayoutDir()), ec))
        if (e.path().extension() == ".ini") savedLayouts.push_back(Narrow(e.path().stem().wstring()));
    std::sort(savedLayouts.begin(), savedLayouts.end());
}

void App::SaveLayout(const std::string& name) {
    if (name.empty()) return;
    std::error_code ec;
    std::filesystem::create_directories(Utf8Path(LayoutDir()), ec);
    size_t n = 0;
    const char* ini = ImGui::SaveIniSettingsToMemory(&n);
    std::ofstream f(Utf8Path(LayoutDir() + "/" + name + ".ini"), std::ios::binary);
    f.write(ini, (std::streamsize)n);
    Status(f ? "Disposition enregistrée : " + name : "Impossible d'enregistrer la disposition " + name);
    ScanLayouts();
}

void App::LoadLayout(const std::string& name) {
    std::ifstream f(Utf8Path(LayoutDir() + "/" + name + ".ini"), std::ios::binary);
    if (!f) { Status("Disposition introuvable : " + name); return; }
    std::stringstream ss;
    ss << f.rdbuf();
    pendingLayoutIni = ss.str();
    pendingLayoutName = name;
}

void App::ApplyPendingLayout() {
    // Before NewFrame: ImGui rebuilds windows and dock nodes from the settings.
    if (pendingLayoutIni.empty()) return;
    ImGui::ClearIniSettings();
    extraWindows.clear();
    ImGui::LoadIniSettingsFromMemory(pendingLayoutIni.c_str(), pendingLayoutIni.size());
    layoutBuilt = true;
    Status("Disposition : " + pendingLayoutName);
    pendingLayoutIni.clear();
}

void App::DeleteLayout(const std::string& name) {
    std::error_code ec;
    std::filesystem::remove(Utf8Path(LayoutDir() + "/" + name + ".ini"), ec);
    ScanLayouts();
}

void App::DrawWindowMenu() {
    if (!ImGui::BeginMenu("Fenêtre")) return;
    if (ImGui::MenuItem("Nouvelle fenêtre")) NewExtraWindow();
    for (size_t i = 0; i < extraWindows.size(); ++i) {
        bool o = extraWindows[i];
        if (ImGui::MenuItem(ExtraWindowName((int)i).c_str(), nullptr, &o)) extraWindows[i] = o;
    }
    ImGui::Separator();
    for (int i = 0; i < kSideTabs; ++i) {
        char key[16];
        snprintf(key, sizeof(key), "Alt+%d", i + 1);
        if (ImGui::MenuItem(kSideTabNames[i], key, sideTab == i && !hideRight)) ShowSideTab(i);
    }
    ImGui::Separator();
    ImGui::MenuItem("Masquer la zone graphe", "Tab", &hideBottom);
    ImGui::MenuItem("Masquer la colonne de droite", "Maj+Tab", &hideRight);
    ImGui::Separator();
    if (ImGui::BeginMenu("Dispositions")) {
        if (savedLayouts.empty()) ImGui::TextDisabled("Aucune disposition enregistrée");
        for (auto& l : savedLayouts)
            if (ImGui::MenuItem(l.c_str())) LoadLayout(l);
        ImGui::Separator();
        if (ImGui::MenuItem("Enregistrer la disposition...")) layoutSavePopup = true;
        if (!savedLayouts.empty() && ImGui::BeginMenu("Supprimer")) {
            for (auto& l : savedLayouts)
                if (ImGui::MenuItem(l.c_str())) { DeleteLayout(l); break; }
            ImGui::EndMenu();
        }
        ImGui::EndMenu();
    }
    if (ImGui::MenuItem("Réinitialiser la disposition")) resetLayout = true;
    ImGui::Separator();
    ImGui::MenuItem("Démo ImGui", nullptr, &showDemo);
    ImGui::EndMenu();
}

void App::DrawLayoutSavePopup() {
    if (layoutSavePopup) {
        ImGui::OpenPopup("Enregistrer la disposition");
        layoutSavePopup = false;
    }
    if (ImGui::BeginPopupModal("Enregistrer la disposition", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Nom de la disposition :");
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        bool enter = ImGui::InputText("##layoutname", &layoutName, ImGuiInputTextFlags_EnterReturnsTrue);
        std::string clean;
        for (char c : layoutName)
            if (!strchr("\\/:*?\"<>|", c)) clean += c;
        bool exists = std::find(savedLayouts.begin(), savedLayouts.end(), clean) != savedLayouts.end();
        if (exists) ImGui::TextColored(ImVec4(1, 0.7f, 0.3f, 1), "Remplace la disposition existante.");
        ImGui::BeginDisabled(clean.empty());
        if (ImGui::Button("Enregistrer") || (enter && !clean.empty())) {
            SaveLayout(clean);
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Annuler")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

}  // namespace sx
