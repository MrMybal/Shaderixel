#pragma once
// Native document format (.sxl).
//
//   "SXL1" | u32 version | u64 jsonSize | json (UTF-8)
//   then for each surface: u32 id | u32 w | u32 h | u32 format | u64 rawSize | u64 packedSize | bytes
//
// Pixels are byte-plane shuffled (better entropy for half/float data) and
// compressed with the Windows XPRESS-Huffman codec, in parallel.

#include "doc/Document.h"
#include "core/Json.h"

#include <memory>

namespace sx {

struct SurfaceBlob {
    Id id = 0;
    u32 width = 0, height = 0;
    gpu::Format format = gpu::Format::RGBA16F;
    std::vector<u8> data;  // tightly packed rows
};

// Blob ids at or above this bit hold the surfaces of smart objects' inner documents
// (id = kSmartChildBlobBit | (smart object index << 22) | inner surface id).
inline constexpr Id kSmartChildBlobBit = 0x80000000u;

// Structure of the document (no pixels) and the surfaces it references.
Json SerializeDocument(const Document& doc);
std::vector<Id> ReferencedSurfaces(const Document& doc);

// Build a document from its JSON; surfaces are created with their saved ids but
// left empty (the caller uploads the pixel blobs).
std::unique_ptr<Document> DeserializeDocument(const Json& j, const std::vector<SurfaceBlob>& blobs, std::string* error);

bool WriteSxl(const std::string& path, const Json& meta, const std::vector<SurfaceBlob>& surfaces, std::string* error);
bool ReadSxl(const std::string& path, Json& meta, std::vector<SurfaceBlob>& surfaces, std::string* error);

}  // namespace sx
