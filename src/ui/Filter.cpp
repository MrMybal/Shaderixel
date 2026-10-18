#include "ui/Filter.h"
#include "ui/Search.h"
#include "ui/Lang.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <cstring>
#include <sstream>
#include <unordered_map>
#include <vector>

// Hook of the patched Dear ImGui (imgui_widgets.cpp).
extern bool (*GSXFilterFn)(ImGuiWindow* window, int kind, const char* label, const char* label_end);

namespace sx::ui {

namespace {

struct FilterEntry {
    std::string text;                 // as typed
    std::vector<std::string> words;   // folded
    int frame = -10;                  // last frame its box was drawn
};

struct Section {
    int frame = -1;
    bool match = false;  // the current section's title matches: its content is shown
    int depth = 0;
    ImGuiWindow* rowWindow = nullptr;  // a matching text: the items on its line are shown too
    float rowY = -1;
};

std::unordered_map<ImGuiID, FilterEntry> gFilters;
std::unordered_map<ImGuiID, Section> gSections;
int gActiveFrame = -10;  // last frame a non-empty filter was drawn (fast path)

std::vector<std::string> Words(const std::string& s) {
    std::vector<std::string> w;
    std::istringstream ss(GlobalSearch::Fold(s));
    for (std::string t; ss >> t;) w.push_back(t);
    return w;
}

bool MatchWords(const std::vector<std::string>& words, const char* label, const char* end) {
    if (!label) return false;
    if (!end) end = label + strlen(label);
    const char* hash = strstr(label, "##");
    if (hash && hash < end) end = hash;
    const std::string src(label, end);
    // The words may be in the interface language or in French (the labels of the code).
    const std::string folded = GlobalSearch::Fold(src), shown = GlobalSearch::Fold(Tr(src));
    for (auto& w : words)
        if (folded.find(w) == std::string::npos && shown.find(w) == std::string::npos) return false;
    return true;
}

// The filter governing a window: its own, or that of the window it is a child of (not across popups).
std::pair<ImGuiID, FilterEntry*> FindFilter(ImGuiWindow* w) {
    const int frame = ImGui::GetFrameCount();
    for (; w; w = w->ParentWindow) {
        auto it = gFilters.find(w->ID);
        if (it != gFilters.end() && it->second.frame >= frame - 1) return {w->ID, it->second.words.empty() ? nullptr : &it->second};
        if (!(w->Flags & ImGuiWindowFlags_ChildWindow) || (w->Flags & ImGuiWindowFlags_Popup)) break;
    }
    return {0, nullptr};
}

bool FilterHook(ImGuiWindow* window, int kind, const char* label, const char* end) {
    const int frame = ImGui::GetFrameCount();
    if (gActiveFrame < frame - 1) return false;  // no filter typed anywhere
    auto [owner, f] = FindFilter(window);
    if (!f) return false;
    if (label && strstr(label, "##sxfilter")) return false;  // the filter box itself
    Section& sec = gSections[owner];
    if (sec.frame != frame) sec = {frame, false, 0};
    const int depth = window->DC.TreeDepth;
    if (kind == 1) {
        // Section: opened, not drawn; inside a matching section, nested sections keep its content.
        if (!(sec.match && depth > sec.depth)) {
            sec.match = MatchWords(f->words, label, end);
            sec.depth = depth;
        }
        return true;
    }
    if (kind == 3) {
        if (sec.match && depth <= sec.depth) sec.match = false;
        return false;
    }
    if (kind == 2) {
        if (sec.match) return false;
        if (label && MatchWords(f->words, label, end)) {
            sec.rowWindow = window;
            sec.rowY = window->DC.CursorPos.y;
            return false;
        }
        return true;
    }
    if (!label || label == end || label[0] == 0 || (label[0] == '#' && label[1] == '#')) return false;  // no visible label
    if (sec.match || (sec.rowWindow == window && sec.rowY == window->DC.CursorPos.y)) return false;
    return !MatchWords(f->words, label, end);
}

}  // namespace

void InstallFilter() { GSXFilterFn = &FilterHook; }

bool FilterBox(const char* hint, float width) {
    ImGuiWindow* w = ImGui::GetCurrentWindow();
    FilterEntry& e = gFilters[w->ID];
    const int frame = ImGui::GetFrameCount();
    e.frame = frame;
    ImGuiIO& io = ImGui::GetIO();
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && io.KeyCtrl && !io.KeyShift && !io.WantTextInput &&
        ImGui::IsKeyPressed(ImGuiKey_F, false))
        ImGui::SetKeyboardFocusHere();
    const float clearW = e.text.empty() ? 0.0f : ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.x;
    ImGui::SetNextItemWidth((width > 0 ? width : ImGui::GetContentRegionAvail().x) - clearW);
    if (ImGui::InputTextWithHint("##sxfilter", hint, &e.text)) e.words = Words(e.text);
    if (ImGui::IsItemActive() && ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        e.text.clear();
        e.words.clear();
    }
    if (!e.text.empty()) {
        ImGui::SameLine();
        if (ImGui::Button("x##sxfilterclear", ImVec2(ImGui::GetFrameHeight(), 0))) {
            e.text.clear();
            e.words.clear();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Effacer le filtre (Échap)");
    }
    if (!e.words.empty()) gActiveFrame = frame;
    return !e.words.empty();
}

bool ListFilter(std::string& text, const char* hint) {
    ImGuiIO& io = ImGui::GetIO();
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && io.KeyCtrl && !io.KeyShift && !io.WantTextInput &&
        ImGui::IsKeyPressed(ImGuiKey_F, false))
        ImGui::SetKeyboardFocusHere();
    const float clearW = text.empty() ? 0.0f : ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.x;
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - clearW);
    ImGui::InputTextWithHint("##sxfilterlist", hint, &text);
    if (ImGui::IsItemActive() && ImGui::IsKeyPressed(ImGuiKey_Escape, false)) text.clear();
    if (!text.empty()) {
        ImGui::SameLine();
        if (ImGui::Button("x##sxfilterlistclear", ImVec2(ImGui::GetFrameHeight(), 0))) text.clear();
    }
    return !Words(text).empty();
}

void MenuFilter() {
    ImGuiWindow* w = ImGui::GetCurrentWindow();
    if (ImGui::IsWindowAppearing()) {
        FilterEntry& e = gFilters[w->ID];
        e.text.clear();
        e.words.clear();
        if (!GlobalSearch::Get().Crawling()) ImGui::SetKeyboardFocusHere();
    }
    FilterBox("Filtrer le menu...", 240.0f * ImGui::GetFontSize() / 16.0f);
    ImGui::Separator();
}

bool FilterSkip(const char* label) {
    ImGuiContext& g = *GImGui;
    return FilterHook(g.CurrentWindow, 0, label, nullptr);
}

const std::string& WindowFilter() {
    static const std::string none;
    auto [owner, f] = FindFilter(ImGui::GetCurrentWindow());
    return f ? f->text : none;
}

bool FilterMatch(const std::string& filter, const std::string& text) {
    const auto w = Words(filter);
    return w.empty() || MatchWords(w, text.c_str(), text.c_str() + text.size());
}

void SetWindowFilter(const char* windowName, const std::string& text) {
    if (ImGuiWindow* w = ImGui::FindWindowByName(windowName)) {
        FilterEntry& e = gFilters[w->ID];
        e.text = text;
        e.words = Words(text);
    }
}

}  // namespace sx::ui
