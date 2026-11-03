#pragma once
#include <imgui_internal.h>
#include <memory>
#include <unordered_map>

namespace sx::ui {
template<class T> T& CyUiViewState(ImGuiID id) {
    using Views = std::unordered_map<ImGuiID, std::unique_ptr<T>>;
    static std::unordered_map<ImGuiContext*, Views> contexts;
    auto* ctx = ImGui::GetCurrentContext();
    if (!contexts.count(ctx)) {
        contexts.emplace(ctx, Views{});
        ImGuiContextHook hook;
        hook.Type = ImGuiContextHookType_Shutdown;
        hook.Callback = [](ImGuiContext* context, ImGuiContextHook*) { contexts.erase(context); };
        ImGui::AddContextHook(ctx, &hook);
    }
    auto& state = contexts.at(ctx)[id];
    if (!state) state = std::make_unique<T>();
    return *state;
}
} // namespace sx::ui
