// FBX loader (ufbx): every mesh node flattened in world space, converted by ufbx to the
// engine's left-handed Y-up space, triangulated, with one material per face group
// (base color / roughness / metallic / emission, base color texture: file or embedded).
#include "mesh/Mesh.h"

#include <ufbx.h>

namespace sx {

namespace {
std::string Str(ufbx_string s) { return std::string(s.data ? s.data : "", s.length); }
}  // namespace

bool LoadFbx(const std::string& path, MeshData& out, std::string* error) {
    ufbx_load_opts opts = {};
    opts.target_axes = ufbx_axes_left_handed_y_up;
    opts.target_unit_meters = 1.0f;
    opts.generate_missing_normals = true;
    ufbx_error err;
    ufbx_scene* scene = ufbx_load_file(path.c_str(), &opts, &err);
    if (!scene) {
        if (error) *error = "FBX illisible : " + Str(err.description);
        return false;
    }
    out = MeshData{};
    out.name = Narrow(Utf8Path(path).stem().wstring());
    // Scene materials -> MeshData materials (index by material element).
    std::vector<int> matIndex(scene->materials.count, -1);
    for (size_t i = 0; i < scene->materials.count; ++i) {
        const ufbx_material* m = scene->materials.data[i];
        MeshMaterialInfo info;
        info.name = Str(m->name);
        const ufbx_material_map& bc = m->pbr.base_color;
        if (bc.has_value) info.baseColor = {(float)bc.value_vec4.x, (float)bc.value_vec4.y, (float)bc.value_vec4.z, 1.0f};
        if (m->pbr.base_factor.has_value) {
            float f = (float)m->pbr.base_factor.value_real;
            info.baseColor = {info.baseColor.x * f, info.baseColor.y * f, info.baseColor.z * f, 1.0f};
        }
        if (m->pbr.roughness.has_value) info.roughness = (float)m->pbr.roughness.value_real;
        if (m->pbr.metalness.has_value) info.metallic = (float)m->pbr.metalness.value_real;
        if (m->pbr.emission_color.has_value) {
            float f = m->pbr.emission_factor.has_value ? (float)m->pbr.emission_factor.value_real : 1.0f;
            info.emissive = float3{(float)m->pbr.emission_color.value_vec4.x, (float)m->pbr.emission_color.value_vec4.y, (float)m->pbr.emission_color.value_vec4.z} * f;
        }
        if (const ufbx_texture* t = bc.texture) {
            if (t->content.size) info.baseColorData.assign((const u8*)t->content.data, (const u8*)t->content.data + t->content.size);
            else info.baseColorPath = Str(t->filename).empty() ? Str(t->absolute_filename) : Str(t->filename);
        }
        matIndex[i] = (int)out.materials.size();
        out.materials.push_back(info);
    }
    bool hasUv = false;
    std::vector<uint32_t> tri;
    // The root carries the axis conversion (and its winding is already handled by ufbx): only
    // a node whose own transform mirrors relative to it needs its triangles reversed.
    const ufbx_real rootDet = scene->root_node ? ufbx_matrix_determinant(&scene->root_node->node_to_world) : 1.0;
    for (size_t ni = 0; ni < scene->nodes.count; ++ni) {
        const ufbx_node* node = scene->nodes.data[ni];
        const ufbx_mesh* mesh = node->mesh;
        if (!mesh) continue;
        const bool mirrored = ufbx_matrix_determinant(&node->geometry_to_world) * rootDet < 0;
        const ufbx_matrix normalMatrix = ufbx_matrix_for_normals(&node->geometry_to_world);  // inverse-transpose
        tri.resize(mesh->max_face_triangles * 3);
        hasUv |= mesh->vertex_uv.exists;
        for (size_t fi = 0; fi < mesh->faces.count; ++fi) {
            const ufbx_face face = mesh->faces.data[fi];
            uint32_t n = ufbx_triangulate_face(tri.data(), tri.size(), mesh, face);
            int mat = 0;
            if (mesh->face_material.count > fi && mesh->materials.count) {
                const ufbx_material* m = mesh->materials.data[mesh->face_material.data[fi] % mesh->materials.count];
                if (m) mat = matIndex[m->typed_id < matIndex.size() ? m->typed_id : 0];
            }
            for (uint32_t t = 0; t < n; ++t) {
                u32 base = (u32)out.vertices.size();
                for (int k = 0; k < 3; ++k) {
                    uint32_t idx = tri[t * 3 + k];
                    MeshVertex v;
                    ufbx_vec3 p = ufbx_transform_position(&node->geometry_to_world, ufbx_get_vertex_vec3(&mesh->vertex_position, idx));
                    v.pos = {(float)p.x, (float)p.y, (float)p.z};
                    if (mesh->vertex_normal.exists) {
                        ufbx_vec3 nn = ufbx_transform_direction(&normalMatrix, ufbx_get_vertex_vec3(&mesh->vertex_normal, idx));
                        v.normal = Normalize({(float)nn.x, (float)nn.y, (float)nn.z});
                    }
                    if (mesh->vertex_uv.exists) {
                        ufbx_vec2 uv = ufbx_get_vertex_vec2(&mesh->vertex_uv, idx);
                        v.uv = {(float)uv.x, 1.0f - (float)uv.y};  // FBX v points up
                    }
                    out.vertices.push_back(v);
                }
                if (mirrored) out.indices.insert(out.indices.end(), {base, base + 2, base + 1});
                else out.indices.insert(out.indices.end(), {base, base + 1, base + 2});
                out.triMaterial.push_back((u16)std::max(mat, 0));
            }
        }
    }
    ufbx_free_scene(scene);
    if (out.indices.empty()) {
        if (error) *error = "aucun triangle";
        return false;
    }
    if (out.materials.empty()) out.triMaterial.clear();
    out.hasUvs = hasUv;
    NormalizeMesh(out);
    return true;
}

}  // namespace sx
