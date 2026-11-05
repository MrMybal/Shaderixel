#include "ui/Documentation.h"
#include "ui/ManualRoutes.h"
#include "ui/CyDocLink.h"
#include <imgui.h>
#include <windows.h>
#include <shlwapi.h>
#include <algorithm>
#include <cctype>
#include <vector>
namespace sx::ui {
std::span<const ManualPage> ManualPages(std::string_view) { return kManualPages; }
std::string_view ResolveManualLanguage(std::string_view code) {
    std::string base(code.substr(0,code.find_first_of("-_")));
    for (auto& c:base) c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return base=="fr" ? "fr" : "en";
}
const char* ManualText(std::string_view language, std::string_view key) {
    const std::span<const ManualUiText> texts=ResolveManualLanguage(language)=="fr" ?
        std::span<const ManualUiText>(kManualUi_fr) : std::span<const ManualUiText>(kManualUi_en);
    for (const auto& text:texts) if (text.key==key) return text.value.data();
    static thread_local std::string fallback; fallback=key; return fallback.c_str();
}
int ManualPageIndex(std::string_view id, std::string_view) {
    for (int i=0;i<static_cast<int>(std::size(kManualPages));++i)
        if (kManualPages[i].id==id || kManualPages[i].route==id) return i;
    return -1;
}
std::string_view ManualToolTopic(int tool) {
    return tool>=0 && tool<static_cast<int>(std::size(kManualToolTopics)) ? kManualToolTopics[tool] : "tools";
}
std::string ManualReferenceTopic(std::string_view kind, std::string_view id, std::string_view fallback) {
    const std::string topic=std::string(kind)+":"+std::string(id);
    if (!id.empty() && ManualPageIndex(topic)>=0) return topic;
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
std::span<const std::string_view> ManualPracticeIds() { return kManualPracticeIds; }
bool ManualPracticeAvailable(std::string_view id) {
    return std::find(std::begin(kManualPracticeIds),std::end(kManualPracticeIds),id)!=std::end(kManualPracticeIds);
}
std::string ManualUrl(const std::filesystem::path& index, std::string_view language,
    std::string_view topic, std::string_view section, std::string_view media) {
    const int page=ManualPageIndex(topic);
    if (page<0 || index.empty()) return {};
    const auto path=std::filesystem::absolute(index).lexically_normal().wstring();
    DWORD size=static_cast<DWORD>(path.size()*3+32);
    std::wstring wide(size,L'\0');
    if (FAILED(UrlCreateFromPathW(path.c_str(),wide.data(),&size,0))) return {};
    wide.resize(size);
    // UrlCreateFromPathW uses legacy single-byte escapes for some characters.
    // Decode that canonical URL, then ask Windows to escape it as UTF-8.
    std::wstring decoded(path.size()*3+32,L'\0');
    DWORD decodedSize=static_cast<DWORD>(decoded.size());
    if (FAILED(UrlUnescapeW(wide.data(),decoded.data(),&decodedSize,0))) return {};
    decoded.resize(decodedSize);
    std::wstring escaped(decoded.size()*12+32,L'\0');
    DWORD escapedSize=static_cast<DWORD>(escaped.size());
    if (FAILED(UrlEscapeW(decoded.c_str(),escaped.data(),&escapedSize,URL_ESCAPE_AS_UTF8|URL_ESCAPE_PERCENT))) return {};
    escaped.resize(escapedSize);
    const int bytes=WideCharToMultiByte(CP_UTF8,0,escaped.data(),static_cast<int>(escaped.size()),nullptr,0,nullptr,nullptr);
    if (bytes<=0) return {};
    std::string encoded(bytes,'\0');
    WideCharToMultiByte(CP_UTF8,0,escaped.data(),static_cast<int>(escaped.size()),encoded.data(),bytes,nullptr,nullptr);
    std::vector<std::pair<std::string,std::string>> state;
    if (!media.empty()) state.emplace_back("media",media);
    return cydoc_native::link(encoded,ResolveManualLanguage(language),kManualPages[page].route,section,state);
}
bool DocumentationState::Navigate(std::string_view id) {
    const auto split=id.find('#');
    const int index=ManualPageIndex(id.substr(0,split));
    if (index<0) return false;
    page=index;anchor=split==std::string_view::npos ? "" : std::string(id.substr(split+1));
    media.clear();open=focus=true;return true;
}
void DocumentationShortcut(DocumentationState& state, std::string_view context) {
    const auto& io=ImGui::GetIO();
    if (!io.KeyCtrl && !io.KeyAlt && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_F1,false)) {
        if (!context.empty()) state.Navigate(context);
        state.open=state.focus=true;
    }
}
} // namespace sx::ui
