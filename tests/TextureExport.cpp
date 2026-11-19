#include "io/TextureExportPlan.h"
#include "mesh/Mesh.h"
#include <cstdio>

int main() {
    using namespace sx;
    using namespace sx::texexport;
    int failures = 0;
    auto check = [&](bool result, const char* name) {
        if (!result) { std::fprintf(stderr, "FAIL %s\n", name); ++failures; }
    };
    const std::vector<Map> maps = {{"BaseColor", {}}, {"ORM", {}}};
    std::vector<Output> outputs;
    const std::vector<std::pair<std::string,Rect>> parts = {{".1001", {}}, {".1002", {}}};
    check(PlanOutputs(parts, maps, "{nom}_{carte}", "Model", ".png", outputs).empty() && outputs.size() == 4 &&
          outputs[0].filename == "Model_BaseColor.1001.png" && outputs[3].filename == "Model_ORM.1002.png", "UDIM destinations");
    check(PlanOutputs({{"_Body.1001",{}},{"_Trim.1001",{}}}, maps, "{nom}_{carte}", "Model", ".png", outputs).empty() &&
          outputs[0].filename == "Model_Body_BaseColor.1001.png" && outputs[2].filename == "Model_Trim_BaseColor.1001.png",
          "surfaces on the same UDIM have separate mapped files");
    check(!PlanOutputs(parts, maps, "{nom}", "Model", ".png", outputs).empty() && outputs.empty(), "reject colliding map names before writing");
    check(PlanOutputs(parts, maps, "{nom}", "Model", ".png", outputs, {{true,false},{false,true}}).empty() &&
          outputs.size() == 2 && outputs[0].map == 0 && outputs[1].map == 1, "only emitted set channels take part in collision checks");
    check(!PlanOutputs({{"_Wood",{}},{"_wood",{}}}, maps, "{nom}_{carte}", "Model", ".tga", outputs).empty(), "Windows case-insensitive collisions");
    check(!PlanOutputs({{"",{}}}, {{"Tiles",{}},{"Tiles.png/Color",{}}}, "{carte}", "", ".png", outputs).empty(),
          "file / nested folder collision before writing");
    check(!PlanOutputs({{"",{}}}, {{"A/B",{}},{"a//B",{}}}, "{carte}", "", ".png", outputs).empty(),
          "equivalent folder spellings on Windows");
    check(!PlanOutputs({{"",{}}}, {{"../outside",{}}}, "{carte}", "", ".png", outputs).empty() &&
          !PlanOutputs({{"",{}}}, {{"CON",{}}}, "{carte}", "", ".png", outputs).empty(), "reject traversal and reserved names");
    check(PlanOutputs({{"_Wood",{}}}, maps, "{nom}_{carte}", "{nom}", ".png", outputs).empty() &&
          outputs[0].filename == "{nom}_Wood_BaseColor.png", "literal token in base name");
    std::vector<u8> coverage{1,0,1,0,0,0};
    auto padding = BuildPadding(coverage, 6, 1, 2);
    std::vector<float> rgba(24);
    rgba[0] = 0.2f; rgba[8] = 0.8f;
    ApplyPadding(padding, 6, rgba);
    check(std::abs(rgba[4]-0.5f) < 1e-6f && rgba[12] == 0.8f && rgba[16] == 0.8f && rgba[20] == 0, "padding rings and edge average");
    MeshData mesh;
    const std::vector<float2> uvs = {{0,1},{1,1},{0,0},{1,1},{2,1},{1,0}};
    for (float2 uv : uvs) mesh.vertices.push_back({{}, {}, uv});
    mesh.indices = {0,1,2,3,4,5};
    check(PackUdims(mesh), "pack original tiles");
    std::string error;
    int atlas = 0;
    check(SizeTextureTiles(mesh, 1024, "1002 = 512", atlas, error) && atlas == 2048, "native resolutions");
    const float4 a = UdimRect(mesh, 0), b = UdimRect(mesh, 1);
    check((a.z-a.x)*atlas == 1024 && (b.z-b.x)*atlas == 512, "independent native export crops");
    auto restored = UdimUvs(mesh);
    check(restored == uvs, "OBJ original coordinates after independent tile sizing");
    MeshData mixed;
    for (float2 uv : uvs) mixed.vertices.push_back({{}, {}, uv});
    mixed.indices = {0,1,2,3,4,5,0,1,2};
    mixed.materials = {{"Body"}, {"Trim"}};
    mixed.triMaterial = {0,0,1};
    std::vector<u32> sources;
    check(PackUdims(mixed, &sources) && mixed.udims == std::vector<u16>{1001,1002,1001} &&
          mixed.udimMaterials == std::vector<u16>{0,0,1} && mixed.indices[0] != mixed.indices[6], "split shared UVs and vertices by material and UDIM");
    check(SizeTextureTiles(mixed, 128, "1001=128\n1002=32\nTrim=32\nTrim.1001=64", atlas, error) && atlas == 256 &&
          mixed.udimScale == std::vector<float>{1,.25f,.5f}, "native material + UDIM overrides take precedence");
    const auto originalMixed = UdimUvs(mixed);
    bool same = sources.size() == originalMixed.size();
    for (size_t v = 0; same && v < sources.size(); ++v) same = originalMixed[v] == uvs[sources[v]];
    check(same, "original UVs of every face restored on overlapping texture sets");
    MeshData single;
    for (float2 uv : std::vector<float2>{{1,1},{2,1},{1,0}}) single.vertices.push_back({{}, {}, uv});
    single.indices = {0,1,2};
    check(PackUdims(single) && single.udims == std::vector<u16>{1002} &&
          SizeTextureTiles(single, 1024, "1002 = 256", atlas, error) && atlas == 256,
          "single occupied UDIM outside 0..1");
    MeshData single1001 = MakePlane();
    check(SizeTextureTiles(single1001, 1024, "1001 = 128", atlas, error) && atlas == 128 &&
          single1001.udims == std::vector<u16>{1001}, "explicit 1001 on an ordinary unit UV layout");
    MeshData singleSet = MakePlane();
    singleSet.materials.push_back({"Body"});
    check(SizeTextureTiles(singleSet, 1024, "Body = 64", atlas, error) && atlas == 64 && singleSet.uvTiles.size() == 1,
          "explicit resolution on a single material");
    check(!SizeTextureTiles(mesh, 1024, "9000 = 256", atlas, error), "reject unknown tile");
    MeshData materials;
    for (int i = 0; i < 2; ++i) {
        materials.materials.push_back({i ? "Glass" : "Body"});
        for (int k = 0; k < 3; ++k) { materials.vertices.push_back({{}, {}, uvs[k]}); materials.indices.push_back((u32)i*3+k); }
        materials.triMaterial.push_back((u16)i);
    }
    check(TileMaterialUvs(materials, 1024, 1) && SizeTextureTiles(materials, 1024, "Glass = 256", atlas, error) &&
          atlas == 2048 && (materials.uvTiles[1].z-materials.uvTiles[1].x)*atlas == 256, "native material regions");
    return failures != 0;
}
