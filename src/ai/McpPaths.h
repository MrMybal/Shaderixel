#pragma once

#include <cymcp/core.hpp>
#include <windows.h>
#include <shlobj.h>

namespace sx::mcplocal {

inline std::filesystem::path StateDirectory() {
    const DWORD count = GetEnvironmentVariableW(L"SHADERIXEL_MCP_STATE_DIR", nullptr, 0);
    if (count != 0) {
        std::wstring value(count, L'\0');
        const DWORD length = GetEnvironmentVariableW(L"SHADERIXEL_MCP_STATE_DIR", value.data(), count);
        if (length == 0 || length >= count) throw cymcp::Error("InvalidConfiguration", "Invalid MCP state directory");
        value.resize(length);
        const std::filesystem::path path(value);
        if (!path.is_absolute()) throw cymcp::Error("InvalidConfiguration", "The MCP state directory must be absolute");
        return path;
    }
    PWSTR value = nullptr;
    const auto status = SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &value);
    std::unique_ptr<wchar_t, decltype(&CoTaskMemFree)> folder(value, CoTaskMemFree);
    if (FAILED(status)) throw cymcp::Error("FileError", "Windows LocalAppData is unavailable");
    return std::filesystem::path(folder.get()) / L"Shaderixel" / L"Mcp";
}

inline std::filesystem::path ConnectionPath(unsigned long pid) {
    return StateDirectory() / ("instance-" + std::to_string(pid) + ".json");
}

}  // namespace sx::mcplocal
