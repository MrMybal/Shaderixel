#include "core/Updates.h"
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace sx;
using namespace sx::updates;
int failures = 0;
void Test(bool ok, const char* why) { if (!ok) { ++failures; std::cerr << why << '\n'; } }
Json Entry(const std::string& version, bool pre = true) {
    Json entry = Json::Object(); entry["tag_name"] = "v" + version; entry["prerelease"] = pre;
    Json a = Json::Object(); std::string name = "Shaderixel-" + version + "-windows-x64-setup.exe";
    a["name"] = name; a["size"] = 2048; a["state"] = "uploaded";
    a["digest"] = "sha256:" + std::string(64, 'a');
    a["browser_download_url"] = "https://github.com/MrMybal/Shaderixel/releases/download/v" + version + "/" + name;
    entry["assets"] = Json::Array(); entry["assets"].Push(a); return entry;
}
int main(int argc, char** argv) {
    Test(CompareVersions("v0.1.0-alpha.10", "0.1.0-alpha.9") == 1, "numeric alpha ordering");
    Test(CompareVersions("0.1.0", "0.1.0-rc.99") == 1, "stable outranks prerelease");
    Test(CompareVersions("0.2.0-alpha.1", "0.1.9") == 1, "base version precedence");
    Test(CompareVersions("0.1.0-alpha.20261104", "0.1.0-alpha.20261104") == 0, "equal version");
    Test(!CompareVersions("../../setup", "0.1.0"), "reject path versions");
    Test(!CompareVersions("0.1.0-alpha.9999999999999999999999999", "0.1.0"), "reject integer overflow");
    Json list = Json::Array(); list.Push(Entry("0.1.0-alpha.20261103")); list.Push(Entry("0.1.0-alpha.20261104"));
    Test(SelectRelease(list, "0.1.0-alpha.20261103", true)->version == "0.1.0-alpha.20261104", "latest alpha selected independently of array order");
    Test(!SelectRelease(list, "0.1.0-alpha.20261104", true), "no equal version reinstall");
    Test(!SelectRelease(list, "0.1.0-alpha.20261105", true), "no downgrade");
    Test(!SelectRelease(list, "0.0.0", false), "stable channel excludes alpha");
    auto bad = Entry("0.2.0-alpha.1"); bad["draft"] = true; list.Push(bad);
    Test(SelectRelease(list, "0.0.0", true)->version == "0.1.0-alpha.20261104", "draft ignored");
    auto checkBad = [](Json entry) { Json a = Json::Array(); a.Push(entry); return !SelectRelease(a, "0.0.0", true); };
    bad = Entry("0.2.0-alpha.1"); bad["assets"] = Json::Array(); Test(checkBad(bad), "missing installer rejected");
    bad = Entry("0.2.0-alpha.1"); Json asset = bad["assets"].At(0); asset["digest"] = ""; bad["assets"] = Json::Array(); bad["assets"].Push(asset); Test(checkBad(bad), "missing digest rejected");
    bad = Entry("0.2.0-alpha.1"); asset = bad["assets"].At(0); asset["browser_download_url"] = "https://github.com/other/repo/setup.exe"; bad["assets"] = Json::Array(); bad["assets"].Push(asset); Test(checkBad(bad), "wrong repository rejected");
    bad = Entry("0.2.0-alpha.1"); asset = bad["assets"].At(0); asset["size"] = -20; bad["assets"] = Json::Array(); bad["assets"].Push(asset); Test(checkBad(bad), "negative size rejected");
    for (auto url : {"http://github.com/file", "https://github.com.evil.example/file", "https://evil.example/github.com/file", "https://user:pass@github.com/file", "https://github.com:8443/file"})
        Test(!AllowedUrl(url), "untrusted redirect rejected");
    Test(AllowedUrl("https://release-assets.githubusercontent.com/file?signature=test"), "GitHub CDN accepted");
    auto path = std::filesystem::temp_directory_path() / L"shaderixel-update-integrity-test.bin";
    { std::ofstream file(path, std::ios::binary); file << "abc"; }
    Release fixture; fixture.size = 3; fixture.sha256 = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    Test(VerifyFile(Narrow(path.wstring()), fixture), "known SHA-256 accepted");
    { std::ofstream file(path, std::ios::binary); file << "abd"; }
    Test(!VerifyFile(Narrow(path.wstring()), fixture), "corruption rejected");
    fixture.size = 4; Test(!VerifyFile(Narrow(path.wstring()), fixture), "truncation rejected");
    std::filesystem::remove(path);
    std::stop_source stopped; stopped.request_stop();
    try { Check("0.0.0", true, stopped.get_token()); Test(false, "cancelled check returned"); } catch (const std::exception&) {}
    if (argc > 1 && std::string(argv[1]) == "--live") {
        try {
            auto release = Check("0.0.0", true); Test(release.has_value(), "live alpha detected");
            if (release) {
                std::cout << "Live release: " << release->version << '\n';
                auto dir = std::filesystem::temp_directory_path() / L"shaderixel-update-download-test";
                auto downloaded = Download(*release, Narrow(dir.wstring()));
                Test(VerifyFile(downloaded, *release), "downloaded installer verified");
                std::cout << "Live installer downloaded and verified: " << release->size << " bytes\n";
                std::filesystem::remove(Utf8Path(downloaded)); std::filesystem::remove(Utf8Path(downloaded).parent_path());
            }
        } catch (const std::exception& e) { Test(false, e.what()); }
    }
    std::cout << "Update tests: " << failures << " failures\n"; return failures ? 1 : 0;
}
