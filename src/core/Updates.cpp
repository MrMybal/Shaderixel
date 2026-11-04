#include "core/Updates.h"
#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <chrono>
#include <charconv>
#include <fstream>
#include <regex>
#include <stdexcept>
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "bcrypt.lib")

namespace sx::updates {
namespace {
using Clock = std::chrono::steady_clock;
struct Internet {
    HINTERNET h = nullptr;
    explicit Internet(HINTERNET value) : h(value) { if (!h) throw std::runtime_error("Network connection failed."); }
    ~Internet() { WinHttpCloseHandle(h); }
    operator HINTERNET() const { return h; }
};
std::string Get(const std::string& initial, u64 maximum, std::stop_token stop,
                const std::function<void(const char*, size_t)>& sink = {}) {
    std::string url = initial, body;
    auto deadline = Clock::now() + std::chrono::minutes(5);
    for (int redirect = 0; redirect < 6; ++redirect) {
        if (stop.stop_requested()) throw std::runtime_error("Cancelled.");
        if (!AllowedUrl(url)) throw std::runtime_error("The update URL is not trusted.");
        auto wide = Widen(url);
        URL_COMPONENTS uc{sizeof(uc)};
        uc.dwHostNameLength = uc.dwUrlPathLength = uc.dwExtraInfoLength = DWORD(-1);
        if (!WinHttpCrackUrl(wide.c_str(), 0, 0, &uc)) throw std::runtime_error("Invalid update URL.");
        std::wstring host(uc.lpszHostName, uc.dwHostNameLength), path(uc.lpszUrlPath, uc.dwUrlPathLength);
        if (uc.dwExtraInfoLength) path.append(uc.lpszExtraInfo, uc.dwExtraInfoLength);
        Internet session(WinHttpOpen(L"Shaderixel-Updater/1", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
        WinHttpSetTimeouts(session, 5000, 5000, 5000, 5000);
        Internet connection(WinHttpConnect(session, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0));
        Internet request(WinHttpOpenRequest(connection, L"GET", path.c_str(), nullptr,
                                           WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
        DWORD disabled = WINHTTP_DISABLE_REDIRECTS;
        WinHttpSetOption(request, WINHTTP_OPTION_DISABLE_FEATURE, &disabled, sizeof(disabled));
        const wchar_t* headers = L"Accept: application/vnd.github+json\r\nX-GitHub-Api-Version: 2022-11-28\r\n";
        if (!WinHttpSendRequest(request, headers, DWORD(-1), nullptr, 0, 0, 0) || !WinHttpReceiveResponse(request, nullptr))
            throw std::runtime_error("Cannot contact GitHub. Check your Internet connection and try again.");
        DWORD status = 0, count = sizeof(status);
        if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                 nullptr, &status, &count, nullptr)) throw std::runtime_error("Invalid GitHub response.");
        if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) {
            DWORD bytes = 0;
            WinHttpQueryHeaders(request, WINHTTP_QUERY_LOCATION, nullptr, nullptr, &bytes, nullptr);
            if (!bytes || bytes > 32768) throw std::runtime_error("Invalid update redirect.");
            std::wstring location(bytes / sizeof(wchar_t), L'\0');
            if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_LOCATION, nullptr, location.data(), &bytes, nullptr))
                throw std::runtime_error("Invalid update redirect.");
            location.resize(bytes / sizeof(wchar_t)); url = Narrow(location); continue;
        }
        if (status != 200) throw std::runtime_error("GitHub returned HTTP " + std::to_string(status) + ". Try again later.");
        u64 received = 0;
        for (;;) {
            if (stop.stop_requested()) throw std::runtime_error("Cancelled.");
            if (Clock::now() > deadline) throw std::runtime_error("The update download timed out.");
            char chunk[65536]; DWORD read = 0;
            if (!WinHttpReadData(request, chunk, sizeof(chunk), &read)) throw std::runtime_error("The update download was interrupted.");
            if (!read) return body;
            received += read;
            if (received > maximum) throw std::runtime_error("The update response is larger than expected.");
            if (sink) sink(chunk, read); else body.append(chunk, read);
        }
    }
    throw std::runtime_error("Too many update redirects.");
}
std::optional<std::vector<u64>> ParseVersion(std::string text) {
    if (text.starts_with('v')) text.erase(0, 1);
    static const std::regex pattern(R"((\d+)\.(\d+)\.(\d+)(?:-(alpha|beta|rc)\.([0-9]+(?:\.[0-9]+)*))?)");
    std::smatch match;
    if (text.size() > 100 || !std::regex_match(text, match, pattern)) return {};
    std::vector<u64> result;
    auto number = [&](std::string s) {
        u64 n = 0; auto r = std::from_chars(s.data(), s.data() + s.size(), n);
        if (r.ec != std::errc{} || r.ptr != s.data() + s.size()) return false;
        result.push_back(n); return true;
    };
    for (int i = 1; i <= 3; ++i) if (!number(match[i].str())) return {};
    std::string stage = match[4].str();
    result.push_back(stage.empty() ? 3 : stage == "rc" ? 2 : stage == "beta" ? 1 : 0);
    std::string tail = match[5].str();
    for (size_t start = 0; start < tail.size();) {
        size_t end = tail.find('.', start);
        if (!number(tail.substr(start, end == std::string::npos ? end : end - start))) return {};
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return result;
}
}

bool AllowedUrl(const std::string& url) {
    auto wide = Widen(url); URL_COMPONENTS uc{sizeof(uc)};
    uc.dwHostNameLength = uc.dwUserNameLength = uc.dwPasswordLength = DWORD(-1);
    if (!WinHttpCrackUrl(wide.c_str(), 0, 0, &uc) || uc.nScheme != INTERNET_SCHEME_HTTPS || uc.nPort != 443 ||
        uc.dwUserNameLength || uc.dwPasswordLength || url.find('#') != std::string::npos) return false;
    std::wstring host(uc.lpszHostName, uc.dwHostNameLength);
    std::transform(host.begin(), host.end(), host.begin(), towlower);
    return host == L"api.github.com" || host == L"github.com" || host == L"release-assets.githubusercontent.com" || host == L"objects.githubusercontent.com";
}
std::optional<int> CompareVersions(std::string a, std::string b) {
    auto x = ParseVersion(a), y = ParseVersion(b);
    if (!x || !y) return {};
    return *x == *y ? 0 : std::lexicographical_compare(x->begin(), x->end(), y->begin(), y->end()) ? -1 : 1;
}
std::optional<Release> SelectRelease(const Json& releases, const std::string& current, bool includePrereleases) {
    if (!releases.IsArray()) return {};
    std::optional<Release> best;
    for (const auto& entry : releases.Elements()) {
        if (entry["draft"].B() || (entry["prerelease"].B() && !includePrereleases)) continue;
        std::string version = entry["tag_name"].S("");
        if (!version.empty() && version[0] == 'v') version.erase(0, 1);
        auto cmp = CompareVersions(version, best ? best->version : current);
        if (!cmp || *cmp <= 0 || (!includePrereleases && version.find('-') != std::string::npos)) continue;
        std::string filename = "Shaderixel-" + version + "-windows-x64-setup.exe";
        for (const auto& asset : entry["assets"].Elements()) {
            auto sha = asset["digest"].S("");
            auto url = asset["browser_download_url"].S("");
            double amount = asset["size"].Num();
            if (!std::isfinite(amount) || amount < 1024 || amount > 1024.0 * 1024 * 1024) continue;
            u64 size = (u64)amount;
            const std::string expected = "https://github.com/MrMybal/Shaderixel/releases/download/v" + version + "/" + filename;
            if (asset["name"].S("") != filename || url != expected || asset["state"].S("") != "uploaded" ||
                size < 1024 || size > 1024ull * 1024 * 1024 || !std::regex_match(sha, std::regex("sha256:[0-9a-f]{64}"))) continue;
            best = Release{version, entry["body"].S(""), url, filename, sha.substr(7), size};
            break;
        }
    }
    return best;
}
bool VerifyFile(const std::string& path, const Release& release) {
    std::error_code ec;
    if (std::filesystem::file_size(Utf8Path(path), ec) != release.size || ec) return false;
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) return false;
    bool ok = BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) >= 0;
    std::ifstream file(Utf8Path(path), std::ios::binary);
    char data[65536];
    while (ok && file) {
        file.read(data, sizeof(data));
        ok = BCryptHashData(hash, reinterpret_cast<PUCHAR>(data), (ULONG)file.gcount(), 0) >= 0;
    }
    unsigned char bytes[32]{};
    ok = ok && file.eof() && BCryptFinishHash(hash, bytes, sizeof(bytes), 0) >= 0;
    if (hash) BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    std::string hex; const char* digits = "0123456789abcdef";
    for (auto b : bytes) { hex += digits[b >> 4]; hex += digits[b & 15]; }
    return ok && hex == release.sha256;
}
std::optional<Release> Check(const std::string& current, bool includePrereleases, std::stop_token stop) {
    auto body = Get("https://api.github.com/repos/MrMybal/Shaderixel/releases?per_page=100", 8 * 1024 * 1024, stop);
    Json list;
    if (!Json::Parse(body, list) || !list.IsArray()) throw std::runtime_error("GitHub returned an invalid release list.");
    return SelectRelease(list, current, includePrereleases);
}
std::string Download(const Release& release, const std::string& directory, std::stop_token stop, std::function<void(u64)> progress) {
    // Revalidate metadata even when called outside the release-list selection code.
    if (!ParseVersion(release.version) || release.filename != "Shaderixel-" + release.version + "-windows-x64-setup.exe" ||
        release.url != "https://github.com/MrMybal/Shaderixel/releases/download/v" + release.version + "/" + release.filename ||
        !std::regex_match(release.sha256, std::regex("[0-9a-f]{64}")) || release.size > 1024ull * 1024 * 1024)
        throw std::runtime_error("Invalid installer metadata.");
    unsigned char random[16];
    if (BCryptGenRandom(nullptr, random, sizeof(random), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) throw std::runtime_error("Cannot create an update cache.");
    std::string id;for (auto b : random) { id += "0123456789abcdef"[b >> 4]; id += "0123456789abcdef"[b & 15]; }
    auto dir = Utf8Path(directory) / id;
    std::filesystem::create_directories(dir);
    auto destination = dir / Utf8Path(release.filename), partial = destination;
    partial += L".part";
    try {
        std::ofstream file(partial, std::ios::binary | std::ios::trunc);
        if (!file) throw std::runtime_error("Cannot write the update download.");
        u64 received = 0;
        Get(release.url, release.size, stop, [&](const char* data, size_t size) {
            file.write(data, size);
            if (!file) throw std::runtime_error("Cannot write the update download.");
            received += size; if (progress) progress(received);
        });
        file.close();
        if (stop.stop_requested()) throw std::runtime_error("Cancelled.");
        if (!VerifyFile(Narrow(partial.wstring()), release)) throw std::runtime_error("Installer verification failed (SHA-256 or size mismatch).");
        std::filesystem::rename(partial, destination);
        return Narrow(destination.wstring());
    } catch (...) {
        std::error_code ec; std::filesystem::remove(partial, ec); std::filesystem::remove(dir, ec); throw;
    }
}
Client::~Client() { Cancel(); }
Snapshot Client::Get() const { std::lock_guard lock(mutex); return state; }
bool Client::Busy() const { auto p = Get().phase; return p == Phase::Checking || p == Phase::Downloading; }
void Client::Cancel() { if (worker.joinable()) worker.request_stop(); }
void Client::CheckNow(bool includePrereleases) {
    if (Busy()) return;
    if (worker.joinable()) worker.join();
    { std::lock_guard lock(mutex); state = {}; state.phase = Phase::Checking; }
    Log("[updates] Checking GitHub; current=%s prereleases=%d", Version, includePrereleases ? 1 : 0);
    worker = std::jthread([this, includePrereleases](std::stop_token stop) {
        try {
            auto release = Check(Version, includePrereleases, stop);
            std::lock_guard lock(mutex);
            if (release) { state.release = *release; state.phase = Phase::Available; Log("[updates] Available: %s", release->version.c_str()); }
            else { state.phase = Phase::Current; Log("[updates] Up to date."); }
        } catch (const std::exception& e) { std::lock_guard lock(mutex); state.error = e.what(); state.phase = Phase::Error; }
    });
}
void Client::DownloadNow(const std::string& directory) {
    auto before = Get();
    if (before.phase != Phase::Available) return;
    if (worker.joinable()) worker.join();
    { std::lock_guard lock(mutex); state.phase = Phase::Downloading; state.received = 0; }
    worker = std::jthread([this, release = before.release, directory](std::stop_token stop) {
        try {
            auto path = Download(release, directory, stop, [this](u64 bytes) { std::lock_guard lock(mutex); state.received = bytes; });
            std::lock_guard lock(mutex); state.installer = path; state.phase = Phase::Ready;
        } catch (const std::exception& e) { std::lock_guard lock(mutex); state.error = e.what(); state.phase = Phase::Error; }
    });
}
}
