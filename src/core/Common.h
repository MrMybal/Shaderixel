#pragma once
// Shared basic types for Shaderixel: small math, ids, hashing, logging.

#include <cstdint>
#include <cstddef>
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>

namespace sx {

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using i32 = int32_t;
using i64 = int64_t;

struct float2 {
    float x = 0, y = 0;
    float2() = default;
    constexpr float2(float x_, float y_) : x(x_), y(y_) {}
    float2 operator+(float2 o) const { return {x + o.x, y + o.y}; }
    float2 operator-(float2 o) const { return {x - o.x, y - o.y}; }
    float2 operator*(float s) const { return {x * s, y * s}; }
    float2 operator/(float s) const { return {x / s, y / s}; }
    bool operator==(const float2&) const = default;
};
inline float Length(float2 v) { return std::sqrt(v.x * v.x + v.y * v.y); }

struct float4 {
    float x = 0, y = 0, z = 0, w = 0;
    float4() = default;
    constexpr float4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
    float& operator[](int i) { return (&x)[i]; }
    float operator[](int i) const { return (&x)[i]; }
    bool operator==(const float4&) const = default;
};

struct int2 {
    int x = 0, y = 0;
    bool operator==(const int2&) const = default;
};

// Integer pixel rectangle [x0,x1) x [y0,y1).
struct Rect {
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    bool Empty() const { return x1 <= x0 || y1 <= y0; }
    int Width() const { return x1 - x0; }
    int Height() const { return y1 - y0; }
    void Merge(const Rect& r) {
        if (r.Empty()) return;
        if (Empty()) { *this = r; return; }
        x0 = std::min(x0, r.x0); y0 = std::min(y0, r.y0);
        x1 = std::max(x1, r.x1); y1 = std::max(y1, r.y1);
    }
    Rect Clipped(int w, int h) const {
        return {std::max(x0, 0), std::max(y0, 0), std::min(x1, w), std::min(y1, h)};
    }
};

using Id = u32;  // 0 = invalid

// FNV-1a based incremental hash, used for render cache signatures.
struct Hasher {
    u64 h = 1469598103934665603ull;
    void Bytes(const void* data, size_t n) {
        auto p = static_cast<const u8*>(data);
        for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 1099511628211ull; }
    }
    template <class T> void Pod(const T& v) { Bytes(&v, sizeof(T)); }
    void Str(const std::string& s) { Bytes(s.data(), s.size()); Pod(s.size()); }
};

void Log(const char* fmt, ...);
std::wstring Widen(const std::string& s);
std::string Narrow(const std::wstring& s);
bool ReadTextFile(const std::string& path, std::string& out);

inline float SrgbToLinear(float c) { return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f); }
inline float LinearToSrgb(float c) { c = std::max(c, 0.0f); return c <= 0.0031308f ? c * 12.92f : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f; }

}  // namespace sx
