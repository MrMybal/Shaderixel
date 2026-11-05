#include "ui/Dimensions.h"
#include "ui/CyUiState.h"
#include "ui/Theme.h"
#include <imgui_stdlib.h>
#include <charconv>
#include <cstdio>

namespace sx::ui {
std::optional<float> ParseDecimal(std::string text) {
    std::replace(text.begin(), text.end(), ',', '.');
    auto first = text.find_first_not_of(" \t");
    if (first == std::string::npos) return {};
    text = text.substr(first, text.find_last_not_of(" \t") - first + 1);
    if (text.starts_with('+')) text.erase(0, 1);
    float value = 0;
    auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || !std::isfinite(value)) return {};
    return value;
}
std::optional<float2> ResizeDimension(float2 size, float2 reference, bool pixels,
                                      int axis, float value, bool lockAspect) {
    if ((axis != 0 && axis != 1) || !std::isfinite(value) || (pixels && value < 0)) return {};
    float original = axis ? size.y : size.x, base = axis ? reference.y : reference.x;
    if (!pixels && (!std::isfinite(base) || base <= 0)) return {};
    float target = pixels ? std::copysign(value, original) : value * base / 100.f;
    float2 result = size;
    if (lockAspect && std::abs(original) > 1e-8f) result = size * (target / original);
    if (axis) result.y = target; else result.x = target;
    if (!std::isfinite(result.x) || !std::isfinite(result.y) || std::abs(result.x) > 1e8f || std::abs(result.y) > 1e8f) return {};
    return result;
}
struct DecimalState { std::string text; float before = 0; };
bool DecimalField(const char* label, float& value) {
    auto id = ImGui::GetID(label);
    auto& state = CyUiViewState<DecimalState>(id);
    const bool active = ImGui::GetActiveID() == id;
    if (!active) {
        char text[64]; auto formatted = std::to_chars(text, text + sizeof(text), value);
        state.text.assign(text, formatted.ptr); state.before = value;
    }
    auto filter = [](ImGuiInputTextCallbackData* data) {
        auto c = data->EventChar;
        if (c == ',') data->EventChar = '.';
        return (c >= '0' && c <= '9') || c == '.' || c == ',' || c == '+' || c == '-' || c == 'e' || c == 'E' || c == ' ' ? 0 : 1;
    };
    bool edited = ImGui::InputText(label, &state.text,
        ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_CallbackCharFilter, filter);
    if (active && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        bool changed = value != state.before; value = state.before; return changed;
    }
    if (!edited) return false;
    auto parsed = ParseDecimal(state.text);
    if (!parsed || *parsed == value) return false;
    value = *parsed; return true;
}
struct DimensionEdit { float2 before; bool changed = false; };
bool Dimensions(const char* id, float2& size, float2 reference, bool& pixels,
                bool& lockAspect, const char* percentHint,
                const std::function<void(float2)>& apply, const std::function<void(bool)>& trackUndo) {
    ImGui::PushID(id);
    int unit = pixels ? 1 : 0;
    if (ImGui::Combo("Unités", &unit, "Pourcentage (%)\0Pixels (px)\0")) pixels = unit == 1;
    if (!pixels && percentHint) HelpTooltip(percentHint);
    ImGui::Checkbox("Conserver les proportions", &lockAspect);
    bool changed = false;
    for (int axis = 0; axis < 2; ++axis) {
        float current = axis ? size.y : size.x, base = axis ? reference.y : reference.x;
        float shown = pixels ? std::abs(current) : base > 0 ? current / base * 100.f : 0.f;
        const char* label = pixels ? (axis ? "Hauteur (px)" : "Largeur (px)") : (axis ? "Hauteur (%)" : "Largeur (%)");
        std::string field = std::string(label) + (axis ? "###height" : "###width");
        auto& edit = CyUiViewState<DimensionEdit>(ImGui::GetID(field.c_str()));
        bool wasActive = ImGui::GetActiveID() == ImGui::GetID(field.c_str());
        float2 before = size;
        bool valueChanged = DecimalField(field.c_str(), shown);
        if (ImGui::IsItemActivated()) { edit.before = before; edit.changed = false; }
        if (wasActive && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            size = edit.before; edit.changed = false; changed = true; if (apply) apply(size);
        } else if (valueChanged) {
            auto next = ResizeDimension(lockAspect ? edit.before : size, reference, pixels, axis, shown, lockAspect);
            if (next && (next->x != size.x || next->y != size.y)) {
                size = *next; changed = true; edit.changed = true; if (apply) apply(size);
            }
        }
        if (trackUndo) trackUndo(edit.changed);
        HelpTooltip("Saisie décimale : point ou virgule. Les dimensions en pixels conservent les miroirs existants.");
    }
    ImGui::PopID();
    return changed;
}
}
