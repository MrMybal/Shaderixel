#include "ai/Bridge.h"
#include "ai/McpPaths.h"

#include <cymcp/changes.hpp>
#include <cymcp/http.hpp>
#include <cymcp/mcp.hpp>
#include <cymcp/performance.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
using namespace std::chrono_literals;

namespace sx {
namespace {

using cymcp::LocalAccessProfile;
constexpr McpAccessOption accessOptions[] = {
    {LocalAccessProfile::free, "free", "Free"},
    {LocalAccessProfile::local_controlled, "local_controlled", "Local contrôlé"},
    {LocalAccessProfile::token_only, "token_only", "Jeton seul"},
    {LocalAccessProfile::protected_access, "protected", "Protégé"},
};

Json ErrorJson(const std::string& code, const std::string& message) {
    Json j = Json::Object();
    j["error"]["code"] = code;
    j["error"]["message"] = message;
    return j;
}

cymcp::Json ToSdk(const Json& j) {
    return cymcp::Json::parse(j.Dump(0));
}

Json FromSdk(const cymcp::Json& j) {
    Json value;
    if (!Json::Parse(j.dump(), value))
        throw cymcp::Error("InvalidJson", "Shaderixel could not read the structured arguments");
    return value;
}

// Each render owns a distinct private directory. Keep it alive until both the
// worker and the main-thread readback have dropped the call.
struct RenderWorkspace {
    fs::path directory;
    ~RenderWorkspace() {
        std::error_code ignored;
        fs::remove(directory / "render.png", ignored);
        fs::remove(directory, ignored);
    }
};

cymcp::Json OutputSchema(const std::string& id) {
    using J = cymcp::Json;
    const J object = {{"type", "object"}}, integer = {{"type", "integer"}};
    const J array = {{"type", "array"}}, string = {{"type", "string"}};
    const J boolean = {{"type", "boolean"}};
    const auto schema = [](J properties, J required) {
        return J{{"type", "object"}, {"properties", std::move(properties)}, {"required", std::move(required)}};
    };
    if (id == "shaderixel.document_info")
        return {{"type", "object"}, {"properties", {{"name", string}, {"width", integer}, {"height", integer},
                {"mode", string}, {"layers", array}, {"hasSelection", boolean}}},
                {"required", {"name", "width", "height", "mode", "layers", "hasSelection"}}};
    if (id == "shaderixel.effects_list")
        return {{"type", "object"}, {"properties", {{"effects", array}, {"total", integer}, {"nextOffset", integer}}},
                {"required", {"effects"}}};
    if (id == "shaderixel.render" || id == "shaderixel.ui_capture")
        return {{"type", "object"}, {"properties", {{"width", integer}, {"height", integer}, {"reference", object}}},
                {"required", {"width", "height", "reference"}}};
    if (id == "shaderixel.document_save")
        return {{"type", "object"}, {"properties", {{"path", string}, {"note", string}}},
                {"required", {"path", "note"}}};
    if (id == "shaderixel.document_new") return schema({{"width", integer}, {"height", integer}}, {"width", "height"});
    if (id == "shaderixel.layer_add" || id == "shaderixel.layer_update")
        return schema({{"id", integer}, {"warning", string}}, {"id"});
    if (id == "shaderixel.filter_add") return schema({{"index", integer}, {"warning", string}}, {"index"});
    if (id == "shaderixel.effect_set_code") return schema({{"name", string}, {"metadataError", string}, {"note", string}}, {"name", "note"});
    if (id == "shaderixel.shader_errors") return schema({{"failed", array}}, {"failed"});
    if (id == "shaderixel.image_import") return schema({{"activeLayer", integer}}, J::array());
    if (id == "shaderixel.paint_stroke") return schema({{"dabs", integer}}, {"dabs"});
    if (id == "shaderixel.graph_info")
        return schema({{"mode", string}, {"output", integer}, {"nodes", array}, {"note", string}}, {"mode", "output", "nodes"});
    if (id == "shaderixel.node_add" || id == "shaderixel.node_update")
        return schema({{"id", integer}, {"kind", string}, {"name", string}, {"pos", array}, {"inputs", array},
                       {"warning", string}}, {"id", "kind", "name", "pos", "inputs"});
    if (id == "shaderixel.effect_graph_ops") return schema({{"ops", array}, {"example", object}}, {"ops", "example"});
    if (id == "shaderixel.effect_graph_get") return schema({{"graph", object}, {"hlsl", string}, {"error", string}}, {"graph", "hlsl"});
    if (id == "shaderixel.effect_graph_set") return schema({{"hlsl", string}}, {"hlsl"});
    if (id == "shaderixel.effect_from_hlsl")
        return schema({{"name", string}, {"inputs", array}, {"outputs", array}, {"parameters", array},
                       {"saved", string}, {"created", string}}, {"name", "inputs", "outputs", "parameters"});
    if (id == "shaderixel.path_from_selection")
        return schema({{"id", integer}, {"name", string}, {"anchors", integer}, {"subpaths", integer}},
                      {"id", "name", "anchors", "subpaths"});
    return object;
}

}  // namespace

struct AiBridge::Sdk {
    explicit Sdk(AiBridge& owner) : host(owner),
        registry(std::make_shared<cymcp::Registry>(cymcp::Policy::allow_all())),
        resources(std::make_shared<cymcp::ResourceRegistry>(cymcp::Policy::allow_all())) {}

    AiBridge& host;
    std::shared_ptr<cymcp::Registry> registry;
    std::shared_ptr<cymcp::ResourceRegistry> resources;
    std::shared_ptr<cymcp::ChangeFeed> changes;
    std::shared_ptr<cymcp::PerformanceMonitor> performance;
    std::unique_ptr<cymcp::McpServer> mcp;
    std::unique_ptr<cymcp::LocalHttpServer> http;
    fs::path state, connection;
    std::string token, url;
    bool cacheDiscovery = true;
    cymcp::LocalAccessSettings access = cymcp::local_access_settings(LocalAccessProfile::free);

    cymcp::Json Invoke(const std::string& name, const cymcp::Json& arguments, const cymcp::CallContext& ctx) {
        auto cancellation = ctx.cancellation.child();
        cancellation.deadline = std::min(cancellation.deadline, cymcp::Clock::now() + 120s);
        auto promise = std::make_shared<std::promise<AiResult>>();
        auto future = promise->get_future();
        Call call;
        call.op = name;
        call.params = FromSdk(arguments);
        std::shared_ptr<RenderWorkspace> render;
        if (name == "shaderixel.render" || name == "shaderixel.ui_capture") {
            if (state.empty()) throw cymcp::Error("FileError", "Private result directory is unavailable");
            render = std::make_shared<RenderWorkspace>();
            render->directory = state / "work" / cymcp::random_token(16);
            cymcp::private_directory(render->directory);
            call.params["file"] = "render.png";
            call.workspace = cymcp::path_utf8(render->directory);
        }
        call.cancelled = [cancellation] {
            return cancellation.cancelled() || cymcp::Clock::now() >= cancellation.deadline;
        };
        call.finish = [promise, keepWorkspace = render](const AiResult& done) {
            AiResult result;
            result.result = done.result;
            result.errorCode = done.errorCode;
            result.errorMessage = done.errorMessage;
            promise->set_value(std::move(result));
        };
        if (!host.Queue(std::move(call))) throw cymcp::Error("Busy", "Shaderixel is closing or its operation queue is full");
        while (future.wait_for(20ms) != std::future_status::ready) cancellation.check();
        const auto reply = future.get();
        if (!reply.errorCode.empty()) throw cymcp::Error(reply.errorCode, reply.errorMessage);
        if (!render) return ToSdk(reply.result.IsObject() ? reply.result : Json::Object());
        cancellation.check();
        auto storageContext = ctx;
        storageContext.cancellation = cancellation;
        auto reference = resources->store_stream([&](std::ostream& sink, const cymcp::CallContext& c) {
            std::ifstream file(render->directory / "render.png", std::ios::binary);
            if (!file) throw cymcp::Error("FileError", "Shaderixel could not read its PNG export");
            char bytes[64 * 1024];
            while (file.read(bytes, sizeof(bytes)) || file.gcount() > 0) {
                c.cancellation.check();
                sink.write(bytes, file.gcount());
            }
            if (!file.eof()) throw cymcp::Error("FileError", "Shaderixel could not read its PNG export");
        }, "image/png", storageContext, true);
        return {{"width", reply.result["width"].I()}, {"height", reply.result["height"].I()}, {"reference", reference}};
    }
};

std::span<const McpAccessOption> McpAccessOptions() { return accessOptions; }

AiSettings::AiSettings() : mcpProfile(LocalAccessProfile::free) {}

Json AiSettings::ToJson() const {
    Json j = Json::Object();
    j["python"] = python;
    j["autoStart"] = autoStart;
    for (const auto& option : McpAccessOptions())
        if (option.profile == mcpProfile) j["mcpProfile"] = option.id;
    j["allowWrites"] = allowWrites;
    j["allowFiles"] = allowFiles;
    j["compactTools"] = compactTools;
    j["spoolResults"] = spoolResults;
    j["cacheDiscovery"] = cacheDiscovery;
    j["notifyChanges"] = notifyChanges;
    j["measureCalls"] = measureCalls;
    j["keepChatHistory"] = keepChatHistory;
    j["contextKiB"] = contextKiB;
    j["imageProvider"] = imageProvider;
    j["openaiEndpoint"] = openaiEndpoint;
    j["openaiModel"] = openaiModel;
    j["openaiKeyEnv"] = openaiKeyEnv;
    j["geminiEndpoint"] = geminiEndpoint;
    j["geminiModel"] = geminiModel;
    j["geminiKeyEnv"] = geminiKeyEnv;
    j["comfyEndpoint"] = comfyEndpoint;
    j["comfyWorkflow"] = comfyWorkflow;
    j["comfyCheckpoint"] = comfyCheckpoint;
    j["comfyNegative"] = comfyNegative;
    j["comfySteps"] = comfySteps;
    j["comfyDenoise"] = comfyDenoise;
    j["assistantConnector"] = assistantConnector;
    j["assistantModel"] = assistantModel;
    j["assistantEndpoint"] = assistantEndpoint;
    j["assistantKeyEnv"] = assistantKeyEnv;
    return j;
}

void AiSettings::FromJson(const Json& j) {
    const AiSettings d;
    python = j["python"].S(d.python);
    autoStart = j["autoStart"].B(d.autoStart);
    mcpProfile = d.mcpProfile;
    if (j.Has("mcpProfile")) {
        mcpProfile = LocalAccessProfile::protected_access;
        const auto id = j["mcpProfile"].S();
        for (const auto& option : McpAccessOptions())
            if (id == option.id) mcpProfile = option.profile;
    }
    allowWrites = j["allowWrites"].B(d.allowWrites);
    allowFiles = j["allowFiles"].B(d.allowFiles);
    compactTools = j["compactTools"].B(d.compactTools);
    spoolResults = j["spoolResults"].B(d.spoolResults);
    cacheDiscovery = j["cacheDiscovery"].B(d.cacheDiscovery);
    notifyChanges = j["notifyChanges"].B(d.notifyChanges);
    measureCalls = j["measureCalls"].B(d.measureCalls);
    keepChatHistory = j["keepChatHistory"].B(d.keepChatHistory);
    contextKiB = std::clamp(j["contextKiB"].I(d.contextKiB), 0, 1024);
    imageProvider = j["imageProvider"].S(d.imageProvider);
    openaiEndpoint = j["openaiEndpoint"].S(d.openaiEndpoint);
    openaiModel = j["openaiModel"].S(d.openaiModel);
    openaiKeyEnv = j["openaiKeyEnv"].S(d.openaiKeyEnv);
    geminiEndpoint = j["geminiEndpoint"].S(d.geminiEndpoint);
    geminiModel = j["geminiModel"].S(d.geminiModel);
    geminiKeyEnv = j["geminiKeyEnv"].S(d.geminiKeyEnv);
    comfyEndpoint = j["comfyEndpoint"].S(d.comfyEndpoint);
    comfyWorkflow = j["comfyWorkflow"].S(d.comfyWorkflow);
    comfyCheckpoint = j["comfyCheckpoint"].S(d.comfyCheckpoint);
    comfyNegative = j["comfyNegative"].S(d.comfyNegative);
    comfySteps = j["comfySteps"].I(d.comfySteps);
    comfyDenoise = j["comfyDenoise"].F(d.comfyDenoise);
    assistantConnector = j["assistantConnector"].S(d.assistantConnector);
    assistantModel = j["assistantModel"].S(d.assistantModel);
    assistantEndpoint = j["assistantEndpoint"].S(d.assistantEndpoint);
    assistantKeyEnv = j["assistantKeyEnv"].S(d.assistantKeyEnv);
}

AiBridge::AiBridge() : sdk(std::make_unique<Sdk>(*this)) {}
AiBridge::~AiBridge() { Shutdown(); }

void AiBridge::Configure(const AiSettings& settings) {
    sdk->access = cymcp::local_access_settings(settings.mcpProfile);
    allowWrites = settings.allowWrites;
    allowFiles = settings.allowFiles;
    sdk->cacheDiscovery = settings.cacheDiscovery;
    cymcp::ResourceOptions storage;
    storage.max_disk_bytes = 512u * 1024 * 1024;
    storage.result_ttl = 15min;
    try {
        sdk->state = mcplocal::StateDirectory();
        cymcp::private_directory(sdk->state);
        if (settings.spoolResults)
            storage.disk_directory = sdk->state / ("results-" + std::to_string(GetCurrentProcessId()));
        if (settings.measureCalls) {
            cymcp::PerformanceOptions options;
            options.jsonl_file = sdk->state / ("performance-" + std::to_string(GetCurrentProcessId()) + ".jsonl");
            sdk->performance = std::make_shared<cymcp::PerformanceMonitor>(options);
            sdk->registry->set_performance_monitor(sdk->performance);
        }
        sdk->resources = std::make_shared<cymcp::ResourceRegistry>(cymcp::Policy::allow_all(), storage);
    } catch (const std::exception& e) {
        Log("MCP state: %s", e.what());
        sdk->state.clear();
    }
    if (settings.notifyChanges) {
        sdk->changes = std::make_shared<cymcp::ChangeFeed>();
        sdk->registry->set_change_feed(sdk->changes);
        sdk->resources->set_change_feed(sdk->changes);
    }
    auto help = std::make_shared<const std::string>(
        "Shaderixel: use document_info to inspect the active document. List effects before adding a filter. "
        "Controlled access profiles apply the write and file permissions from the Assistant panel. "
        "Save explicitly. Editing tools never save automatically. "
        "The render tool returns a private PNG resource; use its manifest to read all chunks.");
    cymcp::Resource guide;
    guide.uri = "shaderixel://assistant/guide";
    guide.name = "Shaderixel operation guide";
    guide.description = "Document, effect, saving and render guidance";
    guide.size = help->size();
    guide.cache_ttl = sdk->cacheDiscovery ? 30s : 0s;
    guide.read = [help](size_t offset, size_t length, const cymcp::CallContext&) {
        return offset >= help->size() ? std::string{} : help->substr(offset, length);
    };
    sdk->resources->add(std::move(guide));
    using J = cymcp::Json;
    sdk->registry->add({"shaderixel.resource_read", "Read a Shaderixel resource or one chunk of a retained result by URI",
        {{"type", "object"}, {"properties", {{"uri", {{"type", "string"}}}}}, {"required", {"uri"}}},
        {{"type", "object"}}, true, [resources = sdk->resources](const J& parameters, const cymcp::CallContext& context) {
            return resources->read(parameters.at("uri").get<std::string>(), context);
        }});
    sdk->registry->add({"shaderixel.resource_grep", "Search text in the registered Shaderixel guide. Use the returned cursor for another page.",
        {{"type", "object"}, {"properties", {{"text", {{"type", "string"}}}, {"cursor", {{"type", "string"}}}}}, {"required", {"text"}}},
        {{"type", "object"}}, true, [resources = sdk->resources](const J& parameters, const cymcp::CallContext& context) {
            return resources->grep(parameters.at("text").get<std::string>(), true, parameters.value("cursor", std::string()), context);
        }});
}

void AiBridge::Register(AiOperation op) {
    if (!op.inputSchema.IsObject()) op.inputSchema = Json::Object();
    if (!op.inputSchema.Has("type")) op.inputSchema["type"] = "object";
    if (!op.inputSchema.Has("properties")) op.inputSchema["properties"] = Json::Object();
    const std::string id = op.id;
    try {
        sdk->registry->add({id, op.description, ToSdk(op.inputSchema),
            op.outputSchema.IsObject() ? ToSdk(op.outputSchema) : OutputSchema(id),
            !op.write, [this, id](const cymcp::Json& args, const cymcp::CallContext& c) { return sdk->Invoke(id, args, c); }});
    } catch (const std::exception& e) {
        // An older native plugin can still run locally. Invalid schemas must not enter MCP.
        Log("MCP registration %s: %s", id.c_str(), e.what());
    }
    std::lock_guard lock(opsMutex);
    ops.push_back(std::move(op));
}

void AiBridge::Unregister(const std::string& id) {
    std::lock_guard lock(opsMutex);
    const auto it = std::find_if(ops.begin(), ops.end(), [&](const AiOperation& op) { return op.id == id; });
    if (it == ops.end()) return;
    ops.erase(it);
    try { sdk->registry->remove(id); }
    catch (const cymcp::Error&) { return; }  // Invalid legacy plugin schemas were never added.
}

std::shared_ptr<const cymcp::Registry> AiBridge::ChatTools(bool compact) const {
    if (!compact) return sdk->registry;
    return cymcp::ToolGateway(sdk->registry, {}, sdk->resources).tools();
}

std::shared_ptr<cymcp::PerformanceMonitor> AiBridge::Performance() const { return sdk->performance; }

bool AiBridge::Start(const std::string&) {
    if (server.Running()) return true;
    pluginToken = net::RandomToken(32);
    const int base = 39500 + static_cast<int>(GetCurrentProcessId() % 500);
    return !pluginToken.empty() &&
           server.Start(base, 41000, [this](const net::Request& r, std::string& out) { return Handle(r, out); });
}

bool AiBridge::ControlsOperations() const { return sdk->access.control_operations; }

bool AiBridge::SetMcpProfile(LocalAccessProfile profile, std::string& error) {
    const auto access = cymcp::local_access_settings(profile);
    if (sdk->access == access) return true;
    const bool restart = sdk->http != nullptr;
    if (restart) StopMcp();
    sdk->access = access;
    return !restart || StartMcp(error);
}

bool AiBridge::StartMcp(std::string& error) {
    if (sdk->http) return true;
    try {
        if (sdk->state.empty()) throw cymcp::Error("FileError", "Private MCP state directory is unavailable");
        cymcp::McpServerOptions options;
        options.resources = sdk->resources;
        options.changes = sdk->changes;
        options.performance = sdk->performance;
        options.cache_ttl = sdk->cacheDiscovery ? 30s : 0s;
        sdk->mcp = std::make_unique<cymcp::McpServer>(*sdk->registry, "Shaderixel", options);
        const auto endpoint = [this](const cymcp::Json& request, const cymcp::CallContext& context) -> cymcp::Json {
            if (request.contains("jsonrpc")) return sdk->mcp->handle(request, context);
            const auto method = request.value("method", std::string());
            if (method == "describe" && request.contains("name"))
                return sdk->registry->describe(request.at("name").get<std::string>());
            if (method == "read") return sdk->resources->read(request.at("uri").get<std::string>(), context);
            if (method == "resources") return sdk->resources->list();
            if (method == "grep")
                return sdk->resources->grep(request.at("text").get<std::string>(), request.value("ignoreCase", false),
                                            request.value("cursor", std::string()), context);
            return cymcp::dispatch_app(*sdk->registry, request, context);
        };
        if (sdk->access.require_token) {
            sdk->token = cymcp::random_token();
            sdk->http = std::make_unique<cymcp::LocalHttpServer>(sdk->token, endpoint);
        } else {
            sdk->http = std::make_unique<cymcp::LocalHttpServer>(
                cymcp::LocalHttpServer::UnauthenticatedLoopback{}, endpoint);
        }
        const int port = sdk->http->start();
        sdk->url = "http://127.0.0.1:" + std::to_string(port);
        const DWORD pid = GetCurrentProcessId();
        sdk->connection = mcplocal::ConnectionPath(pid);
        cymcp::Json descriptor{
            {"pid", pid}, {"url", sdk->url}, {"host", "Shaderixel"},
            {"sdkVersion", cymcp::version}
        };
        if (!sdk->token.empty()) descriptor["token"] = sdk->token;
        const auto data = descriptor.dump();
        auto connection = cymcp::open_private_output_file(sdk->connection);
        if (std::fwrite(data.data(), 1, data.size(), connection.get()) != data.size() ||
            std::fflush(connection.get()) != 0)
            throw cymcp::Error("FileError", "Could not write the private instance descriptor");
        Log("Shaderixel MCP listening on 127.0.0.1:%d", port);
        return true;
    } catch (const std::exception& e) {
        error = e.what();
        StopMcp();
        return false;
    }
}

void AiBridge::StopMcp() {
    if (sdk->mcp) sdk->mcp->cancel_all();
    sdk->http.reset();
    sdk->mcp.reset();
    if (!sdk->connection.empty()) {
        std::error_code ignored;
        fs::remove(sdk->connection, ignored);
        sdk->connection.clear();
    }
    sdk->url.clear();
    sdk->token.clear();
}

std::string AiBridge::McpUrl() const { return sdk->url.empty() ? "" : sdk->url + "/mcp"; }

std::string AiBridge::McpConfiguration() const {
    if (sdk->url.empty()) return {};
    cymcp::Json configuration{{"type", "http"}, {"url", McpUrl()}};
    if (!sdk->token.empty()) configuration["headers"]["Authorization"] = "Bearer " + sdk->token;
    return cymcp::Json{{"mcpServers", {{"shaderixel", std::move(configuration)}}}}.dump(2);
}

void AiBridge::Shutdown() {
    std::deque<Call> waiting;
    {
        std::lock_guard lock(mutex);
        if (closing) return;
        closing = true;
        waiting.swap(queue);
    }
    const auto closed = AiResult::Fail("EditorClosed", "Shaderixel s'est fermé");
    for (auto& c : waiting) c.finish(closed);
    for (auto& p : pending) p.call.finish(closed);
    pending.clear();
    StopMcp();
    server.Stop();
    std::lock_guard lock(mutex);
    calls.clear();
}

std::string AiBridge::Describe() const {
    Json result = Json::Object();
    result["schemaVersion"] = 1;
    result["protocolVersion"] = "1.0";
    result["id"] = "Shaderixel";
    result["name"] = "Shaderixel";
    result["version"] = SX_APP_VERSION;
    result["operations"] = Json::Array();
    std::lock_guard lock(opsMutex);
    for (const auto& op : ops) {
        Json o = Json::Object();
        o["id"] = o["name"] = op.id;
        o["description"] = op.description;
        o["inputSchema"] = op.inputSchema;
        o["outputSchema"]["type"] = "object";
        result["operations"].Push(std::move(o));
    }
    return result.Dump(0);
}

int AiBridge::Handle(const net::Request& req, std::string& out) {
    const auto auth = req.headers.find("authorization");
    if (req.headers.count("origin") || auth == req.headers.end() ||
        !cymcp::constant_time_equal(auth->second, "Bearer " + pluginToken)) return 403;
    if (req.method != "POST" || req.path != "/shaderixel/v1") return 404;
    Json request;
    if (!Json::Parse(req.body, request) || !request.IsObject()) {
        out = ErrorJson("InvalidRequest", "JSON invalide").Dump(0);
        return 400;
    }
    const auto method = request["method"].S();
    if (method == "describe") {
        out = Describe();
        return 200;
    }
    const auto id = request["callId"].S();
    if (method == "ack") {
        std::lock_guard lock(mutex);
        // Acknowledgements only release completed requests.
        if (auto it = calls.find(id); it != calls.end() && it->second.wait_for(0s) == std::future_status::ready)
            calls.erase(it);
        out = "{\"acknowledged\":true}";
        return 200;
    }
    if (method != "invoke" || id.empty() || id.size() > 128) {
        out = ErrorJson("InvalidRequest", "Méthode ou identifiant invalide").Dump(0);
        return 400;
    }
    std::shared_future<std::string> future;
    {
        std::lock_guard lock(mutex);
        if (closing) return 503;
        if (const auto it = calls.find(id); it != calls.end()) {
            future = it->second;
        } else {
            if (calls.size() >= 1024 || queue.size() >= 128) return 503;
            auto promise = std::make_shared<std::promise<std::string>>();
            future = promise->get_future().share();
            Call call;
            call.op = request["operation"].S();
            call.params = request["parameters"];
            call.workspace = request["workspace"].S();
            call.trusted = true;
            call.finish = [promise](const AiResult& r) {
                Json response = ErrorJson(r.errorCode, r.errorMessage);
                if (r.errorCode.empty()) {
                    response = Json::Object();
                    response["result"] = r.result.IsObject() ? r.result : Json::Object();
                }
                promise->set_value(response.Dump(0));
            };
            calls.emplace(id, future);
            queue.push_back(std::move(call));
        }
    }
    if (future.wait_for(4s) != std::future_status::ready) return 0;
    out = future.get();
    return 200;
}

bool AiBridge::Queue(Call call) {
    std::lock_guard lock(mutex);
    if (closing || queue.size() >= 128) return false;
    queue.push_back(std::move(call));
    return true;
}

void AiBridge::Finish(const Call& call, const AiResult& result) {
    call.finish(result);
}

void AiBridge::Pump() {
    std::deque<Call> work;
    {
        std::lock_guard lock(mutex);
        work.swap(queue);
    }
    const auto cancelled = AiResult::Fail("Cancelled", "Opération annulée");
    for (auto& call : work) {
        if (call.cancelled && call.cancelled()) { Finish(call, cancelled); continue; }
        const auto* op = Find(call.op);
        AiResult result;
        try {
            if (!op) result = AiResult::Fail("UnknownOperation", "Opération inconnue");
            else if (!call.trusted && op->write && !CanWrite())
                result = AiResult::Fail("AccessDenied", "Autorisez les modifications dans le panneau Assistant IA");
            else if (!call.trusted && !CanAccessFiles() && (op->fileAccess || op->id == "shaderixel.document_save" ||
                     std::ranges::any_of(op->inputFiles, [&](const auto& name) { return !call.params[name].S().empty(); }) ||
                     (op->id == "shaderixel.effect_from_hlsl" && call.params["target"].S() == "library")))
                result = AiResult::Fail("AccessDenied", "Autorisez l’accès aux fichiers dans le panneau Assistant IA");
            else if (!call.trusted && op->id == "shaderixel.document_save" && !Utf8Path(call.params["path"].S()).is_absolute())
                result = AiResult::Fail("InvalidParameters", "Un chemin absolu est requis pour la sauvegarde");
            else if (!call.trusted && op->id == "shaderixel.document_save" && !call.params["overwrite"].B(false) &&
                     fs::exists(Utf8Path(call.params["path"].S())))
                result = AiResult::Fail("ConfirmationRequired", "Le fichier existe. Confirmez son remplacement avec overwrite.");
            else result = op->run(call.params, call.workspace);
        } catch (const std::exception& e) {
            result = AiResult::Fail("InternalError", e.what());
        }
        if (result.done || !result.poll) Finish(call, result);
        else pending.push_back({std::move(call), std::move(result)});
    }
    for (auto it = pending.begin(); it != pending.end();) {
        if (it->call.cancelled && it->call.cancelled()) {
            Finish(it->call, cancelled);
        } else {
            try {
                if (!it->result.poll(it->result)) { ++it; continue; }
                Finish(it->call, it->result);
            } catch (const std::exception& e) {
                Finish(it->call, AiResult::Fail("InternalError", e.what()));
            }
        }
        it = pending.erase(it);
    }
}

}  // namespace sx
