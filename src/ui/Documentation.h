#pragma once
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
namespace sx::ui {
struct ManualPage { std::string_view id, route, title, kind; };
struct ManualUiText { std::string_view key, value; };
std::span<const ManualPage> ManualPages(std::string_view language = "en");
std::string_view ResolveManualLanguage(std::string_view code);
const char* ManualText(std::string_view language, std::string_view key);
int ManualPageIndex(std::string_view id, std::string_view language = "en");
std::string_view ManualToolTopic(int tool);
std::string_view ManualPanelTopic(std::string_view window);
std::string ManualReferenceTopic(std::string_view kind, std::string_view id, std::string_view fallback);
std::span<const std::string_view> ManualPracticeIds();
bool ManualPracticeAvailable(std::string_view id);
std::string ManualUrl(const std::filesystem::path& index, std::string_view language,
    std::string_view topic, std::string_view section = {}, std::string_view media = {});
struct DocumentationState {
    bool open = false, focus = false;
    int page = 0;
    std::string language;
    std::string anchor, media, exampleRequest, lastUrl;
    bool Navigate(std::string_view id);
};
void DocumentationShortcut(DocumentationState& state, std::string_view context = {});
} // namespace sx::ui
