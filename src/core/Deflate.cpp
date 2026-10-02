#include "core/Deflate.h"

#include <algorithm>
#include <cstring>
#include <queue>

namespace sx::deflate {

namespace {
// Length / distance code tables (RFC 1951 3.2.5).
const u16 kLenBase[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
const u8 kLenExtra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
const u16 kDistBase[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
const u8 kDistExtra[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
const u8 kClOrder[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

struct BitWriter {
    std::vector<u8>& out;
    u64 acc = 0;
    int n = 0;
    void Put(u32 bits, int count) {  // LSB first
        acc |= (u64)bits << n;
        n += count;
        while (n >= 8) { out.push_back((u8)acc); acc >>= 8; n -= 8; }
    }
    void PutRev(u32 code, int len) {  // Huffman codes are sent MSB first
        u32 r = 0;
        for (int i = 0; i < len; ++i) r |= ((code >> i) & 1) << (len - 1 - i);
        Put(r, len);
    }
    void Flush() { if (n > 0) { out.push_back((u8)acc); acc = 0; n = 0; } }
};

// Code lengths limited to maxLen (Huffman, then rebalance overlong codes).
void BuildLengths(const u32* freq, int count, int maxLen, u8* lens) {
    std::fill(lens, lens + count, 0);
    struct Node { u64 w; int left, right; };
    std::vector<Node> nodes;
    using Item = std::pair<u64, int>;
    std::priority_queue<Item, std::vector<Item>, std::greater<Item>> pq;
    for (int i = 0; i < count; ++i)
        if (freq[i]) { nodes.push_back({freq[i], -1 - i, 0}); pq.push({freq[i], (int)nodes.size() - 1}); }
    if (nodes.empty()) return;
    if (nodes.size() == 1) { lens[-1 - nodes[0].left] = 1; return; }
    while (pq.size() > 1) {
        auto a = pq.top(); pq.pop();
        auto b = pq.top(); pq.pop();
        nodes.push_back({a.first + b.first, a.second, b.second});
        pq.push({a.first + b.first, (int)nodes.size() - 1});
    }
    // Depths.
    std::vector<int> depthCount(64, 0);
    std::vector<std::pair<int, int>> stack = {{pq.top().second, 0}};
    std::vector<std::pair<int, int>> leaves;  // symbol, depth
    while (!stack.empty()) {
        auto [ni, d] = stack.back();
        stack.pop_back();
        const Node& nd = nodes[ni];
        if (nd.left < 0) { leaves.push_back({-1 - nd.left, std::max(d, 1)}); continue; }
        stack.push_back({nd.left, d + 1});
        stack.push_back({nd.right, d + 1});
    }
    for (auto& [s, d] : leaves) ++depthCount[std::min(d, 63)];
    // Limit: move overlong leaves up (classic zlib-style overflow fix on the length histogram).
    int overflow = 0;
    for (int d = maxLen + 1; d < 64; ++d) { overflow += depthCount[d]; depthCount[maxLen] += depthCount[d]; depthCount[d] = 0; }
    // Kraft sum must be <= 1: repeatedly split a shorter leaf.
    auto kraft = [&] {
        u64 k = 0;
        for (int d = 1; d <= maxLen; ++d) k += (u64)depthCount[d] << (maxLen - d);
        return k;
    };
    while (kraft() > (1ull << maxLen)) {
        int d = maxLen - 1;
        while (d > 0 && depthCount[d] == 0) --d;
        if (d == 0) break;
        --depthCount[d];
        depthCount[d + 1] += 2;
        --depthCount[maxLen];
    }
    (void)overflow;
    // Assign lengths: most frequent symbols get the shortest codes.
    std::sort(leaves.begin(), leaves.end(), [&](auto& a, auto& b) { return freq[a.first] > freq[b.first] || (freq[a.first] == freq[b.first] && a.first < b.first); });
    size_t k = 0;
    for (int d = 1; d <= maxLen; ++d)
        for (int c = 0; c < depthCount[d] && k < leaves.size(); ++c) lens[leaves[k++].first] = (u8)d;
}

void Canonical(const u8* lens, int count, u16* codes) {
    int blCount[16] = {}, next[16] = {};
    for (int i = 0; i < count; ++i) blCount[lens[i]]++;
    blCount[0] = 0;
    int code = 0;
    for (int b = 1; b < 16; ++b) { code = (code + blCount[b - 1]) << 1; next[b] = code; }
    for (int i = 0; i < count; ++i) if (lens[i]) codes[i] = (u16)next[lens[i]]++;
}

struct Sym { u16 lit; u16 dist; };  // lit < 256: literal; else length code index (dist > 0)

int LenCode(int len) {
    int i = 28;
    while (kLenBase[i] > len) --i;
    return i;
}
int DistCode(int dist) {
    int i = 29;
    while (kDistBase[i] > dist) --i;
    return i;
}

void WriteBlock(BitWriter& bw, const std::vector<u32>& seq, const u8* data, bool last) {
    // seq entries: (len << 16 | dist) for matches with len >= 3, or 0x80000000 | byte for literals.
    u32 lf[286] = {}, df[30] = {};
    for (u32 s : seq) {
        if (s & 0x80000000u) lf[s & 0xFF]++;
        else { lf[257 + LenCode(s >> 16)]++; df[DistCode(s & 0xFFFF)]++; }
    }
    lf[256] = 1;
    u8 ll[286], dl[30];
    BuildLengths(lf, 286, 15, ll);
    BuildLengths(df, 30, 15, dl);
    bool anyDist = false;
    for (int i = 0; i < 30; ++i) anyDist |= dl[i] != 0;
    if (!anyDist) dl[0] = 1;  // at least one distance code
    int hlit = 286, hdist = 30;
    while (hlit > 257 && !ll[hlit - 1]) --hlit;
    while (hdist > 1 && !dl[hdist - 1]) --hdist;
    // Run-length encode the concatenated code lengths.
    std::vector<u8> all(ll, ll + hlit);
    all.insert(all.end(), dl, dl + hdist);
    std::vector<std::pair<u8, u8>> rle;  // symbol, extra
    for (size_t i = 0; i < all.size();) {
        u8 v = all[i];
        size_t r = 1;
        while (i + r < all.size() && all[i + r] == v) ++r;
        if (v == 0 && r >= 3) {
            size_t n = std::min<size_t>(r, 138);
            rle.push_back(n <= 10 ? std::pair<u8, u8>{17, (u8)(n - 3)} : std::pair<u8, u8>{18, (u8)(n - 11)});
            i += n;
        } else if (v != 0 && r >= 4) {
            rle.push_back({v, 0});
            size_t n = std::min<size_t>(r - 1, 6);
            rle.push_back({16, (u8)(n - 3)});
            i += 1 + n;
        } else {
            rle.push_back({v, 0});
            ++i;
        }
    }
    u32 cf[19] = {};
    for (auto& [s, e] : rle) cf[s]++;
    u8 cl[19];
    BuildLengths(cf, 19, 7, cl);
    int hclen = 19;
    while (hclen > 4 && !cl[kClOrder[hclen - 1]]) --hclen;
    u16 lc[286] = {}, dc[30] = {}, cc[19] = {};
    Canonical(ll, 286, lc);
    Canonical(dl, 30, dc);
    Canonical(cl, 19, cc);
    bw.Put(last ? 1 : 0, 1);
    bw.Put(2, 2);  // dynamic Huffman
    bw.Put(hlit - 257, 5);
    bw.Put(hdist - 1, 5);
    bw.Put(hclen - 4, 4);
    for (int i = 0; i < hclen; ++i) bw.Put(cl[kClOrder[i]], 3);
    for (auto& [s, e] : rle) {
        bw.PutRev(cc[s], cl[s]);
        if (s == 16) bw.Put(e, 2);
        else if (s == 17) bw.Put(e, 3);
        else if (s == 18) bw.Put(e, 7);
    }
    for (u32 s : seq) {
        if (s & 0x80000000u) {
            int b = s & 0xFF;
            bw.PutRev(lc[b], ll[b]);
        } else {
            int len = s >> 16, dist = s & 0xFFFF;
            int c = LenCode(len);
            bw.PutRev(lc[257 + c], ll[257 + c]);
            if (kLenExtra[c]) bw.Put(len - kLenBase[c], kLenExtra[c]);
            int d = DistCode(dist);
            bw.PutRev(dc[d], dl[d]);
            if (kDistExtra[d]) bw.Put(dist - kDistBase[d], kDistExtra[d]);
        }
    }
    bw.PutRev(lc[256], ll[256]);
    (void)data;
}
}  // namespace

std::vector<u8> ZlibCompress(const u8* data, size_t size, int level) {
    std::vector<u8> out;
    out.reserve(size / 2 + 64);
    out.push_back(0x78);
    out.push_back(0x9C);
    BitWriter bw{out};
    const int maxChain = level <= 1 ? 4 : level <= 3 ? 8 : level <= 6 ? 32 : 128;
    const size_t kWindow = 32768, kHashSize = 1 << 15;
    std::vector<int> head(kHashSize, -1), prev(kWindow, -1);
    auto hash = [&](size_t i) { return ((data[i] << 10) ^ (data[i + 1] << 5) ^ data[i + 2]) & (kHashSize - 1); };
    std::vector<u32> seq;
    seq.reserve(1 << 16);
    size_t i = 0;
    auto insert = [&](size_t p) {
        if (p + 2 >= size) return;
        size_t h = hash(p);
        prev[p & (kWindow - 1)] = head[h];
        head[h] = (int)p;
    };
    while (i < size) {
        int bestLen = 0, bestDist = 0;
        if (i + 2 < size) {
            int cand = head[hash(i)];
            int chain = maxChain;
            size_t maxLen = std::min<size_t>(258, size - i);
            while (cand >= 0 && i - cand <= kWindow - 1 && chain-- > 0) {
                const u8* a = data + cand;
                const u8* b = data + i;
                if (a[bestLen] == b[bestLen]) {
                    size_t l = 0;
                    while (l < maxLen && a[l] == b[l]) ++l;
                    if ((int)l > bestLen) {
                        bestLen = (int)l;
                        bestDist = (int)(i - cand);
                        if (l == maxLen) break;
                    }
                }
                int nxt = prev[cand & (kWindow - 1)];
                if (nxt >= cand) break;
                cand = nxt;
            }
        }
        if (bestLen >= 3) {
            seq.push_back(((u32)bestLen << 16) | (u32)bestDist);
            for (int k = 0; k < bestLen; ++k) insert(i + k);
            i += bestLen;
        } else {
            seq.push_back(0x80000000u | data[i]);
            insert(i);
            ++i;
        }
        if (seq.size() >= 60000) {
            WriteBlock(bw, seq, data, false);
            seq.clear();
        }
    }
    WriteBlock(bw, seq, data, true);
    bw.Flush();
    // Adler-32.
    u32 a = 1, b = 0;
    for (size_t k = 0; k < size;) {
        size_t n = std::min<size_t>(5552, size - k);
        for (size_t j = 0; j < n; ++j) { a += data[k + j]; b += a; }
        a %= 65521;
        b %= 65521;
        k += n;
    }
    u32 adler = (b << 16) | a;
    out.push_back((u8)(adler >> 24));
    out.push_back((u8)(adler >> 16));
    out.push_back((u8)(adler >> 8));
    out.push_back((u8)adler);
    return out;
}

}  // namespace sx::deflate
