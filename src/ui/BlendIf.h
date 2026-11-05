#pragma once
#include "core/Common.h"
#include <imgui.h>
#include <functional>

namespace sx::ui {
// Gradient handles and four directly editable, ordered thresholds.
// track is called after each item so the host keeps one undo step per edit.
bool BlendIfSlider(const char* id, u8 values[4], ImU32 colour, const std::function<void()>& track = {});
}
