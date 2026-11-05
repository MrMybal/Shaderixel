#include "ui/Documentation.h"
#include "ui/Theme.h"
#include <imgui_internal.h>
#include <cstdio>
int RunDocumentationTests() {
    using namespace sx::ui;
    int failures=0;
    auto check=[&](bool ok,const char* label){if(!ok){++failures;std::fprintf(stderr,"FAIL CyDoc: %s\n",label);}};
    check(ManualPages().size()==942,"all original routes retained");
    check(ManualPageIndex("getting_started")==0,"legacy home");
    check(ManualPageIndex("getting-started")==0,"canonical home");
    check(ManualPageIndex("unknown")==-1,"unknown rejected");
    check(ResolveManualLanguage("fr-CA")=="fr","regional French");
    check(ResolveManualLanguage("FR_fr")=="fr","normalized French");
    check(ResolveManualLanguage("de")=="en","English fallback");
    check(ManualReferenceTopic("effect","ui_uv_nine_slice","image_graph")=="effect:ui_uv_nine_slice","effect route");
    check(ManualReferenceTopic("function","UI UV Nine Slice Panel","libraries")=="function:UI UV Nine Slice Panel","function route");
    check(ManualReferenceTopic("effect","custom","authoring")=="authoring","custom effect fallback");
    check(ManualReferenceTopic("brush","cy_texture","brushes_tablet")=="brush:cy_texture","brush route");
    check(ManualReferenceTopic("command","export","actions")=="command:export","recorded command");
    check(ManualPracticeAvailable("ui_nine_slice") && !ManualPracticeAvailable("../unknown"),"practice allowlist");
    for(int i=0;i<46;++i) check(ManualPageIndex(ManualToolTopic(i))>=0,"all toolbar routes");
    for(const auto& page:ManualPages()) {
        check(ManualPageIndex(page.route)>=0,"every canonical ID resolves");
        check(!page.route.empty() && page.route.find(':')==std::string_view::npos,"CyDoc stable ID syntax");
    }
    auto url=ManualUrl(L"C:\\Shaderixel Help\\été #1\\index.html","fr-CA","effect:ui_uv_nine_slice","parameters","nine-slice");
    check(url.starts_with("file:///C:/"),"Windows file URI");
    check(url.find("%20")!=std::string::npos && url.find("%23")!=std::string::npos,"spaces and file hash escaped");
    check(url.find("%C3%A9")!=std::string::npos,"UTF-8 path escaped");
    check(url.ends_with("#/fr/effect-ui-uv-nine-slice/parameters?media=nine-slice"),"section and media deep link");
    check(url.find('\0')==std::string::npos,"no embedded terminator");
    check(ManualUrl(L"C:\\Help\\index.html","en","unknown").empty(),"unknown URI rejected");
    DocumentationState state;
    check(state.Navigate("ui_textures#nine-slice"),"section navigation");
    check(state.anchor=="nine-slice" && state.focus,"section request");
    check(!state.Navigate("unknown") && state.anchor=="nine-slice","invalid request preserves current route");
    auto* context=ImGui::CreateContext();auto& io=ImGui::GetIO();
    io.IniFilename=nullptr;io.DisplaySize=ImVec2(1280,900);io.DeltaTime=1.f/60;io.ConfigInputTrickleEventQueue=false;
    ApplyTheme(1);LoadFonts(1);unsigned char* pixels;int w,h;
    io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);io.Fonts->SetTexID(1);
    auto frame=[&]{ImGui::NewFrame();DocumentationShortcut(state,"tool:shape");ImGui::EndFrame();};
    frame();state.focus=false;io.AddKeyEvent(ImGuiKey_F1,true);frame();
    check(state.focus && ManualPages()[state.page].id=="tool:shape","F1 requests selected tool in CyDoc");
    io.AddKeyEvent(ImGuiKey_F1,false);frame();ImGui::DestroyContext(context);
    std::printf("CyDoc native routing: %d failures\n",failures);return failures;
}
