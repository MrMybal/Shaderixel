// Search everywhere: the field of the menu bar, its results, reaching a result (ui/Search.h).
#include "ui/App.h"
#include "ui/Search.h"
#include "ui/Theme.h"

#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <filesystem>

namespace sx {

namespace {
std::string IndexFile(const std::string& settingsPath) {
    return Narrow(Utf8Path(settingsPath).parent_path().wstring()) + "\\shaderixel_search.json";
}
// Dock space of a side tab (same ids as Workspace.cpp).
ImGuiID SideTabDock(int i) {
    char id[32];
    snprintf(id, sizeof(id), "##sxside%d", i);
    return ImHashStr(id);
}
std::string ShownName(const std::string& windowName) {
    std::string s = windowName;
    if (size_t p = s.find("##"); p != std::string::npos) s.resize(p);
    return s;
}
}  // namespace

void App::InitSearch() {
    auto& S = GlobalSearch::Get();
    S.Load(IndexFile(settingsPath));
    S.reveal = [this](const std::string& name) { RevealWindow(name); };
}

void App::RevealWindow(const std::string& name) {
    // A panel may be closed, in another side tab (one dock space per tab) or in a hidden zone.
    if (auto it = panelOpen.find(ShownName(name)); it != panelOpen.end()) it->second.open = true;
    ImGuiWindow* w = ImGui::FindWindowByName(name.c_str());
    if (!w || !w->DockNode) return;
    ImGuiDockNode* n = w->DockNode;
    while (n->ParentNode) n = n->ParentNode;
    for (int i = 0; i < kSideTabs; ++i)
        if (n->ID == SideTabDock(i)) {
            ShowSideTab(i);
            return;
        }
    hideBottom = false;
}

void App::SaveSearch() {
    GlobalSearch::Get().Save(IndexFile(settingsPath));
}

void App::UpdateSearch() {
    auto& S = GlobalSearch::Get();
    // Every menu and tab indexed once per version of the application (the index is kept between sessions).
    if (!searchCrawlChecked && frameCounter > 45) {
        searchCrawlChecked = true;
        std::error_code ec;
        namespace fs = std::filesystem;
        wchar_t exe[MAX_PATH] = L"";
        GetModuleFileNameW(nullptr, exe, MAX_PATH);
        const auto idx = Utf8Path(IndexFile(settingsPath));
        const bool stale = !fs::exists(idx) || fs::last_write_time(fs::path(exe), ec) > fs::last_write_time(idx, ec);
        if ((stale && !Automated()) || GetEnvironmentVariableA("SX_CRAWL", nullptr, 0) > 0) {
            S.StartCrawl();
            Status("Indexation de la recherche (menus et onglets)...");
        }
    }
    if (searchWasCrawling && !S.Crawling()) {
        SaveSearch();
        Status("Recherche prête : " + std::to_string(S.Size()) + " éléments (Ctrl+Maj+F)");
    }
    searchWasCrawling = S.Crawling();
    ImGuiIO& io = ImGui::GetIO();
    if (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_F, false)) {
        searchFocusRequest = true;
        searchOpen = true;
    }
}

void App::ActivateSearchResult(const SearchEntry& e) {
    if (e.kind == 0 && e.window.empty()) RevealWindow(e.label);
    GlobalSearch::Get().Activate(e);
    Status(e.kind == 1 ? "Menu : " + e.label : e.kind == 0 ? "Fenêtre : " + e.label : e.context + " > " + e.label);
}

void App::DrawSearchBox() {
    auto& S = GlobalSearch::Get();
    ImGui::SetNextItemWidth(230 * dpi);
    if (searchFocusRequest) {
        ImGui::SetKeyboardFocusHere();
        searchFocusRequest = false;
    }
    const bool enter = ImGui::InputTextWithHint("##globalsearch", "Rechercher partout (Ctrl+Maj+F)", &searchText, ImGuiInputTextFlags_EnterReturnsTrue);
    const bool active = ImGui::IsItemActive();
    const ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
    if (active) searchOpen = true;
    S.query = searchOpen ? searchText : std::string();
    S.highlight = searchHighlight;
    if (!searchOpen || searchText.empty()) return;
    const auto results = S.Query(searchText, 60);
    ImGuiIO& io = ImGui::GetIO();
    if (active) {
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) searchSel = std::min(searchSel + 1, (int)results.size() - 1);
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) searchSel = std::max(searchSel - 1, 0);
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) { searchOpen = false; return; }
    }
    searchSel = std::clamp(searchSel, 0, std::max((int)results.size() - 1, 0));
    const SearchEntry* chosen = nullptr;
    if (enter && !results.empty()) chosen = results[searchSel];
    ImGui::SetNextWindowPos(ImVec2(a.x, b.y + 2));
    ImGui::SetNextWindowSizeConstraints(ImVec2(420 * dpi, 0), ImVec2(560 * dpi, 520 * dpi));
    bool hovered = false;
    if (ImGui::Begin("##searchresults", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                         ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav)) {
        ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
        hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
        if (results.empty()) ImGui::TextDisabled(S.Crawling() ? "Indexation en cours..." : "Aucun résultat.");
        static const char* groups[] = {"Fenêtres et onglets", "Menus", "Propriétés et boutons"};
        static const int order[] = {1, 0, 2};
        int shown = 0;
        for (int gi : order) {
            bool header = false;
            for (size_t i = 0; i < results.size(); ++i) {
                const SearchEntry* e = results[i];
                if (e->kind != gi) continue;
                if (!header) {
                    ImGui::SeparatorText(groups[gi]);
                    header = true;
                }
                ImGui::PushID((int)i);
                const bool sel = (int)i == searchSel;
                if (ImGui::Selectable(e->label.c_str(), sel, ImGuiSelectableFlags_None, ImVec2(220 * dpi, 0))) chosen = e;
                if (sel && active && (ImGui::IsKeyPressed(ImGuiKey_DownArrow) || ImGui::IsKeyPressed(ImGuiKey_UpArrow))) ImGui::SetScrollHereY();
                ImGui::SameLine(230 * dpi);
                ImGui::TextDisabled("%s", e->context.c_str());
                ImGui::PopID();
                ++shown;
            }
        }
        ImGui::Separator();
        ImGui::Checkbox("Surligner dans les panneaux", &searchHighlight);
        ui::HelpTooltip("Les propriétés et boutons visibles qui correspondent sont entourés pendant la recherche.");
        ImGui::SameLine();
        ImGui::TextDisabled("%d résultat(s) - Entrée : y aller", shown);
    }
    ImGui::End();
    if (chosen) {
        ActivateSearchResult(*chosen);
        searchOpen = false;
        ImGui::SetWindowFocus(nullptr);
    } else if (!active && !hovered && io.MouseClicked[0]) {
        searchOpen = false;
    }
}

}  // namespace sx
