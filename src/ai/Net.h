#pragma once
// Minimal networking for the AI integration: a loopback HTTP/1.1 server (bridge
// for the CyToolsCore companion) and a WinHTTP client (companion + providers).

#include "core/Common.h"

#include <atomic>
#include <functional>
#include <map>
#include <thread>

namespace sx::net {

struct Request {
    std::string method, path, body;
    std::map<std::string, std::string> headers;  // lower-case names
};

// Handler returns the HTTP status; status 0 means "drop the connection without
// answering" (used so a client retries a long-running call with the same id).
using Handler = std::function<int(const Request&, std::string& responseBody)>;

class LoopbackServer {
public:
    ~LoopbackServer() { Stop(); }
    bool Start(int firstPort, int lastPort, Handler handler);
    void Stop();
    int Port() const { return port; }
    bool Running() const { return running; }

private:
    void AcceptLoop();
    void Serve(uintptr_t sock);
    uintptr_t listenSock = ~uintptr_t(0);
    int port = 0;
    std::atomic<bool> running{false};
    std::thread thread;
    Handler handler;
};

struct Response {
    int status = 0;
    std::string body;
    std::string error;
};

Response Http(const std::string& method, const std::string& url, const std::vector<std::pair<std::string, std::string>>& headers,
              const std::string& body, int timeoutMs = 30000);

std::string Base64Encode(const void* data, size_t size);
std::vector<u8> Base64Decode(const std::string& s);
std::string RandomToken(size_t bytes = 32);

}  // namespace sx::net
