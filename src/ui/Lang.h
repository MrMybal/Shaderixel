#pragma once
// Interface language. The interface is written in French in the code; another language is a dictionary
// (library/lang/<code>.json: {"French text": "translation"}) applied when the text is drawn and measured
// (hook in Dear ImGui, tools/patches/imgui_i18n.patch), so labels keep their IDs. Texts with numbers are
// matched as patterns ("{n} triangles"); multi-line texts line by line. English is the default.
#include <string>
#include <vector>

namespace sx {

struct Language {
    std::string code, name;  // "en", "English"
};

std::vector<Language> AvailableLanguages();  // French (built in) + every dictionary found
const std::string& CurrentLanguage();
bool SetLanguage(const std::string& code);    // loads the dictionary ("fr": none)
// Translation of a French text (itself when there is no entry), for texts built by the code.
std::string Tr(const std::string& french);
const char* TrC(const char* french);          // pointer valid until the language changes

}  // namespace sx
