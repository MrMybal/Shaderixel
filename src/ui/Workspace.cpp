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
const char* kSideTabNames[App::kSideTabs] = {"Général", "Outils", "Effets", "Fusion", "Peinture", "3D", "IA"};

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
    else if (sscanf(line, "Migration=%d", &i) == 1) a.layoutMigration = i;
    else if (sscanf(line, "Kind=%d", &i) == 1) a.layoutKind = std::clamp(i, 0, 1);
    else if (sscanf(line, "HideRight=%d", &i) == 1) a.hideRight = i != 0;
    else if (sscanf(line, "Bottom=0x%08X", &u) == 1) a.bottomZone = u;
    else if (sscanf(line, "Right=0x%08X", &u) == 1) a.rightZone = u;
    else if (sscanf(line, "Windows=%d", &i) == 1) a.extraWindows.assign(std::clamp(i, 0, 8), true);
    else if (sscanf(line, "WindowOpen%d=", &i) == 1 && i >= 0 && i < (int)a.extraWindows.size()) a.extraWindows[i] = strchr(line, '=')[1] == '1';
    else if (strncmp(line, "Closed=", 7) == 0) a.panelOpen[line + 7].open = false;
}

void SettingsWriteAll(ImGuiContext*, ImGuiSettingsHandler* h, ImGuiTextBuffer* buf) {
    App& a = *gWorkspaceApp;
    buf->appendf("[%s][State]\n", h->TypeName);
    buf->appendf("Tab=%d\nHideBottom=%d\nHideRight=%d\n", a.sideTab, a.hideBottom ? 1 : 0, a.hideRight ? 1 : 0);
    buf->appendf("Bottom=0x%08X\nRight=0x%08X\n", a.bottomZone, a.rightZone);
    buf->appendf("Migration=%d\nKind=%d\n", a.layoutMigration, a.layoutKind);
    buf->appendf("Windows=%d\n", (int)a.extraWindows.size());
    for (size_t i = 0; i < a.extraWindows.size(); ++i) buf->appendf("WindowOpen%d=%d\n", (int)i, a.extraWindows[i] ? 1 : 0);
    for (auto& [name, f] : a.panelOpen)
        if (!f.open) buf->appendf("Closed=%s\n", name.c_str());
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
    layoutMigration = kLayoutMigration;  // the default layout has every panel in place
    ImVec2 work = ImGui::GetMainViewport()->WorkSize;
    ImGui::DockBuilderRemoveNode(dock);
    ImGui::DockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dock, work);
    // Toolbar | center (canvas, 3D view, effect editor) over the bottom zone (graph,
    // layers | layer preview) | right column = side tabs.
    ImGuiID left, rest, right, center, bottom, bottomLeft, bottomRight;
    ImGui::DockBuilderSplitNode(dock, ImGuiDir_Left, ToolbarWidth() / std::max(work.x, 1.0f), &left, &rest);
    ImGui::DockBuilderSplitNode(rest, ImGuiDir_Right, 0.25f, &right, &center);
    ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.30f, &bottom, &center);
    ImGui::DockBuilderSplitNode(bottom, ImGuiDir_Right, 0.21f, &bottomRight, &bottomLeft);
    ImGui::DockBuilderDockWindow("Outils", left);
    const bool p3d = layoutKind == 1;
    if (p3d) {
        // Paint 3D: the model first, the texture (UV) next to it; materials shelf at the bottom.
        ImGui::DockBuilderDockWindow("Vue 3D", center);
        ImGui::DockBuilderDockWindow("Canevas", center);
        ImGui::DockBuilderDockWindow("Rendu", center);
        ImGui::DockBuilderDockWindow("Éditeur d'effets", center);
        ImGui::DockBuilderDockWindow("Matériaux", bottomLeft);
        ImGui::DockBuilderDockWindow("Calques", bottomLeft);
        ImGui::DockBuilderDockWindow("Graphe", bottomLeft);
    } else {
        ImGui::DockBuilderDockWindow("Canevas", center);
        ImGui::DockBuilderDockWindow("Vue 3D", center);
        ImGui::DockBuilderDockWindow("Éditeur d'effets", center);
        ImGui::DockBuilderDockWindow("Graphe", bottomLeft);
        ImGui::DockBuilderDockWindow("Calques", bottomLeft);
    }
    ImGui::DockBuilderDockWindow("Aperçu du calque", bottomRight);
    ImGui::DockBuilderDockWindow("Panneaux", right);
    if (ImGuiDockNode* n = ImGui::DockBuilderGetNode(left)) n->LocalFlags |= ImGuiDockNodeFlags_NoTabBar | ImGuiDockNodeFlags_NoResizeX;
    // The side tabs host: nothing docks over it (a panel dropped there would vanish behind it).
    if (ImGuiDockNode* n = ImGui::DockBuilderGetNode(right)) n->LocalFlags |= ImGuiDockNodeFlags_NoTabBar | ImGuiDockNodeFlags_NoDockingOverMe;
    ImGui::DockBuilderFinish(dock);
    panelOpen.clear();  // the default layout shows every panel
    // Tabs in front once the windows exist (next frames).
    focusAfterLayout = {p3d ? "Matériaux" : "Graphe", p3d ? "Vue 3D" : "Canevas"};
    if (p3d) sideTabRequest = 5;
    bottomZone = bottom;
    rightZone = right;

    // One dockspace per side tab; hidden tabs keep their panels docked.
    ImVec2 sideSize(work.x * 0.25f, work.y - ImGui::GetFrameHeight() * 3);
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
                ImGui::DockBuilderDockWindow("Histogramme", top);
                ImGui::DockBuilderDockWindow("Propriétés", low);
                ImGui::DockBuilderDockWindow("Historique", low);
                break;
            case 1:  // Outils
                ImGui::DockBuilderDockWindow("Options", id);
                ImGui::DockBuilderDockWindow("Tracés", id);
                ImGui::DockBuilderDockWindow("Actions", id);
                ImGui::DockBuilderDockWindow("Compositions de calques", id);
                ImGui::DockBuilderDockWindow("Styles de texte", id);
                break;
            case 2:  // Effets
                ImGui::DockBuilderDockWindow("Effets", id);
                ImGui::DockBuilderDockWindow("Palette de nœuds", id);
                break;
            case 3:  // Fusion
                ImGui::DockBuilderDockWindow("Fusions", id);
                break;
            case 4:  // Peinture
                two(0.42f);
                ImGui::DockBuilderDockWindow("Couleur", top);
                ImGui::DockBuilderDockWindow("Nuancier", top);
                ImGui::DockBuilderDockWindow("Pinceaux", low);
                ImGui::DockBuilderDockWindow("Motifs", low);
                break;
            case 5:  // 3D: project layers and brush material (+ the materials shelf for image documents)
                two(0.55f);
                ImGui::DockBuilderDockWindow("Calques 3D", top);
                if (p3d) ImGui::DockBuilderDockWindow("Scène de rendu", top);
                ImGui::DockBuilderDockWindow("Matériau du pinceau", low);
                if (!p3d) ImGui::DockBuilderDockWindow("Matériaux", top);
                break;
            case 6:  // IA
                ImGui::DockBuilderDockWindow("Assistant IA", id);
                ImGui::DockBuilderDockWindow("Génération IA", id);
                break;
        }
        ImGui::DockBuilderFinish(id);
        // First panel of each group in front.
        auto front = [&](ImGuiID node, const char* window) {
            if (ImGuiDockNode* n = ImGui::DockBuilderGetNode(node)) n->SelectedTabId = ImGui::FindWindowByName(window) ? ImGui::FindWindowByName(window)->TabId : 0;
        };
        if (i == 1) front(id, "Options");
        if (i == 2) front(id, "Effets");
        if (i == 4) { front(top, "Couleur"); front(low, "Pinceaux"); }
        if (i == 0) front(low, "Propriétés");
        if (i == 5) front(top, "Calques 3D");
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
    // Panels of the side tabs always show their tab (move / close), also with older saved layouts.
    ImGuiContext& g = *GImGui;
    for (int n = 0; n < g.DockContext.Nodes.Data.Size; ++n) {
        ImGuiDockNode* node = (ImGuiDockNode*)g.DockContext.Nodes.Data[n].val_p;
        if (!node) continue;
        ImGuiDockNode* root = node;
        while (root->ParentNode) root = root->ParentNode;
        bool side = false;
        for (int i = 0; i < kSideTabs; ++i) side |= root->ID == SideTabDock(i);
        if (side) node->LocalFlags &= ~(ImGuiDockNodeFlags_AutoHideTabBar | ImGuiDockNodeFlags_HiddenTabBar | ImGuiDockNodeFlags_NoTabBar);
    }
    ImGui::End();
}

void App::MigrateLayout() {
    // Panels added in later versions, left floating by an older saved layout: put them in their tab once.
    if (layoutMigration >= kLayoutMigration) return;
    if (++layoutMigrationFrames < 3) return;  // windows created first
    struct P { const char* name; int tab; bool lower; };
    static const P panels[] = {{"Actions", 1, false}, {"Compositions de calques", 1, false}, {"Styles de texte", 1, false},
                               {"Nuancier", 4, false}, {"Motifs", 4, true}, {"Histogramme", 0, false},
                               {"Palette de nœuds", 2, false}, {"Trim sheet", 1, true}, {"Inpainting IA", 6, false},
                               {"Calques 3D", 5, false}, {"Matériau du pinceau", 5, true}, {"Scène de rendu", 5, false}};
    for (const P& p : panels) {
        ImGuiWindow* w = ImGui::FindWindowByName(p.name);
        if (!w || w->DockId || IsPoppedOut(p.name)) continue;
        ImGuiDockNode* n = ImGui::DockBuilderGetNode(SideTabDock(p.tab));
        while (n && n->IsSplitNode()) n = n->ChildNodes[p.lower ? 1 : 0] ? n->ChildNodes[p.lower ? 1 : 0] : n->ChildNodes[0];
        if (n) ImGui::DockBuilderDockWindow(p.name, n->ID);
    }
    layoutMigration = kLayoutMigration;
}

void App::SwitchWorkspaceKind() {
    // Image documents and Paint 3D projects each have their workspace (saved separately).
    DocView* v = Active();
    if (!v || !layoutBuilt) return;
    const int want = v->doc->kind == 1 ? 1 : 0;
    if (want == layoutKind) return;
    size_t n = 0;
    const char* ini = ImGui::SaveIniSettingsToMemory(&n);
    layoutSlot[layoutKind].assign(ini, n);
    {
        std::error_code ec;
        std::filesystem::create_directories(Utf8Path(LayoutDir()), ec);
        std::ofstream f(Utf8Path(LayoutDir() + "/_espace_" + std::to_string(layoutKind) + ".ini"), std::ios::binary);
        f << layoutSlot[layoutKind];
    }
    layoutKind = want;
    if (layoutSlot[want].empty()) {
        std::ifstream f(Utf8Path(LayoutDir() + "/_espace_" + std::to_string(want) + ".ini"), std::ios::binary);
        if (f) layoutSlot[want].assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    }
    if (!layoutSlot[want].empty() && layoutSlot[want].find("Kind=" + std::to_string(want)) != std::string::npos) {
        pendingLayoutIni = layoutSlot[want];
        pendingLayoutName = want ? "espace Paint 3D" : "espace Image";
    } else {
        resetLayout = true;  // default workspace of that kind
    }
}

void App::DockNewPanel(int tab, bool lower) {
    // A panel added after the layout was saved: docked into a leaf of the side tab (first use only).
    ImGuiDockNode* n = ImGui::DockBuilderGetNode(SideTabDock(tab));
    while (n && n->IsSplitNode()) n = n->ChildNodes[lower ? 1 : 0] ? n->ChildNodes[lower ? 1 : 0] : n->ChildNodes[0];
    if (n) ImGui::SetNextWindowDockID(n->ID, ImGuiCond_FirstUseEver);
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

// Graph zone (bottom) folded and unfolded in one click: a fold button on the zone, and when it is
// folded a small bar at the bottom of the canvas to unfold it or pick the layer (or node) to edit.
void App::DrawZoneToggles() {
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize |
                                   ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDocking |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus;  // never takes the keyboard focus from the panels
    if (!hideBottom) {
        ImGuiDockNode* n = bottomZone ? ImGui::DockBuilderGetNode(bottomZone) : nullptr;
        if (!n || !n->IsVisible || n->Size.y < 40) return;
        const float bw = ImGui::GetFrameHeight() * 1.6f;
        ImGui::SetNextWindowPos(ImVec2(n->Pos.x + n->Size.x - bw - 78 * dpi, n->Pos.y + 2));
        ImGui::SetNextWindowBgAlpha(0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        if (ImGui::Begin("##foldBottom", nullptr, flags)) {
            if (ImGui::SmallButton("\xE2\x96\xBC Replier")) hideBottom = true;
            ui::HelpTooltip("Replier la zone du graphe pour éditer en grand (Tab). Une petite barre en bas du canevas la rouvre et permet de changer de calque.");
        }
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }
    ImGuiWindow* canvas = ImGui::FindWindowByName("Canevas");
    if (!canvas || canvas->Hidden) return;
    DocView* v = Active();
    ImGui::SetNextWindowPos(ImVec2(canvas->Pos.x + canvas->Size.x * 0.5f, canvas->Pos.y + canvas->Size.y - 8 * dpi), ImGuiCond_Always, ImVec2(0.5f, 1.0f));
    ImGui::SetNextWindowBgAlpha(0.92f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6 * dpi);
    if (ImGui::Begin("##unfoldBottom", nullptr, flags)) {
        if (ImGui::SmallButton("\xE2\x96\xB2 Graphe / Calques")) hideBottom = false;
        ui::HelpTooltip("Rouvrir la zone du graphe (Tab)");
        if (v) {
            Document& d = *v->doc;
            ImGui::SameLine();
            ImGui::TextDisabled("|");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(220 * dpi);
            if (d.Mode() == DocMode::Layers) {
                const Layer* al = d.ActiveLayer();
                if (ImGui::BeginCombo("##quicklayer", al ? ("Calque : " + al->name).c_str() : "Calque : aucun", ImGuiComboFlags_HeightLarge)) {
                    // Top of the stack first, groups indented.
                    const auto& layers = d.S().layers;
                    for (int i = (int)layers.size() - 1; i >= 0; --i) {
                        const Layer& l = layers[i];
                        int depth = 0;
                        for (Id p = l.parent; p && depth < 8; ++depth) {
                            const Layer* pl = d.FindLayer(p);
                            p = pl ? pl->parent : 0;
                        }
                        std::string label = std::string(depth * 3, ' ') + (l.kind == LayerKind::Group ? "\xE2\x96\xB8 " : "") + l.name + (l.visible ? "" : "  (masqué)");
                        if (ImGui::Selectable((label + "##" + std::to_string(l.id)).c_str(), l.id == d.S().activeLayer)) {
                            d.S().activeLayer = l.id;
                            d.S().editMask = false;
                        }
                    }
                    ImGui::EndCombo();
                }
            } else {
                const Node* an = d.S().graph.Find(d.S().activeNode);
                if (ImGui::BeginCombo("##quicknode", an ? ("Nœud : " + an->name).c_str() : "Nœud : aucun", ImGuiComboFlags_HeightLarge)) {
                    for (auto& n : d.S().graph.nodes)
                        if (ImGui::Selectable((n.name + "##" + std::to_string(n.id)).c_str(), n.id == d.S().activeNode)) d.S().activeNode = n.id;
                    ImGui::EndCombo();
                }
            }
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

bool App::Shown(const char* window) const {
    if (!PanelIsOpen(window)) return false;
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
        if (e.path().extension() == ".ini" && e.path().stem().wstring().rfind(L"_espace_", 0) != 0) savedLayouts.push_back(Narrow(e.path().stem().wstring()));
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
    panelOpen.clear();
    ImGui::LoadIniSettingsFromMemory(pendingLayoutIni.c_str(), pendingLayoutIni.size());
    layoutBuilt = true;
    layoutMigrationFrames = 0;  // a layout saved before some panels existed: MigrateLayout docks them
    Status("Disposition : " + pendingLayoutName);
    pendingLayoutIni.clear();
}

void App::DeleteLayout(const std::string& name) {
    std::error_code ec;
    std::filesystem::remove(Utf8Path(LayoutDir() + "/" + name + ".ini"), ec);
    ScanLayouts();
}

namespace {
// Panels that can be opened large in their own window.
const char* kPopOutPanels[] = {"Effets", "Fusions", "Calques", "Graphe", "Ressources", "Palette de nœuds", "Matériaux", "Pinceaux", "Motifs",
                               "Trim sheet", "Éditeur d'actions", "Éditeur d'effets"};
bool CanPopOut(const std::string& n) {
    for (auto* p : kPopOutPanels) if (n == p) return true;
    return false;
}
}  // namespace

void App::TogglePopOut(const std::string& name) {
    PopOut& P = popOuts[name];
    ImGuiWindow* w = ImGui::FindWindowByName(name.c_str());
    if (!P.out) {
        panelOpen[name].open = true;
        P.dock = w ? (w->DockId ? w->DockId : P.dock) : P.dock;
        if (w && w->DockIsActive) ImGui::DockContextQueueUndockWindow(GImGui, w);
        P.out = true;
        P.pending = 2;
        Status(name + " : ouvert en grand dans une fenêtre séparée (F11 ou l'icône du coin pour le rattacher)");
    } else {
        P.out = false;
        P.pending = 0;
        if (w) {
            w->WindowClass.ViewportFlagsOverrideSet &= ~ImGuiViewportFlags_NoAutoMerge;
            if (P.dock && ImGui::DockBuilderGetNode(P.dock)) ImGui::SetWindowDock(w, P.dock, ImGuiCond_Always);
        }
        Status(name + " : rattaché");
    }
}

void App::UpdatePopOuts() {
    for (auto& [name, P] : popOuts) {
        ImGuiWindow* w = ImGui::FindWindowByName(name.c_str());
        if (!w || !P.out) continue;
        if (P.pending > 0 && --P.pending == 0) {
            if (w->DockIsActive) { P.pending = 1; continue; }  // undock not applied yet
            // Its own OS window, large: on another screen when there is one, else over most of this one.
            w->WindowClass.ViewportFlagsOverrideSet |= ImGuiViewportFlags_NoAutoMerge;
            ImGuiViewport* main = ImGui::GetMainViewport();
            ImVec2 pos = main->WorkPos, size = main->WorkSize;
            const ImGuiPlatformIO& pio = ImGui::GetPlatformIO();
            const ImVec2 c(main->Pos.x + main->Size.x * 0.5f, main->Pos.y + main->Size.y * 0.5f);
            for (auto& m : pio.Monitors) {
                const bool here = c.x >= m.MainPos.x && c.x < m.MainPos.x + m.MainSize.x && c.y >= m.MainPos.y && c.y < m.MainPos.y + m.MainSize.y;
                if (!here && (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)) { pos = m.WorkPos; size = m.WorkSize; break; }
            }
            ImGui::SetWindowSize(w, ImVec2(size.x * 0.86f, size.y * 0.86f), ImGuiCond_Always);
            ImGui::SetWindowPos(w, ImVec2(pos.x + size.x * 0.07f, pos.y + size.y * 0.07f), ImGuiCond_Always);
            ImGui::FocusWindow(w);
        } else if (P.pending == 0 && w->DockIsActive) {
            P.out = false;  // docked back by hand
            w->WindowClass.ViewportFlagsOverrideSet &= ~ImGuiViewportFlags_NoAutoMerge;
        }
    }
    // F11: the panel under the mouse, opened large / put back.
    const ImGuiIO& io = ImGui::GetIO();
    if (ImGui::IsKeyPressed(ImGuiKey_F11, false) && !io.WantTextInput)
        if (ImGuiWindow* h = GImGui->HoveredWindow) {
            h = h->RootWindow;
            if (h && CanPopOut(h->Name)) TogglePopOut(h->Name);
        }
}

void App::PopOutCorner(const char* name) {
    // An icon at the top right of the panel. Submitted first, so it wins the mouse over what is under it;
    // drawn at the end of the frame (DrawPopOutCorners), above the panel's content.
    ImGuiWindow* w = ImGui::GetCurrentWindow();
    const float s = std::floor(ImGui::GetFontSize() * 1.1f);
    const ImVec2 keep = ImGui::GetCursorScreenPos();
    const ImVec2 p(w->InnerRect.Max.x - s - 3 * dpi, w->InnerRect.Min.y + 3 * dpi);
    ImGui::SetCursorScreenPos(p);
    ImGui::PushID("##popout");
    const bool click = ImGui::InvisibleButton("##po", ImVec2(s, s));
    const bool hov = ImGui::IsItemHovered();
    ImGui::PopID();
    if (hov) ImGui::SetTooltip(IsPoppedOut(name) ? "Rattacher le panneau (F11)" : "Ouvrir en grand dans une fenêtre séparée (F11)");
    ImGui::SetCursorScreenPos(keep);
    if (click) TogglePopOut(name);
    popCorners.push_back({name, p, ImVec2(p.x + s, p.y + s), hov});
}

void App::DrawPopOutCorners() {
    for (auto& c : popCorners) {
        ImGuiWindow* w = ImGui::FindWindowByName(c.name.c_str());
        if (!w || !w->Active) continue;
        // Appended to the panel: drawn above its own content.
        ImGui::Begin(c.name.c_str());
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->PushClipRect(w->Rect().Min, w->Rect().Max, false);
        const ImRect r(c.rmin, c.rmax);
        dl->AddRectFilled(r.Min, r.Max, c.hovered ? IM_COL32(90, 110, 160, 255) : IM_COL32(40, 44, 54, 220), 3);
        const float k = r.GetWidth();
        const ImU32 col = c.hovered ? IM_COL32(255, 255, 255, 255) : IM_COL32(190, 196, 210, 255);
        if (!IsPoppedOut(c.name)) {
            // Two windows: "open in another window".
            dl->AddRect(ImVec2(r.Min.x + k * 0.18f, r.Min.y + k * 0.36f), ImVec2(r.Min.x + k * 0.66f, r.Min.y + k * 0.82f), col, 1, 0, 1.3f);
            dl->AddRect(ImVec2(r.Min.x + k * 0.36f, r.Min.y + k * 0.18f), ImVec2(r.Min.x + k * 0.84f, r.Min.y + k * 0.62f), col, 1, 0, 1.3f);
        } else {
            // Arrow into a box: "put back".
            dl->AddRect(ImVec2(r.Min.x + k * 0.18f, r.Min.y + k * 0.18f), ImVec2(r.Min.x + k * 0.82f, r.Min.y + k * 0.82f), col, 1, 0, 1.3f);
            dl->AddLine(ImVec2(r.Min.x + k * 0.72f, r.Min.y + k * 0.28f), ImVec2(r.Min.x + k * 0.38f, r.Min.y + k * 0.62f), col, 1.5f);
            dl->AddLine(ImVec2(r.Min.x + k * 0.38f, r.Min.y + k * 0.62f), ImVec2(r.Min.x + k * 0.38f, r.Min.y + k * 0.40f), col, 1.5f);
            dl->AddLine(ImVec2(r.Min.x + k * 0.38f, r.Min.y + k * 0.62f), ImVec2(r.Min.x + k * 0.60f, r.Min.y + k * 0.62f), col, 1.5f);
        }
        dl->PopClipRect();
        ImGui::End();
    }
    popCorners.clear();
}

void App::DrawPanelsMenu() {
    // Closed panels come back here (also from Fenêtre).
    static const char* names[] = {"Calques", "Graphe", "Aperçu du calque", "Éditeur d'effets", "Vue 3D", "Propriétés", "Historique", "Histogramme",
                                  "Options", "Tracés", "Actions", "Compositions de calques", "Styles de texte", "Effets", "Palette de nœuds", "Fusions", "Couleur", "Nuancier", "Pinceaux", "Motifs", "Matériaux", "Calques 3D", "Matériau du pinceau", "Rendu", "Scène de rendu", "Assistant IA", "Génération IA", "Trim sheet", "Inpainting IA", "Ressources", "Éditeur d'actions"};
    if (!ImGui::BeginMenu("Panneaux")) return;
    for (const char* n : names) {
        bool open = PanelIsOpen(n);
        if (ImGui::MenuItem(n, nullptr, &open)) {
            panelOpen[n].open = open;
            if (open) {
                // Side tab panels: show their tab; others: bring forward.
                ImGuiWindow* w = ImGui::FindWindowByName(n);
                ImGuiDockNode* node = w ? (w->DockNode ? w->DockNode : ImGui::DockBuilderGetNode(w->DockId)) : nullptr;
                while (node && node->ParentNode) node = node->ParentNode;
                for (int i = 0; node && i < kSideTabs; ++i)
                    if (node->ID == SideTabDock(i)) ShowSideTab(i);
                ImGui::SetWindowFocus(n);
            }
        }
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Tout réafficher")) panelOpen.clear();
    ImGui::EndMenu();
}

void App::DrawPopOutMenu() {
    if (!ImGui::BeginMenu("Ouvrir en grand")) return;
    for (const char* n : kPopOutPanels) {
        bool out = IsPoppedOut(n);
        if (ImGui::MenuItem(n, nullptr, out)) TogglePopOut(n);
    }
    ImGui::Separator();
    ImGui::TextDisabled("F11 : le panneau sous la souris");
    ImGui::EndMenu();
}

void App::DrawWindowMenu() {
    if (!ImGui::BeginMenu("Fenêtre")) return;
    DrawPanelsMenu();
    DrawPopOutMenu();
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
