// Search everywhere: the field of the menu bar, its results, reaching a result (ui/Search.h).
#include "ui/App.h"
#include "ui/Search.h"
#include "ui/Theme.h"
#include "doc/Brushes.h"

#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <filesystem>
#include <sstream>

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

void App::BuildSearchRows(const std::string& q) {
    // UI results (index) + contents (effects, brushes, smart materials, library functions).
    auto& S = GlobalSearch::Get();
    searchRows.clear();
    for (const SearchEntry* e : S.Query(q, 50)) searchRows.push_back({e->kind, e->label, e->context, std::string(), *e});
    std::vector<std::string> words;
    {
        std::istringstream ss(GlobalSearch::Fold(q));
        for (std::string w; ss >> w;) words.push_back(w);
    }
    if (words.empty()) return;
    auto match = [&](const std::string& label, const std::string& extra) {
        const std::string hay = GlobalSearch::Fold(label + " " + extra);
        for (auto& w : words)
            if (hay.find(w) == std::string::npos) return -1;
        const std::string lab = GlobalSearch::Fold(label);
        return (lab.rfind(words[0], 0) == 0 ? 2 : 0) + (lab.find(words[0]) != std::string::npos ? 1 : 0);
    };
    struct Hit { int score; SearchRow row; };
    auto add = [&](std::vector<Hit>& hits, size_t max) {
        std::stable_sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b) { return a.score > b.score; });
        for (size_t k = 0; k < hits.size() && k < max; ++k) searchRows.push_back(hits[k].row);
    };
    std::vector<Hit> fx, br, sm, fn;
    for (auto& d : Effects().All())
        if (d && !d->id.empty())
            if (int sc = match(d->name, d->category + " " + d->id); sc >= 0) fx.push_back({sc, {3, d->name, "Effet : " + d->category, d->id, {}}});
    for (auto& b : Brushes().All())
        if (b && b->def)
            if (int sc = match(b->def->name, b->def->category + " pinceau brush"); sc >= 0) br.push_back({sc, {4, b->def->name, "Pinceau : " + b->def->category, b->id, {}}});
    if (DocView* v = Active(); v && v->doc->kind == 1)
        for (const Json& j : SmartMaterials())
            if (int sc = match(j["name"].S(""), j["description"].S("") + " materiau intelligent"); sc >= 0)
                sm.push_back({sc, {5, j["name"].S(""), "Matériau intelligent", j["name"].S(""), {}}});
    for (auto& f : FunctionLibrary())
        if (int sc = match(f.name, f.desc + " fonction"); sc >= 0) fn.push_back({sc, {6, f.name, f.kind == "tool" ? "Outil (bibliothèque)" : "Fonction (bibliothèque)", f.path, {}}});
    add(fx, 8);
    add(br, 6);
    add(sm, 6);
    add(fn, 6);
}

void App::RunSearchRow(const SearchRow& r) {
    // Remembered (recent results, newest first, without duplicates).
    recentSearches.erase(std::remove_if(recentSearches.begin(), recentSearches.end(),
                                        [&](const SearchRow& x) { return x.group == r.group && x.label == r.label && x.payload == r.payload && x.context == r.context; }),
                         recentSearches.end());
    recentSearches.insert(recentSearches.begin(), r);
    if (recentSearches.size() > 10) recentSearches.resize(10);
    DocView* v = Active();
    switch (r.group) {
        case 0: case 1: case 2: ActivateSearchResult(r.entry); return;
        case 3:
            if (v) { ApplyLibraryEffect(*v, r.payload, FxApply::Filter); Status("Effet ajouté au calque actif : " + r.label); }
            else Status("Ouvrez un document pour appliquer un effet");
            return;
        case 4:
            if (xf.active && xf.view) TransformCommit(*xf.view);
            SelectTip(brush, r.payload);
            tool = Tool::Brush;
            Status("Pinceau : " + r.label);
            return;
        case 5:
            if (v && v->doc->kind == 1)
                for (const Json& j : SmartMaterials())
                    if (j["name"].S("") == r.payload) { ApplySmartMaterial(*v, j); return; }
            Status("Matériau intelligent : ouvrez un projet Paint 3D");
            return;
        case 6:
            if (v) {
                InsertLibraryFunction(*v, r.payload, {0, 0});
                Status("Ajouté au graphe : " + r.label);
            }
            return;
        default: return;
    }
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
    if (!searchOpen) return;
    // Rows: the query's results, or the recent ones while the field is empty.
    if (searchText != searchRowsQuery) {
        searchRowsQuery = searchText;
        BuildSearchRows(searchText);
        searchSel = 0;
    }
    const std::vector<SearchRow>& rows = searchText.empty() ? recentSearches : searchRows;
    if (searchText.empty() && (rows.empty() || !active)) {
        if (!active) searchOpen = false;
        return;
    }
    ImGuiIO& io = ImGui::GetIO();
    if (active) {
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) searchSel = std::min(searchSel + 1, (int)rows.size() - 1);
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) searchSel = std::max(searchSel - 1, 0);
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) { searchOpen = false; return; }
    }
    searchSel = std::clamp(searchSel, 0, std::max((int)rows.size() - 1, 0));
    int chosen = -1;
    if (enter && !rows.empty()) chosen = searchSel;
    ImGui::SetNextWindowPos(ImVec2(a.x, b.y + 2));
    ImGui::SetNextWindowSizeConstraints(ImVec2(440 * dpi, 0), ImVec2(600 * dpi, 560 * dpi));
    bool hovered = false;
    if (ImGui::Begin("##searchresults", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                         ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav)) {
        ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
        hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
        if (rows.empty()) ImGui::TextDisabled(S.Crawling() ? "Indexation en cours..." : "Aucun résultat.");
        static const char* groups[] = {"Fenêtres et onglets", "Menus", "Propriétés et boutons", "Effets", "Pinceaux", "Matériaux intelligents", "Fonctions et outils"};
        static const int order[] = {1, 0, 2, 3, 4, 5, 6};
        if (searchText.empty()) ImGui::SeparatorText("Récents");
        int shown = 0;
        for (int gi : order) {
            bool header = searchText.empty();
            for (size_t i = 0; i < rows.size(); ++i) {
                const SearchRow& r = rows[i];
                if (r.group != gi) continue;
                if (!header) {
                    ImGui::SeparatorText(groups[gi]);
                    header = true;
                }
                ImGui::PushID((int)i);
                const bool sel = (int)i == searchSel;
                if (ImGui::Selectable(r.label.c_str(), sel, ImGuiSelectableFlags_None, ImVec2(230 * dpi, 0))) chosen = (int)i;
                if (sel && active && (ImGui::IsKeyPressed(ImGuiKey_DownArrow) || ImGui::IsKeyPressed(ImGuiKey_UpArrow))) ImGui::SetScrollHereY();
                ImGui::SameLine(240 * dpi);
                ImGui::TextDisabled("%s", r.context.c_str());
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
    if (chosen >= 0 && chosen < (int)rows.size()) {
        const SearchRow r = rows[chosen];  // copy: running it may change the recent list
        RunSearchRow(r);
        searchOpen = false;
        searchText.clear();
        ImGui::SetWindowFocus(nullptr);
    } else if (!active && !hovered && io.MouseClicked[0]) {
        searchOpen = false;
    }
}

}  // namespace sx
