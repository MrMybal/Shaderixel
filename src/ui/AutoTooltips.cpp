// Automatic tooltips: the description of the hovered item (menu item, button, tab, property, check box...) read from
// the tooltips library (library/lang/tooltips_<language>.json), shown after the tooltip delay when the item shows no
// tooltip of its own. Keys are the item's label as written in the code (the French source text, before "##"), or
// "Panel|Label" when a label means different things in different panels (the panel's name first).
#include "ui/AutoTooltips.h"
#include "ui/Lang.h"
#include "core/Common.h"
#include "core/Json.h"

#include <imgui_internal.h>
#include <cyui/Core.h>

#include <string>
#include <unordered_map>

namespace sx {
std::string ExeDir();  // App.cpp
}

namespace sx::autotips {

namespace {
struct State {
    std::unordered_map<std::string, std::string> text;  // key -> description (current language)
    std::string language;                                // the language `text` was read for
    // The hovered item of this frame (the last one reported: the innermost).
    ImGuiID hovered = 0;
    std::string label, panel;
    // The item hovered since `since` (seconds); its tooltip waits for the delay.
    ImGuiID shownFor = 0;
    double since = 0;
};
State& S() {
    static State s;
    return s;
}

// The visible part of an ImGui label: before "##", trimmed; "" for icon-only or empty labels.
std::string Visible(const char* label) {
    std::string s = label ? label : "";
    if (size_t p = s.find("##"); p != std::string::npos) s.resize(p);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.pop_back();
    size_t b = 0;
    while (b < s.size() && (s[b] == ' ' || s[b] == '\t')) ++b;
    s.erase(0, b);
    return s.size() > 120 ? std::string() : s;
}

// The panel an item belongs to: its root window's name (docked panels are named after themselves).
std::string PanelOf(ImGuiWindow* w) {
    while (w && (w->Flags & ImGuiWindowFlags_ChildWindow) && !w->DockIsActive && w->ParentWindow) w = w->ParentWindow;
    std::string name = w ? Visible(w->Name) : std::string();
    if (size_t p = name.find(" - "); p != std::string::npos) name.resize(p);  // ("Photo RAW - <document>")
    return name;
}

void Load(const std::string& language) {
    State& s = S();
    s.language = language;
    s.text.clear();
    Json j;
    std::string src;
    for (const std::string& dir : {ExeDir() + "/lang", ExeDir() + "/../../library/lang"})
        if (ReadTextFile(dir + "/tooltips_" + language + ".json", src) && Json::Parse(src, j) && j.IsObject()) break;
    if (!j.IsObject()) return;
    for (auto& [k, v] : j.Items())
        if (!v.S("").empty()) s.text[k] = v.S("");
}
}  // namespace

void OnItemInfo(ImGuiContext* ctx, unsigned id, const char* label, int flags) {
    ImGuiWindow* w = ctx ? ctx->CurrentWindow : nullptr;
    if (!w || id == w->ID) return;  // (a window begins)
    if (!(flags & ImGuiItemStatusFlags_HoveredRect) || ctx->HoveredWindow != w) return;
    std::string lab = Visible(label);
    if (lab.empty()) return;
    State& s = S();
    s.hovered = id;
    s.label = std::move(lab);
    s.panel = PanelOf(w);
}

const std::string* Find(const std::string& panel, const std::string& label) {
    State& s = S();
    if (s.language != CurrentLanguage()) Load(CurrentLanguage());
    if (!panel.empty())
        if (auto it = s.text.find(panel + "|" + label); it != s.text.end()) return &it->second;
    if (auto it = s.text.find(label); it != s.text.end()) return &it->second;
    return nullptr;
}

void EndFrame() {
    ImGuiContext& g = *ImGui::GetCurrentContext();
    State& s = S();
    const ImGuiID id = s.hovered;
    const std::string label = std::move(s.label), panel = std::move(s.panel);
    s.hovered = 0;
    if (!id || g.ActiveId || ImGui::IsMouseDown(ImGuiMouseButton_Left)) { s.shownFor = 0; return; }
    if (s.shownFor != id) {
        s.shownFor = id;
        s.since = ImGui::GetTime();
        return;
    }
    if (ImGui::GetTime() - s.since < g.Style.HoverDelayShort) return;
    // The item's own tooltip wins (any tooltip window begun this frame).
    for (ImGuiWindow* w : g.Windows)
        if ((w->Flags & ImGuiWindowFlags_Tooltip) && w->LastFrameActive == g.FrameCount) return;
    const std::string* text = Find(panel, label);
    if (!text) return;
    if (ImGui::BeginTooltip()) {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 32.0f);
        ImGui::TextUnformatted(text->c_str());
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

}  // namespace sx::autotips
