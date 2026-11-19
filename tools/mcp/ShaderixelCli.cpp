#include "ai/McpPaths.h"
#include "ShaderixelMcpVersion.h"

#include <cymcp/http.hpp>
#include <wincrypt.h>

#include <charconv>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>

namespace {
namespace fs = std::filesystem;
using cymcp::Error;
using cymcp::Json;

std::string Utf8(const wchar_t* value) {
    if (!*value) return {};
    const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, -1, nullptr, 0, nullptr, nullptr);
    if (!size) throw Error("InvalidArguments", "Arguments must be valid Unicode");
    std::string out(size, '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, -1, out.data(), size, nullptr, nullptr);
    out.pop_back();
    return out;
}

std::string ReadLimited(std::istream& stream, std::size_t maximum) {
    std::string data(maximum + 1, '\0');
    stream.read(data.data(), static_cast<std::streamsize>(data.size()));
    data.resize(static_cast<std::size_t>(stream.gcount()));
    if (stream.bad() || data.size() > maximum) throw Error("InvalidInput", "Unable to read the input or it exceeds the size limit");
    if (data.starts_with("\xef\xbb\xbf")) data.erase(0, 3);
    return data;
}

Json ReadJson(std::istream& input, std::size_t size) {
    return cymcp::parse_json(ReadLimited(input, size), size);
}

struct Connection {
    unsigned long pid;
    std::string url, token;
};

bool Running(unsigned long pid) {
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, pid);
    if (!process) return false;
    std::wstring path(32768, L'\0');
    DWORD length = static_cast<DWORD>(path.size());
    const bool running = WaitForSingleObject(process, 0) == WAIT_TIMEOUT &&
                         QueryFullProcessImageNameW(process, 0, path.data(), &length);
    CloseHandle(process);
    if (!running) return false;
    path.resize(length);
    const auto name = fs::path(path).filename().wstring();
    return _wcsicmp(name.c_str(), L"Shaderixel.exe") == 0 ||
           _wcsicmp(name.c_str(), L"ShaderixelVk.exe") == 0;
}

Connection ReadConnection(const fs::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw Error("InstanceUnavailable", "Could not open the Shaderixel instance descriptor");
    const auto data = ReadJson(stream, 16384);
    Connection connection{data.at("pid").get<unsigned long>(), data.at("url").get<std::string>(),
                          data.at("token").get<std::string>()};
    const auto url = cymcp::parse_url(connection.url);
    if (data.at("host") != "Shaderixel" || !connection.url.starts_with("http://127.0.0.1:") ||
        !url.loopback || url.target != "/" || connection.token.size() < 32 || !Running(connection.pid))
        throw Error("InstanceUnavailable", "The Shaderixel instance descriptor is invalid or stale");
    return connection;
}

std::vector<Connection> Instances() {
    std::vector<Connection> connections;
    std::error_code ec;
    const auto path = sx::mcplocal::StateDirectory();
    if (!fs::exists(path, ec)) return connections;
    for (const auto& file : fs::directory_iterator(path)) {
        const auto name = file.path().filename().wstring();
        if (!name.starts_with(L"instance-") || !name.ends_with(L".json") || !file.is_regular_file()) continue;
        try { connections.push_back(ReadConnection(file.path())); }
        catch (const std::exception&) { /* A crashed instance can leave a stale descriptor. */ }
    }
    return connections;
}

Connection ResolveInstance(std::optional<unsigned long> pid, const fs::path& descriptor) {
    if (!descriptor.empty()) return ReadConnection(descriptor);
    if (pid) return ReadConnection(sx::mcplocal::ConnectionPath(*pid));
    const auto candidates = Instances();
    if (candidates.empty()) throw Error("InstanceUnavailable", "Start the MCP server from Shaderixel's AI Assistant panel");
    if (candidates.size() != 1) throw Error("InstanceRequired", "Several Shaderixel instances are open. Run instances and select --instance PID");
    return candidates.front();
}

Json Call(const Connection& connection, const Json& message) {
    cymcp::HttpRequest request;
    request.url = connection.url + "/v1";
    request.method = "POST";
    request.headers = {{"Authorization", "Bearer " + connection.token}, {"Content-Type", "application/json"}};
    request.body = message.dump();
    const auto response = cymcp::HttpClient().perform(request);
    if (response.status == 401) throw Error("AuthenticationRequired", "The Shaderixel instance token is no longer valid");
    const auto value = cymcp::parse_json(response.body, request.max_response_bytes);
    if (response.status != 200 && value.is_object() && value.contains("error")) {
        const auto& error = value.at("error");
        throw Error(error.value("code", "OperationFailed"), error.value("message", "Shaderixel rejected the operation"));
    }
    if (response.status != 200) throw Error("NetworkError", "Unexpected response from Shaderixel");
    return value;
}

std::string Decode(const std::string& text) {
    if (text.empty()) return {};
    DWORD count = 0;
    if (!CryptStringToBinaryA(text.data(), static_cast<DWORD>(text.size()), CRYPT_STRING_BASE64 | CRYPT_STRING_STRICT,
                              nullptr, &count, nullptr, nullptr))
        throw Error("InvalidResource", "Invalid base64 resource");
    std::string bytes(count, '\0');
    if (!CryptStringToBinaryA(text.data(), static_cast<DWORD>(text.size()), CRYPT_STRING_BASE64 | CRYPT_STRING_STRICT,
                              reinterpret_cast<BYTE*>(bytes.data()), &count, nullptr, nullptr))
        throw Error("InvalidResource", "Invalid base64 resource");
    return bytes;
}

Json Download(const Connection& connection, std::string uri, const fs::path& destination) {
    if (destination.empty() || fs::exists(destination))
        throw Error("FileExists", "Choose a new output file");
    fs::path temp = destination;
    temp += ".part-" + cymcp::random_token(16);
    std::size_t offset = 0;
    try {
        auto output = cymcp::open_private_output_file(temp);
        for (;;) {
            const auto reply = Call(connection, {{"method", "read"}, {"uri", uri}});
            const auto& part = reply.at("contents").at(0);
            if (!reply.contains("_meta")) {
                const auto manifest = cymcp::parse_json(part.at("text").get<std::string>());
                if (offset != 0 || manifest.at("type") != "cymcp.resource.v1")
                    throw Error("InvalidResource", "Unexpected resource manifest");
                const auto next = manifest.at("firstUri").get<std::string>();
                if (!next.starts_with("cymcp:") || next == uri) throw Error("InvalidResource", "Invalid resource chunk");
                uri = next;
                continue;
            }
            const auto& page = reply.at("_meta").at("com.cymcp/chunk");
            const auto bytes = part.contains("blob") ? Decode(part.at("blob").get<std::string>()) :
                                                       part.at("text").get<std::string>();
            if (page.at("offset") != offset || page.at("bytes") != bytes.size())
                throw Error("InvalidResource", "Resource chunks are out of order");
            if (std::fwrite(bytes.data(), 1, bytes.size(), output.get()) != bytes.size())
                throw Error("FileError", "Could not write the resource");
            offset += bytes.size();
            if (page.at("eof") == true) {
                if (page.at("totalBytes") != offset) throw Error("InvalidResource", "Resource is incomplete");
                break;
            }
            if (bytes.empty()) throw Error("InvalidResource", "Resource chunk is empty");
            uri = page.at("nextUri").get<std::string>();
        }
        if (std::fclose(output.release()) != 0 || !MoveFileW(temp.c_str(), destination.c_str()))
            throw Error("FileError", "Could not finalize the output file");
    } catch (...) {
        std::error_code ignored;
        fs::remove(temp, ignored);
        throw;
    }
    return {{"path", cymcp::path_utf8(destination)}, {"bytes", offset}};
}

void Help() {
    std::cout << "ShaderixelCli [--instance PID | --connection PATH] COMMAND\n"
                 "  help | version | instances\n"
                 "  list | describe NAME\n"
                 "  invoke NAME [JSON | @FILE | -]\n"
                 "  resources | read URI | grep TEXT [CURSOR]\n"
                 "  download URI PATH\n"
                 "Start the local MCP server from the AI Assistant panel or with Shaderixel.exe --mcp.\n";
}

Json Arguments(const wchar_t* value) {
    const std::wstring arg(value);
    if (arg == L"-") return ReadJson(std::cin, 1024 * 1024);
    if (arg.starts_with(L"@")) {
        std::ifstream stream(fs::path(arg.substr(1)), std::ios::binary);
        if (!stream) throw Error("FileError", "Could not open the JSON arguments file");
        return ReadJson(stream, 1024 * 1024);
    }
    return cymcp::parse_json(Utf8(value));
}

int Run(int argc, wchar_t** argv) {
    int index = 1;
    std::optional<unsigned long> pid;
    fs::path descriptor;
    if (index + 1 < argc && std::wstring_view(argv[index]) == L"--instance") {
        const auto text = Utf8(argv[index + 1]);
        unsigned long value = 0;
        const auto decoded = std::from_chars(text.data(), text.data() + text.size(), value);
        if (decoded.ec != std::errc{} || decoded.ptr != text.data() + text.size() || value == 0)
            throw Error("InvalidArguments", "Expected a positive process ID");
        pid = value;
        index += 2;
    } else if (index + 1 < argc && std::wstring_view(argv[index]) == L"--connection") {
        descriptor = argv[index + 1];
        index += 2;
    }

    const auto command = index < argc ? Utf8(argv[index++]) : "help";
    const int count = argc - index;
    if ((command == "help" || command == "--help" || command == "-h") && count == 0) { Help(); return 0; }
    Json reply;
    if (command == "version" && count == 0) {
        reply = {{"version", SX_APP_VERSION}, {"sdkVersion", cymcp::version}, {"sdkRevision", SX_CYMCP_REVISION}};
    } else if (command == "instances" && count == 0) {
        reply = Json::array();
        for (const auto& instance : Instances()) reply.push_back({{"pid", instance.pid}, {"mcpUrl", instance.url + "/mcp"}});
    } else {
        Json request;
        if (command == "list" && count == 0) request = {{"method", "describe"}};
        else if (command == "describe" && count == 1) request = {{"method", "describe"}, {"name", Utf8(argv[index])}};
        else if (command == "invoke" && (count == 1 || count == 2))
            request = {{"method", "invoke"}, {"name", Utf8(argv[index])},
                       {"arguments", count == 2 ? Arguments(argv[index + 1]) : Json::object()}};
        else if (command == "resources" && count == 0) request = {{"method", "resources"}};
        else if (command == "read" && count == 1) request = {{"method", "read"}, {"uri", Utf8(argv[index])}};
        else if (command == "grep" && (count == 1 || count == 2))
            request = {{"method", "grep"}, {"text", Utf8(argv[index])}, {"cursor", count == 2 ? Utf8(argv[index + 1]) : ""}};
        else if (command == "download" && count == 2) {
            reply = Download(ResolveInstance(pid, descriptor), Utf8(argv[index]), fs::path(argv[index + 1]));
        } else throw Error("InvalidArguments", "See ShaderixelCli --help");
        if (!request.is_null()) reply = Call(ResolveInstance(pid, descriptor), request);
    }
    std::cout << reply.dump() << '\n';
    return std::cout.good() ? 0 : 1;
}
}  // namespace

int wmain(int argc, wchar_t** argv) {
    try {
        return Run(argc, argv);
    } catch (const Error& error) {
        std::cerr << Json{{"error", {{"code", error.code}, {"message", cymcp::redact_error_message(error.what())}}}}.dump() << '\n';
        return error.code == "InvalidArguments" || error.code == "InvalidJson" ? 2 : 1;
    } catch (const std::exception& error) {
        std::cerr << Json{{"error", {{"code", "InvalidRequest"}, {"message", cymcp::redact_error_message(error.what())}}}}.dump() << '\n';
        return 1;
    }
}
