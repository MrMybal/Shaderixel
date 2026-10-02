#include "ai/Net.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "bcrypt.lib")

namespace sx::net {

namespace {
struct WsaInit {
    WsaInit() { WSADATA d; WSAStartup(MAKEWORD(2, 2), &d); }
    ~WsaInit() { WSACleanup(); }
};
WsaInit& Wsa() { static WsaInit w; return w; }

std::string Lower(std::string s) {
    for (auto& c : s) c = (char)tolower((unsigned char)c);
    return s;
}
}  // namespace

bool LoopbackServer::Start(int firstPort, int lastPort, Handler h) {
    Wsa();
    handler = std::move(h);
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return false;
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    BOOL excl = TRUE;
    setsockopt(s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char*)&excl, sizeof(excl));
    for (int p = firstPort; p <= lastPort; ++p) {
        addr.sin_port = htons((u_short)p);
        if (bind(s, (sockaddr*)&addr, sizeof(addr)) == 0) {
            port = p;
            break;
        }
    }
    if (!port || listen(s, 16) != 0) {
        closesocket(s);
        port = 0;
        return false;
    }
    listenSock = (uintptr_t)s;
    running = true;
    thread = std::thread([this] { AcceptLoop(); });
    return true;
}

void LoopbackServer::Stop() {
    if (!running) return;
    running = false;
    closesocket((SOCKET)listenSock);
    if (thread.joinable()) thread.join();
}

void LoopbackServer::AcceptLoop() {
    while (running) {
        SOCKET c = accept((SOCKET)listenSock, nullptr, nullptr);
        if (c == INVALID_SOCKET) break;
        std::thread([this, c] { Serve((uintptr_t)c); }).detach();
    }
}

void LoopbackServer::Serve(uintptr_t sock) {
    SOCKET c = (SOCKET)sock;
    DWORD timeout = 10000;
    setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    std::string data;
    char buf[8192];
    size_t headerEnd = std::string::npos;
    while (headerEnd == std::string::npos && data.size() < 65536) {
        int n = recv(c, buf, sizeof(buf), 0);
        if (n <= 0) { closesocket(c); return; }
        data.append(buf, n);
        headerEnd = data.find("\r\n\r\n");
    }
    if (headerEnd == std::string::npos) { closesocket(c); return; }
    Request req;
    std::string head = data.substr(0, headerEnd);
    size_t lineEnd = head.find("\r\n");
    std::string first = head.substr(0, lineEnd);
    size_t sp1 = first.find(' '), sp2 = first.find(' ', sp1 + 1);
    req.method = first.substr(0, sp1);
    req.path = first.substr(sp1 + 1, sp2 - sp1 - 1);
    size_t pos = lineEnd + 2;
    while (pos < head.size()) {
        size_t e = head.find("\r\n", pos);
        if (e == std::string::npos) e = head.size();
        std::string line = head.substr(pos, e - pos);
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string v = line.substr(colon + 1);
            v.erase(0, v.find_first_not_of(' '));
            req.headers[Lower(line.substr(0, colon))] = v;
        }
        pos = e + 2;
    }
    size_t len = 0;
    if (auto it = req.headers.find("content-length"); it != req.headers.end()) len = (size_t)strtoull(it->second.c_str(), nullptr, 10);
    if (len > (16u << 20)) { closesocket(c); return; }
    req.body = data.substr(headerEnd + 4);
    while (req.body.size() < len) {
        int n = recv(c, buf, sizeof(buf), 0);
        if (n <= 0) break;
        req.body.append(buf, n);
    }
    req.body.resize(std::min(req.body.size(), len));
    std::string out;
    int status = handler ? handler(req, out) : 500;
    if (status != 0) {
        const char* text = status == 200 ? "OK" : status == 403 ? "Forbidden" : status == 404 ? "Not Found" : status == 503 ? "Service Unavailable" : "Error";
        std::string resp = "HTTP/1.1 " + std::to_string(status) + " " + text + "\r\nContent-Type: application/json\r\nContent-Length: " +
                           std::to_string(out.size()) + "\r\nConnection: close\r\n\r\n" + out;
        send(c, resp.data(), (int)resp.size(), 0);
    }
    shutdown(c, SD_BOTH);
    closesocket(c);
}

Response Http(const std::string& method, const std::string& url, const std::vector<std::pair<std::string, std::string>>& headers,
              const std::string& body, int timeoutMs) {
    Response r;
    std::wstring wurl = Widen(url);
    URL_COMPONENTS uc = {sizeof(uc)};
    wchar_t host[256] = {}, path[4096] = {};
    uc.lpszHostName = host;
    uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = 4096;
    wchar_t extra[4096] = {};
    uc.lpszExtraInfo = extra;
    uc.dwExtraInfoLength = 4096;
    if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &uc)) { r.error = "URL invalide"; return r; }
    HINTERNET ses = WinHttpOpen(L"Shaderixel/0.1", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!ses) { r.error = "WinHttpOpen"; return r; }
    WinHttpSetTimeouts(ses, timeoutMs, timeoutMs, timeoutMs, timeoutMs);
    HINTERNET con = WinHttpConnect(ses, host, uc.nPort, 0);
    std::wstring fullPath = std::wstring(path) + extra;
    HINTERNET req = con ? WinHttpOpenRequest(con, Widen(method).c_str(), fullPath.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                             uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0)
                        : nullptr;
    if (!req) {
        r.error = "connexion impossible";
        if (con) WinHttpCloseHandle(con);
        WinHttpCloseHandle(ses);
        return r;
    }
    std::wstring hdr;
    for (auto& [k, v] : headers) hdr += Widen(k) + L": " + Widen(v) + L"\r\n";
    BOOL ok = WinHttpSendRequest(req, hdr.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : hdr.c_str(), (DWORD)-1L, (LPVOID)body.data(), (DWORD)body.size(),
                                 (DWORD)body.size(), 0) &&
              WinHttpReceiveResponse(req, nullptr);
    if (!ok) {
        r.error = "requête échouée (" + std::to_string(GetLastError()) + ")";
    } else {
        DWORD status = 0, size = sizeof(status);
        WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);
        r.status = (int)status;
        for (;;) {
            DWORD avail = 0;
            if (!WinHttpQueryDataAvailable(req, &avail) || avail == 0) break;
            size_t old = r.body.size();
            r.body.resize(old + avail);
            DWORD read = 0;
            WinHttpReadData(req, r.body.data() + old, avail, &read);
            r.body.resize(old + read);
        }
    }
    WinHttpCloseHandle(req);
    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
    return r;
}

std::string Base64Encode(const void* data, size_t size) {
    static const char* t = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const u8* p = (const u8*)data;
    std::string out;
    out.reserve((size + 2) / 3 * 4);
    for (size_t i = 0; i < size; i += 3) {
        u32 v = (u32)p[i] << 16 | (i + 1 < size ? (u32)p[i + 1] << 8 : 0) | (i + 2 < size ? p[i + 2] : 0);
        out += t[(v >> 18) & 63];
        out += t[(v >> 12) & 63];
        out += i + 1 < size ? t[(v >> 6) & 63] : '=';
        out += i + 2 < size ? t[v & 63] : '=';
    }
    return out;
}

std::vector<u8> Base64Decode(const std::string& s) {
    auto val = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+' || c == '-') return 62;
        if (c == '/' || c == '_') return 63;
        return -1;
    };
    std::vector<u8> out;
    out.reserve(s.size() * 3 / 4);
    u32 acc = 0;
    int bits = 0;
    for (char c : s) {
        int v = val(c);
        if (v < 0) continue;
        acc = (acc << 6) | (u32)v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back((u8)(acc >> bits));
        }
    }
    return out;
}

std::string RandomToken(size_t bytes) {
    std::vector<u8> b(bytes);
    BCryptGenRandom(nullptr, b.data(), (ULONG)b.size(), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    static const char* hex = "0123456789abcdef";
    std::string s;
    for (u8 x : b) { s += hex[x >> 4]; s += hex[x & 15]; }
    return s;
}

}  // namespace sx::net
