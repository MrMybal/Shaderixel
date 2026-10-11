#pragma once
// Search everywhere (Unreal-like): every menu item, window / tab and labeled widget (property) the UI
// draws is indexed through Dear ImGui's test engine hooks (IMGUI_ENABLE_TEST_ENGINE), kept between
// sessions; a query lists them, highlights the matching properties in the visible panels, and a result
// is reached: menu path replayed (ActivateItemByID), window / tab brought forward, property scrolled to.
#include "core/Common.h"

#include <imgui.h>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

struct ImGuiContext;
struct ImGuiWindow;

namespace sx {

struct SearchEntry {
    int kind = 0;                       // 0 window / tab, 1 menu item, 2 widget (property, button...)
    std::string label;                  // shown text
    std::string window;                 // window to bring forward (widgets), empty for menus
    std::vector<std::string> path;      // menus: headers from the menu bar; widgets: [window title]
    std::string context;                // section shown under the label
    double seen = 0;                    // last time drawn (ImGui time; 0: loaded from disk)
};

class GlobalSearch {
public:
    static GlobalSearch& Get();
    // Hooks (imgui test engine).
    void OnItemAdd(ImGuiContext* ctx, unsigned id, const float bb[4]);
    void OnItemInfo(ImGuiContext* ctx, unsigned id, const char* label, int flags);

    void Load(const std::string& file);
    void Save(const std::string& file) const;
    std::vector<const SearchEntry*> Query(const std::string& q, size_t max) const;
    static std::string Fold(const std::string& s);  // lower case, accents removed

    // Frame: activation of a result, highlights, crawl (menus opened invisibly, tabs visited).
    void BeginFrame();
    void EndFrame();
    void Activate(const SearchEntry& e);
    void StartCrawl();
    bool Crawling() const { return crawl.active; }
    bool MenusHidden() const { return crawl.active && crawl.phase == 0; }
    size_t Size() const { return entries.size(); }
    std::string query;  // current query (highlights in the panels)
    std::function<void(const std::string& window)> reveal;  // makes a window visible (side tab, closed panel)
    bool highlight = false;

private:
    std::unordered_map<std::string, SearchEntry> entries;  // key: kind | path | label
    std::unordered_map<unsigned, std::vector<std::string>> menuPathOf;  // popup window id -> header path (this frame)
    std::unordered_map<unsigned, std::pair<std::string, unsigned>> lastHeader;  // window id -> last menu header drawn (label, id)
    std::unordered_map<std::string, unsigned> liveMenuIds;  // "a\x1fb\x1fitem" -> id (menus seen recently)
    unsigned lastAddId = 0;
    float lastBB[4] = {};
    struct Match { ImGuiWindow* window; float bb[4]; };
    std::vector<Match> matches;  // this frame: widgets matching the query (highlight)
    struct Pending {
        bool active = false;
        int kind = 0;
        std::vector<std::string> chain;  // menus: headers + item
        size_t step = 0;
        int wait = 0, tries = 0;
        std::string window, label;
        bool found = false;
        float bb[4] = {};
        ImGuiWindow* win = nullptr;
        double flashUntil = 0;
        int focusIn = -1;  // frames before focusing the window (after its side tab is shown)
    } pend;
    struct Crawl {
        bool active = false;
        int phase = 0;  // 0 menus, 1 tabs
        std::vector<std::vector<std::string>> queue;
        size_t index = 0;
        int wait = 0;
        size_t step = 0;
        std::vector<std::string> tabs, restore;
        std::vector<std::vector<std::string>> done;
    } crawl;
    static std::string Key(int kind, const std::vector<std::string>& path, const std::string& label);
    void Remember(int kind, const std::vector<std::string>& path, const std::string& label, const std::string& window, unsigned id);
};

}  // namespace sx
