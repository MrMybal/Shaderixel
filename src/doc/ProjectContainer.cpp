#include "doc/DocFile.h"
#include "core/Deflate.h"
#include <stb_image.h>
#include <cstring>
#include <fstream>
#include <functional>
#include <mutex>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#include <compressapi.h>
#pragma comment(lib, "Cabinet.lib")
#else
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#endif

namespace sx {
namespace {
// Version 1: XPRESS_HUFF (Windows only, still readable on Windows).
// Version 2: byte-plane shuffle, zlib (deflate, own encoder / stb inflate) in 8 MB chunks (portable):
//            [u32 chunks]{[u32 raw][u32 packed][bytes]}...
constexpr u32 kVersion = 3;
constexpr u64 kMaxJsonBytes = 256ull << 20;
constexpr size_t kChunk = 8u << 20;

// Reserve a sibling atomically. Independent application instances never write
// into each other's staging file, and unrelated .tmp files are left alone.
struct TemporaryProject {
    std::filesystem::path path;
    bool Reserve(const std::filesystem::path& destination) {
        static std::atomic<u64> sequence{0};
        for (int attempt = 0; attempt < 64; ++attempt) {
#ifdef _WIN32
            const auto pid = GetCurrentProcessId();
#else
            const auto pid = getpid();
#endif
            auto candidate = destination.parent_path() / (".shaderixel-" + std::to_string(pid) + "-" + std::to_string(++sequence) + ".tmp");
#ifdef _WIN32
            HANDLE handle = CreateFileW(candidate.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (handle == INVALID_HANDLE_VALUE) {
                if (GetLastError() == ERROR_FILE_EXISTS) continue;
                return false;
            }
            CloseHandle(handle);
#else
            int fd = open(candidate.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
            if (fd < 0) { if (errno == EEXIST) continue; return false; }
            close(fd);
#endif
            path = std::move(candidate);
            return true;
        }
        return false;
    }
    ~TemporaryProject() {
        if (!path.empty()) { std::error_code ec; std::filesystem::remove(path, ec); }
    }
};

bool FitsJsonDepth(const Json& value, unsigned depth) {
    if (depth > 256) return false;  // Json::ParseValue limit
    for (const auto& [name, child] : value.Items())
        if (!FitsJsonDepth(child, depth + 1)) return false;
    for (const auto& child : value.Elements())
        if (!FitsJsonDepth(child, depth + 1)) return false;
    return true;
}
u32 ElemSize(gpu::Format f) {
    switch (f) {
        case gpu::Format::RGBA16F: case gpu::Format::R16F: return 2;
        case gpu::Format::RGBA32F: case gpu::Format::R32F: case gpu::Format::R32U: return 4;
        default: return 1;
    }
}

// Byte-plane shuffle: group byte k of every element together.
std::vector<u8> Shuffle(const std::vector<u8>& in, u32 elem) {
    if (elem <= 1) return in;
    size_t n = in.size() / elem;
    std::vector<u8> out(in.size());
    for (u32 b = 0; b < elem; ++b) {
        u8* dst = out.data() + b * n;
        for (size_t i = 0; i < n; ++i) dst[i] = in[i * elem + b];
    }
    return out;
}

std::vector<u8> Unshuffle(const std::vector<u8>& in, u32 elem) {
    if (elem <= 1) return in;
    size_t n = in.size() / elem;
    std::vector<u8> out(in.size());
    for (u32 b = 0; b < elem; ++b) {
        const u8* src = in.data() + b * n;
        for (size_t i = 0; i < n; ++i) out[i * elem + b] = src[i];
    }
    return out;
}

#ifdef _WIN32
bool DecompressXpress(const std::vector<u8>& in, size_t rawSize, std::vector<u8>& out) {
    DECOMPRESSOR_HANDLE h = nullptr;
    if (!CreateDecompressor(COMPRESS_ALGORITHM_XPRESS_HUFF, nullptr, &h)) return false;
    out.resize(rawSize);
    SIZE_T got = 0;
    bool ok = ::Decompress(h, in.data(), in.size(), out.data(), out.size(), &got) != FALSE && got == rawSize;
    CloseDecompressor(h);
    return ok;
}
#endif

// Runs fn(i) for i in [0, count) on all hardware threads.
void ParallelFor(size_t count, const std::function<void(size_t)>& fn) {
    if (!count) return;
    std::atomic<size_t> next{0};
    unsigned n = std::max(1u, std::min<unsigned>(std::thread::hardware_concurrency(), (unsigned)count));
    std::mutex errorLock;
    std::exception_ptr failure;
    std::vector<std::jthread> pool;
    for (unsigned t = 0; t < n; ++t)
        pool.emplace_back([&] {
            try {
                for (size_t i; (i = next.fetch_add(1)) < count;) fn(i);
            } catch (...) {
                next.store(count);
                std::lock_guard lock(errorLock);
                if (!failure) failure = std::current_exception();
            }
        });
    for (auto& th : pool) th.join();
    if (failure) std::rethrow_exception(failure);
}

// Chunked deflate of every buffer, all chunks of all buffers in parallel.
std::vector<std::vector<u8>> PackAll(const std::vector<std::vector<u8>>& raws) {
    struct Job { size_t buf, off, len; std::vector<u8> out; };
    std::vector<Job> jobs;
    for (size_t i = 0; i < raws.size(); ++i)
        for (size_t off = 0; off < raws[i].size() || (off == 0 && raws[i].empty()); off += kChunk) {
            jobs.push_back({i, off, std::min(kChunk, raws[i].size() - off), {}});
            if (raws[i].empty()) break;
        }
    ParallelFor(jobs.size(), [&](size_t j) {
        Job& jb = jobs[j];
        jb.out = deflate::ZlibCompress(raws[jb.buf].data() + jb.off, jb.len, 6);
    });
    std::vector<std::vector<u8>> packed(raws.size());
    std::vector<u32> counts(raws.size(), 0);
    for (auto& jb : jobs) ++counts[jb.buf];
    for (size_t i = 0; i < raws.size(); ++i) {
        packed[i].resize(4);
        memcpy(packed[i].data(), &counts[i], 4);
    }
    for (auto& jb : jobs) {
        auto& p = packed[jb.buf];
        u32 hdr[2] = {(u32)jb.len, (u32)jb.out.size()};
        p.insert(p.end(), (const u8*)hdr, (const u8*)hdr + 8);
        p.insert(p.end(), jb.out.begin(), jb.out.end());
    }
    return packed;
}

// Inverse of PackAll for one buffer whose raw size is known.
bool UnpackChunks(const std::vector<u8>& in, size_t rawSize, std::vector<u8>& out) {
    out.resize(rawSize);
    if (in.size() < 4) return false;
    u32 count;
    memcpy(&count, in.data(), 4);
    struct Chunk { size_t src, srcLen, dst, dstLen; };
    std::vector<Chunk> chunks;
    size_t pos = 4, dst = 0;
    for (u32 c = 0; c < count; ++c) {
        if (pos + 8 > in.size()) return false;
        u32 hdr[2];
        memcpy(hdr, in.data() + pos, 8);
        pos += 8;
        if (pos + hdr[1] > in.size() || dst + hdr[0] > rawSize) return false;
        chunks.push_back({pos, hdr[1], dst, hdr[0]});
        pos += hdr[1];
        dst += hdr[0];
    }
    if (dst != rawSize) return false;
    std::atomic<bool> ok{true};
    ParallelFor(chunks.size(), [&](size_t i) {
        const Chunk& ch = chunks[i];
        int got = stbi_zlib_decode_buffer((char*)out.data() + ch.dst, (int)ch.dstLen, (const char*)in.data() + ch.src, (int)ch.srcLen);
        if (got != (int)ch.dstLen) ok = false;
    });
    return ok;
}
}  // namespace

bool WriteSxl(const std::string& path, const Json& meta, const std::vector<SurfaceBlob>& surfaces, std::string* error, const SavedMesh* mesh) {
    if (meta["mesh"]["binaryGeometry"].B() &&
        (!mesh || mesh->vertices.empty() || mesh->indices.empty() || mesh->indices.size() % 3 ||
         mesh->vertices.size() > UINT32_MAX || mesh->indices.size() > UINT32_MAX ||
         (!mesh->triangleMaterials.empty() && mesh->triangleMaterials.size() != mesh->indices.size() / 3) ||
         (!mesh->triangleParts.empty() && mesh->triangleParts.size() != mesh->indices.size() / 3))) {
        if (error) *error = "maillage incomplet";
        return false;
    }
    if (!FitsJsonDepth(meta, 0)) {
        if (error) *error = "profondeur du document supérieure à celle du format";
        return false;
    }
    std::string json = meta.Dump(1);
    if (json.size() > kMaxJsonBytes) {
        if (error) *error = "métadonnées du projet trop volumineuses";
        return false;
    }
    std::vector<std::vector<u8>> shuffled(surfaces.size());
    ParallelFor(surfaces.size(), [&](size_t i) { shuffled[i] = Shuffle(surfaces[i].data, ElemSize(surfaces[i].format)); });
    std::vector<std::vector<u8>> packed = PackAll(shuffled);
    shuffled.clear();
    TemporaryProject temporary;
    if (!temporary.Reserve(Utf8Path(path))) {
        if (error) *error = "impossible de préparer l'écriture de " + path;
        return false;
    }
    {
        std::ofstream f(temporary.path, std::ios::binary | std::ios::trunc);
        if (!f) {
            if (error) *error = "impossible d'écrire " + path;
            return false;
        }
        auto put = [&](const void* p, size_t n) { f.write((const char*)p, (std::streamsize)n); };
        put("SXL1", 4);
        const u32 version = mesh || meta.Has("smartObjects") ? kVersion : 2;
        put(&version, 4);
        u64 js = json.size();
        put(&js, 8);
        put(json.data(), json.size());
        if (version == 3) {
            const u64 counts[] = {mesh ? mesh->vertices.size() : 0, mesh ? mesh->indices.size() : 0,
                                  mesh ? mesh->triangleMaterials.size() : 0, mesh ? mesh->triangleParts.size() : 0};
            put(counts, sizeof(counts));
            auto array = [&](const auto& items) {
                if (!items.empty()) put(items.data(), items.size() * sizeof(items[0]));
            };
            if (mesh) {
                array(mesh->vertices);
                array(mesh->indices);
                array(mesh->triangleMaterials);
                array(mesh->triangleParts);
            }
            const u64 count = surfaces.size();
            put(&count, sizeof(count));
        }
        for (size_t i = 0; i < surfaces.size(); ++i) {
            auto& s = surfaces[i];
            if (version == 3) put(&s.scope, sizeof(s.scope));
            u32 hdr[4] = {s.id, s.width, s.height, (u32)s.format};
            put(hdr, sizeof(hdr));
            u64 raw = s.data.size(), pk = packed[i].size();
            put(&raw, 8);
            put(&pk, 8);
            put(packed[i].data(), packed[i].size());
        }
        f.flush();
        f.close();  // the last buffered bytes are written here: checked too
        if (f.fail()) {
            if (error) *error = "échec de l'écriture";
            return false;
        }
    }
    std::error_code ec;
    std::filesystem::rename(temporary.path, Utf8Path(path), ec);
    if (ec) {
        if (error) *error = "impossible de remplacer " + path;
        return false;
    }
    temporary.path.clear();
    return true;
}

static bool ReadProject(const std::string& path, Json& meta, std::vector<SurfaceBlob>& surfaces, std::string* error,
                        std::atomic<float>* progress, SavedMesh* mesh, bool geometryOnly) {
    std::ifstream f(Utf8Path(path), std::ios::binary);
    if (!f) {
        if (error) *error = "impossible d'ouvrir " + path;
        return false;
    }
    std::error_code sizeEc;
    const auto fileSize = std::filesystem::file_size(Utf8Path(path), sizeEc);
    if (sizeEc) { if (error) *error = "taille du fichier inconnue"; return false; }
    auto fits = [&](u64 bytes) {
        const auto pos = f.tellg();
        return pos >= 0 && (u64)pos <= fileSize && bytes <= fileSize - (u64)pos;
    };
    auto report = [&](float p) { if (progress) progress->store(p); };
    auto get = [&](void* p, size_t n) { return (bool)f.read((char*)p, (std::streamsize)n); };
    char magic[4];
    u32 version = 0;
    u64 js = 0;
    // Old writers stored the geometry itself in JSON and could produce valid headers
    // above their own 256 MiB reader limit. Accept those if all bytes exist on disk.
    if (!get(magic, 4) || memcmp(magic, "SXL1", 4) != 0 || !get(&version, 4) || !get(&js, 8) ||
        js > (version < 3 ? (2ull << 30) : kMaxJsonBytes) || !fits(js)) {
        if (error) *error = "en-tête invalide";
        return false;
    }
    if (version < 1 || version > kVersion) {
        if (error) *error = "version de fichier non prise en charge (" + std::to_string(version) + ")";
        return false;
    }
#ifndef _WIN32
    if (version == 1) {
        if (error) *error = "fichier .sxl v1 (XPRESS) : réenregistrez-le sous Windows";
        return false;
    }
#endif
    {
        std::string json(js, '\0');
        std::string perr;
        if (!get(json.data(), js) || !Json::Parse(json, meta, &perr)) {
            if (error) *error = "métadonnées incomplètes ou invalides : " + perr;
            return false;
        }
    }
    u64 expected = 0;
    if (version == 3) {
        u64 counts[4] = {};
        if (error) *error = "maillage ou table de surfaces incomplet";
        if (!get(counts, sizeof(counts))) return false;
        if (counts[0] > UINT32_MAX || counts[1] > UINT32_MAX || counts[1] % 3 ||
            (counts[2] && counts[2] != counts[1] / 3) || (counts[3] && counts[3] != counts[1] / 3)) return false;
        SavedMesh skip;
        SavedMesh& dst = mesh ? *mesh : skip;
        auto array = [&](auto& items, u64 count) {
            const u64 bytes = count * sizeof(items[0]);
            if (!fits(bytes)) return false;
            if (!mesh) { f.seekg((std::streamoff)bytes, std::ios::cur); return (bool)f; }
            items.resize((size_t)count);
            return !bytes || get(items.data(), (size_t)bytes);
        };
        if (!array(dst.vertices, counts[0]) || !array(dst.indices, counts[1]) ||
            !array(dst.triangleMaterials, counts[2]) || !array(dst.triangleParts, counts[3]) || !get(&expected, sizeof(expected))) return false;
        if (error) error->clear();
    }
    if (geometryOnly) return true;
    bool truncated = false;
    for (;;) {
        u32 scope = 0;
        if (version == 3) {
            if (surfaces.size() == expected) { truncated = (u64)f.tellg() != fileSize; break; }
            if (!get(&scope, sizeof(scope))) { truncated = true; break; }
        }
        u32 hdr[4];
        if (!get(hdr, sizeof(hdr))) {
            truncated = version == 3 || f.gcount() != 0;
            break;
        }
        u64 raw = 0, pk = 0;
        if (!get(&raw, 8) || !get(&pk, 8) || !fits(pk) || pk > (8ull << 30) || raw > (16ull << 30) || hdr[3] > (u32)gpu::Format::RGBA16) {
            truncated = true;
            break;
        }
        SurfaceBlob b;
        b.id = hdr[0];
        b.scope = scope;
        if (version < 3 && (b.id & kSmartChildBlobBit)) {
            b.scope = 1 + ((b.id & ~kSmartChildBlobBit) >> 22);
            b.id &= 0x3FFFFFu;
        }
        b.width = hdr[1];
        b.height = hdr[2];
        b.format = (gpu::Format)hdr[3];
        std::vector<u8> p((size_t)pk);
        if (!get(p.data(), p.size())) {
            truncated = true;
            break;
        }
        std::vector<u8> decoded;
        bool ok;
#ifdef _WIN32
        if (version == 1) ok = DecompressXpress(p, (size_t)raw, decoded);
        else
#endif
        ok = UnpackChunks(p, (size_t)raw, decoded);
        if (!ok) {
            if (error) *error = "données de pixels corrompues";
            return false;
        }
        const auto elem = ElemSize(b.format);
        b.data = elem == 1 ? std::move(decoded) : Unshuffle(decoded, elem);
        surfaces.push_back(std::move(b));
        report((float)std::min(1.0, (double)f.tellg() / fileSize));
    }
    f.close();
    if (truncated) {
        // Opening it anyway would replace the missing pixels by empty layers (lost at the next save).
        if (error) *error = "fichier incomplet ou endommagé (pixels manquants)";
        return false;
    }
    report(1.0f);
    return true;
}

bool ReadSxl(const std::string& path, Json& meta, std::vector<SurfaceBlob>& surfaces, std::string* error, std::atomic<float>* progress, SavedMesh* mesh) {
    try {
        Json loadedMeta;
        std::vector<SurfaceBlob> loadedSurfaces;
        SavedMesh loadedMesh;
        if (!ReadProject(path, loadedMeta, loadedSurfaces, error, progress, mesh ? &loadedMesh : nullptr, false)) return false;
        meta = std::move(loadedMeta);
        surfaces = std::move(loadedSurfaces);
        if (mesh) *mesh = std::move(loadedMesh);
        return true;
    } catch (const std::exception& e) { if (error) *error = e.what(); return false; }
}

bool ReadSxlMesh(const std::string& path, MeshData& mesh, std::string* error) {
    Json meta;
    SavedMesh geometry;
    std::vector<SurfaceBlob> unused;
    try {
        MeshData loaded;
        if (!ReadProject(path, meta, unused, error, nullptr, &geometry, true) ||
            !meta.Has("mesh") || !MeshFromJson(meta["mesh"], loaded, &geometry)) return false;
        mesh = std::move(loaded);
        return true;
    } catch (const std::exception& e) { if (error) *error = e.what(); return false; }
}

}  // namespace sx
