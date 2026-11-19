#pragma once
// Native document format (.sxl).
//
//   "SXL1" | u32 version | u64 jsonSize | json (UTF-8)
// v3 adds a binary mesh (four u64 counts, then vertex/index/material/part arrays)
// after JSON, and a document scope before each surface header. v1/v2 remain readable.
// Surface headers contain id, width, height, format, raw and packed sizes.

#include "doc/Document.h"
#include "core/Json.h"
#include "mesh/Mesh.h"

#include <atomic>
#include <memory>

namespace sx {

struct SurfaceBlob {
    Id id = 0;
    u32 width = 0, height = 0;
    gpu::Format format = gpu::Format::RGBA16F;
    std::vector<u8> data;  // tightly packed rows
    u32 scope = 0;         // 0 = main document; child documents have distinct scopes
};

struct SavedMesh {
    std::vector<MeshVertex> vertices;
    std::vector<u32> indices;
    std::vector<u16> triangleMaterials;
    std::vector<u32> triangleParts;
};
Json MeshToJson(const MeshData& m, bool embedGeometry = true);
bool MeshFromJson(const Json& j, MeshData& m, SavedMesh* geometry = nullptr);
bool ReadSxlMesh(const std::string& path, MeshData& mesh, std::string* error);

// Legacy v1/v2 packed the document scope into the blob id. New projects store
// scope separately so nested smart objects can keep their own local surface ids.
inline constexpr Id kSmartChildBlobBit = 0x80000000u;

// Structure of the document (no pixels) and the surfaces it references.
Json SerializeDocument(const Document& doc);
std::vector<Id> ReferencedSurfaces(const Document& doc);

// Build a document from its JSON; surfaces are created with their saved ids but
// left empty (the caller uploads the pixel blobs).
std::unique_ptr<Document> DeserializeDocument(const Json& j, const std::vector<SurfaceBlob>& blobs, std::string* error, u32 scope = 0);

// Graph JSON (nodes, links, variables, inner function graphs): documents and .sxfn functions.
Json GraphToJson(const Graph& g);
void GraphFromJson(const Json& jg, Graph& g);

bool WriteSxl(const std::string& path, const Json& meta, const std::vector<SurfaceBlob>& surfaces, std::string* error,
              const SavedMesh* mesh = nullptr);
// progress (optional, any thread): 0..1 while the file is read then unpacked.
bool ReadSxl(const std::string& path, Json& meta, std::vector<SurfaceBlob>& surfaces, std::string* error,
             std::atomic<float>* progress = nullptr, SavedMesh* mesh = nullptr);

}  // namespace sx
