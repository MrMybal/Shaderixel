#pragma once
// Host side of the CyToolsCore integration (same protocol as CyTexture's
// Unreal bridge): a loopback HTTP endpoint answering `describe`, `invoke`
// (idempotent per callId) and `ack`, plus the companion process
// (CyUEMCPSDK `run.py --editor <connection.json>`) which turns these host
// operations into an MCP server and provides the in-app assistant.
//
// Operations always execute on the main thread (GPU + document access): the
// HTTP thread queues the call and the app pumps the queue once per frame.

#include "ai/Net.h"
#include "core/Json.h"

#include <deque>
#include <future>
#include <mutex>
#include <unordered_map>

namespace sx {

struct AiResult {
    bool done = true;
    Json result;
    std::string errorCode, errorMessage;
    std::function<bool(AiResult&)> poll;  // for async work: return true when finished
    static AiResult Ok(Json j) { AiResult r; r.result = std::move(j); return r; }
    static AiResult Fail(const std::string& code, const std::string& msg) { AiResult r; r.errorCode = code; r.errorMessage = msg; return r; }
};

struct AiOperation {
    std::string id;
    std::string description;
    Json inputSchema;       // JSON schema of parameters
    bool write = false;     // modifies the document (gated by the user)
    std::vector<std::string> inputFiles;  // parameter paths the companion copies in
    std::function<AiResult(const Json& params, const std::string& workspace)> run;
};

struct AiSettings {
    std::string python = "python";
    std::string companionScript;  // CyUEMCPSDK Bridge/run.py
    bool autoStart = false;
    bool allowWrites = false;
    std::string imageProvider = "openai";
    std::string openaiEndpoint = "https://api.openai.com/v1";
    std::string openaiModel = "gpt-image-1";
    std::string openaiKeyEnv = "OPENAI_API_KEY";
    std::string geminiEndpoint = "https://generativelanguage.googleapis.com/v1beta";
    std::string geminiModel = "gemini-2.5-flash-image";
    std::string geminiKeyEnv = "GEMINI_API_KEY";
    std::string comfyEndpoint = "http://127.0.0.1:8188";
    std::string comfyWorkflow;  // API-format workflow file (empty = built-in graph)
    std::string comfyCheckpoint = "sd_xl_base_1.0.safetensors";
    std::string comfyNegative = "blurry, low quality, watermark, text";
    int comfySteps = 25;
    float comfyDenoise = 0.6f;
    std::string assistantConnector = "codex";
    std::string assistantModel;
    Json ToJson() const;
    void FromJson(const Json& j);
};

class AiBridge {
public:
    ~AiBridge() { Shutdown(); }
    // Main thread only; the network thread lists them under opsMutex (Describe).
    void Register(AiOperation op) {
        std::lock_guard lock(opsMutex);
        ops.push_back(std::move(op));
    }
    void Unregister(const std::string& id) {
        std::lock_guard lock(opsMutex);
        for (size_t i = 0; i < ops.size(); ++i)
            if (ops[i].id == id) { ops.erase(ops.begin() + i); return; }
    }
    // Token of the script plugins started by the user (their writes are allowed).
    const std::string& PluginToken() const { return pluginToken; }
    const std::vector<AiOperation>& Operations() const { return ops; }
    const AiOperation* Find(const std::string& id) const {
        for (auto& o : ops) if (o.id == id) return &o;
        return nullptr;
    }
    bool Start(const std::string& projectHint);  // HTTP endpoint + connection file
    void Shutdown();
    void Pump();                                  // main thread, once per frame

    bool LaunchCompanion(const AiSettings& s, std::string* error);
    void StopCompanion();
    bool CompanionRunning() const;
    // Companion service endpoint (after it published <connection>.service.json).
    bool ServiceReady() const { return !serviceUrl.empty(); }
    const std::string& ServiceUrl() const { return serviceUrl; }
    const std::string& ServiceToken() const { return serviceToken; }

    bool Running() const { return server.Running(); }
    int Port() const { return server.Port(); }
    const std::string& ConnectionFile() const { return connectionFile; }
    std::atomic<bool> allowWrites{false};
    std::atomic<int> callsServed{0};
    std::string lastOperation;

private:
    struct Call {
        std::string callId, op, workspace;
        Json params;
        bool trusted = false;  // script plugin started by the user
        std::shared_ptr<std::promise<std::string>> promise;
    };
    struct Pending {
        Call call;
        AiResult result;
    };
    int Handle(const net::Request& req, std::string& out);
    std::string Describe() const;
    std::string Finish(const Call& c, const AiResult& r);

    net::LoopbackServer server;
    std::string token, pluginToken, connectionFile, serviceFile, project;
    std::vector<AiOperation> ops;
    std::mutex mutex;
    mutable std::mutex opsMutex;
    std::deque<Call> queue;
    std::vector<Pending> pending;
    std::unordered_map<std::string, std::shared_future<std::string>> calls;  // callId -> response
    void* companion = nullptr;  // process handle
    std::string serviceUrl, serviceToken;
    double lastServiceCheck = 0;
};

}  // namespace sx
