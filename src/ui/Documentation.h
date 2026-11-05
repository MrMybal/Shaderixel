#pragma once
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <functional>
#include <imgui.h>

namespace sx::ui {

struct ManualBlock { std::string_view kind, text, target; };
struct ManualPage { std::string_view id, title, kind, search; std::span<const ManualBlock> blocks; };
struct ManualUiText { std::string_view key, value; };
struct ManualLanguage { std::string_view code, name; std::span<const ManualPage> pages; std::span<const ManualUiText> ui; };
struct ManualAsset { std::string_view path; int resource, width, height; };
struct ManualImage { ImTextureID texture = 0; int width = 0, height = 0; };
std::span<const ManualLanguage> ManualLanguages();
std::span<const ManualAsset> ManualAssets();
std::string_view ResolveManualLanguage(std::string_view code);
std::span<const ManualPage> ManualPages(std::string_view language = "en");
const char* ManualText(std::string_view language, std::string_view key);

int ManualPageIndex(std::string_view id, std::string_view language = "en");
// Ordinals follow the Tool enum; the generator verifies complete enum/toolbar coverage.
std::string_view ManualToolTopic(int tool);
std::string_view ManualPanelTopic(std::string_view window);
// Unknown or user-authored references open the relevant handbook chapter.
std::string ManualReferenceTopic(std::string_view kind, std::string_view id, std::string_view fallback);
// Every query word must occur; title matches rank ahead of body matches.
std::vector<int> ManualResults(std::string_view query, int category = 0, std::string_view language = "en");

struct DocumentationState {
    bool open = false, focus = false, resetScroll = false, focusSearch = false;
    int page = 0, category = 0;
    std::string query;
    std::string language; // empty follows the application; an unsupported code falls back to English
    std::string zoomAsset, zoomCaption;
    bool zoomOpen = false;
    float textScale = 1.f;
    int scrollToBlock = -1;
    std::string exampleRequest;
    std::vector<int> history{0};
    int historyPosition = 0;
    bool Navigate(std::string_view id);
    void Select(int index);
    void Back();
    void Forward();
};

// F1 works without a document and while a text field owns the keyboard.
void DocumentationShortcut(DocumentationState& state, std::string_view context = {});
bool DocumentationFocused();
// Returns true only when the user requests the separately shipped browser edition.
bool DocumentationWindow(DocumentationState& state, float dpi, bool browserAvailable,
    std::string_view interfaceLanguage = "en",
    const std::function<ManualImage(std::string_view)>& imageSource = {},
    const std::function<bool(std::string_view)>& exampleAvailable = {});

} // namespace sx::ui
