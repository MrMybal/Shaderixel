#include "ui/CodeEdit.h"
#include "ui/Theme.h"

#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <algorithm>
#include <cctype>
#include <cstring>

namespace sx::ui {
namespace {

enum Tok { kText, kKeyword, kType, kFunc, kNumber, kComment, kString, kPreproc, kName, kAttr };

ImU32 TokColor(Tok t) {
    switch (t) {
        case kKeyword: return IM_COL32(197, 134, 192, 255);
        case kType: return IM_COL32(86, 156, 214, 255);
        case kFunc: return IM_COL32(220, 220, 170, 255);
        case kNumber: return IM_COL32(181, 206, 168, 255);
        case kComment: return IM_COL32(106, 153, 85, 255);
        case kString: return IM_COL32(206, 145, 120, 255);
        case kPreproc: return IM_COL32(155, 155, 210, 255);
        case kName: return IM_COL32(78, 201, 176, 255);
        case kAttr: return IM_COL32(140, 140, 150, 255);
        default: return IM_COL32(212, 212, 212, 255);
    }
}

bool IsIdStart(char c) { return std::isalpha((unsigned char)c) || c == '_' || (unsigned char)c >= 0x80; }
bool IsId(char c) { return std::isalnum((unsigned char)c) || c == '_' || (unsigned char)c >= 0x80; }

bool IsKeyword(std::string_view w) {
    static const char* k[] = {"if", "else", "for", "while", "do", "return", "break", "continue", "switch", "case", "default", "discard",
                              "true", "false", "in", "out", "inout", "uniform", "static", "const", "struct", "cbuffer", "typedef",
                              "groupshared", "precise", "nointerpolation", "linear", "register", "packoffset"};
    for (const char* s : k) if (w == s) return true;
    return false;
}

bool IsType(std::string_view w) {
    static const char* base[] = {"float", "half", "int", "uint", "bool", "double", "min16float", "min16int", "dword"};
    for (const char* b : base) {
        size_t n = std::strlen(b);
        if (w.compare(0, n, b) != 0) continue;
        std::string_view rest = w.substr(n);
        if (rest.empty()) return true;
        // float2, float4x4
        if (rest.size() == 1 && rest[0] >= '1' && rest[0] <= '4') return true;
        if (rest.size() == 3 && rest[0] >= '1' && rest[0] <= '4' && rest[1] == 'x' && rest[2] >= '1' && rest[2] <= '4') return true;
    }
    static const char* k[] = {"void", "Texture2D", "Texture3D", "TextureCube", "Texture2DArray", "RWTexture2D", "SamplerState", "Buffer",
                              "StructuredBuffer", "RWStructuredBuffer", "matrix", "vector", "EffectContext", "auto"};
    for (const char* s : k) if (w == s) return true;
    return false;
}

struct Span {
    size_t a, b;
    Tok t;
};

// Tokens of one line. `inComment`: inside a /* */ comment at the start of the line (updated).
void TokenizeLine(std::string_view s, bool& inComment, const std::vector<std::string>* names, std::vector<Span>& out) {
    out.clear();
    size_t i = 0;
    if (inComment) {
        size_t e = s.find("*/");
        size_t end = e == std::string_view::npos ? s.size() : e + 2;
        out.push_back({0, end, kComment});
        if (e == std::string_view::npos) return;
        inComment = false;
        i = end;
    }
    size_t first = s.find_first_not_of(" \t");
    if (first != std::string_view::npos && s[first] == '#' && i == 0) {
        out.push_back({0, s.size(), kPreproc});
        return;
    }
    while (i < s.size()) {
        char c = s[i];
        if (c == '/' && i + 1 < s.size() && s[i + 1] == '/') {
            out.push_back({i, s.size(), kComment});
            return;
        }
        if (c == '/' && i + 1 < s.size() && s[i + 1] == '*') {
            size_t e = s.find("*/", i + 2);
            if (e == std::string_view::npos) {
                out.push_back({i, s.size(), kComment});
                inComment = true;
                return;
            }
            out.push_back({i, e + 2, kComment});
            i = e + 2;
            continue;
        }
        if (c == '"' || c == '\'') {
            size_t j = i + 1;
            while (j < s.size() && s[j] != c) j += s[j] == '\\' ? 2 : 1;
            j = std::min(s.size(), j + 1);
            out.push_back({i, j, kString});
            i = j;
            continue;
        }
        if (std::isdigit((unsigned char)c) || (c == '.' && i + 1 < s.size() && std::isdigit((unsigned char)s[i + 1]))) {
            size_t j = i + 1;
            while (j < s.size() && (std::isalnum((unsigned char)s[j]) || s[j] == '.' ||
                                    ((s[j] == '+' || s[j] == '-') && (s[j - 1] == 'e' || s[j - 1] == 'E'))))
                ++j;
            out.push_back({i, j, kNumber});
            i = j;
            continue;
        }
        if (IsIdStart(c)) {
            size_t j = i + 1;
            while (j < s.size() && IsId(s[j])) ++j;
            std::string_view w = s.substr(i, j - i);
            Tok t = kText;
            if (IsKeyword(w)) t = kKeyword;
            else if (IsType(w)) t = kType;
            else if (names && std::find(names->begin(), names->end(), w) != names->end()) t = kName;
            else {
                size_t k = j;
                while (k < s.size() && s[k] == ' ') ++k;
                if (k < s.size() && s[k] == '(') t = kFunc;
            }
            if (i > 0 && s[i - 1] == '[') {
                size_t k = j;
                if (k < s.size() && s[k] == ']') t = kAttr;  // [loop], [unroll], [branch]
            }
            out.push_back({i, j, t});
            i = j;
            continue;
        }
        ++i;  // punctuation, spaces: default color (drawn in runs below)
    }
}

}  // namespace

bool LooksMinified(const std::string& code) {
    size_t start = 0;
    while (start < code.size()) {
        size_t e = code.find('\n', start);
        if (e == std::string::npos) e = code.size();
        std::string_view line(code.data() + start, e - start);
        if (line.size() > 120 && std::count(line.begin(), line.end(), ';') >= 4) return true;
        start = e + 1;
    }
    return false;
}

std::string FormatHlsl(const std::string& code, int baseIndent) {
    if (!LooksMinified(code)) return code;
    std::string out;
    out.reserve(code.size() * 5 / 4);
    int indent = baseIndent, paren = 0, templ = 0;
    std::vector<bool> ctrl;  // open parentheses: after if / for / while / switch
    bool lineStart = true;
    // What came last: binary operators follow a value (name, number, closing bracket), unary ones do not.
    enum Last { kNone, kIdent, kKeyword, kNumber, kClose, kOp } last = kNone;
    std::string lastWord;
    auto newline = [&] {
        while (!out.empty() && out.back() == ' ') out.pop_back();
        if (!out.empty() && out.back() != '\n') out += '\n';
        lineStart = true;
    };
    auto emit = [&](std::string_view s) {
        if (lineStart) {
            out.append((size_t)std::max(0, indent) * 4, ' ');
            lineStart = false;
        }
        out.append(s);
    };
    auto space = [&] {
        if (!lineStart && !out.empty() && out.back() != ' ' && out.back() != '\n') out += ' ';
    };
    auto nextNonSpace = [&](size_t i) {
        while (i < code.size() && (code[i] == ' ' || code[i] == '\t')) ++i;
        return i;
    };
    auto lineBegins = [&](const char* word) {
        size_t ls = out.rfind('\n');
        ls = ls == std::string::npos ? 0 : ls + 1;
        while (ls < out.size() && out[ls] == ' ') ++ls;
        return out.compare(ls, std::strlen(word), word) == 0;
    };
    static const char* kTemplates[] = {"Texture1D", "Texture2D", "Texture3D", "TextureCube", "Texture2DArray", "RWTexture2D", "RWTexture3D",
                                       "Buffer", "RWBuffer", "StructuredBuffer", "RWStructuredBuffer", "vector", "matrix"};
    size_t i = 0;
    while (i < code.size()) {
        char c = code[i];
        if (c == '\r') { ++i; continue; }
        if (c == '\n') {
            if (paren == 0) newline();
            else space();
            ++i;
            continue;
        }
        if (c == ' ' || c == '\t') {
            space();
            ++i;
            continue;
        }
        if (c == '#' && lineStart) {
            size_t e = code.find('\n', i);
            if (e == std::string::npos) e = code.size();
            emit(std::string_view(code).substr(i, e - i));
            newline();
            i = e;
            last = kNone;
            continue;
        }
        if (c == '/' && i + 1 < code.size() && code[i + 1] == '/') {
            size_t e = code.find('\n', i);
            if (e == std::string::npos) e = code.size();
            emit(std::string_view(code).substr(i, e - i));
            newline();
            i = e;
            continue;
        }
        if (c == '/' && i + 1 < code.size() && code[i + 1] == '*') {
            size_t e = code.find("*/", i + 2);
            e = e == std::string::npos ? code.size() : e + 2;
            emit(std::string_view(code).substr(i, e - i));
            i = e;
            continue;
        }
        if (c == '"' || c == '\'') {
            size_t j = i + 1;
            while (j < code.size() && code[j] != c) j += code[j] == '\\' ? 2 : 1;
            j = std::min(code.size(), j + 1);
            emit(std::string_view(code).substr(i, j - i));
            i = j;
            last = kNumber;
            continue;
        }
        // Numbers in one piece: 1.5e-3, .5f, 0x1F.
        if (std::isdigit((unsigned char)c) || (c == '.' && i + 1 < code.size() && std::isdigit((unsigned char)code[i + 1]))) {
            size_t j = i + 1;
            while (j < code.size() && (std::isalnum((unsigned char)code[j]) || code[j] == '.' ||
                                       ((code[j] == '+' || code[j] == '-') && (code[j - 1] == 'e' || code[j - 1] == 'E') &&
                                        !(code[i] == '0' && i + 1 < code.size() && (code[i + 1] == 'x' || code[i + 1] == 'X')))))
                ++j;
            emit(std::string_view(code).substr(i, j - i));
            i = j;
            last = kNumber;
            continue;
        }
        if (IsIdStart(c)) {
            size_t j = i + 1;
            while (j < code.size() && IsId(code[j])) ++j;
            std::string_view w = std::string_view(code).substr(i, j - i);
            emit(w);
            i = j;
            lastWord = w;
            last = IsKeyword(w) ? kKeyword : kIdent;
            // "if(" -> "if ("
            if ((w == "if" || w == "for" || w == "while" || w == "switch") && i < code.size() && code[i] == '(') {
                out += ' ';
                ctrl.push_back(true);
                ++paren;
                emit("(");
                ++i;
                last = kNone;
            }
            continue;
        }
        switch (c) {
            case ';':
                emit(";");
                ++i;
                last = kNone;
                if (paren == 0) newline();
                else if (i < code.size() && code[i] != ' ' && code[i] != ')') out += ' ';
                continue;
            case ',':
                emit(",");
                ++i;
                last = kNone;
                if (i < code.size() && code[i] != ' ' && code[i] != '\n') out += ' ';
                continue;
            case '(':
            case '[':
                if (c == '(') {
                    ++paren;
                    ctrl.push_back(false);
                }
                emit(c == '(' ? "(" : "[");
                ++i;
                last = kNone;
                continue;
            case ')': {
                paren = std::max(0, paren - 1);
                const bool wasCtrl = !ctrl.empty() && ctrl.back();
                if (!ctrl.empty()) ctrl.pop_back();
                emit(")");
                ++i;
                last = kClose;
                if (wasCtrl && i < code.size() && IsIdStart(code[i])) out += ' ';  // if (x) break;
                continue;
            }
            case ']':
                emit("]");
                ++i;
                last = kClose;
                if (i < code.size() && IsIdStart(code[i])) out += ' ';  // [loop] for
                continue;
            case '{':
                space();
                emit("{");
                ++indent;
                ++i;
                last = kNone;
                newline();
                continue;
            case '}': {
                newline();
                --indent;
                emit("}");
                ++i;
                last = kClose;
                size_t k = nextNonSpace(i);
                if (k < code.size() && (code[k] == ';' || code[k] == ',' || code[k] == ')')) continue;
                if (code.compare(k, 4, "else") == 0 && (k + 4 >= code.size() || !IsId(code[k + 4]))) {
                    out += ' ';
                    i = k;
                    continue;
                }
                newline();
                continue;
            }
            case '.':
                emit(".");
                ++i;
                last = kNone;
                continue;
            default: break;
        }
        // Operators: the longest one, then spaces around the binary ones.
        static const char* kOps[] = {"<<=", ">>=", "==", "!=", "<=", ">=", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "&&", "||",
                                     "++", "--", "->", "::", "<<", ">>"};
        std::string op(1, c);
        for (const char* o : kOps)
            if (code.compare(i, std::strlen(o), o) == 0) { op = o; break; }
        i += op.size();
        const bool afterValue = last == kIdent || last == kNumber || last == kClose;
        bool binary = true;
        if (op == "++" || op == "--" || op == "->" || op == "::" || op == "!" || op == "~") binary = false;
        else if ((op == "-" || op == "+" || op == "*" || op == "&") && !afterValue) binary = false;
        else if (op == "<" && last == kIdent && std::find_if(std::begin(kTemplates), std::end(kTemplates), [&](const char* t) { return lastWord == t; }) != std::end(kTemplates)) {
            binary = false;
            ++templ;
        } else if (op == ">" && templ > 0) {
            binary = false;
            --templ;
        } else if (op == ":" && (lineBegins("case") || lineBegins("default"))) binary = false;
        if (!binary) {
            emit(op);
            last = (op == "++" || op == "--") && afterValue ? kClose : (op == ">" ? kIdent : kOp);
            continue;
        }
        space();
        emit(op);
        out += ' ';
        i = nextNonSpace(i);
        last = kOp;
    }
    while (!out.empty() && (out.back() == ' ' || out.back() == '\n')) out.pop_back();
    return out;
}

bool CodeEditor(const char* label, std::string* text, ImVec2 size, bool readOnly, const std::vector<std::string>* names) {
    ImGuiContext& g = *GImGui;
    ImGui::PushFont(GetFonts().mono, 0.0f);
    ImFont* font = ImGui::GetFont();
    const float fs = ImGui::GetFontSize();
    // Line numbers on the left.
    int lines = 1 + (int)std::count(text->begin(), text->end(), '\n');
    char numBuf[16];
    snprintf(numBuf, sizeof(numBuf), "%d", std::max(99, lines));
    const float gutter = font->CalcTextSizeA(fs, FLT_MAX, 0, numBuf).x + fs * 0.9f;
    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (size.x <= 0) size.x = std::max(50.0f, avail.x + size.x);
    if (size.y <= 0) size.y = std::max(fs * 4, avail.y + size.y);
    ImGui::BeginGroup();
    const ImVec2 gutterPos = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(gutter, size.y));
    ImGui::SameLine(0, 0);
    ImGuiWindow* parent = ImGui::GetCurrentWindow();
    const ImGuiID id = ImGui::GetID(label);
    // The field draws the selection and the caret; the text itself is drawn over it in colors.
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 0, 0, 0));
    ImGuiInputTextFlags flags = ImGuiInputTextFlags_AllowTabInput | (readOnly ? ImGuiInputTextFlags_ReadOnly : 0);
    bool changed = ImGui::InputTextMultiline(label, text, ImVec2(size.x - gutter, size.y), flags);
    ImGui::PopStyleColor();
    ImGuiWindow* child = nullptr;
    for (ImGuiWindow* w : g.Windows)
        if (w->ParentWindow == parent && w->ChildId == id) { child = w; break; }
    ImGui::EndGroup();
    if (child) {
        float scrollX = 0;
        if (ImGuiInputTextState* st = ImGui::GetInputTextState(id)) scrollX = st->Scroll.x;
        const ImVec2 origin(child->Pos.x + g.Style.FramePadding.x - scrollX, child->Pos.y + g.Style.FramePadding.y - child->Scroll.y);
        const ImRect clip(child->Pos, ImVec2(child->Pos.x + child->Size.x - child->ScrollbarSizes.x, child->Pos.y + child->Size.y - child->ScrollbarSizes.y));
        ImDrawList* dl = child->DrawList;
        dl->PushClipRect(clip.Min, clip.Max, true);
        ImDrawList* pdl = parent->DrawList;
        pdl->PushClipRect(ImVec2(gutterPos.x, clip.Min.y), ImVec2(gutterPos.x + gutter, clip.Max.y), true);
        std::vector<Span> spans;
        bool inComment = false;
        size_t start = 0;
        int n = 0;
        while (start <= text->size()) {
            size_t e = text->find('\n', start);
            if (e == std::string::npos) e = text->size();
            const float y = origin.y + n * fs;
            std::string_view line(text->data() + start, e - start);
            // Comments spanning lines need the lines above even when they are not visible.
            if (y + fs >= clip.Min.y && y <= clip.Max.y) {
                TokenizeLine(line, inComment, names, spans);
                float x = origin.x;
                size_t at = 0;
                auto draw = [&](size_t a, size_t b, ImU32 col) {
                    if (b <= a) return;
                    const char* p = line.data() + a;
                    dl->AddText(font, fs, ImVec2(x, y), col, p, p + (b - a));
                    x += font->CalcTextSizeA(fs, FLT_MAX, 0, p, p + (b - a)).x;
                };
                for (const Span& s : spans) {
                    draw(at, s.a, TokColor(kText));
                    draw(s.a, s.b, TokColor(s.t));
                    at = s.b;
                }
                draw(at, line.size(), TokColor(kText));
                char nb[16];
                snprintf(nb, sizeof(nb), "%d", n + 1);
                const float w = font->CalcTextSizeA(fs, FLT_MAX, 0, nb).x;
                pdl->AddText(font, fs, ImVec2(gutterPos.x + gutter - w - fs * 0.45f, y), IM_COL32(110, 112, 125, 255), nb);
            } else {
                TokenizeLine(line, inComment, names, spans);
                if (y > clip.Max.y) break;
            }
            start = e + 1;
            ++n;
        }
        pdl->PopClipRect();
        dl->PopClipRect();
    }
    ImGui::PopFont();
    return changed;
}

}  // namespace sx::ui
