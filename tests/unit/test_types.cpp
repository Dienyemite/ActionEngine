/*
 * Unit Tests — Types
 *
 * Covers Result<T,E> including the Result<std::string> case that was
 * ill-formed before fix #40 (variant<T,T> is undefined behaviour).
 */
#include <catch2/catch_test_macros.hpp>

#include "core/types.h"

using namespace action;

// ============================================================
// Result<T,E> — basic contract
// ============================================================

TEST_CASE("Result - Ok wraps a value and is_ok returns true", "[types][result]") {
    Result<int> r = Ok(42);
    REQUIRE(is_ok(r));
    REQUIRE(get_value(r) == 42);
}

TEST_CASE("Result - Err wraps an error and is_ok returns false", "[types][result]") {
    Result<int> r = Err<std::string>("something went wrong");
    REQUIRE_FALSE(is_ok(r));
    REQUIRE(get_error(r) == "something went wrong");
}

TEST_CASE("Result - Ok and Err are distinct variant members for same type (fix #40)", "[types][result]") {
    // Before fix #40 this was std::variant<std::string, std::string> — ill-formed.
    // OkResult<string> and ErrResult<string> are distinct types, so this must compile
    // and route correctly.
    Result<std::string> ok  = Ok<std::string>("hello");
    Result<std::string> err = Err<std::string>("oops");

    REQUIRE(is_ok(ok));
    REQUIRE_FALSE(is_ok(err));
    REQUIRE(get_value(ok)  == "hello");
    REQUIRE(get_error(err) == "oops");
}

TEST_CASE("Result - get_value returns mutable reference", "[types][result]") {
    Result<int> r = Ok(10);
    get_value(r) = 99;
    REQUIRE(get_value(r) == 99);
}

TEST_CASE("Result - custom error type", "[types][result]") {
    enum class Code { NotFound, PermissionDenied };
    Result<float, Code> r = Err<Code>(Code::NotFound);
    REQUIRE_FALSE(is_ok(r));
    REQUIRE(get_error(r) == Code::NotFound);
}

// ============================================================
// Handle
// ============================================================

TEST_CASE("Handle - default-constructed is invalid", "[types][handle]") {
    MeshHandle h{};
    REQUIRE_FALSE(h.is_valid());
}

TEST_CASE("Handle - equality compares index and generation", "[types][handle]") {
    Handle<int> a{1, 0};
    Handle<int> b{1, 0};
    Handle<int> c{1, 1};  // same index, different generation
    REQUIRE(a == b);
    REQUIRE(a != c);
}

// ============================================================
// Entity encoding
// ============================================================

TEST_CASE("Entity - MakeEntity round-trips index and generation", "[types][entity]") {
    u32 index = 12345;
    u32 gen   = 7;
    Entity e  = MakeEntity(index, gen);
    REQUIRE(EntityIndex(e)      == index);
    REQUIRE(EntityGeneration(e) == gen);
}

TEST_CASE("Entity - INVALID_ENTITY is distinguishable from any valid entity", "[types][entity]") {
    Entity e = MakeEntity(0, 0);
    REQUIRE(e != INVALID_ENTITY);
}
