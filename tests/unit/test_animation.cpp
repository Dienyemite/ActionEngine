/*
 * Unit Tests — Blender Integration: Animation System + .aeimport parser
 *
 * Covers:
 *   - AEImport struct defaults and LoadAEImport() with in-memory JSON
 *   - AnimationClip keyframe sampling (position lerp, rotation slerp)
 *   - Skeleton ComputeWorldTransforms / ComputeSkinningMatrices
 *   - AnimationLibrary add / lookup by name
 *   - AnimTransition::Evaluate condition logic
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "assets/aeimport.h"
#include "animation/animation_clip.h"
#include "animation/animation_library.h"
#include "animation/animation_state_machine.h"
#include "animation/skeleton.h"

#include <fstream>
#include <filesystem>
#include <cstring>

using namespace action;
using namespace Catch::Matchers;
namespace fs = std::filesystem;

// ─── Helpers ─────────────────────────────────────────────────────────────────

// Write a minimal .aeimport JSON file to a temp path, return the path.
static std::string WriteTempSidecar(const std::string& json_text) {
    auto path = fs::temp_directory_path() / "test_blender.aeimport";
    std::ofstream f(path);
    f << json_text;
    return path.string();
}

// Build a 1-bone Skeleton (root only, identity transforms).
static Skeleton MakeTrivialSkeleton(const std::string& name = "TestSkeleton") {
    Skeleton skel;
    skel.name = name;

    Bone root;
    root.name = "Root";
    root.parent_index       = -1;
    root.local_rest_transform = mat4::identity();
    root.inv_bind_pose        = mat4::identity();
    skel.bones.push_back(root);
    return skel;
}

// Build a simple AnimationClip for bone 0 with two position keyframes at t=0 and t=1.
static AnimationClip MakeLinearPosClip(vec3 from, vec3 to) {
    AnimationClip clip;
    clip.name     = "TestClip";
    clip.duration = 1.0f;
    clip.looping  = true;

    BoneChannel ch;
    ch.bone_index = 0;
    ch.position_keys = {{0.0f, from}, {1.0f, to}};
    ch.rotation_keys = {{0.0f, quat::identity()}, {1.0f, quat::identity()}};
    ch.scale_keys    = {{0.0f, {1,1,1}},           {1.0f, {1,1,1}}};
    clip.channels.push_back(ch);
    return clip;
}

// ─── .aeimport parser tests ───────────────────────────────────────────────────

TEST_CASE(".aeimport - default values when file is missing", "[aeimport]") {
    AEImport data = LoadAEImport("/nonexistent/path/none.aeimport");
    CHECK_FALSE(data.is_valid());
    // Default overrides must be safe / non-zero
    CHECK(data.import_settings.scale == 1.0f);
    CHECK(data.import_settings.flip_uvs == true);
}

TEST_CASE(".aeimport - parses import_settings block", "[aeimport]") {
    const char* json = R"({
        "ae_version": 1,
        "source_file": "character.blend",
        "import_settings": {
            "scale": 0.01,
            "flip_uvs": false,
            "generate_normals": true,
            "optimize_meshes": false,
            "up_axis": "Z",
            "import_animations": true
        },
        "lod": { "generate": false, "level_count": 1, "distances": [] },
        "mesh_overrides": [],
        "animation_names": ["Walk", "Run"]
    })";

    std::string path = WriteTempSidecar(json);
    AEImport data = LoadAEImport(path);

    REQUIRE(data.is_valid());
    CHECK(data.ae_version == 1);
    CHECK(data.source_file == "character.blend");
    CHECK_THAT(data.import_settings.scale, WithinAbs(0.01f, 1e-5f));
    CHECK(data.import_settings.flip_uvs == false);
    CHECK(data.import_settings.generate_normals == true);
    CHECK(data.import_settings.up_axis == "Z");
    CHECK(data.import_settings.import_animations == true);
    CHECK(data.animation_names.size() == 2);
    CHECK(data.animation_names[0] == "Walk");
}

TEST_CASE(".aeimport - parses lod block and distances", "[aeimport]") {
    const char* json = R"({
        "ae_version": 1,
        "source_file": "tree.glb",
        "import_settings": {},
        "lod": {
            "generate": true,
            "level_count": 3,
            "distances": [20.0, 60.0]
        },
        "mesh_overrides": [],
        "animation_names": []
    })";

    std::string path = WriteTempSidecar(json);
    AEImport data = LoadAEImport(path);

    REQUIRE(data.is_valid());
    CHECK(data.lod.generate == true);
    CHECK(data.lod.level_count == 3);
    REQUIRE(data.lod.distances.size() == 2);
    CHECK_THAT(data.lod.distances[0], WithinAbs(20.0f, 1e-4f));
    CHECK_THAT(data.lod.distances[1], WithinAbs(60.0f, 1e-4f));
}

TEST_CASE(".aeimport - parses mesh_overrides", "[aeimport]") {
    const char* json = R"({
        "ae_version": 1,
        "source_file": "wall.glb",
        "import_settings": {},
        "lod": {"generate": false, "level_count": 1, "distances": []},
        "mesh_overrides": [{
            "name": "WallMesh",
            "collision": "trimesh",
            "physics_layer": 2,
            "cast_shadows": false,
            "lod_bias": 1.5,
            "static": true
        }],
        "animation_names": []
    })";

    std::string path = WriteTempSidecar(json);
    AEImport data = LoadAEImport(path);

    REQUIRE(data.is_valid());
    REQUIRE(data.mesh_overrides.size() == 1);
    const auto& ov = data.mesh_overrides[0];
    CHECK(ov.name == "WallMesh");
    CHECK(ov.collision_type == "trimesh");
    CHECK(ov.physics_layer == 2);
    CHECK(ov.cast_shadows == false);
    CHECK(ov.is_static == true);
    CHECK_THAT(ov.lod_bias, WithinAbs(1.5f, 1e-5f));
}

TEST_CASE(".aeimport - FindMeshOverride returns nullptr when not found", "[aeimport]") {
    AEImport data;
    data.ae_version = 1; // make is_valid() return true
    CHECK(data.FindMeshOverride("DoesNotExist") == nullptr);
}

TEST_CASE(".aeimport - FindMeshOverride finds correct entry", "[aeimport]") {
    AEImport data;
    data.ae_version = 1;
    AEMeshOverride ov;
    ov.name = "Target";
    ov.collision_type = "box";
    data.mesh_overrides.push_back(ov);

    const AEMeshOverride* found = data.FindMeshOverride("Target");
    REQUIRE(found != nullptr);
    CHECK(found->collision_type == "box");
}

// ─── AnimationClip sampling ───────────────────────────────────────────────────

TEST_CASE("AnimationClip - samples midpoint position correctly", "[animation][clip]") {
    AnimationClip clip = MakeLinearPosClip({0, 0, 0}, {10, 0, 0});

    std::vector<mat4> local_transforms(1, mat4::identity());
    clip.Sample(0.5f, local_transforms);

    // Translation column of the output matrix should be (5, 0, 0)
    // Engine mat4: m[col][row], translation is column 3: m[3][0..2]
    CHECK_THAT(local_transforms[0].m[3][0], WithinAbs(5.0f, 1e-4f));
    CHECK_THAT(local_transforms[0].m[3][1], WithinAbs(0.0f, 1e-4f));
    CHECK_THAT(local_transforms[0].m[3][2], WithinAbs(0.0f, 1e-4f));
}

TEST_CASE("AnimationClip - samples at t=0 returns first key", "[animation][clip]") {
    AnimationClip clip = MakeLinearPosClip({1, 2, 3}, {7, 8, 9});

    std::vector<mat4> out(1, mat4::identity());
    clip.Sample(0.0f, out);

    CHECK_THAT(out[0].m[3][0], WithinAbs(1.0f, 1e-4f));
    CHECK_THAT(out[0].m[3][1], WithinAbs(2.0f, 1e-4f));
    CHECK_THAT(out[0].m[3][2], WithinAbs(3.0f, 1e-4f));
}

TEST_CASE("AnimationClip - looping wraps time past duration", "[animation][clip]") {
    AnimationClip clip = MakeLinearPosClip({0, 0, 0}, {10, 0, 0});
    clip.looping = true;

    // Sample() expects a pre-wrapped time; the playback system applies fmod.
    // t = 1.5 mod 1.0 = 0.5 → position = (5, 0, 0)
    float wrapped = std::fmod(1.5f, clip.duration);
    std::vector<mat4> out(1, mat4::identity());
    clip.Sample(wrapped, out);
    CHECK_THAT(out[0].m[3][0], WithinAbs(5.0f, 0.01f));
}

TEST_CASE("AnimationClip - identity rotation produces correct matrix diagonal", "[animation][clip]") {
    AnimationClip clip = MakeLinearPosClip({0, 0, 0}, {0, 0, 0});

    std::vector<mat4> out(1, mat4::identity());
    clip.Sample(0.5f, out);

    // Upper-left 3×3 should be identity (scale=1, no rotation)
    CHECK_THAT(out[0].m[0][0], WithinAbs(1.0f, 1e-4f));
    CHECK_THAT(out[0].m[1][1], WithinAbs(1.0f, 1e-4f));
    CHECK_THAT(out[0].m[2][2], WithinAbs(1.0f, 1e-4f));
}

// ─── Skeleton ────────────────────────────────────────────────────────────────

TEST_CASE("Skeleton - FindBone returns correct index", "[animation][skeleton]") {
    Skeleton skel = MakeTrivialSkeleton();
    CHECK(skel.FindBone("Root") == 0);
    CHECK(skel.FindBone("NonExistent") == -1);
}

TEST_CASE("Skeleton - ComputeWorldTransforms root is local transform", "[animation][skeleton]") {
    Skeleton skel = MakeTrivialSkeleton();
    std::vector<mat4> local(1, mat4::identity());
    // Translate the root 5 units along X
    local[0].m[3][0] = 5.0f;

    std::vector<mat4> world;
    skel.ComputeWorldTransforms(local, world);
    REQUIRE(world.size() == 1);
    CHECK_THAT(world[0].m[3][0], WithinAbs(5.0f, 1e-5f));
}

TEST_CASE("Skeleton - ComputeSkinningMatrices with identity inv_bind returns world", "[animation][skeleton]") {
    Skeleton skel = MakeTrivialSkeleton();
    std::vector<mat4> world(1, mat4::identity());
    world[0].m[3][0] = 3.0f; // translate

    std::vector<mat4> skin;
    skel.ComputeSkinningMatrices(world, skin);
    REQUIRE(skin.size() == 1);
    // skinning = world * inv_bind_pose; inv_bind_pose = identity → skinning = world
    CHECK_THAT(skin[0].m[3][0], WithinAbs(3.0f, 1e-5f));
}

// ─── AnimationLibrary ─────────────────────────────────────────────────────────

TEST_CASE("AnimationLibrary - add and get skeleton by name", "[animation][library]") {
    AnimationLibrary lib;
    Skeleton skel = MakeTrivialSkeleton("Hero");
    lib.AddSkeleton(skel);

    const Skeleton* result = lib.GetSkeleton("Hero");
    REQUIRE(result != nullptr);
    CHECK(result->name == "Hero");
    CHECK(result->bones.size() == 1);
}

TEST_CASE("AnimationLibrary - get unknown skeleton returns nullptr", "[animation][library]") {
    AnimationLibrary lib;
    CHECK(lib.GetSkeleton("Unknown") == nullptr);
}

TEST_CASE("AnimationLibrary - add and get clip by name", "[animation][library]") {
    AnimationLibrary lib;
    AnimationClip clip = MakeLinearPosClip({0,0,0}, {1,0,0});
    clip.name = "Walk";
    lib.AddClip(clip);

    const AnimationClip* result = lib.GetClip("Walk");
    REQUIRE(result != nullptr);
    CHECK(result->name == "Walk");
    CHECK(result->duration == 1.0f);
}

TEST_CASE("AnimationLibrary - duplicate skeleton name overwrites", "[animation][library]") {
    AnimationLibrary lib;

    Skeleton a = MakeTrivialSkeleton("Char");
    a.bones[0].name = "OldRoot";
    lib.AddSkeleton(a);

    Skeleton b = MakeTrivialSkeleton("Char");
    b.bones[0].name = "NewRoot";
    lib.AddSkeleton(b);

    const Skeleton* result = lib.GetSkeleton("Char");
    REQUIRE(result != nullptr);
    CHECK(result->bones[0].name == "NewRoot");
}

// ─── AnimTransition condition evaluation ─────────────────────────────────────

TEST_CASE("AnimTransition - float Greater triggers when above threshold", "[animation][statemachine]") {
    AnimTransition t;
    t.from_state = 0;
    t.to_state   = 1;
    t.blend_duration = 0.2f;
    t.conditions.push_back({"Speed", AnimCondition::Op::FloatGreater, 0.5f});

    AnimationStateMachineComponent sm;
    sm.AddState("Idle", "Idle", 1.0f, true);
    sm.AddState("Run",  "Run",  1.0f, true);

    sm.SetFloat("Speed", 0.3f);
    CHECK_FALSE(t.Evaluate(sm.float_params, sm.bool_params, sm.triggers));

    sm.SetFloat("Speed", 0.8f);
    CHECK(t.Evaluate(sm.float_params, sm.bool_params, sm.triggers));
}

TEST_CASE("AnimTransition - BoolTrue fires only when parameter is true", "[animation][statemachine]") {
    AnimTransition t;
    t.from_state = 0;
    t.to_state   = 1;
    t.blend_duration = 0.1f;
    t.conditions.push_back({"IsGrounded", AnimCondition::Op::BoolTrue, 0.0f});

    AnimationStateMachineComponent sm;
    sm.AddState("A", "ClipA");
    sm.AddState("B", "ClipB");

    sm.SetBool("IsGrounded", false);
    CHECK_FALSE(t.Evaluate(sm.float_params, sm.bool_params, sm.triggers));

    sm.SetBool("IsGrounded", true);
    CHECK(t.Evaluate(sm.float_params, sm.bool_params, sm.triggers));
}

TEST_CASE("AnimTransition - Trigger fires once then clears", "[animation][statemachine]") {
    AnimTransition t;
    t.from_state = 0;
    t.to_state   = 1;
    t.blend_duration = 0.0f;
    t.conditions.push_back({"Attack", AnimCondition::Op::Trigger, 0.0f});

    AnimationStateMachineComponent sm;
    sm.AddState("Idle",   "Idle",   1.0f, false);
    sm.AddState("Attack", "Attack", 1.0f, false);

    // Not set — should not fire
    CHECK_FALSE(t.Evaluate(sm.float_params, sm.bool_params, sm.triggers));

    sm.SetTrigger("Attack");
    CHECK(t.Evaluate(sm.float_params, sm.bool_params, sm.triggers));   // fires and consumes the trigger

    // Trigger was consumed inside Evaluate — a second eval should NOT fire
    CHECK_FALSE(t.Evaluate(sm.float_params, sm.bool_params, sm.triggers));
}
