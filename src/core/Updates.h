#pragma once
#include "core/Json.h"
#include <atomic>
#include <functional>
#include <mutex>
#include <optional>
#include <stop_token>
#include <thread>

#ifndef SX_APP_VERSION
#define SX_APP_VERSION "0.0.0"
#endif
namespace sx::updates {
inline constexpr const char* Version = SX_APP_VERSION;
inline constexpr const char* ReleasesUrl = "https://github.com/MrMybal/Shaderixel/releases";
struct Release {
    std::string version, notes, url, filename, sha256;
    u64 size = 0;
};
enum class Phase { Idle, Checking, Current, Available, Downloading, Ready, Error };
struct Snapshot {
    Phase phase = Phase::Idle;
    Release release;
    std::string error, installer;
    u64 received = 0;
};
std::optional<int> CompareVersions(std::string a, std::string b);
std::optional<Release> SelectRelease(const Json& releases, const std::string& current, bool includePrereleases);
bool AllowedUrl(const std::string& url);
bool VerifyFile(const std::string& path, const Release& release);
// Both functions run on a worker. No installation is ever automatic.
std::optional<Release> Check(const std::string& current, bool includePrereleases, std::stop_token stop = {});
std::string Download(const Release& release, const std::string& directory, std::stop_token stop = {},
                     std::function<void(u64)> progress = {});
class Client {
public:
    ~Client();
    Snapshot Get() const;
    void CheckNow(bool includePrereleases);
    void DownloadNow(const std::string& directory);
    void Cancel();
private:
    mutable std::mutex mutex;
    Snapshot state;
    std::jthread worker;
    bool Busy() const;
};
}
