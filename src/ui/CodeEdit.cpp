#include "ui/CodeEdit.h"
#include "ui/Theme.h"

#include <imgui_internal.h>
#include <cyui/modules/CodeEditor.h>
#include <TextEditor.h>
#include <unordered_map>
#include <memory>
#include <algorithm>
#include <cctype>
#include <cstring>

namespace sx::ui {
namespace {
// Identifier boundaries are also used by Shaderixel's source formatter.
bool IsIdStart(char c) { return std::isalpha((unsigned char)c) || c=='_' || (unsigned char)c>=0x80; }
bool IsId(char c) { return std::isalnum((unsigned char)c) || c=='_' || (unsigned char)c>=0x80; }
bool IsKeyword(std::string_view word) {
    const auto tokens = cyui::highlightCode(word, cyui::CodeLanguage::hlsl);
    return !tokens.empty() && tokens.front().role == cyui::TokenRole::keyword;
}
struct EditorView {
    cyui::CodeEditor editor;
    std::string published;
    const std::string* owner = nullptr;
    std::vector<std::string> semanticNames;
    size_t semanticRevision = ~size_t(0);
    bool focused = false, dirty = false;
};
struct EditorViews {
    std::unordered_map<ImGuiID, std::unique_ptr<EditorView>> views;
    ImGuiID item = 0;
    int frame = -1;
    bool committed = false;
};
std::unordered_map<ImGuiContext*, EditorViews> editors;
EditorView& View(const char* label) {
    auto* context = ImGui::GetCurrentContext();
    if (!editors.count(context)) {
        editors.emplace(context, EditorViews{});
        ImGuiContextHook hook;
        hook.Type = ImGuiContextHookType_Shutdown;
        hook.Callback = [](ImGuiContext* ctx, ImGuiContextHook*) { editors.erase(ctx); };
        ImGui::AddContextHook(context, &hook);
    }
    auto& view = editors.at(context).views[ImGui::GetID(label)];
    if (!view) {
        view = std::make_unique<EditorView>();
        view->editor.setLanguage(cyui::CodeLanguage::hlsl);
    }
    return *view;
}
// Shared syntax supplies lexical rules; Shaderixel adds only semantic pin/parameter names.
void SemanticNames(cyui::CodeEditor& editor, const std::vector<std::string>& names) {
    const auto text = editor.native().GetText();
    const auto tokens = cyui::highlightCode(text, cyui::CodeLanguage::hlsl);
    std::vector<TextEditor::SharedSyntaxToken> spans;
    using C = TextEditor::Color;
    auto role = [](cyui::TokenRole r) {
        switch (r) {
            case cyui::TokenRole::keyword: return C::keyword;
            case cyui::TokenRole::type: return C::declaration;
            case cyui::TokenRole::function: return C::sharedFunction;
            case cyui::TokenRole::number: return C::number;
            case cyui::TokenRole::string: return C::string;
            case cyui::TokenRole::comment: return C::comment;
            case cyui::TokenRole::preprocessor: return C::preprocessor;
            default: return C::text;
        }
    };
    size_t byte=0, line=0, glyph=0;
    auto advance = [&](size_t end) {
        while (byte < end) {
            unsigned char c = (unsigned char)text[byte++];
            if (c == '\n') { ++line; glyph=0; }
            else if (c != '\r' && (c & 0xc0) != 0x80) ++glyph;
        }
    };
    for (const auto& token : tokens) {
        auto color = role(token.role);
        if (token.role == cyui::TokenRole::text && std::find(names.begin(), names.end(), text.substr(token.start, token.length)) != names.end()) color = C::knownIdentifier;
        advance(token.start);
        const auto end = token.start + token.length;
        while (byte < end) {
            const auto row=line, start=glyph;
            while (byte < end && text[byte] != '\n') advance(byte+1);
            if (glyph > start) spans.push_back({row, start, glyph, color});
            if (byte < end) advance(byte+1);
        }
    }
    editor.native().SetSharedSyntaxTokens(spans);
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

std::vector<std::pair<int, std::string>> ParseShaderErrors(const std::string& log) {
    // Two formats: "name.slang(12): error 30019: message", and
    // "error[E30015]: message" followed by "  --> name.slang:46:30" and the line with "^^^^ detail".
    std::vector<std::string> lines;
    for (size_t start = 0; start < log.size();) {
        size_t e = log.find('\n', start);
        if (e == std::string::npos) e = log.size();
        lines.push_back(log.substr(start, e - start));
        start = e + 1;
    }
    auto isNumber = [](const std::string& s) { return !s.empty() && std::all_of(s.begin(), s.end(), [](char c) { return std::isdigit((unsigned char)c); }); };
    std::vector<std::pair<int, std::string>> out;
    std::string pending;
    for (size_t i = 0; i < lines.size(); ++i) {
        const std::string& line = lines[i];
        size_t a = line.find_first_not_of(" \t");
        std::string t = a == std::string::npos ? std::string() : line.substr(a);
        // Old format.
        size_t p = line.find("): ");
        size_t o = p == std::string::npos ? std::string::npos : line.rfind('(', p);
        if (o != std::string::npos) {
            std::string num = line.substr(o + 1, p - o - 1);
            if (size_t comma = num.find(','); comma != std::string::npos) num = num.substr(0, comma);
            std::string msg = line.substr(p + 3);
            if (isNumber(num) && (msg.rfind("error", 0) == 0 || msg.rfind("warning", 0) == 0)) {
                out.push_back({std::atoi(num.c_str()), msg});
                continue;
            }
        }
        // New format: the message, then its position.
        if ((t.rfind("error", 0) == 0 || t.rfind("warning", 0) == 0) && t.find(": ") != std::string::npos) {
            pending = t.substr(t.find(": ") + 2);
            continue;
        }
        if (t.rfind("-->", 0) == 0 && !pending.empty()) {
            size_t c2 = t.rfind(':'), c1 = c2 == std::string::npos || c2 == 0 ? std::string::npos : t.rfind(':', c2 - 1);
            std::string num = c1 == std::string::npos ? std::string() : t.substr(c1 + 1, c2 - c1 - 1);
            if (isNumber(num)) {
                std::string msg = pending;
                // Detail after the carets, a few lines below.
                for (size_t k = i + 1; k < lines.size() && k < i + 5; ++k)
                    if (size_t caret = lines[k].find('^'); caret != std::string::npos) {
                        size_t d = lines[k].find_first_not_of("^ ", caret);
                        if (d != std::string::npos) msg = lines[k].substr(d);
                        break;
                    }
                out.push_back({std::atoi(num.c_str()), msg});
            }
            pending.clear();
        }
    }
    return out;
}

static std::string Trimmed(std::string_view s) {
    size_t a = s.find_first_not_of(" \t\r"), b = s.find_last_not_of(" \t\r");
    return a == std::string_view::npos ? std::string() : std::string(s.substr(a, b - a + 1));
}

int FindCodeLine(const std::string& code, const std::string& line) {
    const std::string want = Trimmed(line);
    if (want.empty()) return -1;
    int n = 0;
    for (size_t start = 0; start <= code.size(); ++n) {
        size_t e = code.find('\n', start);
        if (e == std::string::npos) e = code.size();
        if (Trimmed(std::string_view(code).substr(start, e - start)) == want) return n;
        start = e + 1;
    }
    return -1;
}

std::string LineOf(const std::string& text, int n) {
    size_t start = 0;
    for (int i = 1; i < n && start < text.size(); ++i) {
        size_t e = text.find('\n', start);
        if (e == std::string::npos) return {};
        start = e + 1;
    }
    size_t e = text.find('\n', start);
    return text.substr(start, (e == std::string::npos ? text.size() : e) - start);
}

bool CodeEditor(const char* label, std::string* text, ImVec2 size, bool readOnly,
                const std::vector<std::string>* names, const std::vector<CodeMark>* marks) {
    auto& view = View(label);
    if (view.owner != text || view.published != *text) {
        view.editor.setText(*text);
        view.owner = text;
        view.published = *text;
        view.dirty = false;
        view.semanticRevision = ~size_t(0);
    }
    view.editor.setReadOnly(readOnly);
    auto& native = view.editor.native();
    native.ClearMarkers();
    if (marks) for (const auto& mark : *marks)
        if (mark.line >= 0) native.AddMarker((size_t)mark.line, cyui::palette().error,
            cyui::alpha(cyui::palette().error, .25f), mark.message, mark.message);
    const std::vector<std::string> noNames;
    const auto& semanticNames = names ? *names : noNames;
    const bool namesChanged = view.semanticNames != semanticNames;
    if (namesChanged) view.editor.setSyntaxEnabled(true);
    view.editor.refreshSyntax();
    if ((namesChanged || view.semanticRevision != native.GetTextRevision()) && !semanticNames.empty())
        SemanticNames(view.editor, semanticNames);
    view.semanticNames = semanticNames;
    view.semanticRevision = native.GetTextRevision();
    const auto id=ImGui::GetID(label);
    auto* parent=ImGui::GetCurrentWindow();
    ImGui::PushFont(cyui::monoFont(), 0.f);
    const bool changed = view.editor.draw(label, size);
    ImGui::PopFont();
    bool focused=false;
    for (auto* w=GImGui->NavWindow; w; w=w->ParentWindow)
        if (w->ChildId==id && w->ParentWindow==parent) { focused=true; break; }
    auto& context=editors.at(ImGui::GetCurrentContext());
    context.item=ImGui::GetItemID(); context.frame=ImGui::GetFrameCount();
    context.committed=view.focused && !focused && view.dirty;
    if (context.committed) view.dirty=false;
    view.focused=focused;
    view.dirty|=changed && !readOnly;
    if (changed && !readOnly) {
        *text = view.editor.text();
        view.published = *text;
        return true;
    }
    return false;
}

bool CodeEditorCommitted() {
    const auto it=editors.find(ImGui::GetCurrentContext());
    return it!=editors.end() && it->second.frame==ImGui::GetFrameCount() &&
        it->second.item==ImGui::GetItemID() && it->second.committed;
}

}  // namespace sx::ui
