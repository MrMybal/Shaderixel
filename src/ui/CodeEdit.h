#pragma once
// HLSL code editing: an InputTextMultiline with syntax colors, line numbers, and a formatter
// that lays out minified code (one statement per line, indented blocks).
#include <imgui.h>
#include <string>
#include <vector>

namespace sx::ui {

// Code field (monospace font). `names`: identifiers shown in their own color (pins, parameters).
// Returns true when the text changed (read-only: never).
bool CodeEditor(const char* label, std::string* text, ImVec2 size, bool readOnly = false, const std::vector<std::string>* names = nullptr);

// Lays out C-like code: a statement per line, blocks indented by 4 spaces, a space after commas.
// Strings, comments and preprocessor lines are kept as they are. Already laid out code is unchanged.
std::string FormatHlsl(const std::string& code, int baseIndent = 1);
// The code looks minified (long lines holding several statements).
bool LooksMinified(const std::string& code);

}  // namespace sx::ui
