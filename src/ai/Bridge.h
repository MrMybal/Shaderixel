#pragma once
// Shaderixel operations execute on the main thread. The native SDK serves the
// assistant, CLI and MCP. The script-plugin HTTP protocol stays compatible.

#include "ai/Net.h"
#include "core/Json.h"

#include <deque>
#include <future>
#include <mutex>
#include <span>
#include <unordered_map>

namespace cymcp { class Registry; class PerformanceMonitor; enum class LocalAccessProfile; }

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
    bool write = false;     // changes document or application state
    std::vector<std::string> inputFiles;  // named path arguments requiring file access
    std::function<AiResult(const Json& params, const std::string& workspace)> run;
    Json outputSchema;
    bool fileAccess = false;  // commands with indirect file access, including arbitrary UI input
};

struct McpAccessOption {
    cymcp::LocalAccessProfile profile;
    const char* id;
    const char* label;
};
std::span<const McpAccessOption> McpAccessOptions();

struct AiSettings {
    AiSettings();
    std::string python = "python";
    bool autoStart = false;
    cymcp::LocalAccessProfile mcpProfile;
    bool allowWrites = false;
    bool allowFiles = false;
    bool compactTools = true;
    bool spoolResults = true;
    bool cacheDiscovery = true;
    bool notifyChanges = true;
    bool measureCalls = false;
    bool keepChatHistory = true;
    int contextKiB = 0;
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
    std::string assistantModel, assistantEndpoint, assistantKeyEnv;
    Json ToJson() const;
    void FromJson(const Json& j);
};

class AiBridge {
public:
    AiBridge();
    ~AiBridge();
    void Configure(const AiSettings&);
    void Register(AiOperation op);
    void Unregister(const std::string& id);
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

    bool StartMcp(std::string& error);
    void StopMcp();
    bool SetMcpProfile(cymcp::LocalAccessProfile profile, std::string& error);
    std::string McpUrl() const;
    std::string McpConfiguration() const;
    std::shared_ptr<const cymcp::Registry> ChatTools(bool compact) const;
    std::shared_ptr<cymcp::PerformanceMonitor> Performance() const;

    bool Running() const { return server.Running(); }
    int Port() const { return server.Port(); }
    std::atomic<bool> allowWrites{false};
    std::atomic<bool> allowFiles{false};
    bool ControlsOperations() const;
    bool CanWrite() const { return !ControlsOperations() || allowWrites; }
    bool CanAccessFiles() const { return !ControlsOperations() || allowFiles; }

private:
    struct Call {
        std::string op, workspace;
        Json params;
        bool trusted = false;  // script plugin started by the user
        std::function<bool()> cancelled;
        std::function<void(const AiResult&)> finish;
    };
    struct Pending {
        Call call;
        AiResult result;
    };
    int Handle(const net::Request& req, std::string& out);
    std::string Describe() const;
    bool Queue(Call);
    void Finish(const Call&, const AiResult&);

    net::LoopbackServer server;
    std::string pluginToken;
    std::vector<AiOperation> ops;
    std::mutex mutex;
    mutable std::mutex opsMutex;
    std::deque<Call> queue;
    std::vector<Pending> pending;
    std::unordered_map<std::string, std::shared_future<std::string>> calls;  // callId -> response
    bool closing = false; // guarded by mutex
    struct Sdk;
    std::unique_ptr<Sdk> sdk;
};

}  // namespace sx
