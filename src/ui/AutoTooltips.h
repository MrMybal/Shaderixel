#pragma once
// Automatic tooltips from the tooltips library (AutoTooltips.cpp).
#include <string>

struct ImGuiContext;

namespace sx::autotips {
// From the ImGui item hook: remembers the hovered item of the frame (its label and panel).
void OnItemInfo(ImGuiContext* ctx, unsigned id, const char* label, int flags);
// End of frame: the hovered item's description as a tooltip, after the delay, unless it shows its own.
void EndFrame();
// The description of a label (optionally specific to a panel), nullptr if none.
const std::string* Find(const std::string& panel, const std::string& label);
}  // namespace sx::autotips
