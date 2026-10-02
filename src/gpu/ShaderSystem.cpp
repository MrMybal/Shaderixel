#include "gpu/ShaderSystem.h"

#include <slang.h>
#include <slang-com-ptr.h>

#include <chrono>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace sx::gpu {

namespace {
ShaderSystem g_shaders;

i64 FileTime(const std::string& path) {
    std::error_code ec;
    auto t = fs::last_write_time(Utf8Path(path), ec);
    return ec ? 0 : (i64)t.time_since_epoch().count();
}

double Now() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

u64 HashStr(const std::string& s) {
    Hasher h;
    h.Str(s);
    return h.h;
}
}  // namespace

ShaderSystem& Shaders() { return g_shaders; }

bool ShaderSystem::Init(const std::string& shaderDir_, const std::string& cacheDir_) {
    shaderDir = shaderDir_;
    cacheDir = cacheDir_;
    std::error_code ec;
    fs::create_directories(Utf8Path(cacheDir), ec);
    worker = std::thread([this] { Worker(); });
    return true;
}

void ShaderSystem::Shutdown() {
    {
        std::lock_guard lock(mutex);
        quit = true;
    }
    cv.notify_all();
    if (worker.joinable()) worker.join();
    std::lock_guard lock(mutex);
    programs.clear();
}

bool ShaderSystem::Expand(const std::string& path, std::string& out, std::vector<std::pair<std::string, i64>>& deps, int depth) {
    if (depth > 16) return false;
    for (auto& d : deps) if (d.first == path) return true;  // include once
    std::string text;
    if (!ReadTextFile(path, text)) {
        out += "#error \"cannot open " + path + "\"\n";
        return false;
    }
    deps.push_back({path, FileTime(path)});
    size_t pos = 0;
    while (pos < text.size()) {
        size_t eol = text.find('\n', pos);
        if (eol == std::string::npos) eol = text.size();
        std::string line = text.substr(pos, eol - pos);
        size_t first = line.find_first_not_of(" \t");
        if (first != std::string::npos && line.compare(first, 8, "#include") == 0) {
            size_t q0 = line.find('"', first);
            size_t q1 = q0 == std::string::npos ? q0 : line.find('"', q0 + 1);
            if (q1 != std::string::npos) {
                std::string inc = line.substr(q0 + 1, q1 - q0 - 1);
                Expand(shaderDir + "/" + inc, out, deps, depth + 1);
                out += "\n";
                pos = eol + 1;
                continue;
            }
        }
        out.append(line);
        out += "\n";
        pos = eol + 1;
    }
    return true;
}

const std::string& ShaderSystem::File(const std::string& relPath) {
    auto it = files.find(relPath);
    if (it == files.end()) {
        FileEntry e;
        Expand(shaderDir + "/" + relPath, e.expanded, e.deps, 0);
        it = files.emplace(relPath, std::move(e)).first;
    }
    return it->second.expanded;
}

void ShaderSystem::Update() {
    double t = Now();
    if (t - lastCheck < 0.5) return;
    lastCheck = t;
    for (auto& [rel, e] : files) {
        bool changed = false;
        for (auto& [path, time] : e.deps)
            if (FileTime(path) != time) { changed = true; break; }
        if (changed) {
            e.expanded.clear();
            e.deps.clear();
            Expand(shaderDir + "/" + rel, e.expanded, e.deps, 0);
            ++fileGeneration;
            Log("Shader reloaded: %s", rel.c_str());
        }
    }
}

ProgramPtr ShaderSystem::Request(const std::string& name, const std::string& source) {
    u64 h = HashStr(source);
    std::lock_guard lock(mutex);
    auto it = programs.find(h);
    if (it != programs.end()) return it->second;
    auto p = std::make_shared<Program>();
    p->name = name;
    p->source = source;
    p->hash = h;
    programs[h] = p;
    queue.push_back(p);
    cv.notify_one();
    return p;
}

ProgramPtr ShaderSystem::CompileNow(const std::string& name, const std::string& source) {
    u64 h = HashStr(source);
    ProgramPtr p;
    {
        std::lock_guard lock(mutex);
        auto it = programs.find(h);
        if (it != programs.end()) {
            p = it->second;
        } else {
            p = std::make_shared<Program>();
            p->name = name;
            p->source = source;
            p->hash = h;
            programs[h] = p;
            queue.push_front(p);
            cv.notify_one();
        }
    }
    while (p->state.load() == Program::State::Pending) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    return p;
}

const ComputePipeline* ShaderSystem::Builtin(const std::string& relPath) {
    ProgramPtr p = Request(relPath, File(relPath));
    return p->state.load() == Program::State::Ready ? p->pipeline.get() : nullptr;
}

std::vector<ProgramPtr> ShaderSystem::FailedPrograms() {
    std::vector<ProgramPtr> out;
    std::lock_guard lock(mutex);
    for (auto& [h, p] : programs) if (p->state.load() == Program::State::Failed) out.push_back(p);
    return out;
}

void ShaderSystem::Worker() {
    slang::IGlobalSession* gs = nullptr;
    if (SLANG_FAILED(slang::createGlobalSession(&gs))) {
        Log("Slang: failed to create global session");
    }
    globalSession = gs;
    for (;;) {
        ProgramPtr p;
        {
            std::unique_lock lock(mutex);
            cv.wait(lock, [this] { return quit || !queue.empty(); });
            if (quit) break;
            p = queue.front();
            queue.pop_front();
        }
        Compile(*p);
    }
    if (gs) gs->release();
    globalSession = nullptr;
}

ProgramPtr ShaderSystem::CompileGraphicsNow(const std::string& name, const std::string& source) {
    u64 h = HashStr("GFX:" + source);
    ProgramPtr p;
    {
        std::lock_guard lock(mutex);
        auto it = programs.find(h);
        if (it != programs.end()) p = it->second;
        else {
            p = std::make_shared<Program>();
            p->name = name;
            p->source = source;
            p->hash = h;
            p->graphics = true;
            programs[h] = p;
            queue.push_front(p);
            cv.notify_one();
        }
    }
    while (p->state.load() == Program::State::Pending) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    return p;
}

void ShaderSystem::Compile(Program& p) {
    if (p.graphics) {
        auto* gs = static_cast<slang::IGlobalSession*>(globalSession);
        if (!gs) { p.errors = "Slang unavailable"; p.state = Program::State::Failed; return; }
        slang::TargetDesc target = {};
        target.format = SLANG_DXIL;
        target.profile = gs->findProfile("sm_6_5");
        slang::SessionDesc sd = {};
        sd.targets = &target;
        sd.targetCount = 1;
        Slang::ComPtr<slang::ISession> session;
        gs->createSession(sd, session.writeRef());
        Slang::ComPtr<slang::IBlob> diag;
        std::string modName = "g" + std::to_string(p.hash);
        slang::IModule* mod = session ? session->loadModuleFromSourceString(modName.c_str(), (p.name + ".slang").c_str(), p.source.c_str(), diag.writeRef()) : nullptr;
        Slang::ComPtr<slang::IEntryPoint> vsE, psE;
        if (!mod || SLANG_FAILED(mod->findAndCheckEntryPoint("vsMain", SLANG_STAGE_VERTEX, vsE.writeRef(), diag.writeRef())) ||
            SLANG_FAILED(mod->findAndCheckEntryPoint("psMain", SLANG_STAGE_FRAGMENT, psE.writeRef(), diag.writeRef()))) {
            p.errors = diag ? (const char*)diag->getBufferPointer() : "vsMain/psMain introuvables";
            Log("Shader '%s' failed: %s", p.name.c_str(), p.errors.c_str());
            p.state = Program::State::Failed;
            return;
        }
        slang::IComponentType* parts[] = {mod, vsE, psE};
        Slang::ComPtr<slang::IComponentType> composite, linked;
        session->createCompositeComponentType(parts, 3, composite.writeRef(), diag.writeRef());
        Slang::ComPtr<slang::IBlob> v, f;
        if (!composite || SLANG_FAILED(composite->link(linked.writeRef(), diag.writeRef())) ||
            SLANG_FAILED(linked->getEntryPointCode(0, 0, v.writeRef(), diag.writeRef())) || SLANG_FAILED(linked->getEntryPointCode(1, 0, f.writeRef(), diag.writeRef()))) {
            p.errors = diag ? (const char*)diag->getBufferPointer() : "échec de compilation graphique";
            Log("Shader '%s' failed: %s", p.name.c_str(), p.errors.c_str());
            p.state = Program::State::Failed;
            return;
        }
        p.vs.assign((const u8*)v->getBufferPointer(), (const u8*)v->getBufferPointer() + v->getBufferSize());
        p.ps.assign((const u8*)f->getBufferPointer(), (const u8*)f->getBufferPointer() + f->getBufferSize());
        Log("Compiled graphics '%s'", p.name.c_str());
        p.state = Program::State::Ready;
        return;
    }
    // Disk cache: <hash>.dxil with a 8-byte header (group sizes).
    char fname[64];
    snprintf(fname, sizeof(fname), "%016llx.dxil", (unsigned long long)p.hash);
    std::string cachePath = cacheDir + "/" + fname;
    {
        std::ifstream f(Utf8Path(cachePath), std::ios::binary);
        if (f) {
            u32 gx = 8, gy = 8;
            f.read((char*)&gx, 4);
            f.read((char*)&gy, 4);
            std::vector<char> blob((std::istreambuf_iterator<char>(f)), {});
            if (!blob.empty()) {
                p.pipeline = Dev().CreatePipeline(blob.data(), blob.size(), gx, gy);
                if (p.pipeline) { p.state = Program::State::Ready; return; }
            }
        }
    }

    auto* gs = static_cast<slang::IGlobalSession*>(globalSession);
    if (!gs) { p.errors = "Slang unavailable"; p.state = Program::State::Failed; return; }

    auto t0 = Now();
    slang::TargetDesc target = {};
    target.format = SLANG_DXIL;
    target.profile = gs->findProfile("sm_6_5");
    slang::SessionDesc sd = {};
    sd.targets = &target;
    sd.targetCount = 1;
    const char* search[] = {shaderDir.c_str()};
    sd.searchPaths = search;
    sd.searchPathCount = 1;
    Slang::ComPtr<slang::ISession> session;
    if (SLANG_FAILED(gs->createSession(sd, session.writeRef()))) {
        p.errors = "createSession failed";
        p.state = Program::State::Failed;
        return;
    }

    Slang::ComPtr<slang::IBlob> diag;
    std::string modName = "m" + std::to_string(p.hash);
    slang::IModule* mod = session->loadModuleFromSourceString(modName.c_str(), (p.name + ".slang").c_str(), p.source.c_str(), diag.writeRef());
    if (!mod) {
        p.errors = diag ? (const char*)diag->getBufferPointer() : "module load failed";
        Log("Shader '%s' failed:\n%s", p.name.c_str(), p.errors.c_str());
        p.state = Program::State::Failed;
        return;
    }
    Slang::ComPtr<slang::IEntryPoint> entry;
    if (SLANG_FAILED(mod->findAndCheckEntryPoint("main", SLANG_STAGE_COMPUTE, entry.writeRef(), diag.writeRef()))) {
        p.errors = diag ? (const char*)diag->getBufferPointer() : "entry point 'main' not found";
        Log("Shader '%s' failed:\n%s", p.name.c_str(), p.errors.c_str());
        p.state = Program::State::Failed;
        return;
    }
    slang::IComponentType* parts[] = {mod, entry};
    Slang::ComPtr<slang::IComponentType> composite, linked;
    session->createCompositeComponentType(parts, 2, composite.writeRef(), diag.writeRef());
    if (!composite || SLANG_FAILED(composite->link(linked.writeRef(), diag.writeRef()))) {
        p.errors = diag ? (const char*)diag->getBufferPointer() : "link failed";
        p.state = Program::State::Failed;
        return;
    }
    Slang::ComPtr<slang::IBlob> code;
    if (SLANG_FAILED(linked->getEntryPointCode(0, 0, code.writeRef(), diag.writeRef())) || !code) {
        p.errors = diag ? (const char*)diag->getBufferPointer() : "code generation failed";
        Log("Shader '%s' failed:\n%s", p.name.c_str(), p.errors.c_str());
        p.state = Program::State::Failed;
        return;
    }
    SlangUInt groups[3] = {8, 8, 1};
    if (auto* layout = linked->getLayout()) {
        if (auto* ep = layout->getEntryPointByIndex(0)) ep->getComputeThreadGroupSize(3, groups);
    }
    p.pipeline = Dev().CreatePipeline(code->getBufferPointer(), code->getBufferSize(), (u32)groups[0], (u32)groups[1]);
    if (!p.pipeline) {
        p.errors = "pipeline creation failed";
        p.state = Program::State::Failed;
        return;
    }
    std::ofstream f(Utf8Path(cachePath), std::ios::binary);
    u32 gx = (u32)groups[0], gy = (u32)groups[1];
    f.write((const char*)&gx, 4);
    f.write((const char*)&gy, 4);
    f.write((const char*)code->getBufferPointer(), code->getBufferSize());
    Log("Compiled '%s' in %.0f ms", p.name.c_str(), (Now() - t0) * 1000.0);
    p.state = Program::State::Ready;
}

}  // namespace sx::gpu
