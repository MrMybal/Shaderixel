#pragma once
// Text filter of a window (Unreal-like "Search details"): with a filter box drawn in a window, the labeled
// widgets of that window (buttons, checkboxes, sliders, inputs, combos, colors, selectables, menu items) whose
// label does not contain the words typed are hidden; sections (tree nodes, collapsing headers, separator texts,
// sub-menus) open without being drawn, and a section whose title matches keeps all its content. Implemented
// with a hook in the labeled widgets of Dear ImGui (tools/patches/imgui_filter.patch).
#include <string>

namespace sx::ui {

void InstallFilter();  // once, after the ImGui context is created

// Filter box of the current window (Ctrl+F focuses it, Escape clears it). True while the filter is active.
bool FilterBox(const char* hint = "Filtrer (Ctrl+F)...", float width = -1.0f);
// Filter box of a list drawn by hand (layers, history...): only the rows are filtered, by the caller with
// FilterMatch; the other widgets of the window stay.
bool ListFilter(std::string& text, const char* hint = "Filtrer (Ctrl+F)...");
// Same, for the top of a menu: cleared and focused each time the menu opens.
void MenuFilter();
// A custom widget (drawn by hand) described by label is hidden by the filter of the current window.
bool FilterSkip(const char* label);
// Filter text of the current window (empty if none), for lists drawn by hand (layers, history...).
const std::string& WindowFilter();
// Every word of the filter is in the text (case and accents ignored).
bool FilterMatch(const std::string& filter, const std::string& text);
// Filter of a window by name (demos, search results).
void SetWindowFilter(const char* windowName, const std::string& text);

}  // namespace sx::ui
