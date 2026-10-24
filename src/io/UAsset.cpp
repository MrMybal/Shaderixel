// Unreal .uasset support: textures through the CyImgView reader (host shim below), meshes
// through the matching Unreal Editor (glTF export in a temporary project, cached).
#include "io/UAsset.h"

#include <atomic>
#include "core/Json.h"
#include "mesh/Mesh.h"
#include "io/uasset/cyimg_plugin.h"

#include <ShlObj.h>
#include <winioctl.h>
#include <stb_image.h>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

bool CyUAsset_Info(const wchar_t* path, std::string& cls, std::string& engine);
extern "C" const CyImgPlugin* CyImg_GetPlugin(void);

// Decoding context of the shim (opaque to the reader).
struct CyImgContext {
    std::vector<uint8_t> bgra;
    uint32_t w = 0, h = 0;
    std::wstring error, format;
};

namespace sx {

namespace fs = std::filesystem;

namespace {

uint32_t HostTarget(CyImgContext*) { return 0; }
int HostCancelled(CyImgContext*) { return 0; }
uint8_t* HostAlloc(CyImgContext* c, uint32_t w, uint32_t h, int, uint32_t* stride) {
    c->w = w;
    c->h = h;
    c->bgra.assign((size_t)w * h * 4, 0);
    *stride = w * 4;
    return c->bgra.data();
}
int HostDecodeEncoded(CyImgContext* c, const void* data, size_t size) {
    int w = 0, h = 0, comp = 0;
    stbi_uc* px = stbi_load_from_memory((const stbi_uc*)data, (int)size, &w, &h, &comp, 4);
    if (!px) return CYIMG_E_FAIL;
    uint32_t stride = 0;
    uint8_t* dst = HostAlloc(c, (uint32_t)w, (uint32_t)h, 1, &stride);
    for (size_t i = 0; i < (size_t)w * h; ++i) {  // RGBA straight -> BGRA premultiplied
        const uint8_t* s = &px[i * 4];
        uint8_t* d = &dst[i * 4];
        d[0] = (uint8_t)(s[2] * s[3] / 255);
        d[1] = (uint8_t)(s[1] * s[3] / 255);
        d[2] = (uint8_t)(s[0] * s[3] / 255);
        d[3] = s[3];
    }
    stbi_image_free(px);
    return CYIMG_OK;
}
void HostOrientation(CyImgContext*, uint32_t) {}
void HostFormat(CyImgContext* c, const wchar_t* name) { c->format = name ? name : L""; }
void HostInfo(CyImgContext*, const wchar_t*, const wchar_t*) {}
uint32_t HostQuality(CyImgContext*) { return 1; }
void HostRefinable(CyImgContext*) {}
void HostError(CyImgContext* c, const wchar_t* msg) { c->error = msg ? msg : L""; }
uint8_t* HostFrame(CyImgContext*, uint32_t*) { return nullptr; }
void HostDelay(CyImgContext*, uint32_t, uint32_t) {}
const wchar_t* HostTranslate(CyImgContext*, const char*, const wchar_t* fallback) { return fallback; }

const CyImgHost kHost = {CYIMG_API_VERSION, sizeof(CyImgHost), HostTarget, HostCancelled, HostAlloc, HostDecodeEncoded, HostOrientation,
                         HostFormat, HostInfo, HostQuality, HostRefinable, HostError, HostFrame, HostDelay, HostTranslate};

bool IsMeshClass(const std::string& c) { return c == "StaticMesh" || c == "SkeletalMesh"; }

std::string LocalDir(const char* sub) {
    PWSTR p = nullptr;
    std::string out;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &p))) out = Narrow(p) + "\\Shaderixel\\" + sub;
    CoTaskMemFree(p);
    return out;
}

// Installed engines: version ("5.3") -> UnrealEditor-Cmd.exe (launcher manifest, then source builds).
std::vector<std::pair<std::string, std::string>> Engines() {
    std::vector<std::pair<std::string, std::string>> out;
    std::string text;
    Json j;
    if (ReadTextFile("C:\\ProgramData\\Epic\\UnrealEngineLauncher\\LauncherInstalled.dat", text) && Json::Parse(text, j))
        for (auto& e : j["InstallationList"].Elements()) {
            std::string app = e["AppName"].S(""), loc = e["InstallLocation"].S("");
            if (app.rfind("UE_", 0) != 0 || loc.empty()) continue;
            for (const char* exe : {"\\Engine\\Binaries\\Win64\\UnrealEditor-Cmd.exe", "\\Engine\\Binaries\\Win64\\UE4Editor-Cmd.exe"})
                if (fs::exists(Utf8Path(loc + exe))) {
                    out.push_back({app.substr(3), loc + exe});
                    break;
                }
        }
    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Epic Games\\Unreal Engine\\Builds", 0, KEY_READ, &key) == ERROR_SUCCESS) {
        wchar_t name[256], value[1024];
        for (DWORD i = 0;; ++i) {
            DWORD nl = 256, vl = sizeof(value), type = 0;
            if (RegEnumValueW(key, i, name, &nl, nullptr, &type, (BYTE*)value, &vl) != ERROR_SUCCESS) break;
            if (type != REG_SZ) continue;
            std::string loc = Narrow(value);
            if (fs::exists(Utf8Path(loc + "\\Engine\\Binaries\\Win64\\UnrealEditor-Cmd.exe"))) out.push_back({Narrow(name), loc + "\\Engine\\Binaries\\Win64\\UnrealEditor-Cmd.exe"});
        }
        RegCloseKey(key);
    }
    return out;
}

std::pair<int, int> Version(const std::string& v) {
    int a = 0, b = 0;
    if (sscanf_s(v.c_str(), "%d.%d", &a, &b) < 1) return {0, 0};
    return {a, b};
}

// The engine of the asset: the project's association, else the package version (or the next newer one).
std::string PickEngine(const std::string& association, const std::string& packageVersion) {
    if (char env[1024]; GetEnvironmentVariableA("SHADERIXEL_UNREAL_EDITOR", env, sizeof(env)) > 0) return env;
    auto engines = Engines();
    for (auto& e : engines)
        if (!association.empty() && e.first == association) return e.second;
    auto want = Version(association.empty() || association[0] == '{' ? packageVersion : association);
    std::string best;
    std::pair<int, int> bestV = {999, 999};
    for (auto& e : engines) {
        auto v = Version(e.first);
        if (v.first == 0) continue;
        if (v >= want && v < bestV) { bestV = v; best = e.second; }
    }
    if (best.empty() && !engines.empty()) best = engines.back().second;
    return best;
}

// NTFS junction (like mklink /J: no administrator rights needed), set directly on an empty folder with
// FSCTL_SET_REPARSE_POINT: no command line, so no cmd.exe expansion of % or quotes in the paths.
bool Junction(const fs::path& link, const fs::path& target) {
    std::error_code ec;
    std::wstring abs = fs::absolute(target, ec).lexically_normal().wstring();
    if (ec || abs.empty()) return false;
    if (abs.rfind(L"\\\\?\\", 0) == 0) abs = abs.substr(4);
    if (abs.size() < 3 || abs[1] != L':') return false;  // junctions point at local drive paths only
    while (abs.size() > 3 && (abs.back() == L'\\' || abs.back() == L'/')) abs.pop_back();
    const std::wstring sub = L"\\??\\" + abs;  // NT path of the target
    const size_t subBytes = sub.size() * sizeof(wchar_t), printBytes = abs.size() * sizeof(wchar_t);
    // REPARSE_DATA_BUFFER with a MountPointReparseBuffer (the structure is not in the user-mode headers):
    // tag, data length, reserved, then substitute / print name offsets and lengths and the two names, each null-terminated.
    const size_t dataBytes = 4 * sizeof(USHORT) + subBytes + sizeof(wchar_t) + printBytes + sizeof(wchar_t);
    if (dataBytes > 16 * 1024 - 8) return false;
    std::vector<BYTE> buf(8 + dataBytes, 0);
    const DWORD tag = IO_REPARSE_TAG_MOUNT_POINT;
    const USHORT dataLen = (USHORT)dataBytes;
    const USHORT names[4] = {0, (USHORT)subBytes, (USHORT)(subBytes + sizeof(wchar_t)), (USHORT)printBytes};
    memcpy(buf.data(), &tag, 4);
    memcpy(buf.data() + 4, &dataLen, 2);
    memcpy(buf.data() + 8, names, sizeof(names));
    memcpy(buf.data() + 16, sub.c_str(), subBytes);
    memcpy(buf.data() + 16 + subBytes + sizeof(wchar_t), abs.c_str(), printBytes);
    if (!CreateDirectoryW(link.c_str(), nullptr)) return false;
    HANDLE h = CreateFileW(link.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        RemoveDirectoryW(link.c_str());
        return false;
    }
    DWORD ret = 0;
    const BOOL ok = DeviceIoControl(h, FSCTL_SET_REPARSE_POINT, buf.data(), (DWORD)buf.size(), nullptr, 0, &ret, nullptr);
    CloseHandle(h);
    if (!ok) {
        RemoveDirectoryW(link.c_str());  // still an empty folder
        return false;
    }
    return fs::exists(link, ec);
}

// Real path of an existing folder (resolves redirections such as packaged-app AppData virtualization).
fs::path FinalPath(const fs::path& p) {
    HANDLE h = CreateFileW(p.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (h == INVALID_HANDLE_VALUE) return p;
    std::wstring buf(32768, L'\0');
    DWORD n = GetFinalPathNameByHandleW(h, buf.data(), (DWORD)buf.size(), FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    CloseHandle(h);
    if (!n || n >= buf.size()) return p;
    buf.resize(n);
    if (buf.rfind(L"\\\\?\\", 0) == 0) buf.erase(0, 4);
    return fs::path(buf);
}

std::string Stamp(const std::string& path) {
    std::error_code ec;
    auto t = fs::last_write_time(Utf8Path(path), ec).time_since_epoch().count();
    auto s = fs::file_size(Utf8Path(path), ec);
    return std::to_string(t) + ":" + std::to_string(s);
}

std::string CacheBase(const std::string& path) {
    u64 h = 1469598103934665603ull;
    for (unsigned char c : path) { h ^= c; h *= 1099511628211ull; }
    char hex[24];
    snprintf(hex, sizeof(hex), "%016llx", (unsigned long long)h);
    return LocalDir("UAssetCache") + "\\" + hex;
}

const char* kExportScript = R"PY(
import json, pathlib, traceback
import unreal

req = json.loads(pathlib.Path(REQUEST).read_text(encoding='utf-8'))
try:
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous([req['package'].rsplit('/', 1)[0]], force_rescan=True)
    asset = unreal.load_asset(req['package'])
    if not isinstance(asset, (unreal.StaticMesh, unreal.SkeletalMesh)):
        raise RuntimeError("ce .uasset n'est pas un StaticMesh / SkeletalMesh chargeable par cette version d'Unreal")
    done = False
    if hasattr(unreal, 'GLTFExporter') and hasattr(unreal.GLTFExporter, 'export_to_gltf'):
        options = unreal.GLTFExportOptions()
        options.export_uniform_scale = 0.01
        options.bake_material_inputs = unreal.GLTFMaterialBakeMode.DISABLED
        unreal.GLTFExporter.export_to_gltf(asset, req['target'], options, set())
        done = pathlib.Path(req['target']).is_file()
    if not done:
        task = unreal.AssetExportTask()
        task.object = asset
        task.filename = req['target']
        task.automated = True
        task.prompt = False
        task.replace_identical = True
        unreal.Exporter.run_asset_export_task(task)
    if not pathlib.Path(req['target']).is_file():
        raise RuntimeError("export glTF impossible (plugin GLTFExporter ?)")
    # Materials (the glTF has no textures without baking): base color texture and scalar values of
    # each slot's material, by material name.
    MEL = unreal.MaterialEditingLibrary
    slots = asset.static_materials if isinstance(asset, unreal.StaticMesh) else asset.materials
    table = {}
    texdir = pathlib.Path(req['textures'])
    texdir.mkdir(parents=True, exist_ok=True)
    def role(name):
        n = name.lower()
        if 'normal' in n or n.endswith('_n'):
            return ''
        for k in ('basecolor', 'base_color', 'albedo', 'diffuse', 'color', 'colour', '_bc', 'tint'):
            if k in n:
                return 'base'
        return ''
    for slot in slots:
        mi = slot.material_interface
        if mi is None or mi.get_name() in table:
            continue
        entry = {}
        try:
            base = mi.get_base_material()
            names = [str(n) for n in MEL.get_texture_parameter_names(base)]
            tex = None
            for n in sorted(names, key=lambda s: 0 if role(s) else 1):
                if not role(n):
                    continue
                if isinstance(mi, unreal.MaterialInstance):
                    tex = MEL.get_material_instance_texture_parameter_value(mi, n)
                if tex is None:
                    tex = MEL.get_material_default_texture_parameter_value(base, n)
                if tex is not None:
                    break
            if tex is not None:
                out = texdir / (tex.get_name() + '.png')
                task = unreal.AssetExportTask()
                task.object = tex
                task.filename = str(out)
                task.automated = True
                task.prompt = False
                task.replace_identical = True
                unreal.Exporter.run_asset_export_task(task)
                if out.is_file():
                    entry['base'] = str(out)
            for key, pname in (('roughness', 'Roughness'), ('metallic', 'Metallic')):
                for n in [str(n) for n in MEL.get_scalar_parameter_names(base)]:
                    if pname.lower() in n.lower():
                        if isinstance(mi, unreal.MaterialInstance):
                            entry[key] = MEL.get_material_instance_scalar_parameter_value(mi, n)
                        else:
                            entry[key] = MEL.get_material_default_scalar_parameter_value(base, n)
                        break
        except Exception as e:
            unreal.log_warning('Shaderixel: material %s: %s' % (mi.get_name(), e))
        table[mi.get_name()] = entry
    pathlib.Path(req['materials']).write_text(json.dumps(table, indent=1), encoding='utf-8')
except Exception as error:
    pathlib.Path(req['error']).write_text(str(error), encoding='utf-8')
    traceback.print_exc()
    raise
)PY";

std::string Py(const std::string& s) {  // Python string literal (UTF-8 source), every special character escaped
    std::string t = "'";
    for (char c : s) {
        if (c == '\\') t += '/';  // Windows separators as forward slashes (Unreal and pathlib accept both)
        else if (c == '\'') t += "\\'";
        else if (c == '\n') t += "\\n";
        else if (c == '\r') t += "\\r";
        else if (c == '\0') t += "\\x00";
        else t += c;
    }
    return t + "'";
}

}  // namespace

bool ReadUAssetInfo(const std::string& path, UAssetInfo& out) {
    out = {};
    if (!CyUAsset_Info(Widen(path).c_str(), out.cls, out.engine)) return false;
    out.mesh = IsMeshClass(out.cls);
    out.texture = !out.mesh && out.cls.find("Texture") != std::string::npos;
    return true;
}

bool LoadUAssetImage(const std::string& path, std::vector<u8>& rgba, u32& w, u32& h, std::string* error, std::string* format) {
    CyImgContext ctx;
    const CyImgPlugin* plugin = CyImg_GetPlugin();
    // The reader decodes PNG / JPEG payloads with WIC: COM on this thread.
    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    int r = plugin->decode(&kHost, &ctx, Widen(path).c_str());
    if (SUCCEEDED(com)) CoUninitialize();
    if (r != CYIMG_OK || ctx.bgra.empty()) {
        if (error) *error = ctx.error.empty() ? std::string("texture illisible") : Narrow(ctx.error);
        return false;
    }
    w = ctx.w;
    h = ctx.h;
    rgba.resize(ctx.bgra.size());
    for (size_t i = 0; i < (size_t)w * h; ++i) {  // BGRA premultiplied -> RGBA straight
        const u8* s = &ctx.bgra[i * 4];
        u8* d = &rgba[i * 4];
        const int a = s[3];
        auto un = [a](int c) { return (u8)(a ? std::min(255, (c * 255 + a / 2) / a) : 0); };
        d[0] = un(s[2]);
        d[1] = un(s[1]);
        d[2] = un(s[0]);
        d[3] = (u8)a;
    }
    if (format) *format = Narrow(ctx.format);
    return true;
}

void ApplyUAssetMaterials(const std::string& glb, MeshData& mesh) {
    std::string text;
    Json j;
    const std::string table = Narrow(Utf8Path(glb).replace_extension(".materials.json").wstring());
    if (!ReadTextFile(table, text) || !Json::Parse(text, j)) return;
    for (auto& m : mesh.materials) {
        const Json& e = j[m.name];
        if (!e.IsObject()) continue;
        if (e.Has("base")) {
            m.baseColorPath = e["base"].S("");
            m.baseColorData.clear();
            m.baseColor = {1, 1, 1, 1};
        }
        // The glTF exported without baking has no PBR values: Unreal's, else a dielectric.
        m.roughness = e["roughness"].F(0.6f);
        m.metallic = e["metallic"].F(0.0f);
    }
}

std::string UAssetMeshCached(const std::string& path) {
    const std::string base = CacheBase(path);
    std::string stamp;
    if (!ReadTextFile(base + ".src", stamp) || stamp != Stamp(path) || !fs::exists(Utf8Path(base + ".glb"))) return "";
    return base + ".glb";
}

static std::atomic<bool> gCancelConversions{false};
void CancelUAssetConversions() { gCancelConversions = true; }

bool ConvertUAssetMesh(const std::string& path, std::string& glbOut, std::string* error, const std::function<void(const std::string&)>& progress) {
    auto fail = [&](const std::string& m) {
        if (error) *error = m;
        return false;
    };
    UAssetInfo info;
    if (!ReadUAssetInfo(path, info)) return fail("package Unreal illisible");
    if (!info.mesh) return fail("ce .uasset est un " + info.cls + ", pas un maillage");
    // The Content folder of its project (or of the engine) and the package name.
    fs::path input = fs::absolute(Utf8Path(path)), content;
    for (auto p = input.parent_path(); !p.empty() && p != p.parent_path(); p = p.parent_path())
        if (_wcsicmp(p.filename().c_str(), L"Content") == 0) { content = p; break; }
    if (content.empty()) return fail("le .uasset doit rester dans le dossier Content de son projet (avec ses dépendances)");
    const bool engine = _wcsicmp(content.parent_path().filename().c_str(), L"Engine") == 0;
    std::string association;
    if (!engine) {
        std::error_code ec;
        for (auto& e : fs::directory_iterator(content.parent_path(), ec))
            if (e.path().extension() == ".uproject") {
                std::string text;
                Json j;
                if (ReadTextFile(Narrow(e.path().wstring()), text) && Json::Parse(text, j)) association = j["EngineAssociation"].S("");
                break;
            }
    }
    fs::path rel = fs::relative(input, content);
    rel.replace_extension();
    const std::string package = (engine ? "/Engine/" : "/Game/") + Narrow(rel.generic_wstring());
    const std::string editor = PickEngine(association, info.engine);
    if (editor.empty()) return fail("aucun Unreal Editor installé pour ce projet (" + (association.empty() ? info.engine : association) + ")");

    // Temporary project: the Content folders mounted (junctions), writable folders local.
    const std::string base = CacheBase(path);
    std::error_code ec;
    fs::create_directories(Utf8Path(LocalDir("UAssetCache")), ec);
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    fs::path job = Utf8Path(LocalDir("UAssetJobs")) / ("job-" + std::to_string(now));
    fs::create_directories(job / "Content", ec);
    job = FinalPath(job);  // the real folder (virtualized AppData of packaged parents): what Unreal will see
    std::vector<fs::path> mounts;
    if (!engine)
        for (auto& e : fs::directory_iterator(content, ec)) {
            std::wstring name = e.path().filename().wstring(), lower = name;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
            if (e.is_directory() && lower != L"developers" && lower != L"collections") {
                if (Junction(job / "Content" / name, e.path())) mounts.push_back(job / "Content" / name);
            } else if (e.is_regular_file()) {
                fs::copy_file(e.path(), job / "Content" / name, ec);
            }
        }
    auto cleanup = [&]() {
        if (GetEnvironmentVariableA("SHADERIXEL_KEEP_UASSET_JOB", nullptr, 0) > 0) return;  // diagnosis
        for (auto& m : mounts) RemoveDirectoryW(m.c_str());  // the link only, never the project's files
        std::error_code e2;
        bool linksGone = true;
        for (auto& m : mounts) linksGone &= !fs::exists(m, e2);
        if (linksGone) fs::remove_all(job, e2);
    };
    const fs::path target = job / "model.glb", err = job / "error.txt", req = job / "request.json", script = job / "export.py";
    {
        Json r = Json::Object();
        r["package"] = package;
        r["target"] = Narrow(target.wstring());
        r["error"] = Narrow(err.wstring());
        r["textures"] = base + "_tex";
        r["materials"] = base + ".materials.json";
        std::ofstream(req, std::ios::binary) << r.Dump(1);
        std::ofstream(job / "Preview.uproject", std::ios::binary)
            << "{\"FileVersion\":3,\"Plugins\":[{\"Name\":\"PythonScriptPlugin\",\"Enabled\":true},{\"Name\":\"GLTFExporter\",\"Enabled\":true}]}";
        std::ofstream(script, std::ios::binary) << "REQUEST = " << Py(Narrow(req.wstring())) << "\n" << kExportScript;
    }
    if (progress) progress("Conversion par " + Narrow(Utf8Path(editor).parent_path().parent_path().parent_path().parent_path().filename().wstring()) + "...");
    std::wstring cmd = L"\"" + Widen(editor) + L"\" \"" + (job / "Preview.uproject").wstring() + L"\" -run=pythonscript -script=\"" + script.wstring() +
                       L"\" -unattended -nosplash -NoSound -NoSourceControl -NoShaderCompile -NoSave -nullrhi -ddc=NoShared -abslog=\"" +
                       (job / "unreal.log").wstring() + L"\" -UserDir=\"" + (job / "User").wstring() + L"\"";
    // Environment: local DDC in the job, and older RunUAT-style lookups unaffected.
    std::wstring env;
    if (wchar_t* block = GetEnvironmentStringsW()) {
        for (wchar_t* p = block; *p; p += wcslen(p) + 1)
            if (_wcsnicmp(p, L"NoDefaultCurrentDirectoryInExePath=", 35) != 0) {
                env += p;
                env += L'\0';
            }
        FreeEnvironmentStringsW(block);
    }
    // One DDC for every conversion (faster after the first one), on a short path: Unreal refuses cache
    // paths longer than 119 characters.
    fs::create_directories(Utf8Path(LocalDir("DDC")), ec);
    fs::path ddc = FinalPath(Utf8Path(LocalDir("DDC")));
    if (ddc.wstring().size() > 110) {
        wchar_t tmp[MAX_PATH];
        GetTempPathW(MAX_PATH, tmp);
        ddc = FinalPath(fs::path(tmp)) / "SxDDC";
        fs::create_directories(ddc, ec);
    }
    env += L"UE-LocalDataCachePath=" + ddc.wstring() + L'\0';
    env += L"UE-SharedDataCachePath=None";
    env += L'\0';
    STARTUPINFOW si = {sizeof(si)};
    PROCESS_INFORMATION pi = {};
    std::vector<wchar_t> line(cmd.begin(), cmd.end());
    line.push_back(0);
    if (!CreateProcessW(nullptr, line.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT, (void*)env.c_str(),
                        job.c_str(), &si, &pi)) {
        cleanup();
        return fail("impossible de lancer " + editor);
    }
    // Wait (30 minutes at most), or stop when the application closes.
    for (int waited = 0; waited < 30 * 60 * 1000 && WaitForSingleObject(pi.hProcess, 200) == WAIT_TIMEOUT; waited += 200)
        if (gCancelConversions) {
            TerminateProcess(pi.hProcess, 1);
            WaitForSingleObject(pi.hProcess, 5000);
            break;
        }
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    if (!fs::exists(target)) {
        std::string why;
        ReadTextFile(Narrow(err.wstring()), why);
        fs::copy_file(job / "unreal.log", Utf8Path(base + ".log"), fs::copy_options::overwrite_existing, ec);  // for diagnosis
        cleanup();
        return fail("Unreal n'a pas exporté le maillage" + (why.empty() ? " (code " + std::to_string(code) + ")" : " : " + why));
    }
    fs::copy_file(target, Utf8Path(base + ".glb"), fs::copy_options::overwrite_existing, ec);
    std::ofstream(Utf8Path(base + ".src"), std::ios::binary) << Stamp(path);
    cleanup();
    glbOut = base + ".glb";
    return true;
}

}  // namespace sx
