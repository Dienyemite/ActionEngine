#include "aeimport.h"
#include "core/logging.h"
#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace action {

using json = nlohmann::json;

// Helper: safely read a JSON value with a default
template<typename T>
static T jget(const json& obj, const char* key, T def) {
    if (obj.contains(key) && !obj[key].is_null()) {
        try { return obj[key].get<T>(); }
        catch (...) {}
    }
    return def;
}

AEImport LoadAEImport(const std::string& filepath) {
    AEImport result;

    std::ifstream f(filepath);
    if (!f.is_open()) {
        // Not an error — sidecar is optional
        return result;
    }

    json root;
    try {
        f >> root;
    } catch (const json::parse_error& e) {
        LOG_WARN("AEImport: JSON parse error in '{}': {}", filepath, e.what());
        return result;
    }

    result.ae_version        = jget<i32>(root, "ae_version", 0);
    result.source_file       = jget<std::string>(root, "source_file", "");
    result.blender_version   = jget<std::string>(root, "blender_version", "");
    result.export_timestamp  = jget<std::string>(root, "export_timestamp", "");

    // import_settings block
    if (root.contains("import_settings") && root["import_settings"].is_object()) {
        const auto& is = root["import_settings"];
        result.import_settings.scale             = jget<float>(is, "scale",             1.0f);
        result.import_settings.flip_uvs          = jget<bool> (is, "flip_uvs",          true);
        result.import_settings.generate_normals  = jget<bool> (is, "generate_normals",  true);
        result.import_settings.optimize_meshes   = jget<bool> (is, "optimize_meshes",   false);
        result.import_settings.up_axis           = jget<std::string>(is, "up_axis",     "Y");
        result.import_settings.import_animations = jget<bool> (is, "import_animations", false);
    }

    // lod block
    if (root.contains("lod") && root["lod"].is_object()) {
        const auto& lod = root["lod"];
        result.lod.generate    = jget<bool>(lod, "generate",    false);
        result.lod.level_count = jget<i32> (lod, "level_count", 4);
        if (lod.contains("distances") && lod["distances"].is_array()) {
            for (const auto& d : lod["distances"]) {
                if (d.is_number()) {
                    result.lod.distances.push_back(d.get<float>());
                }
            }
        }
    }

    // mesh_overrides array
    if (root.contains("mesh_overrides") && root["mesh_overrides"].is_array()) {
        for (const auto& ov : root["mesh_overrides"]) {
            if (!ov.is_object()) continue;
            AEMeshOverride mo;
            mo.name            = jget<std::string>(ov, "name",           "");
            mo.collision_type  = jget<std::string>(ov, "collision",      "convex_hull");
            mo.physics_layer   = jget<i32>        (ov, "physics_layer",  0);
            mo.cast_shadows    = jget<bool>        (ov, "cast_shadows",   true);
            mo.lod_bias        = jget<float>       (ov, "lod_bias",       1.0f);
            mo.is_static       = jget<bool>        (ov, "static",         false);
            if (!mo.name.empty()) {
                result.mesh_overrides.push_back(std::move(mo));
            }
        }
    }

    // animation_names array
    if (root.contains("animation_names") && root["animation_names"].is_array()) {
        for (const auto& n : root["animation_names"]) {
            if (n.is_string()) {
                result.animation_names.push_back(n.get<std::string>());
            }
        }
    }

    LOG_DEBUG("Loaded .aeimport '{}': v{}, {} mesh overrides, {} animations",
              filepath, result.ae_version,
              result.mesh_overrides.size(),
              result.animation_names.size());

    return result;
}

std::string GetAEImportPath(const std::string& asset_filepath) {
    std::filesystem::path p(asset_filepath);
    p.replace_extension(".aeimport");
    return p.string();
}

} // namespace action
