#include "doc/DocFile.h"
#include "mesh/UvUnwrap.h"

#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <compressapi.h>

namespace {
void Check(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

void TestRevisions() {
    sx::DocumentChanges changes;
    changes = true;
    const auto snapshot = changes.Revision();
    changes = true;
    Check(!changes.MarkSaved(snapshot) && changes, "saving a stale revision cleared a newer edit");
    Check(changes.MarkSaved(changes.Revision()) && !changes, "latest revision remained dirty after success");
}

void TestContainers(const std::filesystem::path& dir) {
    using namespace sx;
    MeshData source = MakePlane();
    source.partNames = {"Plane_low"};
    source.triPart.assign(source.indices.size() / 3, 0);
    source.materials.resize(2);
    source.materials[0].name = "Fabric";
    source.materials[1].name = "Metal";
    source.triMaterial = {0, 1};
    source.udims = {1001, 1001};
    source.udimMaterials = {0, 1};
    source.udimCols = 2;
    source.udimScale = {1.0f, 0.5f};
    Json meta = Json::Object();
    meta["mesh"] = MeshToJson(source, false);
    Check(!meta["mesh"].Has("vertices"), "new metadata still contains base64 vertices");
    SavedMesh mesh{source.vertices, source.indices, source.triMaterial, source.triPart};
    const std::string path = Narrow((dir / "project.sx3d").wstring());
    std::string error;
    std::vector<SurfaceBlob> layers;
    for (u32 scope = 0; scope != 3; ++scope) {
        SurfaceBlob b;
        b.id = 100;
        b.scope = scope;
        b.width = b.height = 1;
        b.format = gpu::Format::RGBA8;
        b.data = {(u8)scope, 1, 2, 255};
        layers.push_back(std::move(b));
    }
    Check(WriteSxl(path, meta, layers, &error, &mesh), "binary mesh write failed");
    Json readMeta;
    SavedMesh readMesh;
    std::vector<SurfaceBlob> readLayers;
    Check(ReadSxl(path, readMeta, readLayers, &error, nullptr, &readMesh), "binary mesh read failed");
    MeshData out;
    Check(MeshFromJson(readMeta["mesh"], out, &readMesh) && out.indices == source.indices &&
          out.vertices.size() == source.vertices.size() && out.partNames == source.partNames && out.triPart == source.triPart &&
          out.udims == source.udims && out.udimScale == source.udimScale &&
          out.udimMaterials == source.udimMaterials && out.triMaterial == source.triMaterial,
          "mesh geometry or face parts lost");
    Check(readLayers.size() == layers.size(), "lost document scopes");
    for (size_t i = 0; i < layers.size(); ++i)
        Check(readLayers[i].id == layers[i].id && readLayers[i].scope == layers[i].scope &&
              readLayers[i].data == layers[i].data, "nested pixel surfaces collided");
    MeshData preview;
    Check(ReadSxlMesh(path, preview, &error) && preview.indices == source.indices, "v3 thumbnail reader failed");
    Check(ReadSxl(path, readMeta, readLayers, &error) && readLayers.size() == layers.size(), "repeat read duplicated surface records");
    Json deep = Json::Object();
    Json* branch = &deep;
    for (int i = 0; i < 260; ++i) branch = &(*branch)["child"];
    Check(!WriteSxl(path, deep, {}, &error), "wrote an unreadable project over a good one");
    MeshData previous;
    Check(ReadSxlMesh(path, previous, &error), "failed save replaced the previous project");
    const auto onDisk = Utf8Path(path);
    const auto locked = CreateFileW(onDisk.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    Check(locked != INVALID_HANDLE_VALUE, "could not lock the destination");
    const bool replaced = WriteSxl(path, meta, layers, &error, &mesh);
    CloseHandle(locked);
    Check(!replaced && !error.empty(), "writer reported success for locked destination");
    MeshData unlocked;
    Check(ReadSxlMesh(path, unlocked, &error) && unlocked.indices == source.indices, "failed replacement damaged the existing file");
    std::filesystem::resize_file(onDisk, std::filesystem::file_size(onDisk) - 1);
    Check(!ReadSxl(path, readMeta, readLayers, &error), "lost pixel data was silently accepted");
    Check(readLayers.size() == layers.size() && readLayers.back().data == layers.back().data,
          "failed read replaced the last complete set of surfaces");

    // Version 2 embeds geometry in JSON and encodes child scopes in the surface id.
    const auto legacy = Narrow((dir / "legacy.sx3d").wstring());
    meta["mesh"] = MeshToJson(source);
    SurfaceBlob old = layers[1];
    old.scope = 0;
    old.id |= kSmartChildBlobBit;
    Check(WriteSxl(legacy, meta, {old}, &error), "legacy fixture failed");
    MeshData oldMesh;
    readLayers.clear();
    Check(ReadSxlMesh(legacy, oldMesh, &error) && oldMesh.indices == source.indices, "legacy geometry unreadable");
    Check(ReadSxl(legacy, readMeta, readLayers, &error) && readLayers.size() == 1 &&
          readLayers[0].scope == 1 && readLayers[0].id == 100, "legacy nested content unreadable");

    // Original Windows projects used XPRESS-Huffman for each surface.
    COMPRESSOR_HANDLE compressor = nullptr;
    Check(CreateCompressor(COMPRESS_ALGORITHM_XPRESS_HUFF, nullptr, &compressor), "v1 fixture compressor");
    std::vector<u8> raw(512, 73);
    SIZE_T written = 0;
    Compress(compressor, raw.data(), raw.size(), nullptr, 0, &written);
    std::vector<u8> packed(written);
    const bool encoded = Compress(compressor, raw.data(), raw.size(), packed.data(), packed.size(), &written) != FALSE;
    CloseCompressor(compressor);
    Check(encoded, "v1 fixture compression");
    const auto v1 = dir / "v1.sxl";
    {
        std::ofstream file(v1, std::ios::binary);
        auto write = [&](const auto& value) { file.write((const char*)&value, sizeof(value)); };
        file.write("SXL1", 4);
        write(u32{1});
        write(u64{2});
        file.write("{}", 2);
        const u32 hdr[] = {102, 32, 16, (u32)gpu::Format::R8};
        write(hdr);
        write((u64)raw.size());
        write((u64)written);
        file.write((const char*)packed.data(), written);
    }
    readLayers.clear();
    Check(ReadSxl(Narrow(v1.wstring()), readMeta, readLayers, &error) && readLayers.size() == 1 &&
          readLayers[0].data == raw, "v1 XPRESS pixels unreadable");
}

void TestPartialGltf(const std::filesystem::path& dir) {
    using namespace sx;
    // Two real primitives, only the first has authored UVs. They share a position accessor.
    const float data[] = {0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1};
    std::ofstream(dir / "mesh.bin", std::ios::binary).write((const char*)data, sizeof(data));
    const auto path = dir / "mixed.gltf";
    std::ofstream(path) << R"({"asset":{"version":"2.0"},"scenes":[{"nodes":[0]}],"nodes":[{"mesh":0}],"meshes":[{"primitives":[{"attributes":{"POSITION":0,"TEXCOORD_0":1}},{"attributes":{"POSITION":0}}]}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},{"bufferView":1,"componentType":5126,"count":3,"type":"VEC2"}],"bufferViews":[{"buffer":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":24}],"buffers":[{"uri":"mesh.bin","byteLength":60}]})";
    MeshData result;
    std::string error;
    Check(LoadGltf(Narrow(path.wstring()), result, &error) && result.hasUvs && result.indices.size() == 6, "partial glTF import failed");
    Check(result.vertices[0].uv == float2{0, 0} && result.vertices[1].uv == float2{1, 0} &&
          result.vertices[2].uv == float2{0, 1}, "authored UVs changed");
    const auto a = result.vertices[result.indices[3]].uv, b = result.vertices[result.indices[4]].uv, c = result.vertices[result.indices[5]].uv;
    Check(std::abs((b.x-a.x)*(c.y-a.y) - (c.x-a.x)*(b.y-a.y)) > 0.001f, "repaired face has zero UV area");
    Check(PackUdims(result) && result.udims == std::vector<u16>{1001, 1002}, "repaired faces overlap authored UVs");
}

// Matches the unshared vertices from an FBX import. In v2 the base64 fields alone
// would take 288 MB, exceeding the previous 256 MiB metadata limit.
void TestLargeMesh(const std::filesystem::path& dir) {
    using namespace sx;
    constexpr u32 triangles = 2'000'000;
    SavedMesh input;
    input.vertices.reserve(triangles * 3);
    input.indices.reserve(triangles * 3);
    for (u32 i = 0; i < triangles; ++i) {
        const float x = (float)(i % 2000), y = (float)(i / 2000);
        for (const float2 uv : {float2{0,0}, float2{1,0}, float2{0,1}}) {
            input.indices.push_back((u32)input.vertices.size());
            input.vertices.push_back({{x + uv.x, y + uv.y, 0}, {0,0,1}, uv});
        }
    }
    Json metadata = Json::Object();
    metadata["mesh"] = MeshToJson(MeshData{}, false);
    const auto file = dir / "large.sx3d";
    const auto path = Narrow(file.wstring());
    std::string error;
    Check(WriteSxl(path, metadata, {}, &error, &input), "failed to write two million triangles");
    MeshData restored;
    Check(ReadSxlMesh(path, restored, &error), "failed to reopen two million triangles");
    Check(restored.indices == input.indices && restored.vertices.size() == input.vertices.size() &&
          memcmp(restored.vertices.data(), input.vertices.data(), input.vertices.size() * sizeof(MeshVertex)) == 0,
          "vertices or indices changed in the large mesh");
    std::printf("%u triangles round-tripped, %llu bytes on disk.\n", triangles, (unsigned long long)std::filesystem::file_size(file));
}
}

int main(int argc, char** argv) {
    const auto dir = std::filesystem::temp_directory_path() /
        ("shaderixel-persistence-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64()));
    try {
        Check(std::filesystem::create_directory(dir), "test directory already exists");
        TestRevisions();
        TestContainers(dir);
        TestPartialGltf(dir);
        if (argc == 2 && std::string_view(argv[1]) == "--large-mesh") TestLargeMesh(dir);
        std::filesystem::remove_all(dir);
        std::puts("Project persistence and partial glTF UVs passed.");
        return 0;
    } catch (const std::exception& e) { std::fprintf(stderr, "%s\n", e.what()); return 1; }
}
