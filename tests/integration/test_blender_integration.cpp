/*
 * Integration Tests — Blender Integration: AssetImporter + animation extraction
 *
 * Tests the full import pipeline on a minimal in-memory FBX/glTF produced by
 * Assimp's Exporter, or on a synthetic aiScene built via Assimp's test helpers.
 * Because the build machines may not have network access, these tests avoid
 * downloading external files. They instead:
 *
 *   1. Write a tiny OBJ (static mesh) to temp and verify the sidecar override
 *      path is resolved correctly.
 *   2. Write a companion .aeimport sidecar for the OBJ and verify that
 *      ImportSettings are overridden from the sidecar before import.
 *   3. Verify that Import() succeeds on a valid mesh file and produces at
 *      least one ImportedMesh with non-zero vertices.
 *
 * Full skeletal animation integration (aiScene with mAnimations) requires a
 * real FBX / glTF file; that is covered by the e2e test and manual QA.
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "assets/asset_importer.h"
#include "assets/aeimport.h"
#include "animation/animation_library.h"
#include "animation/animation_clip.h"
#include "animation/skeleton.h"

#include <fstream>
#include <filesystem>

using namespace action;
using namespace Catch::Matchers;
namespace fs = std::filesystem;

// ─── Helpers ─────────────────────────────────────────────────────────────────

// Write a minimal valid OBJ file (a single triangle) to a temp path.
static std::string WriteTempOBJ() {
    auto path = fs::temp_directory_path() / "ae_test_mesh.obj";
    std::ofstream f(path);
    f << "# ActionEngine integration test mesh\n"
      << "v  0.0  0.0  0.0\n"
      << "v  1.0  0.0  0.0\n"
      << "v  0.0  1.0  0.0\n"
      << "vn 0.0  0.0  1.0\n"
      << "f  1//1  2//1  3//1\n";
    return path.string();
}

// Write a companion .aeimport sidecar next to the OBJ.
static std::string WriteTempSidecar(const std::string& obj_path, float scale) {
    fs::path sidecar = fs::path(obj_path);
    sidecar.replace_extension(".aeimport");

    std::ofstream f(sidecar);
    f << "{\n"
      << "  \"ae_version\": 1,\n"
      << "  \"source_file\": \"ae_test_mesh.obj\",\n"
      << "  \"import_settings\": {\n"
      << "    \"scale\": " << scale << ",\n"
      << "    \"flip_uvs\": false,\n"
      << "    \"generate_normals\": true,\n"
      << "    \"optimize_meshes\": false,\n"
      << "    \"up_axis\": \"Y\",\n"
      << "    \"import_animations\": false\n"
      << "  },\n"
      << "  \"lod\": {\"generate\": false, \"level_count\": 1, \"distances\": []},\n"
      << "  \"mesh_overrides\": [],\n"
      << "  \"animation_names\": []\n"
      << "}\n";
    return sidecar.string();
}

// ─── GetAEImportPath ──────────────────────────────────────────────────────────

TEST_CASE("GetAEImportPath - replaces extension with .aeimport", "[aeimport][path]") {
    std::string result = GetAEImportPath("/some/dir/character.fbx");
    CHECK(result == "/some/dir/character.aeimport");
}

TEST_CASE("GetAEImportPath - handles .glb extension", "[aeimport][path]") {
    std::string result = GetAEImportPath("assets/hero.glb");
    CHECK(result == "assets/hero.aeimport");
}

TEST_CASE("GetAEImportPath - handles already .aeimport extension", "[aeimport][path]") {
    std::string result = GetAEImportPath("mesh.aeimport");
    CHECK(result == "mesh.aeimport");
}

// ─── AssetImporter — static mesh import ──────────────────────────────────────

TEST_CASE("AssetImporter - imports a minimal OBJ triangle", "[importer][integration]") {
    std::string obj_path = WriteTempOBJ();

    AssetImporter importer;
    importer.Initialize(nullptr);   // no AssetManager needed for parsing-only test

    ImportSettings settings;
    settings.generate_normals = true;
    settings.flip_uvs         = false;

    ImportResult result = importer.Import(obj_path, settings);

    REQUIRE(result.success);
    CHECK(result.scene.meshes.size() >= 1);
    CHECK(result.scene.meshes[0].vertices.size() >= 3);
    CHECK(result.scene.meshes[0].indices.size()  >= 3);
}

TEST_CASE("AssetImporter - sidecar overrides ImportSettings.scale", "[importer][integration][aeimport]") {
    std::string obj_path  = WriteTempOBJ();
    std::string side_path = WriteTempSidecar(obj_path, 0.01f);

    // Verify the sidecar file was actually written
    REQUIRE(fs::exists(side_path));

    AssetImporter importer;
    importer.Initialize(nullptr);

    // Default settings use scale=1.0 — the sidecar should override to 0.01
    ImportSettings settings; // scale=1.0 by default
    ImportResult result = importer.Import(obj_path, settings);

    REQUIRE(result.success);
    // The triangle verts at (0,0,0),(1,0,0),(0,1,0) scaled by 0.01 should be near 0
    const auto& verts = result.scene.meshes[0].vertices;
    bool any_above_half = false;
    for (const auto& v : verts) {
        if (v.position.x > 0.5f || v.position.y > 0.5f) {
            any_above_half = true;
        }
    }
    // With scale=0.01 the max coordinate is 0.01, so none should exceed 0.5
    CHECK_FALSE(any_above_half);
}

TEST_CASE("AssetImporter - reports error for missing file", "[importer][integration]") {
    AssetImporter importer;
    importer.Initialize(nullptr);

    ImportResult result = importer.Import("/nonexistent/path/missing.obj");
    CHECK_FALSE(result.success);
    CHECK_FALSE(result.error_message.empty());
}

TEST_CASE("AssetImporter - skeleton is empty for non-skinned mesh", "[importer][integration][animation]") {
    std::string obj_path = WriteTempOBJ();

    AssetImporter importer;
    importer.Initialize(nullptr);

    ImportSettings settings;
    settings.import_animations = true;  // request anim extraction — but OBJ has no bones

    ImportResult result = importer.Import(obj_path, settings);

    REQUIRE(result.success);
    CHECK_FALSE(result.scene.skeleton.is_valid()); // OBJ has no skeleton
    CHECK(result.scene.animations.empty());
}

// ─── AnimationLibrary — integration with Skeleton / Clip ─────────────────────

TEST_CASE("AnimationLibrary - skeleton and clip round-trip", "[animation][library][integration]") {
    AnimationLibrary lib;

    // Add a skeleton
    Skeleton skel;
    skel.name = "PlayerSkeleton";
    {
        Bone hip; hip.name = "Hip"; hip.parent_index = -1;
        hip.local_rest_transform = mat4::identity();
        hip.inv_bind_pose        = mat4::identity();
        skel.bones.push_back(hip);

        Bone spine; spine.name = "Spine"; spine.parent_index = 0;
        spine.local_rest_transform = mat4::identity();
        spine.inv_bind_pose        = mat4::identity();
        skel.bones.push_back(spine);
    }
    lib.AddSkeleton(skel);

    // Add a clip
    AnimationClip clip;
    clip.name     = "Idle";
    clip.duration = 2.0f;
    clip.looping  = true;
    {
        BoneChannel hip_ch;
        hip_ch.bone_index = 0;
        hip_ch.position_keys = {{0.0f, {0,0,0}}, {2.0f, {0,0,0}}};
        hip_ch.rotation_keys = {{0.0f, quat::identity()}, {2.0f, quat::identity()}};
        hip_ch.scale_keys    = {{0.0f, {1,1,1}},           {2.0f, {1,1,1}}};
        clip.channels.push_back(hip_ch);
    }
    lib.AddClip(clip);

    // Verify both round-trip correctly
    const Skeleton*      found_skel = lib.GetSkeleton("PlayerSkeleton");
    const AnimationClip* found_clip = lib.GetClip("Idle");

    REQUIRE(found_skel != nullptr);
    REQUIRE(found_clip != nullptr);
    CHECK(found_skel->bones.size() == 2);
    CHECK(found_skel->FindBone("Spine") == 1);
    CHECK(found_clip->duration == 2.0f);
    CHECK(found_clip->channels.size() == 1);
}

TEST_CASE("AnimationLibrary - enumerates all clips", "[animation][library][integration]") {
    AnimationLibrary lib;

    for (const char* name : {"Walk", "Run", "Jump", "Attack"}) {
        AnimationClip c;
        c.name     = name;
        c.duration = 1.0f;
        c.looping  = true;
        lib.AddClip(c);
    }

    const auto& clips = lib.GetClips();
    CHECK(clips.size() == 4);
}
