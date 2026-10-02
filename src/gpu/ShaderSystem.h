#pragma once
// Runtime shader compilation with Slang (HLSL-compatible source -> DXIL).
// Compilation happens on a worker thread so the UI never stalls: callers ask for
// a program every frame and simply get nullptr until it is ready. Results are
// cached in memory (by source hash) and on disk (DXIL blobs) for fast startup.
// Shader files in shaders/ are hot-reloaded when modified.

#include "gpu/Device.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace sx::gpu {

struct Program {
    enum class State { Pending, Ready, Failed };
    std::atomic<State> state{State::Pending};
    PipelinePtr pipeline;
    std::string name;
    std::string errors;
    std::string source;
    u64 hash = 0;
};
using ProgramPtr = std::shared_ptr<Program>;

class ShaderSystem {
public:
    bool Init(const std::string& shaderDir, const std::string& cacheDir);
    void Shutdown();

    // Called once per frame: checks shader files for modification (hot reload).
    void Update();

    // Read a shader file from the shader dir with #include "..." expanded.
    // Cached; re-read when the file (or an include) changes on disk.
    const std::string& File(const std::string& relPath);

    // Get or start compiling a compute program from complete source text.
    ProgramPtr Request(const std::string& name, const std::string& source);

    // Convenience: compile a shader file from shaders/ directly.
    const ComputePipeline* Builtin(const std::string& relPath);

    // Synchronous compile; used where a result is needed immediately (tests/tools).
    ProgramPtr CompileNow(const std::string& name, const std::string& source);

    const std::string& ShaderDir() const { return shaderDir; }
    u64 FileGeneration() const { return fileGeneration; }
    std::vector<ProgramPtr> FailedPrograms();

private:
    struct FileEntry {
        std::string expanded;
        std::vector<std::pair<std::string, i64>> deps;  // path, mtime
    };
    bool Expand(const std::string& path, std::string& out, std::vector<std::pair<std::string, i64>>& deps, int depth);
    void Worker();
    void Compile(Program& p);

    std::string shaderDir, cacheDir;
    std::unordered_map<std::string, FileEntry> files;
    std::unordered_map<u64, ProgramPtr> programs;
    std::mutex mutex;
    std::condition_variable cv;
    std::deque<ProgramPtr> queue;
    std::thread worker;
    bool quit = false;
    double lastCheck = 0;
    u64 fileGeneration = 0;
    void* globalSession = nullptr;  // slang::IGlobalSession* (worker thread only)
};

ShaderSystem& Shaders();

}  // namespace sx::gpu
