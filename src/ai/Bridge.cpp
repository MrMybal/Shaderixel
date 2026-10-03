#include "ai/Bridge.h"

#include <windows.h>
#include <shlobj.h>

#include <chrono>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace sx {

namespace {
double Now() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

std::string AppDataDir() {
    PWSTR p = nullptr;
    std::string out;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &p))) out = Narrow(p) + "\\Shaderixel\\Assistant";
    CoTaskMemFree(p);
    return out;
}

Json ErrorJson(const std::string& code, const std::string& msg) {
    Json e = Json::Object();
    e["code"] = code;
    e["message"] = msg;
    Json j = Json::Object();
    j["error"] = e;
    return j;
}
}  // namespace

Json AiSettings::ToJson() const {
    Json j = Json::Object();
    j["python"] = python;
    j["companionScript"] = companionScript;
    j["autoStart"] = autoStart;
    j["allowWrites"] = allowWrites;
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
    return j;
}

void AiSettings::FromJson(const Json& j) {
    AiSettings d;
    python = j["python"].S(d.python);
    companionScript = j["companionScript"].S(d.companionScript);
    autoStart = j["autoStart"].B(d.autoStart);
    allowWrites = j["allowWrites"].B(d.allowWrites);
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
}

bool AiBridge::Start(const std::string& projectHint) {
    if (server.Running()) return true;
    token = net::RandomToken(32);
    project = projectHint;
    int base = 39500 + (int)(GetCurrentProcessId() % 500);
    if (!server.Start(base, 41000, [this](const net::Request& r, std::string& out) { return Handle(r, out); })) return false;
    std::string dir = AppDataDir();
    std::error_code ec;
    fs::create_directories(Utf8Path(dir), ec);
    connectionFile = dir + "\\shaderixel.connection.json";
    serviceFile = dir + "\\shaderixel.connection.service.json";
    fs::remove(Utf8Path(serviceFile), ec);
    Json c = Json::Object();
    c["url"] = "http://127.0.0.1:" + std::to_string(server.Port()) + "/shaderixel/v1";
    c["token"] = token;
    c["pid"] = (u32)GetCurrentProcessId();
    c["project"] = projectHint;
    c["host"] = "Shaderixel";
    std::ofstream(Utf8Path(connectionFile), std::ios::binary) << c.Dump(1);
    Log("AI bridge listening on 127.0.0.1:%d", server.Port());
    return true;
}

void AiBridge::Shutdown() {
    StopCompanion();
    server.Stop();
    std::error_code ec;
    if (!connectionFile.empty()) fs::remove(Utf8Path(connectionFile), ec);
    if (!serviceFile.empty()) fs::remove(Utf8Path(serviceFile), ec);
    std::lock_guard lock(mutex);
    for (auto& c : queue) c.promise->set_value(ErrorJson("EditorClosed", "Shaderixel s'est fermé").Dump(0));
    queue.clear();
}

std::string AiBridge::Describe() const {
    Json j = Json::Object();
    j["schemaVersion"] = 1;
    j["protocolVersion"] = "1.0";
    j["id"] = "Shaderixel";
    j["name"] = "Shaderixel MCP";
    j["version"] = "0.1.0";
    j["description"] = "Shaderixel: GPU image editor operations (documents, layers, effects, selections, rendering).";
    Json platform = Json::Object();
    platform["os"] = "Windows";
    platform["architecture"] = "x64";
    platform["status"] = "Experimental";
    Json platforms = Json::Array();
    platforms.Push(platform);
    j["platforms"] = platforms;
    Json conc = Json::Object();
    conc["policy"] = "Concurrent";
    conc["maxWorkers"] = 4;
    j["concurrency"] = conc;
    Json arr = Json::Array();
    for (auto& op : ops) {
        Json o = Json::Object();
        o["id"] = op.id;
        o["name"] = op.id;
        o["description"] = op.description + (op.write ? " (modifie le document ; nécessite l'autorisation d'écriture dans Shaderixel)" : "");
        Json schema = op.inputSchema.IsObject() ? op.inputSchema : Json::Object();
        if (!schema.Has("type")) schema["type"] = "object";
        if (!schema.Has("properties")) schema["properties"] = Json::Object();
        o["inputSchema"] = schema;
        Json out = Json::Object();
        out["type"] = "object";
        o["outputSchema"] = out;
        Json meta = Json::Object();
        meta["modifiesProject"] = op.write;
        meta["undoSupported"] = true;
        meta["savesToDisk"] = false;
        Json files = Json::Array();
        for (auto& f : op.inputFiles) files.Push(f);
        meta["inputFiles"] = files;
        o["x-cy-unreal"] = meta;  // field name expected by the CyUEMCPSDK companion
        o["canCancel"] = false;
        o["canRunAsync"] = true;
        o["supportsProgress"] = false;
        o["supportedBackends"] = Json::Array();
        Json perms = Json::Array();
        perms.Push(op.write ? "ue.write" : "ue.read");
        o["permissions"] = perms;
        arr.Push(o);
    }
    j["operations"] = arr;
    return j.Dump(0);
}

int AiBridge::Handle(const net::Request& req, std::string& out) {
    if (req.headers.count("origin")) return 403;  // never callable from a browser page
    auto auth = req.headers.find("authorization");
    if (auth == req.headers.end() || auth->second != "Bearer " + token) return 403;
    if (req.method != "POST" || req.path != "/shaderixel/v1") return 404;
    Json j;
    if (!Json::Parse(req.body, j) || !j.IsObject()) {
        out = ErrorJson("InvalidRequest", "JSON invalide").Dump(0);
        return 400;
    }
    std::string method = j["method"].S("");
    if (method == "describe") {
        out = Describe();
        return 200;
    }
    std::string callId = j["callId"].S("");
    if (method == "ack") {
        std::lock_guard lock(mutex);
        calls.erase(callId);
        out = "{\"acknowledged\":true}";
        return 200;
    }
    if (method != "invoke" || callId.empty()) {
        out = ErrorJson("InvalidRequest", "méthode inconnue").Dump(0);
        return 400;
    }
    std::shared_future<std::string> fut;
    {
        std::lock_guard lock(mutex);
        auto it = calls.find(callId);
        if (it != calls.end()) {
            fut = it->second;  // retry of an existing call: same result
        } else {
            if (calls.size() > 1024) return 503;
            Call c;
            c.callId = callId;
            c.op = j["operation"].S("");
            c.params = j["parameters"];
            c.workspace = j["workspace"].S("");
            c.promise = std::make_shared<std::promise<std::string>>();
            fut = c.promise->get_future().share();
            calls[callId] = fut;
            queue.push_back(std::move(c));
        }
    }
    // The companion times out after 5 s and retries with the same callId.
    if (fut.wait_for(std::chrono::milliseconds(4000)) != std::future_status::ready) return 0;
    out = fut.get();
    return 200;
}

std::string AiBridge::Finish(const Call& c, const AiResult& r) {
    ++callsServed;
    if (!r.errorCode.empty()) return ErrorJson(r.errorCode, r.errorMessage).Dump(0);
    Json j = Json::Object();
    j["result"] = r.result.IsObject() ? r.result : Json::Object();
    return j.Dump(0);
}

void AiBridge::Pump() {
    std::deque<Call> work;
    {
        std::lock_guard lock(mutex);
        work.swap(queue);
    }
    for (auto& c : work) {
        const AiOperation* op = nullptr;
        for (auto& o : ops) if (o.id == c.op) op = &o;
        AiResult r;
        if (!op) r = AiResult::Fail("UnknownOperation", "Opération inconnue : " + c.op);
        else if (op->write && !allowWrites) r = AiResult::Fail("AccessDenied", "L'écriture par l'IA est désactivée dans Shaderixel (panneau Assistant IA).");
        else {
            try {
                r = op->run(c.params, c.workspace);
            } catch (const std::exception& e) {
                r = AiResult::Fail("InternalError", e.what());
            }
        }
        lastOperation = c.op;
        if (r.done || !r.poll) c.promise->set_value(Finish(c, r));
        else pending.push_back({c, r});
    }
    for (size_t i = 0; i < pending.size();) {
        Pending& p = pending[i];
        if (p.result.poll(p.result)) {
            p.call.promise->set_value(Finish(p.call, p.result));
            pending.erase(pending.begin() + i);
        } else {
            ++i;
        }
    }
    // Companion service endpoint discovery.
    if (companion && serviceUrl.empty() && Now() - lastServiceCheck > 0.25) {
        lastServiceCheck = Now();
        std::string text;
        Json s;
        if (ReadTextFile(serviceFile, text) && Json::Parse(text, s) && s["url"].S("").rfind("http://127.0.0.1:", 0) == 0) {
            serviceUrl = s["url"].S();
            serviceToken = s["token"].S();
            Log("AI companion ready at %s", serviceUrl.c_str());
        }
    }
}

bool AiBridge::LaunchCompanion(const AiSettings& s, std::string* error) {
    if (CompanionRunning()) return true;
    if (!server.Running()) { if (error) *error = "pont non démarré"; return false; }
    if (s.companionScript.empty() || !fs::exists(Utf8Path(s.companionScript))) {
        if (error) *error = "script compagnon introuvable (CyUEMCPSDK/Bridge/run.py)";
        return false;
    }
    serviceUrl.clear();
    std::wstring cmd = L"\"" + Widen(s.python) + L"\" -B \"" + Widen(s.companionScript) + L"\" --editor \"" + Widen(connectionFile) + L"\"";
    std::wstring cwd = Utf8Path(s.companionScript).parent_path().wstring();
    STARTUPINFOW si = {sizeof(si)};
    PROCESS_INFORMATION pi = {};
    std::vector<wchar_t> buf(cmd.begin(), cmd.end());
    buf.push_back(0);
    if (!CreateProcessW(nullptr, buf.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, cwd.c_str(), &si, &pi)) {
        if (error) *error = "impossible de lancer Python (" + std::to_string(GetLastError()) + ")";
        return false;
    }
    CloseHandle(pi.hThread);
    companion = pi.hProcess;
    Log("AI companion started (pid %lu)", pi.dwProcessId);
    return true;
}

bool AiBridge::CompanionRunning() const {
    if (!companion) return false;
    DWORD code = 0;
    return GetExitCodeProcess((HANDLE)companion, &code) && code == STILL_ACTIVE;
}

void AiBridge::StopCompanion() {
    if (!companion) return;
    // The companion exits when the connection file disappears; give it a moment, then force.
    std::error_code ec;
    fs::remove(Utf8Path(connectionFile), ec);
    if (WaitForSingleObject((HANDLE)companion, 3000) == WAIT_TIMEOUT) TerminateProcess((HANDLE)companion, 0);
    CloseHandle((HANDLE)companion);
    companion = nullptr;
    serviceUrl.clear();
    serviceToken.clear();
    // Recreate the connection file so a later restart (or external MCP clients) can reconnect.
    if (server.Running()) {
        Json c = Json::Object();
        c["url"] = "http://127.0.0.1:" + std::to_string(server.Port()) + "/shaderixel/v1";
        c["token"] = token;
        c["pid"] = (u32)GetCurrentProcessId();
        c["project"] = project;
        std::ofstream(Utf8Path(connectionFile), std::ios::binary) << c.Dump(1);
    }
}

}  // namespace sx
