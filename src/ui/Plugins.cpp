// Plugins (sdk/include/shaderixel_plugin.h): native DLLs and script plugins found in
// <exe>/plugins/<Name>/ and %LOCALAPPDATA%/Shaderixel/Plugins/<Name>/ (plugin.json each).
// Native plugins add menu commands, dockable windows, canvas tools and operations through
// the C host table below; script plugins add menu commands running a program that drives
// Shaderixel through the loopback bridge (SHADERIXEL_URL / SHADERIXEL_TOKEN).
#include "ui/App.h"
#include "ui/Theme.h"

#include <ShlObj.h>
#include <shellapi.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <thread>

namespace sx {

namespace fs = std::filesystem;

namespace {
App* gApp = nullptr;
std::string gReturn;  // strings returned to plugins (valid until the next host call)

App& A() { return *gApp; }

// ---------------------------------------------------------------- UI table (ImGui underneath)
void UiText(const char* t) { ImGui::TextUnformatted(t ? t : ""); }
void UiTextDisabled(const char* t) { ImGui::TextDisabled("%s", t ? t : ""); }
void UiTextWrapped(const char* t) { ImGui::TextWrapped("%s", t ? t : ""); }
int UiButton(const char* l) { return ImGui::Button(l ? l : "?") ? 1 : 0; }
int UiCheckbox(const char* l, int* v) {
    bool b = v && *v;
    if (!ImGui::Checkbox(l ? l : "?", &b)) return 0;
    if (v) *v = b ? 1 : 0;
    return 1;
}
int UiSliderFloat(const char* l, float* v, float lo, float hi, const char* f) { return v && ImGui::SliderFloat(l ? l : "?", v, lo, hi, f ? f : "%.3f") ? 1 : 0; }
int UiSliderInt(const char* l, int* v, int lo, int hi) { return v && ImGui::SliderInt(l ? l : "?", v, lo, hi) ? 1 : 0; }
int UiInputText(const char* l, char* buf, int size) { return buf && size > 0 && ImGui::InputText(l ? l : "?", buf, (size_t)size) ? 1 : 0; }
int UiColorEdit(const char* l, float rgba[4]) { return rgba && ImGui::ColorEdit4(l ? l : "?", rgba, ImGuiColorEditFlags_NoInputs) ? 1 : 0; }
int UiCombo(const char* l, int* cur, const char* items) { return cur && items && ImGui::Combo(l ? l : "?", cur, items) ? 1 : 0; }
void UiSameLine() { ImGui::SameLine(); }
void UiSeparator(const char* title) {
    if (title && *title) ImGui::SeparatorText(title);
    else ImGui::Separator();
}
void UiTooltip(const char* t) {
    if (t && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", t);
}
void UiPushId(int id) { ImGui::PushID(id); }
void UiPopId() { ImGui::PopID(); }
int UiCollapsingHeader(const char* l) { return ImGui::CollapsingHeader(l ? l : "?") ? 1 : 0; }
float UiContentWidth() { return ImGui::GetContentRegionAvail().x; }
void UiSetNextWidth(float w) { ImGui::SetNextItemWidth(w); }
int UiColorButton(const char* id, const float rgba[4], float size) {
    if (!rgba) return 0;
    const ImVec4 c(rgba[0], rgba[1], rgba[2], rgba[3]);
    const float s = size > 0 ? size : ImGui::GetFrameHeight();
    if (ImGui::ColorButton(id ? id : "##c", c, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_AlphaPreview, ImVec2(s, s))) return 1;
    return ImGui::IsItemClicked(ImGuiMouseButton_Right) ? 2 : 0;
}

const SxUi kUi = {UiText,   UiTextDisabled, UiTextWrapped, UiButton,  UiCheckbox, UiSliderFloat,      UiSliderInt,    UiInputText,    UiColorEdit,
                  UiCombo,  UiSameLine,     UiSeparator,   UiTooltip, UiPushId,   UiPopId,            UiCollapsingHeader, UiContentWidth, UiSetNextWidth,
                  UiColorButton};

}  // namespace

// ---------------------------------------------------------------- host table (friend of App)
struct PluginGlue {
    static void HLog(const char* m) { Log("[plugin %s] %s", A().pluginCurrent.c_str(), m ? m : ""); }
    static void HStatus(const char* m) { A().Status(m ? m : ""); }
    static void HAddCommand(const char* path, SxCommandFn fn, void* user) {
        if (path && fn) A().pluginCommands.push_back({A().pluginCurrent, path, fn, user});
    }
    static void HAddPanel(const char* title, SxPanelFn fn, void* user) {
        if (title && fn) A().pluginPanels.push_back({A().pluginCurrent, title, fn, user, false});
    }
    static void HAddTool(const char* name, const char* help, SxToolFn fn, void* user) {
        if (name && fn) A().pluginTools.push_back({A().pluginCurrent, name, help ? help : "", fn, user});
    }
    static void HAddOperation(const char* id, const char* desc, const char* schema, int writes, SxOperationFn fn, void* user) {
        if (!id || !fn) return;
        App& a = A();
        const std::string plugin = a.pluginCurrent;
        AiOperation op;
        op.id = id;
        op.description = desc ? desc : "";
        Json s;
        op.inputSchema = schema && Json::Parse(schema, s) ? s : Json::Object();
        op.write = writes != 0;
        op.run = [fn, user, plugin](const Json& params, const std::string&) {
            App& app = A();
            const std::string prev = app.pluginCurrent;
            app.pluginCurrent = plugin;
            const char* out = fn(user, params.Dump(0).c_str());
            app.pluginCurrent = prev;
            Json r;
            if (!out || !Json::Parse(out, r)) r = Json::Object();
            if (r.IsObject() && r.Has("error")) return AiResult::Fail("PluginError", r["error"].S("erreur du plugin"));
            return AiResult::Ok(r);
        };
        a.bridge.Unregister(id);
        a.bridge.Register(std::move(op));
        a.pluginOps.push_back({plugin, id});
    }
    static void HCall(const char* opId, const char* params, SxResultFn done, void* user) {
        App& a = A();
        auto fail = [&](const std::string& msg) {
            if (!done) return;
            Json e = Json::Object();
            e["error"] = msg;
            const std::string s = e.Dump(0);
            done(user, 0, s.c_str());
        };
        const AiOperation* op = opId ? a.bridge.Find(opId) : nullptr;
        if (!op) { fail(std::string("opération inconnue : ") + (opId ? opId : "")); return; }
        Json p;
        if (!params || !*params) p = Json::Object();
        else if (!Json::Parse(params, p)) { fail("paramètres JSON invalides"); return; }
        AiResult r;
        try {
            r = op->run(p, a.PluginWorkspace(a.pluginCurrent));
        } catch (const std::exception& e) {
            r = AiResult::Fail("InternalError", e.what());
        }
        if (!r.done && r.poll) {
            a.pluginCalls.push_back({a.pluginCurrent, std::move(r), done, user});  // finished in UpdatePlugins
            return;
        }
        a.FinishPluginCall(a.pluginCurrent, r, done, user);
    }
    static const char* HOperations() {
        Json list = Json::Array();
        for (auto& o : A().bridge.Operations()) {
            Json j = Json::Object();
            j["id"] = o.id;
            j["description"] = o.description;
            j["input"] = o.inputSchema;
            j["writes"] = o.write;
            list.Push(j);
        }
        gReturn = list.Dump(1);
        return gReturn.c_str();
    }
    static int HDocumentSize(int* w, int* h) {
        DocView* v = A().Active();
        if (!v) return 0;
        if (w) *w = (int)v->doc->Width();
        if (h) *h = (int)v->doc->Height();
        return 1;
    }
    static const char* HWorkspace() {
        gReturn = A().PluginWorkspace(A().pluginCurrent);
        return gReturn.c_str();
    }
    static void HOpenPanel(const char* title, int open) {
        for (auto& p : A().pluginPanels)
            if (title && p.title == title) p.open = open != 0;
    }
    static void HSelectTool(const char* name) {
        App& a = A();
        for (size_t i = 0; i < a.pluginTools.size(); ++i)
            if (name && a.pluginTools[i].name == name) {
                a.pluginTool = (int)i;
                a.tool = Tool::Plugin;
            }
    }
    static ImVec2 DocToScreen(float x, float y) {
        auto& c = A().pluginDraw;
        return ImVec2(c.canvasMin.x + c.pan.x + x * c.zoom, c.canvasMin.y + c.pan.y + y * c.zoom);
    }
    static void HDrawLine(float x0, float y0, float x1, float y1, unsigned col, float t) {
        if (auto* dl = A().pluginDraw.dl) dl->AddLine(DocToScreen(x0, y0), DocToScreen(x1, y1), col, t);
    }
    static void HDrawCircle(float x, float y, float r, unsigned col, float t) {
        if (auto* dl = A().pluginDraw.dl) dl->AddCircle(DocToScreen(x, y), r * A().pluginDraw.zoom, col, 48, t);
    }
    static void HDrawRect(float x0, float y0, float x1, float y1, unsigned col, float t) {
        if (auto* dl = A().pluginDraw.dl) dl->AddRect(DocToScreen(x0, y0), DocToScreen(x1, y1), col, 0, 0, t);
    }
    static void HDrawText(float x, float y, unsigned col, const char* text) {
        if (auto* dl = A().pluginDraw.dl) dl->AddText(DocToScreen(x, y), col, text ? text : "");
    }
    static void* HImGuiContext() { return ImGui::GetCurrentContext(); }
    static void HGetColor(int which, float rgba[4]) {
        if (!rgba) return;
        const float4& c = which ? A().bg : A().fg;
        rgba[0] = c.x;
        rgba[1] = c.y;
        rgba[2] = c.z;
        rgba[3] = c.w;
    }
    static void HSetColor(int which, const float rgba[4]) {
        if (!rgba) return;
        float4& c = which ? A().bg : A().fg;
        c = {std::clamp(rgba[0], 0.0f, 1.0f), std::clamp(rgba[1], 0.0f, 1.0f), std::clamp(rgba[2], 0.0f, 1.0f), std::clamp(rgba[3], 0.0f, 1.0f)};
    }
    static const char* HSettingGet(const char* key, const char* def) {
        App& a = A();
        const Json& p = a.pluginSettings[a.pluginCurrent];
        gReturn = key && p.IsObject() && p.Has(key) ? p[key].S(def ? def : "") : std::string(def ? def : "");
        return gReturn.c_str();
    }
    static void HSettingSet(const char* key, const char* value) {
        App& a = A();
        if (!key || a.pluginCurrent.empty()) return;
        if (!a.pluginSettings.IsObject()) a.pluginSettings = Json::Object();
        if (!a.pluginSettings[a.pluginCurrent].IsObject()) a.pluginSettings[a.pluginCurrent] = Json::Object();
        if (value && *value) a.pluginSettings[a.pluginCurrent][key] = std::string(value);
        else a.pluginSettings[a.pluginCurrent].Erase(key);
        a.pluginSettingsDirty = true;
    }
    static void HOnEvent(SxEventFn fn, void* user) {
        if (fn) A().pluginListeners.push_back({A().pluginCurrent, fn, user});
    }
    static unsigned HActiveLayer() {
        DocView* v = A().Active();
        return v ? (unsigned)v->doc->S().activeLayer : 0u;
    }
    static void HPixelsRead(unsigned layer, int x, int y, int w, int h, int format, SxPixelsFn done, void* user) {
        App& a = A();
        Surface* s = a.PluginSurface(layer);
        if (!s || !s->tex) { if (done) done(user, 0, 0, 0, nullptr); return; }
        const int W = (int)s->tex->Width(), H = (int)s->tex->Height();
        if (w <= 0 || h <= 0) { x = 0; y = 0; w = W; h = H; }
        Rect r = {std::clamp(x, 0, W), std::clamp(y, 0, H), std::clamp(x + w, 0, W), std::clamp(y + h, 0, H)};
        if (r.Empty()) { if (done) done(user, 0, 0, 0, nullptr); return; }
        a.pluginReads.push_back({a.pluginCurrent, a.ctx->ReadbackTexture(s->tex.get(), &r), format, done, user});
    }
    static int HPixelsWrite(unsigned layer, int x, int y, int w, int h, int format, const void* pixels, const char* label) {
        App& a = A();
        Id sid = 0;
        Surface* s = a.PluginSurface(layer, &sid);
        DocView* v = a.Active();
        if (!s || !s->tex || !v || !pixels || w <= 0 || h <= 0) return 0;
        const int W = (int)s->tex->Width(), H = (int)s->tex->Height();
        if (x < 0 || y < 0 || x + w > W || y + h > H) return 0;
        const gpu::Format fmt = s->tex->GetFormat();
        if (fmt != gpu::Format::RGBA16F && fmt != gpu::Format::RGBA32F) return 0;
        // Into the layer's format: linear, premultiplied (8-bit input: sRGB straight).
        const size_t n = (size_t)w * h;
        std::vector<float> lin(n * 4);
        for (size_t i = 0; i < n; ++i) {
            float c[4];
            if (format == SX_PIXELS_RGBA8) {
                const unsigned char* p = (const unsigned char*)pixels + i * 4;
                const float al = p[3] / 255.0f;
                for (int k = 0; k < 3; ++k) c[k] = SrgbToLinear(p[k] / 255.0f) * al;
                c[3] = al;
            } else {
                memcpy(c, (const float*)pixels + i * 4, 16);
            }
            memcpy(&lin[i * 4], c, 16);
        }
        auto after = gpu::Dev().CreateTexture((u32)w, (u32)h, fmt, "PluginPixels");
        auto before = gpu::Dev().CreateTexture((u32)w, (u32)h, fmt, "UndoBefore");
        if (!after || !before) return 0;
        if (fmt == gpu::Format::RGBA32F) {
            a.ctx->Upload(after.get(), lin.data(), (u32)w * 16);
        } else {
            std::vector<u16> hv(n * 4);
            for (size_t i = 0; i < n * 4; ++i) hv[i] = FloatToHalf(lin[i]);
            a.ctx->Upload(after.get(), hv.data(), (u32)w * 8);
        }
        const Rect r = {x, y, x + w, y + h};
        a.ctx->CopyRegion(before.get(), 0, 0, s->tex.get(), r);
        a.ctx->CopyRegion(s->tex.get(), x, y, after.get(), Rect{0, 0, w, h});
        v->doc->TouchSurface(sid);
        v->history.PushPaint(label && *label ? label : "Plugin : pixels", sid, r, before, after);
        v->doc->dirty = true;
        return 1;
    }
};

namespace {
SxHost MakeHost() {
    SxHost h = {};
    h.api_version = SX_PLUGIN_API_VERSION;
    h.app_version = "1.0";
    h.ui = &kUi;
    using G = PluginGlue;
    h.log = G::HLog;
    h.status = G::HStatus;
    h.add_command = G::HAddCommand;
    h.add_panel = G::HAddPanel;
    h.add_tool = G::HAddTool;
    h.add_operation = G::HAddOperation;
    h.call = G::HCall;
    h.operations_json = G::HOperations;
    h.document_size = G::HDocumentSize;
    h.workspace_dir = G::HWorkspace;
    h.open_panel = G::HOpenPanel;
    h.select_tool = G::HSelectTool;
    h.draw_line = G::HDrawLine;
    h.draw_circle = G::HDrawCircle;
    h.draw_rect = G::HDrawRect;
    h.draw_text = G::HDrawText;
    h.imgui_context = G::HImGuiContext;
    h.imgui_version = IMGUI_VERSION;
    h.active_layer = G::HActiveLayer;
    h.pixels_read = G::HPixelsRead;
    h.pixels_write = G::HPixelsWrite;
    h.get_color = G::HGetColor;
    h.set_color = G::HSetColor;
    h.setting_get = G::HSettingGet;
    h.setting_set = G::HSettingSet;
    h.on_event = G::HOnEvent;
    return h;
}
SxHost gHost;

std::string UserPluginDir() {
    const char* la = getenv("LOCALAPPDATA");
    return la ? std::string(la) + "\\Shaderixel\\Plugins" : std::string();
}
}  // namespace

// ---------------------------------------------------------------- lifecycle

std::string App::PluginWorkspace(const std::string& plugin) {
    wchar_t tmp[MAX_PATH] = L"";
    GetTempPathW(MAX_PATH, tmp);
    std::string id = plugin.empty() ? std::string("host") : plugin;
    for (auto& c : id) if (c == '/' || c == '\\' || c == ':') c = '_';
    const std::string dir = Narrow(tmp) + "Shaderixel\\plugins\\" + id;
    std::error_code ec;
    fs::create_directories(Utf8Path(dir), ec);
    return dir;
}

std::vector<std::string> App::PluginDirs() {
    std::vector<std::string> dirs = {ExeDir() + "\\plugins"};
    if (const char* t = getenv("SX_PLUGINDIR"); t && *t) dirs.push_back(t);  // tests, development
    else if (std::string u = UserPluginDir(); !u.empty()) dirs.push_back(u);
    return dirs;
}

void App::InitPlugins() {
    gApp = this;
    gHost = MakeHost();
    // Disabled plugins (by id), kept next to the settings.
    std::ifstream f(Utf8Path(Narrow(Utf8Path(settingsPath).parent_path().wstring()) + "\\plugins_disabled.txt"));
    for (std::string line; std::getline(f, line);)
        if (!line.empty()) pluginsDisabled.insert(line);
    {
        std::string text;
        if (!ReadTextFile(Narrow(Utf8Path(settingsPath).parent_path().wstring()) + "\\plugins_settings.json", text) || !Json::Parse(text, pluginSettings) || !pluginSettings.IsObject())
            pluginSettings = Json::Object();
    }
    ScanPlugins();
    for (auto& p : plugins)
        if (p.enabled) LoadPlugin(p);
    int loaded = 0;
    for (auto& p : plugins) loaded += p.loaded ? 1 : 0;
    Log("Plugins: %d found, %d loaded", (int)plugins.size(), loaded);
}

void App::ScanPlugins() {
    for (const std::string& root : PluginDirs()) {
        std::error_code ec;
        for (auto& e : fs::directory_iterator(Utf8Path(root), ec)) {
            if (!e.is_directory()) continue;
            const std::string dir = Narrow(e.path().wstring());
            std::string text;
            Json m;
            if (!ReadTextFile(dir + "\\plugin.json", text) || !Json::Parse(text, m) || !m.IsObject()) continue;
            PluginEntry p;
            p.dir = dir;
            p.id = m["id"].S(Narrow(e.path().filename().wstring()));
            bool dup = false;
            for (auto& q : plugins) dup |= q.id == p.id;
            if (dup) continue;  // the first folder wins (package before user)
            p.name = m["name"].S(p.id);
            p.version = m["version"].S("");
            p.author = m["author"].S("");
            p.description = m["description"].S("");
            p.dll = m["dll"].S("");
            for (const Json& c : m["panels"].Elements())
                if (!c["title"].S("").empty() && !c["run"].S("").empty()) p.panels.push_back({c["title"].S(""), c["run"].S("")});
            for (const Json& c : m["commands"].Elements()) {
                if (c["menu"].S("").empty() || c["run"].S("").empty()) continue;
                p.scripts.push_back({c["menu"].S(""), c["run"].S(""), c["description"].S("")});
            }
            p.enabled = !pluginsDisabled.count(p.id);
            plugins.push_back(std::move(p));
        }
    }
}

bool App::LoadPlugin(PluginEntry& p) {
    if (p.loaded) return true;
    p.error.clear();
    if (!p.dll.empty()) {
        // A copy is loaded: the original can be rebuilt meanwhile (then "Recharger").
        const std::string src = p.dir + "\\" + p.dll;
        const std::string copy = PluginWorkspace(p.id) + "\\" + std::to_string(GetTickCount64()) + "_" + p.dll;
        std::error_code ec;
        fs::copy_file(Utf8Path(src), Utf8Path(copy), fs::copy_options::overwrite_existing, ec);
        if (ec) { p.error = "DLL introuvable : " + p.dll; return false; }
        HMODULE mod = LoadLibraryExW(Utf8Path(copy).c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if (!mod) { p.error = "chargement de la DLL impossible (" + std::to_string(GetLastError()) + ")"; return false; }
        auto load = (SxPluginLoadFn)GetProcAddress(mod, "sx_plugin_load");
        if (!load) {
            FreeLibrary(mod);
            p.error = "fonction sx_plugin_load absente";
            return false;
        }
        SxPlugin info = {};
        pluginCurrent = p.id;
        int ok = 0;
        try {
            ok = load(&gHost, &info);
        } catch (...) {
            ok = 0;
        }
        pluginCurrent.clear();
        if (!ok) {
            RemovePluginRegistrations(p.id);
            FreeLibrary(mod);
            p.error = "le plugin a refusé de se charger (version de l'API ?)";
            return false;
        }
        p.module = mod;
        p.unload = info.unload;
        p.unloadUser = info.user;
        p.loadedCopy = copy;
    }
    for (auto& pn : p.panels) {
        ScriptPanel sp;
        sp.plugin = p.id;
        sp.title = pn.title;
        sp.run = pn.run;
        sp.dir = p.dir;
        scriptPanels.push_back(std::move(sp));
    }
    p.loaded = true;
    return true;
}

void App::RemovePluginRegistrations(const std::string& id) {
    auto drop = [&](auto& v) { v.erase(std::remove_if(v.begin(), v.end(), [&](const auto& x) { return x.plugin == id; }), v.end()); };
    if (tool == Tool::Plugin && pluginTool >= 0 && pluginTool < (int)pluginTools.size() && pluginTools[pluginTool].plugin == id) {
        tool = Tool::Brush;
        pluginTool = -1;
    }
    drop(pluginCommands);
    drop(pluginPanels);
    drop(pluginTools);
    drop(pluginCalls);
    drop(pluginReads);
    drop(pluginListeners);
    for (auto& sp : scriptPanels)
        if (sp.plugin == id) StopScriptPanel(sp);
    drop(scriptPanels);
    for (auto& o : pluginOps)
        if (o.plugin == id) bridge.Unregister(o.op);
    drop(pluginOps);
}

void App::UnloadPlugin(PluginEntry& p) {
    if (!p.loaded) return;
    if (p.unload) {
        pluginCurrent = p.id;
        p.unload(p.unloadUser);
        pluginCurrent.clear();
    }
    RemovePluginRegistrations(p.id);
    if (p.module) FreeLibrary((HMODULE)p.module);
    if (!p.loadedCopy.empty()) {
        std::error_code ec;
        fs::remove(Utf8Path(p.loadedCopy), ec);
    }
    p.module = nullptr;
    p.unload = nullptr;
    p.loaded = false;
}

void App::ShutdownPlugins() {
    for (auto& sp : scriptPanels) StopScriptPanel(sp);
    for (auto& p : plugins) UnloadPlugin(p);
    for (auto& pr : pluginProcs) CloseHandle(pr.process);
    pluginProcs.clear();
}

void App::FinishPluginCall(const std::string& plugin, const AiResult& r, SxResultFn done, void* user) {
    if (!done) return;
    Json out;
    if (!r.errorCode.empty()) {
        out = Json::Object();
        out["error"] = r.errorMessage.empty() ? r.errorCode : r.errorMessage;
    } else {
        out = r.result.IsObject() ? r.result : Json::Object();
    }
    const std::string s = out.Dump(0);
    const std::string prev = pluginCurrent;
    pluginCurrent = plugin;
    done(user, r.errorCode.empty() ? 1 : 0, s.c_str());
    pluginCurrent = prev;
}

Surface* App::PluginSurface(unsigned layer, Id* id) {
    // A pixel layer's surface (0: the active paint target).
    DocView* v = Active();
    if (!v) return nullptr;
    Document& d = *v->doc;
    Id sid = 0;
    if (!layer) sid = d.PaintTarget();
    else if (Layer* l = d.FindLayer((Id)layer); l && l->kind == LayerKind::Pixel) sid = l->surface;
    if (id) *id = sid;
    return sid ? d.GetSurface(sid) : nullptr;
}

void App::PluginEvent(int event, const std::string& text) {
    const auto listeners = pluginListeners;  // a listener may register / unload meanwhile
    for (auto& l : listeners) CallPlugin(l.plugin, [&] { l.fn(l.user, event, text.c_str()); });
}

void App::UpdatePlugins() {
    // Events: active document, edits (at most once per frame), settings written when changed.
    {
        DocView* v = Active();
        if ((const void*)v != pluginLastDoc) {
            pluginLastDoc = v;
            pluginLastEdit = v ? v->doc->ContentVersion() * 31 + v->history.Count() : 0;
            if (!pluginListeners.empty()) PluginEvent(SX_EVENT_DOCUMENT_ACTIVATED, v ? v->doc->Name() : "");
        } else if (v) {
            const u64 k = v->doc->ContentVersion() * 31 + v->history.Count() * 7 + v->history.Cursor();
            if (k != pluginLastEdit) {
                pluginLastEdit = k;
                if (!pluginListeners.empty()) PluginEvent(SX_EVENT_DOCUMENT_EDITED, "");
            }
        }
        if (pluginSettingsDirty) {
            pluginSettingsDirty = false;
            std::ofstream(Utf8Path(Narrow(Utf8Path(settingsPath).parent_path().wstring()) + "\\plugins_settings.json"), std::ios::binary) << pluginSettings.Dump(1);
        }
    }
    // Pixel reads: converted to the asked format, then given to the plugin.
    for (size_t i = 0; i < pluginReads.size();) {
        auto& rd = pluginReads[i];
        if (rd.rb && !rd.rb->ready) { ++i; continue; }
        PluginRead r = std::move(rd);
        pluginReads.erase(pluginReads.begin() + i);
        if (!r.done) continue;
        const gpu::Readback* rb = r.rb.get();
        const size_t n = rb ? (size_t)rb->width * rb->height : 0;
        std::vector<float> f(n * 4);
        bool ok = rb != nullptr;
        if (ok && rb->format == gpu::Format::RGBA16F && rb->data.size() >= n * 8) {
            const u16* h = (const u16*)rb->data.data();
            for (size_t k = 0; k < n * 4; ++k) f[k] = HalfToFloat(h[k]);
        } else if (ok && rb->format == gpu::Format::RGBA32F && rb->data.size() >= n * 16) {
            memcpy(f.data(), rb->data.data(), n * 16);
        } else {
            ok = false;
        }
        std::vector<unsigned char> b;
        if (ok && r.format == SX_PIXELS_RGBA8) {
            b.resize(n * 4);
            for (size_t k = 0; k < n; ++k) {
                const float al = std::clamp(f[k * 4 + 3], 0.0f, 1.0f);
                for (int c = 0; c < 3; ++c) {
                    const float v = al > 1e-6f ? f[k * 4 + c] / al : 0.0f;
                    b[k * 4 + c] = (unsigned char)std::lround(std::clamp(LinearToSrgb(v), 0.0f, 1.0f) * 255.0f);
                }
                b[k * 4 + 3] = (unsigned char)std::lround(al * 255.0f);
            }
        }
        const void* data = !ok ? nullptr : r.format == SX_PIXELS_RGBA8 ? (const void*)b.data() : (const void*)f.data();
        CallPlugin(r.plugin, [&] { r.done(r.user, ok ? 1 : 0, ok ? (int)rb->width : 0, ok ? (int)rb->height : 0, data); });
    }
    for (size_t i = 0; i < pluginCalls.size();) {
        auto& c = pluginCalls[i];
        if (!c.result.poll || c.result.poll(c.result)) {
            PluginCall done = std::move(c);
            pluginCalls.erase(pluginCalls.begin() + i);
            FinishPluginCall(done.plugin, done.result, done.done, done.user);
        } else {
            ++i;
        }
    }
    // Script commands: their end and last output line in the status bar.
    for (size_t i = 0; i < pluginProcs.size();) {
        auto& pr = pluginProcs[i];
        if (WaitForSingleObject(pr.process, 0) != WAIT_OBJECT_0) { ++i; continue; }
        DWORD code = 0;
        GetExitCodeProcess(pr.process, &code);
        CloseHandle(pr.process);
        std::string text, last;
        ReadTextFile(pr.log, text);
        std::istringstream ss(text);
        for (std::string line; std::getline(ss, line);)
            if (!line.empty() && line != "\r") last = line;
        if (!last.empty() && last.back() == '\r') last.pop_back();
        Log("[plugin %s] %s : code %lu, %s", pr.plugin.c_str(), pr.label.c_str(), code, last.c_str());
        Status(pr.label + (code == 0 ? " : terminé" : " : erreur (" + std::to_string(code) + ")") + (last.empty() ? "" : " - " + last));
        pluginProcs.erase(pluginProcs.begin() + i);
    }
}

void App::RunPluginScript(const PluginEntry& p, const PluginEntry::Script& s) {
    // The command line runs in the plugin folder; the bridge address and a token are in the environment.
    if (!bridge.Running()) bridge.Start(ExeDir() + "/Shaderixel");
    static int runCount = 0;
    const std::string log = PluginWorkspace(p.id) + "\\run_" + std::to_string(++runCount % 8) + ".log";  // each run its own output
    SECURITY_ATTRIBUTES sa = {sizeof(sa), nullptr, TRUE};
    HANDLE out = CreateFileW(Utf8Path(log).c_str(), GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    std::wstring env;
    {
        // Current environment + SHADERIXEL_URL / SHADERIXEL_TOKEN / SHADERIXEL_WORKSPACE.
        wchar_t* block = GetEnvironmentStringsW();
        for (wchar_t* e = block; e && *e; e += wcslen(e) + 1) {
            std::wstring kv = e;
            if (kv.rfind(L"SHADERIXEL_", 0) == 0) continue;
            env += kv;
            env.push_back(L'\0');
        }
        FreeEnvironmentStringsW(block);
        auto add = [&](const std::wstring& kv) { env += kv; env.push_back(L'\0'); };
        add(L"SHADERIXEL_URL=http://127.0.0.1:" + std::to_wstring(bridge.Port()) + L"/shaderixel/v1");
        add(L"SHADERIXEL_TOKEN=" + Widen(bridge.PluginToken()));
        add(L"SHADERIXEL_WORKSPACE=" + Widen(PluginWorkspace(p.id)));
        add(L"PYTHONIOENCODING=utf-8");
        env.push_back(L'\0');
    }
    STARTUPINFOW si = {sizeof(si)};
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = si.hStdError = out;
    si.hStdInput = nullptr;
    PROCESS_INFORMATION pi = {};
    std::wstring cmd = L"cmd /c " + Widen(s.run);
    const BOOL ok = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT, env.data(), Utf8Path(p.dir).c_str(), &si, &pi);
    if (out != INVALID_HANDLE_VALUE) CloseHandle(out);
    if (!ok) {
        Status("Plugin « " + p.name + " » : impossible de lancer " + s.run);
        return;
    }
    CloseHandle(pi.hThread);
    const std::string label = s.menu.substr(s.menu.find_last_of('/') + 1);
    pluginProcs.push_back({p.id, label, log, pi.hProcess});
    Status(label + "...");
}

void App::CallPlugin(const std::string& plugin, const std::function<void()>& fn) {
    const std::string prev = pluginCurrent;
    pluginCurrent = plugin;
    fn();
    pluginCurrent = prev;
}

// ---------------------------------------------------------------- UI

namespace {
// Menu path "A/B/C": submenus A, B, item C. Returns true when the item was chosen.
bool MenuPath(const std::string& path) {
    std::vector<std::string> parts;
    std::stringstream ss(path);
    for (std::string s; std::getline(ss, s, '/');)
        if (!s.empty()) parts.push_back(s);
    if (parts.empty()) return false;
    size_t opened = 0;
    bool chosen = false;
    for (; opened + 1 < parts.size(); ++opened)
        if (!ImGui::BeginMenu(parts[opened].c_str())) break;
    if (opened + 1 == parts.size()) chosen = ImGui::MenuItem(parts.back().c_str());
    for (size_t i = 0; i < opened; ++i) ImGui::EndMenu();
    return chosen;
}
}  // namespace

void App::DrawPluginsMenu() {
    if (!ImGui::BeginMenu("Plugins")) return;
    for (auto& c : pluginCommands)
        if (MenuPath(c.path)) CallPlugin(c.plugin, [&] { c.fn(c.user); });
    for (auto& p : plugins)
        if (p.loaded)
            for (auto& s : p.scripts)
                if (MenuPath(s.menu)) RunPluginScript(p, s);
    if ((!pluginPanels.empty() || !scriptPanels.empty()) && ImGui::BeginMenu("Fenêtres")) {
        for (auto& p : pluginPanels) ImGui::MenuItem(p.title.c_str(), nullptr, &p.open);
        for (auto& p : scriptPanels) ImGui::MenuItem(p.title.c_str(), nullptr, &p.open);
        ImGui::EndMenu();
    }
    if (!pluginTools.empty() && ImGui::BeginMenu("Outils")) {
        for (size_t i = 0; i < pluginTools.size(); ++i)
            if (ImGui::MenuItem(pluginTools[i].name.c_str(), nullptr, tool == Tool::Plugin && pluginTool == (int)i)) {
                pluginTool = (int)i;
                tool = Tool::Plugin;
            }
        ImGui::EndMenu();
    }
    if (pluginCommands.empty() && pluginPanels.empty() && pluginTools.empty()) {
        bool anyScript = false;
        for (auto& p : plugins) anyScript |= p.loaded && !p.scripts.empty();
        if (!anyScript) ImGui::TextDisabled("Aucun plugin chargé");
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Gestionnaire de plugins...")) pluginManager = true;
    ImGui::EndMenu();
}

void App::DrawPluginPanels() {
    for (auto& sp : scriptPanels) {
        if (!sp.open) {
            if (sp.process) StopScriptPanel(sp);  // closed: its program ends
            continue;
        }
        if (!sp.process && sp.error.empty()) StartScriptPanel(sp);
        ImGui::SetNextWindowSize(ImVec2(320 * dpi, 360 * dpi), ImGuiCond_FirstUseEver);
        bool open = true;
        if (ImGui::Begin(sp.title.c_str(), &open)) DrawScriptPanel(sp);
        ImGui::End();
        sp.open = open;
    }
    for (size_t i = 0; i < pluginPanels.size(); ++i) {
        auto& p = pluginPanels[i];
        if (!p.open) continue;
        ImGui::SetNextWindowSize(ImVec2(320 * dpi, 360 * dpi), ImGuiCond_FirstUseEver);
        bool open = true;
        if (ImGui::Begin(p.title.c_str(), &open)) {
            const SxPanelFn fn = p.fn;
            void* user = p.user;
            CallPlugin(p.plugin, [&] { fn(user); });
        }
        ImGui::End();
        if (i < pluginPanels.size()) pluginPanels[i].open = open && pluginPanels[i].open;
    }
}

void App::DrawPluginManager() {
    if (!pluginManager) return;
    ImGui::SetNextWindowSize(ImVec2(620 * dpi, 420 * dpi), ImGuiCond_Appearing);
    if (!ImGui::Begin("Plugins", &pluginManager, ImGuiWindowFlags_NoDocking)) { ImGui::End(); return; }
    ImGui::TextDisabled("Dossiers : <Shaderixel>\\plugins et %%LOCALAPPDATA%%\\Shaderixel\\Plugins (un sous-dossier avec plugin.json par plugin).");
    if (ImGui::Button("Rechercher de nouveaux plugins")) {
        const size_t before = plugins.size();
        ScanPlugins();
        for (size_t i = before; i < plugins.size(); ++i)
            if (plugins[i].enabled) LoadPlugin(plugins[i]);
        Status(std::to_string(plugins.size() - before) + " nouveau(x) plugin(s)");
    }
    ImGui::SameLine();
    if (ImGui::Button("Ouvrir le dossier utilisateur")) {
        const std::string u = UserPluginDir();
        std::error_code ec;
        fs::create_directories(Utf8Path(u), ec);
        ShellExecuteW(nullptr, L"open", Utf8Path(u).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
    ImGui::SameLine();
    if (ImGui::Button("SDK et exemples")) {
        const std::wstring sdk = Utf8Path(ExeDir()).wstring() + L"\\SDK";
        ShellExecuteW(nullptr, L"open", sdk.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
    ImGui::Separator();
    if (ImGui::BeginTable("##plugins", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("Actif", ImGuiTableColumnFlags_WidthFixed, 44 * dpi);
        ImGui::TableSetupColumn("Plugin", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Apporte", ImGuiTableColumnFlags_WidthFixed, 150 * dpi);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 90 * dpi);
        ImGui::TableHeadersRow();
        bool changed = false;
        for (size_t i = 0; i < plugins.size(); ++i) {
            auto& p = plugins[i];
            ImGui::PushID((int)i);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (ImGui::Checkbox("##on", &p.enabled)) {
                if (p.enabled) LoadPlugin(p);
                else UnloadPlugin(p);
                if (p.enabled) pluginsDisabled.erase(p.id);
                else pluginsDisabled.insert(p.id);
                changed = true;
            }
            ImGui::TableNextColumn();
            ImGui::Text("%s  %s", p.name.c_str(), p.version.c_str());
            if (!p.description.empty()) {
                ImGui::PushTextWrapPos(0.0f);
                ImGui::TextDisabled("%s", p.description.c_str());
                ImGui::PopTextWrapPos();
            }
            ImGui::TextDisabled("%s%s%s", p.dll.empty() ? "script" : "natif", p.author.empty() ? "" : " - ", p.author.c_str());
            if (!p.error.empty()) ImGui::TextColored(ImVec4(1, 0.45f, 0.4f, 1), "%s", p.error.c_str());
            ImGui::TableNextColumn();
            int nc = 0, np = 0, nt = 0, no = 0;
            for (auto& c : pluginCommands) nc += c.plugin == p.id;
            for (auto& c : pluginPanels) np += c.plugin == p.id;
            for (auto& c : pluginTools) nt += c.plugin == p.id;
            for (auto& c : pluginOps) no += c.plugin == p.id;
            nc += (int)p.scripts.size();
            ImGui::TextDisabled("%d commande(s), %d fenêtre(s)\n%d outil(s), %d opération(s)", nc, np, nt, no);
            ImGui::TableNextColumn();
            ImGui::BeginDisabled(!p.enabled || p.dll.empty());
            if (ImGui::SmallButton("Recharger")) {
                UnloadPlugin(p);
                LoadPlugin(p);
                Status(p.error.empty() ? "Plugin « " + p.name + " » rechargé" : "Plugin « " + p.name + " » : " + p.error);
            }
            ImGui::EndDisabled();
            ImGui::PopID();
        }
        ImGui::EndTable();
        if (changed) {
            std::ofstream f(Utf8Path(Narrow(Utf8Path(settingsPath).parent_path().wstring()) + "\\plugins_disabled.txt"), std::ios::binary);
            for (auto& id : pluginsDisabled) f << id << "\n";
        }
    }
    ImGui::End();
}

// ---------------------------------------------------------------- canvas tool

void App::PluginToolInput(DocView& v, ImVec2 canvasMin, bool hovered) {
    if (pluginTool < 0 || pluginTool >= (int)pluginTools.size()) return;
    const PluginTool t = pluginTools[pluginTool];
    ImGuiIO& io = ImGui::GetIO();
    pluginDraw = {canvasMin, v.pan, v.zoom, ImGui::GetWindowDrawList()};
    const float2 dp = ScreenToDoc(v, canvasMin, io.MousePos);
    SxToolEvent e = {};
    e.x = dp.x;
    e.y = dp.y;
    e.pressure = frameSamples.empty() ? 1.0f : std::clamp(frameSamples.back().pressure, 0.0f, 1.0f);
    e.modifiers = (io.KeyShift ? SX_MOD_SHIFT : 0) | (io.KeyCtrl ? SX_MOD_CTRL : 0) | (io.KeyAlt ? SX_MOD_ALT : 0);
    e.zoom = v.zoom;
    e.view = 0;
    auto send = [&](int type) {
        e.type = type;
        CallPlugin(t.plugin, [&] { t.fn(t.user, &e); });
    };
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        pluginToolDown = true;
        send(SX_TOOL_PRESS);
    } else if (pluginToolDown && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        if (io.MouseDelta.x != 0 || io.MouseDelta.y != 0) send(SX_TOOL_DRAG);
    } else if (pluginToolDown) {
        pluginToolDown = false;
        send(SX_TOOL_RELEASE);
    } else if (hovered && (io.MouseDelta.x != 0 || io.MouseDelta.y != 0)) {
        send(SX_TOOL_HOVER);
    }
    if (hovered || pluginToolDown) send(SX_TOOL_DRAW);
    pluginDraw.dl = nullptr;
}

void App::PluginTool3DInput(DocView& v, float2 local, u32 w, u32 h, bool hovered) {
    // Over the model: the texture pixel under the cursor (the plugin paints the document as usual).
    if (pluginTool < 0 || pluginTool >= (int)pluginTools.size() || !v.mesh) return;
    const PluginTool t = pluginTools[pluginTool];
    ImGuiIO& io = ImGui::GetIO();
    float2 uv;
    SxToolEvent e = {};
    e.x = e.y = -1;
    if (hovered && PickMeshUv(*v.mesh, local, w, h, uv)) {
        e.x = uv.x * v.doc->Width();
        e.y = uv.y * v.doc->Height();
    }
    e.pressure = frameSamples.empty() ? 1.0f : std::clamp(frameSamples.back().pressure, 0.0f, 1.0f);
    e.modifiers = (io.KeyShift ? SX_MOD_SHIFT : 0) | (io.KeyCtrl ? SX_MOD_CTRL : 0) | (io.KeyAlt ? SX_MOD_ALT : 0);
    e.zoom = 1;
    e.view = 1;
    pluginDraw.dl = nullptr;  // no overlay in the 3D view
    auto send = [&](int type) {
        e.type = type;
        CallPlugin(t.plugin, [&] { t.fn(t.user, &e); });
    };
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.KeyAlt && e.x >= 0) {
        pluginToolDown = true;
        send(SX_TOOL_PRESS);
    } else if (pluginToolDown && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        if ((io.MouseDelta.x != 0 || io.MouseDelta.y != 0) && e.x >= 0) send(SX_TOOL_DRAG);
    } else if (pluginToolDown) {
        pluginToolDown = false;
        send(SX_TOOL_RELEASE);
    } else if (hovered && (io.MouseDelta.x != 0 || io.MouseDelta.y != 0)) {
        send(SX_TOOL_HOVER);
    }
}

// ---------------------------------------------------------------- script panels

// Lines read from the program's stdout by a thread (the window takes them each frame).
struct ScriptPanelIo {
    std::mutex mutex;
    std::vector<std::string> lines;
    std::string partial;
    std::atomic<bool> ended{false};
    std::thread reader;
    void* stdoutRead = nullptr;
    ~ScriptPanelIo() {
        if (reader.joinable()) reader.join();
        if (stdoutRead) CloseHandle(stdoutRead);
    }
};

void App::StartScriptPanel(ScriptPanel& sp) {
    if (!bridge.Running()) bridge.Start(ExeDir() + "/Shaderixel");
    SECURITY_ATTRIBUTES sa = {sizeof(sa), nullptr, TRUE};
    HANDLE outR = nullptr, outW = nullptr, inR = nullptr, inW = nullptr;
    if (!CreatePipe(&outR, &outW, &sa, 0) || !CreatePipe(&inR, &inW, &sa, 0)) { sp.error = "tubes indisponibles"; return; }
    SetHandleInformation(outR, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(inW, HANDLE_FLAG_INHERIT, 0);
    std::wstring env;
    {
        wchar_t* block = GetEnvironmentStringsW();
        for (wchar_t* e = block; e && *e; e += wcslen(e) + 1) {
            std::wstring kv = e;
            if (kv.rfind(L"SHADERIXEL_", 0) == 0 || kv.rfind(L"PYTHONUNBUFFERED", 0) == 0) continue;
            env += kv;
            env.push_back(L'\0');
        }
        FreeEnvironmentStringsW(block);
        auto add = [&](const std::wstring& kv) { env += kv; env.push_back(L'\0'); };
        add(L"SHADERIXEL_URL=http://127.0.0.1:" + std::to_wstring(bridge.Port()) + L"/shaderixel/v1");
        add(L"SHADERIXEL_TOKEN=" + Widen(bridge.PluginToken()));
        add(L"SHADERIXEL_WORKSPACE=" + Widen(PluginWorkspace(sp.plugin)));
        add(L"PYTHONIOENCODING=utf-8");
        add(L"PYTHONUNBUFFERED=1");
        env.push_back(L'\0');
    }
    // Errors of the program go to a log (its stdout is the protocol).
    const std::string log = PluginWorkspace(sp.plugin) + "\\panel_errors.log";
    HANDLE errW = CreateFileW(Utf8Path(log).c_str(), GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    STARTUPINFOW si = {sizeof(si)};
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = outW;
    si.hStdError = errW;
    si.hStdInput = inR;
    PROCESS_INFORMATION pi = {};
    std::wstring cmd = L"cmd /c " + Widen(sp.run);
    const BOOL ok = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT, env.data(), Utf8Path(sp.dir).c_str(), &si, &pi);
    CloseHandle(outW);
    CloseHandle(inR);
    if (errW != INVALID_HANDLE_VALUE) CloseHandle(errW);
    if (!ok) {
        CloseHandle(outR);
        CloseHandle(inW);
        sp.error = "impossible de lancer : " + sp.run;
        return;
    }
    CloseHandle(pi.hThread);
    sp.process = pi.hProcess;
    sp.stdinWrite = inW;
    auto io = std::make_shared<ScriptPanelIo>();
    io->stdoutRead = outR;
    ScriptPanelIo* raw = io.get();
    io->reader = std::thread([raw] {
        char buf[4096];
        DWORD n = 0;
        while (ReadFile(raw->stdoutRead, buf, sizeof(buf), &n, nullptr) && n > 0) {
            std::lock_guard lock(raw->mutex);
            raw->partial.append(buf, n);
            size_t pos;
            while ((pos = raw->partial.find('\n')) != std::string::npos) {
                std::string line = raw->partial.substr(0, pos);
                if (!line.empty() && line.back() == '\r') line.pop_back();
                raw->lines.push_back(std::move(line));
                raw->partial.erase(0, pos + 1);
            }
        }
        raw->ended = true;
    });
    sp.io = io;
    sp.ui = Json::Array();
    Json hello = Json::Object();
    hello["event"] = "open";
    SendScriptPanel(sp, hello);
}

void App::StopScriptPanel(ScriptPanel& sp) {
    if (!sp.process) return;
    Json bye = Json::Object();
    bye["event"] = "close";
    SendScriptPanel(sp, bye);
    if (sp.stdinWrite) CloseHandle(sp.stdinWrite);  // end of input: the program may end by itself
    sp.stdinWrite = nullptr;
    if (WaitForSingleObject(sp.process, 300) != WAIT_OBJECT_0) TerminateProcess(sp.process, 0);
    CloseHandle(sp.process);
    sp.process = nullptr;
    sp.io.reset();  // joins the reader (its pipe ended with the process)
}

void App::SendScriptPanel(ScriptPanel& sp, const Json& msg) {
    if (!sp.stdinWrite) return;
    const std::string line = msg.Dump(0) + "\n";
    DWORD n = 0;
    WriteFile(sp.stdinWrite, line.data(), (DWORD)line.size(), &n, nullptr);
}

void App::DrawScriptPanel(ScriptPanel& sp) {
    // Messages of the program: {"ui": [...]} (whole window), {"status": "..."}, {"log": "..."}.
    if (sp.io) {
        std::vector<std::string> lines;
        {
            std::lock_guard lock(sp.io->mutex);
            lines.swap(sp.io->lines);
        }
        for (auto& line : lines) {
            Json m;
            if (!Json::Parse(line, m) || !m.IsObject()) {
                Log("[plugin %s] %s", sp.plugin.c_str(), line.c_str());
                continue;
            }
            if (m.Has("ui") && m["ui"].IsArray()) sp.ui = m["ui"];
            if (m.Has("status")) Status(m["status"].S(""));
            if (m.Has("log")) Log("[plugin %s] %s", sp.plugin.c_str(), m["log"].S("").c_str());
        }
        if (sp.io->ended && sp.process && WaitForSingleObject(sp.process, 0) == WAIT_OBJECT_0) {
            DWORD code = 0;
            GetExitCodeProcess(sp.process, &code);
            std::string err;
            ReadTextFile(PluginWorkspace(sp.plugin) + "\\panel_errors.log", err);
            sp.error = "le programme s'est arrêté (code " + std::to_string(code) + ")" + (err.empty() ? "" : " :\n" + err.substr(err.size() > 600 ? err.size() - 600 : 0));
            CloseHandle(sp.process);
            sp.process = nullptr;
            if (sp.stdinWrite) CloseHandle(sp.stdinWrite);
            sp.stdinWrite = nullptr;
            sp.io.reset();
        }
    }
    if (!sp.error.empty()) {
        ImGui::TextColored(ImVec4(1, 0.5f, 0.4f, 1), "%s", sp.error.c_str());
        if (ImGui::Button("Relancer")) sp.error.clear();
        return;
    }
    if (sp.ui.Size() == 0) {
        ImGui::TextDisabled("Démarrage du script...");
        return;
    }
    auto send = [&](const char* event, const Json& w, const Json* value) {
        Json m = Json::Object();
        m["event"] = event;
        m["id"] = w["id"].S("");
        if (value) m["value"] = *value;
        SendScriptPanel(sp, m);
    };
    // Widgets; the values are kept here (smooth sliders) and sent when they change.
    for (size_t i = 0; i < sp.ui.Size(); ++i) {
        Json& w = const_cast<Json&>(sp.ui.At(i));
        const std::string type = w["type"].S("text"), label = w["label"].S(w["id"].S("")) + "##" + w["id"].S(std::to_string(i));
        ImGui::PushID((int)i);
        if (type == "text") ImGui::TextWrapped("%s", w["text"].S("").c_str());
        else if (type == "note") ImGui::TextDisabled("%s", w["text"].S("").c_str());
        else if (type == "separator") {
            if (w["label"].S("").empty()) ImGui::Separator();
            else ImGui::SeparatorText(w["label"].S("").c_str());
        } else if (type == "same_line") ImGui::SameLine();
        else if (type == "button") {
            if (ImGui::Button(label.c_str())) send("click", w, nullptr);
        } else if (type == "slider") {
            const bool isInt = w["int"].B(false);
            if (isInt) {
                int v = w["value"].I(0);
                if (ImGui::SliderInt(label.c_str(), &v, w["min"].I(0), w["max"].I(100))) { w["value"] = v; Json jv = v; send("change", w, &jv); }
            } else {
                float v = w["value"].F(0);
                if (ImGui::SliderFloat(label.c_str(), &v, w["min"].F(0), w["max"].F(1), w["format"].S("%.2f").c_str())) { w["value"] = v; Json jv = v; send("change", w, &jv); }
            }
        } else if (type == "checkbox") {
            bool v = w["value"].B(false);
            if (ImGui::Checkbox(label.c_str(), &v)) { w["value"] = v; Json jv = v; send("change", w, &jv); }
        } else if (type == "input") {
            std::string v = w["value"].S("");
            if (ImGui::InputText(label.c_str(), &v)) { w["value"] = v; Json jv = v; send("change", w, &jv); }
        } else if (type == "color") {
            float c[4] = {1, 1, 1, 1};
            const Json& jc = w["value"];
            for (int k = 0; k < 4 && k < (int)jc.Size(); ++k) c[k] = jc.At(k).F(1);
            if (ImGui::ColorEdit4(label.c_str(), c, ImGuiColorEditFlags_NoInputs)) {
                Json jv = Json::Array();
                for (float f : c) jv.Push(f);
                w["value"] = jv;
                send("change", w, &jv);
            }
        } else if (type == "combo") {
            std::string items;
            for (const Json& it : w["items"].Elements()) { items += it.S(""); items.push_back('\0'); }
            items.push_back('\0');
            int v = w["value"].I(0);
            if (ImGui::Combo(label.c_str(), &v, items.c_str())) { w["value"] = v; Json jv = v; send("change", w, &jv); }
        }
        if (w.Has("tooltip") && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", w["tooltip"].S("").c_str());
        ImGui::PopID();
    }
}

}  // namespace sx
