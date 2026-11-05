#include "ui/Documentation.h"
#include "ui/ManualData.h"
#include "ui/Theme.h"
#include <imgui_stdlib.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cctype>

// The handbook owns its locale, independently of the editor translation hook.
extern bool (*GSXTranslateFn)(const char**, const char**);

namespace sx::ui {
namespace {
std::string Lower(std::string_view value) {
    std::string out;
    const char* p = value.data();
    const char* end = p + value.size();
    while (p < end) {
        unsigned cp = 0;
        int count = ImTextCharFromUtf8(&cp, p, end);
        if (count <= 0) break;
        const char* begin = p; p += count;
        if (cp >= 0x300 && cp <= 0x36f) continue;
        if (cp == 0x153 || cp == 0x152) { out += "oe"; continue; }
        if (cp == 0xe6 || cp == 0xc6) { out += "ae"; continue; }
        const char* replacement = nullptr;
        if ((cp>=0xc0 && cp<=0xc5)||(cp>=0xe0 && cp<=0xe5)) replacement="a";
        else if (cp==0xc7 || cp==0xe7) replacement="c";
        else if ((cp>=0xc8 && cp<=0xcb)||(cp>=0xe8 && cp<=0xeb)) replacement="e";
        else if ((cp>=0xcc && cp<=0xcf)||(cp>=0xec && cp<=0xef)) replacement="i";
        else if (cp==0xd1||cp==0xf1) replacement="n";
        else if ((cp>=0xd2&&cp<=0xd6)||(cp>=0xf2&&cp<=0xf6)) replacement="o";
        else if ((cp>=0xd9&&cp<=0xdc)||(cp>=0xf9&&cp<=0xfc)) replacement="u";
        else if (cp==0xdd||cp==0xfd||cp==0xff) replacement="y";
        if (replacement) out += replacement;
        else if (cp < 128) out += static_cast<char>(std::tolower(static_cast<unsigned char>(cp)));
        else out.append(begin,p);
    }
    return out;
}
struct ManualTranslationScope {
    decltype(GSXTranslateFn) saved = GSXTranslateFn;
    ManualTranslationScope() { GSXTranslateFn = nullptr; }
    ~ManualTranslationScope() { GSXTranslateFn = saved; }
};
std::vector<std::string> Words(std::string_view text) {
    auto lower = Lower(text);
    std::vector<std::string> words;
    size_t begin = 0;
    while (begin < lower.size()) {
        begin = lower.find_first_not_of(" \t\r\n", begin);
        if (begin == std::string::npos) break;
        size_t end = lower.find_first_of(" \t\r\n", begin);
        words.push_back(lower.substr(begin, end - begin));
        if (end == std::string::npos) break;
        begin = end;
    }
    return words;
}
bool Matches(std::string_view text, const std::vector<std::string>& words) {
    return std::all_of(words.begin(), words.end(), [&](const std::string& w) { return text.find(w) != std::string_view::npos; });
}
} // namespace

std::span<const ManualLanguage> ManualLanguages() { return kManualLanguages; }
std::span<const ManualAsset> ManualAssets() { return kManualAssets; }

std::string_view ResolveManualLanguage(std::string_view code) {
    auto base = Lower(code);
    base.resize(base.find_first_of("-_") == std::string::npos ? base.size() : base.find_first_of("-_"));
    for (const auto& language : kManualLanguages) if (language.code == base) return language.code;
    return "en";
}
std::span<const ManualPage> ManualPages(std::string_view language) {
    const auto code = ResolveManualLanguage(language);
    for (const auto& entry : kManualLanguages) if (entry.code == code) return entry.pages;
    return kManualLanguages[0].pages;
}
const char* ManualText(std::string_view language, std::string_view key) {
    const auto code = ResolveManualLanguage(language);
    for (const auto& entry : kManualLanguages) if (entry.code == code)
        for (const auto& text : entry.ui) if (text.key == key) return text.value.data();
    static thread_local std::string fallback;
    fallback = key;
    return fallback.c_str();
}
int ManualPageIndex(std::string_view id, std::string_view language) {
    const auto pages = ManualPages(language);
    for (int i=0; i<static_cast<int>(pages.size()); ++i) if (pages[i].id==id) return i;
    return -1;
}
std::string_view ManualToolTopic(int tool) {
    return tool>=0 && tool<static_cast<int>(std::size(kManualToolTopics)) ? kManualToolTopics[tool] : "tools";
}
std::string ManualReferenceTopic(std::string_view kind, std::string_view id, std::string_view fallback) {
    const std::string topic=std::string(kind)+":"+std::string(id);
    if (!id.empty() && ManualPageIndex(topic)>=0) return topic;
    // A library's displayed name can differ from its source filename.
    if (kind=="function") {
        for (const auto& name:kManualFunctionNames) if (name.key==id) return std::string(name.value);
        for (const auto& page:ManualPages()) if (page.kind=="Functions" && page.title==id) return std::string(page.id);
    }
    return ManualPageIndex(fallback)>=0 ? std::string(fallback) : "getting_started";
}
std::string_view ManualPanelTopic(std::string_view window) {
    const auto suffix=window.find("###");
    for (const auto& entry:kManualPanelTopics)
        if (entry.key==window || (suffix!=std::string_view::npos && entry.key==window.substr(suffix))) return entry.value;
    return {};
}

std::vector<int> ManualResults(std::string_view query, int category, std::string_view language) {
    const auto pages=ManualPages(language);
    const auto words=Words(query);
    const std::string_view categories[]={"","Handbook","Effects","Functions","Tools","Commands","Brushes","Nodes"};
    category=std::clamp(category,0,7);
    std::vector<int> result;
    for (int i=0; i<static_cast<int>(pages.size()); ++i) {
        if (category && pages[i].kind!=categories[category]) continue;
        if (Matches(pages[i].search,words)) result.push_back(i);
    }
    if (!words.empty()) std::stable_partition(result.begin(),result.end(),
        [&](int i){return Matches(Lower(pages[i].title),words);});
    return result;
}

void DocumentationState::Select(int index) {
    if (index < 0 || index >= static_cast<int>(ManualPages().size())) return;
    open = focus = resetScroll = true;
    scrollToBlock = -1;
    if (page == index) return;
    page = index;
    history.resize(historyPosition + 1);
    history.push_back(index);
    historyPosition = static_cast<int>(history.size()) - 1;
}
bool DocumentationState::Navigate(std::string_view id) {
    int index = ManualPageIndex(id);
    if (index < 0) return false;
    Select(index);
    return true;
}
void DocumentationState::Back() {
    if (historyPosition > 0) { page = history[--historyPosition]; resetScroll = true; scrollToBlock=-1; }
}
void DocumentationState::Forward() {
    if (historyPosition + 1 < static_cast<int>(history.size())) { page = history[++historyPosition]; resetScroll = true; scrollToBlock=-1; }
}
void DocumentationShortcut(DocumentationState& state, std::string_view context) {
    const auto& io = ImGui::GetIO();
    if (!io.KeyCtrl && !io.KeyAlt && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_F1, false)) {
        if (!DocumentationFocused() && !context.empty()) state.Navigate(context);
        state.open = state.focus = true;
    }
}

bool DocumentationFocused() {
    auto* window = ImGui::FindWindowByName("###ShaderixelDocumentation");
    auto* focused = ImGui::GetCurrentContext()->NavWindow;
    for (auto* parent=focused;parent;parent=parent->ParentWindow)
        if (window && window->RootWindow==parent->RootWindow) return true;
    return false;
}

bool DocumentationWindow(DocumentationState& state, float dpi, bool browserAvailable,
    std::string_view interfaceLanguage, const std::function<ManualImage(std::string_view)>& imageSource,
    const std::function<bool(std::string_view)>& exampleAvailable) {
    if (!state.open) return false;
    ManualTranslationScope translation;
    const auto language=ResolveManualLanguage(state.language.empty()?interfaceLanguage:state.language);
    const auto pages=ManualPages(language);
    auto T=[&](const char* key){return ManualText(language,key);};
    auto label=[&](const char* key){return std::string(T(key))+"###"+key;};
    const auto* viewport=ImGui::GetMainViewport();
    ImGui::SetNextWindowSize(ImVec2(std::min(1200.f*dpi,viewport->WorkSize.x-32*dpi),
                                   std::min(880.f*dpi,viewport->WorkSize.y-32*dpi)),ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(viewport->GetWorkCenter(),ImGuiCond_Appearing,ImVec2(.5f,.5f));
    ImGui::SetNextWindowSizeConstraints(ImVec2(440*dpi,340*dpi),ImVec2(FLT_MAX,FLT_MAX));
    if (state.focus) {ImGui::SetNextWindowFocus();state.focus=false;}
    bool browse=false, requestZoom=false;
    const std::string title=std::string(T("Documentation"))+"###ShaderixelDocumentation";
    if (!ImGui::Begin(title.c_str(),&state.open,ImGuiWindowFlags_NoDocking|ImGuiWindowFlags_NoCollapse)) {
        ImGui::End(); return false;
    }
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
        auto& io=ImGui::GetIO();
        if (io.KeyCtrl&&ImGui::IsKeyPressed(ImGuiKey_F,false)) state.focusSearch=true;
        if (io.KeyAlt&&ImGui::IsKeyPressed(ImGuiKey_LeftArrow,false)) state.Back();
        if (io.KeyAlt&&ImGui::IsKeyPressed(ImGuiKey_RightArrow,false)) state.Forward();
    }
    bool first=true;
    auto button=[&](const char* key,bool disabled=false) {
        const auto text=label(key);
        const float width=ImGui::CalcTextSize(T(key)).x+ImGui::GetStyle().FramePadding.x*2;
        if (!first) {ImGui::SameLine();if (ImGui::GetContentRegionAvail().x<width) ImGui::NewLine();}
        first=false;
        ImGui::BeginDisabled(disabled); bool clicked=ImGui::Button(text.c_str()); ImGui::EndDisabled();
        return clicked;
    };
    if (button("Back",state.historyPosition==0)) state.Back();
    if (button("Forward",state.historyPosition+1>=static_cast<int>(state.history.size()))) state.Forward();
    if (button("Contents")) {state.query.clear();state.category=1;state.Navigate("getting_started");}
    if (button("Open in browser",!browserAvailable)) browse=true;
    if (!browserAvailable&&ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("%s",T("Browser edition is missing. The embedded manual remains available."));
    const char* languageName="English";
    for (const auto& l:ManualLanguages()) if (l.code==language) languageName=l.name.data();
    ImGui::SetNextItemWidth(std::min(320*dpi,ImGui::GetContentRegionAvail().x));
    const auto languagePreview=state.language.empty()?std::string(T("Automatic"))+" · "+languageName:std::string(languageName);
    if (ImGui::BeginCombo("##manual_language",languagePreview.c_str())) {
        if (ImGui::Selectable(T("Automatic"),state.language.empty())) {state.language.clear();state.resetScroll=true;state.zoomOpen=false;}
        for (const auto& entry:ManualLanguages())
            if (ImGui::Selectable(entry.name.data(),state.language==entry.code)) {state.language=entry.code;state.resetScroll=true;state.zoomOpen=false;}
        ImGui::EndCombo();
    }
    const auto requested=Lower(state.language.empty()?interfaceLanguage:std::string_view(state.language));
    const auto requestedBase=requested.substr(0,requested.find_first_of("-_"));
    if (language=="en"&&requestedBase!="en")
        ImGui::TextWrapped("%s",T("Fallback"));
    ImGui::SameLine();
    if (ImGui::GetContentRegionAvail().x<200*dpi) ImGui::NewLine();
    ImGui::SetNextItemWidth(std::min(240*dpi,ImGui::GetContentRegionAvail().x));
    const auto sizePreview=std::string(T("Text size"))+" · "+std::to_string(static_cast<int>(state.textScale*100+.5f))+"%";
    if (ImGui::BeginCombo("##manual_text_size",sizePreview.c_str())) {
        for (float scale:{.85f,1.f,1.15f,1.3f,1.5f}) {
            const std::string size=std::to_string(static_cast<int>(scale*100+.5f))+"%";
            if (ImGui::Selectable(size.c_str(),state.textScale==scale)) state.textScale=scale;
        }
        ImGui::EndCombo();
    }
    ImGui::Separator();
    const float sidebar=std::clamp(ImGui::GetContentRegionAvail().x*.29f,145*dpi,320*dpi);
    ImGui::BeginChild("##manual_navigation",ImVec2(sidebar,0),ImGuiChildFlags_Borders);
    if (state.focusSearch) {ImGui::SetKeyboardFocusHere();state.focusSearch=false;}
    ImGui::SetNextItemWidth(-FLT_MIN);
    bool chooseFirst=ImGui::InputTextWithHint("##manual_search",T("Search documentation (Ctrl+F)..."),&state.query,ImGuiInputTextFlags_EnterReturnsTrue);
    if (ImGui::IsItemActive()&&ImGui::IsKeyPressed(ImGuiKey_Escape,false)) state.query.clear();
    const char* categories[]={"All documentation","Handbook","Effects","Functions","Tools","Commands","Brushes","Nodes"};
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##manual_category",T(categories[state.category]))) {
        for (int i=0;i<8;++i) if (ImGui::Selectable(label(categories[i]).c_str(),state.category==i)) state.category=i;
        ImGui::EndCombo();
    }
    const auto results=ManualResults(state.query,state.category,language);
    ImGui::TextDisabled("%zu %s",results.size(),T("matching pages"));
    if (chooseFirst&&!results.empty()) state.Select(results.front());
    ImGui::Separator(); ImGui::BeginChild("##manual_results");
    if (results.empty()) ImGui::TextWrapped("%s",T("No results. Clear the search or choose another category."));
    ImGuiListClipper clipper; clipper.Begin(static_cast<int>(results.size()));
    while (clipper.Step()) for (int row=clipper.DisplayStart;row<clipper.DisplayEnd;++row) {
        const int index=results[row];const auto& page=pages[index];
        ImGui::PushID(index);
        if (ImGui::Selectable(page.title.data(),state.page==index)) state.Select(index);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s\n%s",page.title.data(),T(page.kind.data()));
        ImGui::PopID();
    }
    ImGui::EndChild();ImGui::EndChild();ImGui::SameLine();
    ImGui::BeginChild("##manual_article",ImVec2(0,0),ImGuiChildFlags_Borders);
    const auto& page=pages[state.page];
    if (state.resetScroll) {ImGui::SetScrollY(0);state.resetScroll=false;}
    const bool hasSections=std::any_of(page.blocks.begin(),page.blocks.end(),[](const auto& block){return block.kind=="heading";});
    if (hasSections) {
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##manual_sections",T("In this chapter"))) {
            for (int i=0;i<static_cast<int>(page.blocks.size());++i) if (page.blocks[i].kind=="heading") {
                ImGui::PushID(i);
                if (ImGui::Selectable(page.blocks[i].text.data())) state.scrollToBlock=i;
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
    }
    ImGui::PushFont(nullptr,ImGui::GetFontSize()*std::clamp(state.textScale,.85f,1.5f));
    ImGui::TextDisabled("%s | %s | %s",T(page.kind.data()),languageName,T("Offline"));
    ImGui::Spacing();ImGui::PushFont(nullptr,ImGui::GetFontSize()*1.35f);
    ImGui::TextWrapped("%s",page.title.data());ImGui::PopFont();ImGui::Separator();
    int blockIndex=0;
    for (const auto& block:page.blocks) {
        if (blockIndex==state.scrollToBlock) {
            ImGui::SetScrollFromPosY(ImGui::GetCursorPosY(),0.f);
            state.scrollToBlock=-1;
        }
        ImGui::PushID(blockIndex++);
        if (block.kind=="heading") {
            ImGui::Spacing();ImGui::Spacing();ImGui::PushFont(nullptr,ImGui::GetFontSize()*1.12f);
            ImGui::TextWrapped("%s",block.text.data());ImGui::PopFont();ImGui::Separator();
        } else if (block.kind=="link") {
            const auto pos=ImGui::GetCursorScreenPos();const float width=ImGui::GetContentRegionAvail().x;
            const float height=ImGui::CalcTextSize(block.text.data(),nullptr,false,width-12*dpi).y+8*dpi;
            if (ImGui::Selectable("##manual_link",false,0,ImVec2(0,height))) state.Navigate(block.target);
            const auto fg=ImGui::GetStyleColorVec4(ImGuiCol_Text),ac=ImGui::GetStyleColorVec4(ImGuiCol_SliderGrabActive);
            ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(),ImGui::GetFontSize(),ImVec2(pos.x+6*dpi,pos.y+4*dpi),
                ImGui::GetColorU32(ImVec4(fg.x*.65f+ac.x*.35f,fg.y*.65f+ac.y*.35f,fg.z*.65f+ac.z*.35f,fg.w)),
                block.text.data(),block.text.data()+block.text.size(),width-12*dpi);
        } else if (block.kind=="example") {
            const bool available=exampleAvailable && exampleAvailable(block.target);
            ImGui::BeginDisabled(!available);
            if (ImGui::Button(T("Open example"))) state.exampleRequest=block.target;
            ImGui::EndDisabled();
            if (!available && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("%s",T("Example files missing"));
            ImGui::TextWrapped("%s",block.text.data());
            ImGui::TextWrapped("%s",T("Open example hint"));
            ImGui::Spacing();
        } else if (block.kind=="image") {
            const auto picture=imageSource?imageSource(block.target):ManualImage{};
            if (picture.texture&&picture.width>0&&picture.height>0) {
                float width=std::min(ImGui::GetContentRegionAvail().x,static_cast<float>(picture.width));
                float height=width*picture.height/picture.width;
                if (height>380*dpi) {width*=380*dpi/height;height=380*dpi;}
                ImGui::Image(picture.texture,ImVec2(width,height));
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s",T("Click to enlarge"));
                if (ImGui::IsItemClicked()) {state.zoomAsset=block.target;state.zoomCaption=block.text;state.zoomOpen=true;requestZoom=true;}
            } else ImGui::TextDisabled("%s",T("Image unavailable"));
            ImGui::TextWrapped("%s",block.text.data());ImGui::Spacing();
        } else {
            if (block.kind=="bullet") {ImGui::Bullet();ImGui::SameLine();}
            ImGui::TextWrapped("%s",block.text.data());ImGui::Spacing();
        }
        ImGui::PopID();
    }
    ImGui::Spacing();ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Text,ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("%s",T("Reference describes shipped defaults. User presets may differ."));
    ImGui::PopStyleColor();ImGui::PopFont();ImGui::EndChild();
    const auto zoomTitle=std::string(T("Illustration"))+"###ManualImageZoom";
    if (requestZoom||(state.zoomOpen&&!state.zoomAsset.empty()&&!ImGui::IsPopupOpen(zoomTitle.c_str()))) ImGui::OpenPopup(zoomTitle.c_str());
    ImGui::SetNextWindowSize(ImVec2(std::min(1400*dpi,viewport->WorkSize.x*.94f),std::min(950*dpi,viewport->WorkSize.y*.92f)),ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal(zoomTitle.c_str(),&state.zoomOpen,ImGuiWindowFlags_NoSavedSettings)) {
        const auto picture=imageSource?imageSource(state.zoomAsset):ManualImage{};
        const auto avail=ImGui::GetContentRegionAvail();
        if (picture.texture&&picture.width>0&&picture.height>0) {
            const float scale=std::max(.01f,std::min(avail.x/picture.width,std::max(32.f,avail.y-100*dpi)/picture.height));
            ImGui::Image(picture.texture,ImVec2(picture.width*scale,picture.height*scale));
        }
        ImGui::TextWrapped("%s",state.zoomCaption.c_str());
        if (ImGui::Button(label("Close").c_str())||ImGui::IsKeyPressed(ImGuiKey_Escape)) {state.zoomOpen=false;ImGui::CloseCurrentPopup();}
        ImGui::EndPopup();
    }
    ImGui::End();
    return browse;
}
} // namespace sx::ui
