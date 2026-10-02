#pragma once
// zlib (RFC 1950/1951) encoder with dynamic Huffman blocks and hash-chain LZ77.
// Portable replacement for platform compressors; decoding uses stb_image's inflate.

#include "core/Common.h"

namespace sx::deflate {

// level 1 (fast) .. 9 (smallest): controls the match search depth.
std::vector<u8> ZlibCompress(const u8* data, size_t size, int level = 6);

}  // namespace sx::deflate
