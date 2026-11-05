/*
MIT License

Copyright (c) 2026 CyDoc contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/
// CyDoc native route helper (MIT), pinned by cmake/dependencies.json.
#pragma once
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cydoc_native {
inline std::string encode(std::string_view value) {
    constexpr char hex[] = "0123456789ABCDEF";
    std::string result;
    for (unsigned char c : value) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') result += char(c);
        else { result += '%'; result += hex[c >> 4]; result += hex[c & 15]; }
    }
    return result;
}
// The host supplies the absolute file URI, using its platform path API.
inline std::string link(std::string base, std::string_view locale, std::string_view page,
                        std::string_view section = {},
                        const std::vector<std::pair<std::string, std::string>>& state = {}) {
    base.erase(base.find('#') == std::string::npos ? base.size() : base.find('#'));
    base += "#/" + encode(locale) + "/" + encode(page);
    if (!section.empty()) base += "/" + encode(section);
    bool first = true;
    for (const auto& [key, value] : state) { base += first ? '?' : '&'; first = false; base += encode(key) + "=" + encode(value); }
    return base;
}
}
