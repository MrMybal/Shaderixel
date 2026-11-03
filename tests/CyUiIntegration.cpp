#include "ui/Theme.h"
#include "ui/CodeEdit.h"
#include "ui/ToneCurve.h"
#include <cyui/modules/Images.h>
#include <cyui/modules/Nodes.h>
#include <cyui/modules/Plots.h>
#include <imgui_internal.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>

// Shaderixel's production hooks feed its search index; this isolated UI consumer
// deliberately has no index, GPU, platform input or native application window.
void ImGuiTestEngineHook_ItemAdd(ImGuiContext*, ImGuiID, const ImRect&, const ImGuiLastItemData*) {}
void ImGuiTestEngineHook_ItemInfo(ImGuiContext*, ImGuiID, const char*, ImGuiItemStatusFlags) {}
void ImGuiTestEngineHook_Log(ImGuiContext*, const char*, ...) {}
const char* ImGuiTestEngine_FindItemDebugLabel(ImGuiContext*, ImGuiID) { return nullptr; }
namespace {
int failures=0;
void check(bool condition,const char* message) {
    if (!condition) { std::fprintf(stderr,"FAIL: %s\n",message); ++failures; }
}
void context(const char* theme="violet", bool comfortable=true, float dpi=1.f) {
    auto* created=ImGui::CreateContext();
    ImGui::SetCurrentContext(created);
    created->ErrorCallback=[](ImGuiContext*,void*,const char* message) {
        std::fprintf(stderr,"FAIL: ImGui stack/state error: %s\n",message); ++failures;
    };
    auto& io=ImGui::GetIO();
    io.IniFilename=nullptr; io.DisplaySize={1000,800}; io.DeltaTime=1.f/60;
    io.ConfigInputTrickleEventQueue=false;
    sx::ui::ApplyTheme(dpi,theme,comfortable); sx::ui::LoadFonts(dpi);
    unsigned char* pixels; int width,height;
    io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height); io.Fonts->SetTexID(1);
}
template<class F> void frame(F draw, ImVec2 mouse={-100,-100},int down=-1) {
    auto& io=ImGui::GetIO(); io.AddMousePosEvent(mouse.x,mouse.y);
    if(down>=0) io.AddMouseButtonEvent(0,down!=0);
    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0,0}); ImGui::SetNextWindowSize({1000,800});
    ImGui::Begin("Test",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoMove);
    ImGui::SetCursorScreenPos({20,20}); draw(); ImGui::End();
    cyui::renderNotifications(); ImGui::Render();
}
template<class F> void chord(F draw,ImGuiKey key) {
    auto& io=ImGui::GetIO(); io.AddKeyEvent(ImGuiMod_Ctrl,true); io.AddKeyEvent(key,true);frame(draw);
    io.AddKeyEvent(key,false);io.AddKeyEvent(ImGuiMod_Ctrl,false);frame(draw);
}
float evaluate(const sx::CurvePoints& points,float x) {
    for(size_t i=1;i<points.size();++i) if(x<=points[i].x) {
        float k=(x-points[i-1].x)/std::max(.0001f,points[i].x-points[i-1].x);
        return points[i-1].y+(points[i].y-points[i-1].y)*k;
    }
    return points.back().y;
}
}
int main() {
    for(const auto* theme:{"mid-grey","light-grey","amber-night","violet","navy"})
        for(bool comfortable:{false,true}) for(float dpi:{1.f,1.5f}) {
            context(theme,comfortable,dpi);
            const auto expected=cyui::palette().bg0;
            cyui::GradientState gradient;
            std::vector<cyui::GradientStop> stops={{11,0,{0,0,0,1}},{22,1,{1,1,1,1}}};
            std::string source="// shader\r\nfloat4 main() { return 1; }";
            frame([&]{
                check(cyui::palette().bg0==expected,"palette follows active context");
                check(std::abs(cyui::dp(10)-10*dpi)<.001f,"DPI-scaled geometry");
                cyui::gradientEditor("gradient",gradient,stops,{300,60},true,1);
                sx::ui::CodeEditor("code",&source,{600,200},true);
                cyui::TextFilter filter;filter.query="eclat";
                check(filter.matches("Éclat"),"accent-insensitive common filtering");
            });
            check(source=="// shader\r\nfloat4 main() { return 1; }","read-only editor preserves CRLF and absent final newline");
            ImGui::DestroyContext();
        }
    context();
    std::string source="float value = 1;";
    bool changed=false, committedCode=false;
    int codeCommits=0;
    auto code=[&]{
        changed|=sx::ui::CodeEditor("code",&source,{600,220});
        committedCode=sx::ui::CodeEditorCommitted();
        if(committedCode) ++codeCommits;
        ImGui::Button("Outside",{120,30});
        check(!sx::ui::CodeEditorCommitted(),"code commit does not leak to the next control");
    };
    frame(code);frame(code,{160,40});frame(code,{160,40},1);frame(code,{160,40},0);
    const auto original=source;
    ImGui::GetIO().AddInputCharactersUTF8("Z");frame(code);
    check(changed && source!=original,"native editor accepts synthetic text input");
    chord(code,ImGuiKey_Z);check(source==original,"native Undo restores exact original shader bytes");
    chord(code,ImGuiKey_Y);check(source!=original,"native Redo restores the edit");
    frame(code,{60,260});frame(code,{60,260},1);frame(code,{60,260},0);frame(code);
    check(codeCommits==1,"one host code transaction when the editor loses focus");
    source="external replacement\r\nwithout final newline";frame(code);
    check(source=="external replacement\r\nwithout final newline","host document replacement is authoritative");
    auto readOnly=[&]{sx::ui::CodeEditor("code",&source,{600,220},true);};
    const auto readonly=source;
    ImGui::GetIO().AddInputCharactersUTF8("X");frame(readOnly);
    check(source==readonly,"read-only shader rejects input");
    sx::CurvePoints points={{0,0},{1,1}};
    bool begin=false,commit=false,cancel=false;
    auto curve=[&]{sx::ui::CurveEditor("tone",points,cyui::palette().accent,220,begin,commit,cancel,nullptr,evaluate);};
    frame(curve);frame(curve,{130,130});frame(curve,{130,130},1);
    check(begin && points.size()==3,"tone curve keeps single-click insertion");
    frame(curve,{170,90},1);frame(curve,{170,90},0);
    check(commit,"one tone-curve commit at gesture release");
    const auto committed=points;
    frame(curve,{170,90});frame(curve,{170,90},1);frame(curve,{190,70},1);
    ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape,true);frame(curve,{190,70},1);
    check(cancel && points==committed,"Escape cancels the tone preview without committing");
    ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape,false);frame(curve,{190,70},0);
    frame(curve,{170,90});frame(curve,{170,90},1);frame(curve,{170,-60},1);frame(curve,{170,-60},0);
    check(points.size()==2 && commit,"drag-out deletes an interior key and commits once");
    const auto formatted=sx::ui::FormatHlsl("float4 f(){float x=0;float y=1;float z=2;float q=3;return float4(x,y,z,q);}");
    check(formatted.find("return")!=std::string::npos,"host HLSL formatter still works");
    auto* first=ImGui::GetCurrentContext();context("light-grey",false,1.5f);
    frame([&]{sx::ui::CodeEditor("code",&source,{300,100},true);});
    ImGui::DestroyContext();ImGui::SetCurrentContext(first);
    check(cyui::currentThemeKey()=="violet","destroying another context preserves this context's theme and editor state");
    ImGui::DestroyContext();
    std::printf("CyUI Shaderixel integration: %d failures\n",failures);
    return failures?EXIT_FAILURE:EXIT_SUCCESS;
}
