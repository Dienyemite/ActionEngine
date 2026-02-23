/*
 * Unit Tests — Math
 *
 * Outside-In TDD: these are the lowest-level tests that drove the
 * implementation of math.h. Red → Green → Refactor.
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "core/math/math.h"

using namespace action;
using Catch::Approx;

// ============================================================
// SmoothStep
// ============================================================

TEST_CASE("SmoothStep - returns 0 below lower edge", "[math][smoothstep]") {
    REQUIRE(SmoothStep(0.0f, 1.0f, -1.0f) == Approx(0.0f));
    REQUIRE(SmoothStep(0.0f, 1.0f,  0.0f) == Approx(0.0f));
}

TEST_CASE("SmoothStep - returns 1 above upper edge", "[math][smoothstep]") {
    REQUIRE(SmoothStep(0.0f, 1.0f, 1.0f) == Approx(1.0f));
    REQUIRE(SmoothStep(0.0f, 1.0f, 2.0f) == Approx(1.0f));
}

TEST_CASE("SmoothStep - midpoint returns 0.5", "[math][smoothstep]") {
    // t = 0.5 → 0.5*0.5*(3 - 2*0.5) = 0.25 * 2 = 0.5
    REQUIRE(SmoothStep(0.0f, 1.0f, 0.5f) == Approx(0.5f));
}

TEST_CASE("SmoothStep - degenerate: edge0 == edge1 returns 0 below, 1 above", "[math][smoothstep]") {
    // Fix #30: guard against division by zero
    REQUIRE(SmoothStep(0.5f, 0.5f, 0.0f) == Approx(0.0f));
    REQUIRE(SmoothStep(0.5f, 0.5f, 1.0f) == Approx(1.0f));
}

TEST_CASE("SmoothStep - output is monotonically non-decreasing", "[math][smoothstep]") {
    float prev = SmoothStep(0.0f, 1.0f, 0.0f);
    for (int i = 1; i <= 10; ++i) {
        float t   = static_cast<float>(i) / 10.0f;
        float cur = SmoothStep(0.0f, 1.0f, t);
        REQUIRE(cur >= prev);
        prev = cur;
    }
}

// ============================================================
// Quaternion
// ============================================================

TEST_CASE("quat::from_axis_angle - unit axis produces unit quaternion", "[math][quat]") {
    quat q = quat::from_axis_angle({0, 1, 0}, Radians(90.0f));
    float len_sq = q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w;
    REQUIRE(len_sq == Approx(1.0f).epsilon(1e-5f));
}

TEST_CASE("quat::from_axis_angle - unnormalized axis still produces unit quaternion", "[math][quat]") {
    // Fix #31: axis must be normalized inside from_axis_angle
    quat q = quat::from_axis_angle({0, 5, 0}, Radians(90.0f));  // length-5 axis
    float len_sq = q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w;
    REQUIRE(len_sq == Approx(1.0f).epsilon(1e-5f));
}

TEST_CASE("quat::from_axis_angle - identity rotation maps y-axis to y-axis", "[math][quat]") {
    // 0-degree rotation on any axis should give identity quaternion (w=1, xyz=0)
    quat q = quat::from_axis_angle({1, 0, 0}, 0.0f);
    REQUIRE(q.w == Approx(1.0f).epsilon(1e-5f));
    REQUIRE(std::abs(q.x) == Approx(0.0f).margin(1e-5f));
    REQUIRE(std::abs(q.y) == Approx(0.0f).margin(1e-5f));
    REQUIRE(std::abs(q.z) == Approx(0.0f).margin(1e-5f));
}

// ============================================================
// Clamp / Lerp
// ============================================================

TEST_CASE("Clamp - stays within [min, max]", "[math][clamp]") {
    REQUIRE(Clamp(-1.0f, 0.0f, 1.0f) == Approx(0.0f));
    REQUIRE(Clamp( 2.0f, 0.0f, 1.0f) == Approx(1.0f));
    REQUIRE(Clamp( 0.5f, 0.0f, 1.0f) == Approx(0.5f));
}

TEST_CASE("Lerp - midpoint", "[math][lerp]") {
    REQUIRE(Lerp(0.0f, 10.0f, 0.5f) == Approx(5.0f));
    REQUIRE(Lerp(0.0f, 10.0f, 0.0f) == Approx(0.0f));
    REQUIRE(Lerp(0.0f, 10.0f, 1.0f) == Approx(10.0f));
}
