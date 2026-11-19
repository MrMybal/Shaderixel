#include "mesh/MeshBake.h"
#include <cstdio>

using namespace sx;

namespace {
void Triangle(MeshData& mesh, const std::string& name, float z) {
    const u32 first = (u32)mesh.vertices.size();
    mesh.triPart.push_back((u32)mesh.partNames.size());
    mesh.partNames.push_back(name);
    for (const auto& uv : {float2{0,0}, float2{1,0}, float2{0,1}})
        mesh.vertices.push_back({{uv.x, uv.y, z}, {0,0,1}, uv});
    mesh.indices.insert(mesh.indices.end(), {first, first+1, first+2});
    mesh.boundsMax = {1,1,z};
}
}

int main() {
    int failures = 0;
    auto check = [&](bool ok, const char* label) {
        if (!ok) { std::fprintf(stderr, "FAIL %s\n", label); ++failures; }
    };
    MeshData low, high, cage;
    Triangle(low, "Door_low", 0);
    Triangle(high, "handle_high", 0.2f);
    Triangle(high, "DOOR_high", 0.1f);
    Triangle(cage, "door_cage", 0.15f);
    MeshRaycaster rays;
    rays.Build(high);
    float3 normal, weights, hit;
    u32 triangle = ~0u;
    check(rays.Ray({0.25f,0.25f,1}, {0,0,-1}, 2, &hit, &normal, nullptr, &triangle, &weights), "hit both parts");
    check(triangle == 0 && std::abs(hit.z-0.2f)<1e-5f && normal.z>0.99f, "nearest face");
    check(std::abs(weights.x-0.5f)<1e-5f && std::abs(weights.y-0.25f)<1e-5f && std::abs(weights.z-0.25f)<1e-5f, "barycentric UV");
    check(!rays.Ray({2,2,1},{0,0,-1},2,nullptr,nullptr), "ray outside mesh");
    BakeOptions opt;
    opt.size=64; opt.aoRays=4; opt.padding=0; opt.front=0.25f; opt.back=0.25f;
    const size_t pixel = (size_t)8*64+8;
    auto run = [&](const MeshData* custom) {
        const auto result = BakeMeshes(low, &high, opt, nullptr, nullptr, custom);
        return result.ok && result.misses == 0 ? result.height[pixel] : -1.0f;
    };
    check(std::abs(run(nullptr)-0.9f)<1e-4f, "overlapping high pieces");
    opt.matchByName=true;
    check(std::abs(run(nullptr)-0.7f)<1e-4f, "case-insensitive low/high name matching");
    opt.matchByName=false;
    check(std::abs(run(&cage)-0.7f)<1e-4f, "custom cage excludes outside piece");
    cage.indices.pop_back();
    check(!BakeMeshes(low, &high, opt, nullptr, nullptr, &cage).ok, "incompatible cage rejected");
    return failures ? 1 : 0;
}
