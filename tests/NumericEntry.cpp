// Native ImGui/CyUI interaction checks. No OS mouse or keyboard events.
#include "ui/BlendIf.h"
#include "ui/Theme.h"
#include <imgui_internal.h>
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

int RunNumericEntryTests() {
    auto* previous = ImGui::GetCurrentContext();
    auto* context = ImGui::CreateContext();
    ImGui::SetCurrentContext(context);
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = {1000, 800};
    io.ConfigInputTrickleEventQueue = false;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    int failures = 0;
    context->ErrorCallback = [](ImGuiContext*, void* data, const char* message) {
        ++*static_cast<int*>(data); std::fprintf(stderr, "FAIL numeric UI: %s\n", message);
    };
    context->ErrorCallbackUserData = &failures;
    sx::ui::ApplyTheme(1, "violet", true); sx::ui::LoadFonts(1);
    unsigned char* pixels; int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height); io.Fonts->SetTexID(1);
    auto check = [&](bool ok, const std::string& message) {
        if (!ok) { ++failures; std::fprintf(stderr, "FAIL numeric: %s\n", message.c_str()); }
    };
    ImRect rect;
    std::function<void()> draw;
    auto frame = [&](ImVec2 mouse = ImVec2(-100,-100), int down = -1, float dt = 1.f/60) {
        io.DeltaTime = dt; io.AddMousePosEvent(mouse.x, mouse.y);
        if (down >= 0) io.AddMouseButtonEvent(0, down != 0);
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({0,0}); ImGui::SetNextWindowSize({1000,800});
        ImGui::Begin("Numeric test", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
        ImGui::SetCursorScreenPos({20,20}); ImGui::SetNextItemWidth(320);
        draw(); rect = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        ImGui::End(); ImGui::Render();
    };
    auto key = [&](ImGuiKey k) { io.AddKeyEvent(k,true); frame(); io.AddKeyEvent(k,false); frame(); };
    auto replace = [&](const char* text) {
        io.AddKeyEvent(ImGuiMod_Ctrl,true); io.AddKeyEvent(ImGuiKey_A,true); frame();
        io.AddKeyEvent(ImGuiKey_A,false); io.AddKeyEvent(ImGuiMod_Ctrl,false); frame();
        io.AddInputCharactersUTF8(text); frame();
    };
    auto open = [&](bool ctrl = false, int component = -1, int components = 1, bool expected = true) {
        frame({-100,-100},0,.6f); // end the previous double-click sequence
        frame();
        ImVec2 p{100, rect.Min.y + ImGui::GetFrameHeight() * .5f};
        if (component >= 0) {
            float gap = ImGui::GetStyle().ItemInnerSpacing.x;
            float width = (320 - (components - 1) * gap) / components;
            p.x = 20 + component * (width + gap) + width * .5f;
        }
        frame(p);
        if (ctrl) io.AddKeyEvent(ImGuiMod_Ctrl,true);
        frame(p,1); frame(p,0);
        if (!ctrl) { frame(p,1); frame(p,0); }
        else { io.AddKeyEvent(ImGuiMod_Ctrl,false); frame(p); }
        check((context->TempInputId != 0 && ImGui::TempInputIsActive(context->TempInputId)) == expected,
              expected ? "entry gesture opens a text field" : "non-editable controls do not open text input");
    };

    for (int mode = 0; mode < 5; ++mode) {
        float value = 37.625f;
        int commits = 0;
        draw = [&] {
            if (mode == 0) ImGui::SliderFloat("Value", &value, 0, 100, "%.0f %%");
            if (mode == 1) ImGui::DragFloat("Value", &value, .1f, -100, 100, "%.0f px");
            if (mode == 2) ImGui::SliderFloat("Value", &value, .01f, 1000, "%.1f", ImGuiSliderFlags_Logarithmic);
            if (mode == 3) ImGui::VSliderFloat("Value", {200,120}, &value, 0, 100, "%.0f");
            if (mode == 4) ImGui::SliderFloat("Value", &value, 0, 100, "%.0f", ImGuiSliderFlags_AlwaysClamp);
            if (ImGui::IsItemDeactivatedAfterEdit()) ++commits;
        };
        open();
        auto* state = ImGui::GetInputTextState(context->TempInputId);
        check(state && std::string(state->TextA.Data).find("37.625") != std::string::npos, "entry retains decimals hidden by display units/rounding");
        key(ImGuiKey_Escape);
        check(value == 37.625f && commits == 0, "opening and cancelling entry does not round the value or add history");
        open(); replace("38,125"); key(ImGuiKey_Enter);
        check(value == 38.125f && commits == 1, "double-click accepts decimal comma, units and one commit, mode " + std::to_string(mode));
        open(true); replace("42.75"); key(ImGuiKey_Enter);
        check(value == 42.75f && commits == 2, "Ctrl+click remains available");
        open(); replace("77,375"); key(ImGuiKey_Escape);
        check(value == 42.75f && commits == 2, "Escape cancels a typed replacement without an undo step");
        open(); replace("1e999"); key(ImGuiKey_Enter);
        check(std::isfinite(value) && value == 42.75f, "overflow cannot enter a slider value");
        if (mode == 4) {
            open(); replace("999"); key(ImGuiKey_Enter);
            check(value == 100, "AlwaysClamp input respects upper bound");
            open(); replace("-999"); key(ImGuiKey_Enter);
            check(value == 0, "AlwaysClamp input respects lower bound");
        }
    }
    for (int kind = 0; kind < 2; ++kind) {
        int value = 7;
        draw = [&] { if (kind) ImGui::DragInt("Integer", &value, 1, 0, 100); else ImGui::SliderInt("Integer", &value, 0, 100); };
        open(); replace("43"); key(ImGuiKey_Enter);
        check(value == 43, "integer controls accept typed values");
    }
    double value = .1234567890123456, min = -100, max = 100;
    draw = [&] { ImGui::SliderScalar("Double", ImGuiDataType_Double, &value, &min, &max, "%.0f units"); };
    open(); key(ImGuiKey_Escape);
    check(value == .1234567890123456, "double entry does not round the original value");
    open(); replace("0,2345678901234567"); key(ImGuiKey_Enter);
    check(std::abs(value - .2345678901234567) < 1e-16, "double precision and decimal comma");
    ImS64 big = 100, low = 0, high = 10000000000000000LL;
    draw = [&] { ImGui::SliderScalar("64-bit", ImGuiDataType_S64, &big, &low, &high); };
    open(); replace("9007199254740991"); key(ImGuiKey_Enter);
    check(big == 9007199254740991LL, "64-bit integer entry avoids float precision loss");
    for (int n = 2; n <= 4; ++n) {
        float values[4] = {10,20,30,40};
        float lo = 0, hi = 100;
        draw = [&] { ImGui::SliderScalarN("Vector", ImGuiDataType_Float, values, n, &lo, &hi, "%.0f"); };
        open(false,1,n); replace("55,125"); key(ImGuiKey_Enter);
        check(values[0] == 10 && values[1] == 55.125f && values[2] == 30 && values[3] == 40,
              "vector slider types only the selected component");
    }
    float angle = 0;
    draw = [&] { ImGui::SliderAngle("Angle", &angle); };
    open(); replace("37,625"); key(ImGuiKey_Enter);
    check(std::abs(angle - 37.625f * IM_PI / 180.f) < 1e-6f, "angle entry converts degrees to radians once");

    float readOnly = 25;
    draw = [&] { ImGui::SliderFloat("No input", &readOnly, 0, 100, "%.2f", ImGuiSliderFlags_NoInput); };
    open(false,-1,1,false);
    check(readOnly == 25, "NoInput still permits relative dragging without a click jump");
    draw = [&] { ImGui::BeginDisabled(); ImGui::SliderFloat("Disabled", &readOnly, 0, 100); ImGui::EndDisabled(); };
    open(false,-1,1,false);
    check(readOnly == 25, "disabled slider does not change");
    float keyboardValue = 10;
    ImGuiID keyboardId = 0;
    draw = [&] { ImGui::SliderFloat("Keyboard", &keyboardValue, 0, 100); keyboardId = ImGui::GetItemID(); };
    frame();
    ImGui::ActivateItemByID(keyboardId); context->NavNextActivateFlags = ImGuiActivateFlags_PreferInput; frame(); frame();
    check(context->TempInputId == keyboardId, "keyboard activation opens manual input");
    replace("12,875"); key(ImGuiKey_Enter);
    check(keyboardValue == 12.875f, "keyboard entry preserves decimal precision");

    float tabValue = 5;
    ImGuiID tabId = 0; int tabCommits = 0;
    draw = [&] {
        ImGui::SliderFloat("Tab entry", &tabValue, 0, 100); tabId = ImGui::GetItemID();
        if (ImGui::IsItemDeactivatedAfterEdit()) ++tabCommits;
        ImGui::Button("Next control");
    };
    frame({-100,-100},0,.6f); frame({100,30});
    io.AddKeyEvent(ImGuiMod_Ctrl,true); frame({100,30},1); frame({100,30},0);
    io.AddKeyEvent(ImGuiMod_Ctrl,false); frame();
    replace("18,875"); key(ImGuiKey_Tab);
    check(tabValue == 18.875f && tabCommits == 1 && context->ActiveId != tabId,
          "Tab confirms the value once and leaves the text field");

    float hinted = 20; ImGuiID hintId = 0, expectedHintId = 0;
    draw = [&] { expectedHintId = ImGui::GetID("Hinted value"); ImGui::SliderFloat("Hinted value", &hinted, 0, 100); hintId = ImGui::GetItemID(); };
    frame({-100,-100},0,.6f);
    for (int i = 0; i < 12; ++i) frame({100,30},-1,.1f);
    check(hintId != 0 && expectedHintId == hintId && hinted == 20,
          "hover hint preserves item identity and does not change the value");

    // CyUI's value controls already own their entry popup: keep this path intact.
    for (int mode = 0; mode < 2; ++mode) {
        float value = 12.5f;
        draw = [&] { if (mode) cyui::dragValue("CyUI", &value, .1f, 0, 100, "%.1f", 320); else cyui::sliderBar("CyUI", &value, 0, 100, "%.1f", 320); };
        frame({-100,-100},0,.6f); frame();
        ImVec2 p{100,30}; frame(p); frame(p,1); frame(p,0); frame(p,1); frame(p,0); frame();
        replace("27,375"); key(ImGuiKey_Enter);
        check(value == 27.375f, "CyUI custom control keeps typed comma input");
    }

    sx::u8 ranges[4] = {0,0,255,255};
    std::vector<ImRect> fields;
    int commits = 0;
    draw = [&] {
        fields.clear();
        sx::ui::BlendIfSlider("blend", ranges, IM_COL32_WHITE, [&] {
            fields.emplace_back(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
            if (ImGui::IsItemDeactivatedAfterEdit()) ++commits;
        });
    };
    frame({-100,-100},0,.6f); frame();
    auto openRange = [&](int index) {
        frame({-100,-100},0,.6f); frame();
        auto p = fields[index + 1].GetCenter();
        frame(p); frame(p,1); frame(p,0); frame(p,1); frame(p,0);
        check(context->TempInputId != 0, "Blend If threshold opens direct entry");
    };
    openRange(0); replace("64"); key(ImGuiKey_Enter);
    check(ranges[0] == 64 && ranges[1] == 64 && ranges[2] == 255 && ranges[3] == 255, "typed dark threshold preserves ordered pair");
    openRange(3); replace("192"); key(ImGuiKey_Enter);
    check(ranges[2] == 192 && ranges[3] == 192, "typed light threshold preserves ordered pair");
    openRange(1); replace("96"); key(ImGuiKey_Enter);
    check(ranges[0] == 64 && ranges[1] == 96 && ranges[2] == 192 && ranges[3] == 192, "inner threshold can be split by typing");
    check(commits == 3, "Blend If commits once per typed field");
    ImGui::DestroyContext();
    ImGui::SetCurrentContext(previous);
    std::printf("Numeric entry integration: %d failures\n", failures);
    return failures;
}
