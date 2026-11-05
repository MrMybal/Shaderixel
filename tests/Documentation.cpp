#include "ui/Documentation.h"
#include "ui/Theme.h"
#include <imgui_internal.h>
#include <algorithm>
#include <cstdio>
extern bool (*GSXTranslateFn)(const char**, const char**);
namespace { int manualTranslationCalls = 0; }

int RunDocumentationTests() {
    using namespace sx::ui;
    int failures = 0;
    auto check = [&](bool ok, const char* what) {
        if (!ok) { std::fprintf(stderr, "FAIL documentation: %s\n", what); ++failures; }
    };
    check(ManualPageIndex("getting_started") == 0, "stable home page");
    check(ManualPageIndex("not-a-page") == -1, "unknown ID rejected");
    check(ManualReferenceTopic("effect","ui_uv_nine_slice","image_graph")=="effect:ui_uv_nine_slice", "effect selection resolves a shipped reference");
    check(ManualReferenceTopic("function","UI UV Nine Slice Panel","libraries")=="function:UI UV Nine Slice Panel", "function selection resolves a shipped reference");
    check(ManualReferenceTopic("function","Fondu A/B","libraries")=="function:Fondu AB", "display names differing from filenames resolve their reference");
    check(ManualReferenceTopic("effect","user_effect","authoring")=="authoring", "user-authored effect uses relevant chapter");
    check(ManualReferenceTopic("function","Renamed function","libraries")=="libraries", "unknown or renamed function has usable help");
    check(ManualReferenceTopic("effect","","unknown")=="getting_started", "invalid fallback cannot create a dead link");
    check(ManualLanguages().size()==2, "English and French registered");
    check(ResolveManualLanguage("fr-CA")=="fr" && ResolveManualLanguage("FR_fr")=="fr", "regional language variants");
    check(ResolveManualLanguage("de")=="en", "unsupported language falls back to English");
    check(ManualPages("fr").size()==ManualPages().size(), "identical catalog coverage");
    check(ManualPages("fr")[0].title=="Prise en main", "French handbook title");
    check(ManualAssets().size()==42, "all bilingual images have embedded resource identities");
    check(ManualToolTopic(0)=="tool:move" && ManualToolTopic(45)=="tool:color_replace","tool ordinals resolve first and last toolbar entries");
    check(ManualToolTopic(-1)=="tools" && ManualToolTopic(46)=="tools","invalid tool ordinals open the index");
    check(ManualReferenceTopic("command","export","actions")=="command:export","selected action step resolves its reference");
    check(ManualReferenceTopic("command","unknown","actions")=="actions","unknown action command falls back to its guide");
    check(ManualPanelTopic("Analyse d'image")=="photo","image analysis has contextual help");
    check(ManualPanelTopic("Mises à jour###Software updates")=="settings","translated dialog resolves stable ID");
    check(ManualPanelTopic("Photo RAW - Example###cameraraw")=="photo","document-specific title resolves stable help");
    check(ManualPanelTopic("##closeprompt")=="documents","close prompt resolves save guidance");
    check(ManualPanelTopic("Unknown").empty(),"unknown panel retains existing help behavior");
    int counts[7]{};
    for (const auto& page : ManualPages()) {
        if (page.kind == "Handbook") ++counts[0];
        if (page.kind == "Effects") ++counts[1];
        if (page.kind == "Functions") ++counts[2];
        if (page.kind == "Tools") ++counts[3];
        if (page.kind == "Commands") ++counts[4];
        if (page.kind == "Brushes") ++counts[5];
        if (page.kind == "Nodes") ++counts[6];
        check(!page.title.empty() && !page.blocks.empty(), "every page has content");
        for (const auto& block : page.blocks)
            if (block.kind == "link") check(ManualPageIndex(block.target) >= 0, "every related-page link resolves");
    }
    check(counts[0] ==44 && counts[1] >= 449 && counts[2] >= 162 && counts[3]==46 && counts[4]==20 && counts[5]==53 && counts[6]==168, "handbook and complete shipped catalogs");
    check(ManualResults("",7).size()==168,"node category includes all compiled registries");
    check(ManualResults("",6).size()==53,"brush category includes all shipped programmable tips");
    check(ManualReferenceTopic("brush","cy_texture","brushes_tablet")=="brush:cy_texture","brush help resolves stable identifier");
    check(!ManualResults("aquarelle",6,"fr").empty(),"French brush names are searchable");
    check(ManualResults(" \n\t ").size() == std::size(ManualPages()), "whitespace query includes all pages");
    check(ManualResults("not_a_real_parameter_993429").empty(), "no-match query");
    check(ManualResults("NINE SLICE") == ManualResults("nine slice"), "case-insensitive search");
    auto results = ManualResults("source margin", 2);
    check(!results.empty(), "parameter text is searchable");
    check(std::all_of(results.begin(), results.end(), [](int i) { return ManualPages()[i].kind == "Effects"; }), "category filter");
    results = ManualResults("UI UV Nine Slice Panel", 3);
    check(!results.empty() && ManualPages()[results[0]].title == "UI UV Nine Slice Panel", "title hits ranked first");
    check(!ManualResults("375.5", 1).empty(), "body-only handbook search");
    check(!ManualResults("ECRÊTAGE",1,"fr").empty(), "French search ignores accents and uppercase");
    check(ManualResults("écrêtage",1,"fr")==ManualResults("ecretage",1,"fr"), "French normalized query parity");
    check(ManualResults("",4).size()==46,"tool category contains every toolbar entry");
    check(ManualResults("",5).size()==20,"command category contains every registered action command");
    results=ManualResults("folder suffix",5);
    check(std::any_of(results.begin(),results.end(),[](int i){return ManualPages()[i].id=="command:export";}),"command parameter descriptions are searchable");
    results=ManualResults("éclaircir",4,"fr");
    check(std::any_of(results.begin(),results.end(),[](int i){return ManualPages("fr")[i].id=="tool:dodge";}),"French tool search matches its documented purpose");

    DocumentationState state;
    check(state.Navigate("dimensions") && state.open, "navigation opens documentation");
    check(state.Navigate("pbr"), "navigate forward");
    state.Back(); check(ManualPages()[state.page].id == "dimensions", "back history");
    state.Forward(); check(ManualPages()[state.page].id == "pbr", "forward history");
    state.Back(); state.Navigate("shortcuts");
    state.Forward(); check(ManualPages()[state.page].id == "shortcuts", "new navigation drops old forward branch");
    int old = state.page;
    check(!state.Navigate("invalid") && state.page == old, "unknown navigation leaves page intact");
    state.Select(-1); state.Select(1000000); check(state.page == old, "bad indices cannot corrupt state");
    while (state.historyPosition) state.Back();
    state.Back(); check(state.page == 0, "back stops at home");

    auto* ctx = ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr; io.DisplaySize = ImVec2(1280, 900); io.DeltaTime = 1.f/60;
    io.ConfigInputTrickleEventQueue = false;
    ctx->ErrorCallback = [](ImGuiContext*, void* user, const char* message) {
        ++*static_cast<int*>(user); std::fprintf(stderr, "FAIL documentation ImGui: %s\n", message);
    };
    ctx->ErrorCallbackUserData = &failures;
    ApplyTheme(1); LoadFonts(1);
    unsigned char* pixels; int w, h;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h); io.Fonts->SetTexID(1);
    state = {};
    bool browserRequested = false;
    std::string interfaceLanguage = "en";
    std::string contextTopic;
    std::vector<std::string> requestedImages;
    std::vector<std::string> requestedExamples;
    bool provideExamples=false;
    bool provideImage = false;
    bool inspectVisibleText = false;
    int manualHookCallsDuringWindow = 0;
    auto frame = [&](bool browser = false) {
        ImGui::NewFrame(); DocumentationShortcut(state,contextTopic);
        if (inspectVisibleText) ImGui::LogToBuffer();
        const int hookCallsBeforeWindow=manualTranslationCalls;
        browserRequested |= DocumentationWindow(state, 1, browser, interfaceLanguage,
            [&](std::string_view name){requestedImages.emplace_back(name);return provideImage?ManualImage{1,1200,640}:ManualImage{};},
            [&](std::string_view name){requestedExamples.emplace_back(name);return provideExamples;});
        manualHookCallsDuringWindow=manualTranslationCalls-hookCallsBeforeWindow;
        if (inspectVisibleText) {
            const std::string visible = ctx->LogBuffer.c_str();
            check(visible.find("matching pages") != std::string::npos, "visible status is logged");
            check(visible.find("###") == std::string::npos, "visible prose never displays stable widget ID suffixes");
            ImGui::LogFinish();
        }
        ImGui::Render();
    };
    frame(); check(!state.open, "manual stays closed until requested");
    io.AddKeyEvent(ImGuiKey_F1, true); frame();
    check(state.open, "F1 opens without an application document");
    io.AddKeyEvent(ImGuiKey_F1, false); frame();
    contextTopic="effect:ui_uv_nine_slice";
    ImGui::FocusWindow(nullptr);state.open=false;
    io.AddKeyEvent(ImGuiKey_F1,true);frame();
    check(ManualPages()[state.page].id==contextTopic, "contextual F1 opens selected effect reference");
    io.AddKeyEvent(ImGuiKey_F1,false);frame();
    contextTopic="layers";
    io.AddKeyEvent(ImGuiKey_F1,true);frame();
    check(ManualPages()[state.page].id=="effect:ui_uv_nine_slice", "F1 inside reader preserves article instead of editor context");
    io.AddKeyEvent(ImGuiKey_F1,false);frame();contextTopic.clear();
    provideImage=true;state.language="fr";
    state.zoomAsset="media/nine_slice_fr.png";state.zoomCaption="Schéma de découpe";state.zoomOpen=true;
    frame();frame();
    check(state.zoomOpen, "illustration enlargement renders in the reader");
    io.AddKeyEvent(ImGuiKey_Escape,true);frame();
    io.AddKeyEvent(ImGuiKey_Escape,false);frame();
    check(!state.zoomOpen, "Escape closes enlarged illustration");
    provideImage=false;state.language.clear();
    state.Navigate("ui_textures"); state.language.clear(); interfaceLanguage="fr";
    requestedImages.clear(); frame();
    check(std::find(requestedImages.begin(),requestedImages.end(),"media/nine_slice_fr.png")!=requestedImages.end(), "automatic French selects French illustration");
    const int subject=state.page;
    state.language="en"; requestedImages.clear(); frame();
    check(state.page==subject, "changing language retains subject and history");
    check(std::find(requestedImages.begin(),requestedImages.end(),"media/nine_slice_en.png")!=requestedImages.end(), "explicit language overrides interface language");
    auto savedHook=GSXTranslateFn;
    GSXTranslateFn=[](const char**,const char**){++manualTranslationCalls;return false;};
    state.language="fr"; interfaceLanguage="en";
    manualTranslationCalls=0; frame();
    check(manualHookCallsDuringWindow==0, "manual locale cannot be rewritten by editor translation hook");
    check(GSXTranslateFn!=nullptr, "editor hook restored after reader");
    GSXTranslateFn=savedHook;
    state.language.clear();interfaceLanguage="de";requestedImages.clear();frame();
    check(std::find(requestedImages.begin(),requestedImages.end(),"media/nine_slice_en.png")!=requestedImages.end(), "unavailable interface locale uses English assets");
    interfaceLanguage="en";state.Navigate("getting_started");frame();
    check(DocumentationFocused(), "manual owns keyboard focus");
    inspectVisibleText = true; frame(); inspectVisibleText = false;
    state.Navigate("effect:ui_uv_nine_slice");state.textScale=1.5f;frame();
    for (int i=0;i<static_cast<int>(ManualPages()[state.page].blocks.size());++i)
        if (ManualPages()[state.page].blocks[i].kind=="heading") state.scrollToBlock=i;
    check(state.scrollToBlock>0,"long reference exposes chapter sections");
    frame();frame();
    bool articleScrolled=false;
    for (auto* window:ctx->Windows) if (std::string_view(window->Name).find("##manual_article")!=std::string_view::npos) articleScrolled=window->Scroll.y>0;
    check(articleScrolled && state.scrollToBlock==-1,"section jump scrolls the enlarged article to its heading");
    state.textScale=1.f;state.Navigate("getting_started");frame();
    state.Navigate("tutorials");requestedExamples.clear();frame();
    check(requestedExamples.size()==6 && state.exampleRequest.empty(),"practice links render disabled when example files are unavailable");
    provideExamples=true;frame();
    // Activate through ImGui navigation, without any operating-system input.
    ImGuiWindow* articleWindow=nullptr;
    for (auto* window:ctx->Windows) if (std::string_view(window->Name).find("##manual_article")!=std::string_view::npos) articleWindow=window;
    int exampleBlock=0;
    for (const auto& block:ManualPages()[state.page].blocks) {if (block.kind=="example") break;++exampleBlock;}
    if (articleWindow) {
        const ImGuiID scope=ImHashData(&exampleBlock,sizeof(exampleBlock),articleWindow->ID);
        ImGui::ActivateItemByID(ImHashStr("Open example",0,scope));frame();
    }
    check(state.exampleRequest=="ui_nine_slice","activating an example queues its registered ID without opening files in the reader");
    state.exampleRequest.clear();provideExamples=false;state.Navigate("getting_started");frame();
    state.category=4;state.language="fr";
    for (int i:ManualResults("",4,"fr")) {state.Select(i);frame();}
    check(state.open && ManualPages()[state.page].kind=="Tools","all French tool references render with the tool-only navigation");
    state.language.clear();state.category=0;state.Navigate("getting_started");frame();
    state.category=5;state.language="fr";
    for (int i:ManualResults("",5,"fr")) {state.Select(i);frame();}
    check(ManualPages()[state.page].kind=="Commands","all French command references render with JSON defaults");
    state.language.clear();state.category=0;state.Navigate("getting_started");frame();
    state.category=6;state.language="fr";
    for (int i:ManualResults("",6,"fr")) {state.Select(i);frame();}
    check(ManualPages()[state.page].kind=="Brushes","all French brush references render");
    state.language.clear();state.category=0;state.Navigate("getting_started");frame();
    for (const char* language:{"en","fr"}) {
        state.category=7;state.language=language;
        for (int i:ManualResults("",7,language)) {state.Select(i);frame();}
        check(ManualPages()[state.page].kind=="Nodes","all built-in node references render in both languages");
    }
    state.language.clear();state.category=0;state.Navigate("getting_started");frame();
    state.focusSearch = true; frame(); frame();
    io.AddInputCharactersUTF8("nine slice"); frame();
    check(state.query == "nine slice", "search field accepts internal queued input");
    io.AddKeyEvent(ImGuiKey_Enter, true); frame();
    check(state.page==ManualResults("nine slice").front(), "Enter opens first matching page");
    io.AddKeyEvent(ImGuiKey_Enter, false); frame();
    io.AddKeyEvent(ImGuiMod_Alt, true); io.AddKeyEvent(ImGuiKey_LeftArrow, true); frame();
    check(state.page == 0, "Alt+Left navigates history");
    io.AddKeyEvent(ImGuiKey_LeftArrow, false); io.AddKeyEvent(ImGuiMod_Alt, false); frame();
    state.query = "no_match_92133"; frame();
    check(state.open, "no results leaves current article readable");
    state.query.clear(); state.category = 3;
    state.Navigate("function:UI UV Nine Slice Panel"); frame(true);
    check(ManualPages()[state.page].kind == "Functions", "function reference renders with cross links");
    state.Navigate("effect:ui_uv_nine_slice"); frame();
    check(!browserRequested, "missing browser edition never launches anything");
    state.open = false; frame();
    io.WantTextInput = true;
    io.AddKeyEvent(ImGuiKey_F1, true); frame();
    check(state.open, "F1 remains available while typing");
    io.AddKeyEvent(ImGuiKey_F1, false); frame();
    ImGui::DestroyContext(ctx);
    std::printf("Documentation integration: %d failures (%d handbook, %d effects, %d functions, %d tools, %d commands, %d brushes, %d nodes)\n", failures, counts[0], counts[1], counts[2],counts[3],counts[4],counts[5],counts[6]);
    return failures;
}
