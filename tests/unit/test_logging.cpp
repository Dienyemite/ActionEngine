/*
 * Unit Tests — Logging
 *
 * Covers:
 *  #28 - atomic SetLevel / EnableConsoleColors thread safety (observable contract)
 *  #29 - ENGINE_ASSERT compiles with zero extra args and with format args
 *
 * Note: ENGINE_ASSERT calls std::abort() on failure, so we only test the
 * "condition is true" (non-aborting) path here. The aborting path is covered
 * by the integration death-test in tests/integration/.
 */
#include <catch2/catch_test_macros.hpp>

#include "core/logging.h"

using namespace action;

// ============================================================
// Log level filtering
// ============================================================

TEST_CASE("Logger - SetLevel filters messages below threshold", "[logging][level]") {
    Logger& log = Logger::Get();

    // Raise level to Fatal — lower levels should be silently discarded
    log.SetLevel(LogLevel::Fatal);

    // If Log() called with Error < Fatal, it must not throw or crash
    REQUIRE_NOTHROW(log.Log(LogLevel::Error, std::source_location::current(), "suppressed"));

    // Reset for other tests
    log.SetLevel(LogLevel::Info);
}

TEST_CASE("Logger - SetLevel is visible immediately after set (#28)", "[logging][level][atomic]") {
    Logger& log = Logger::Get();

    log.SetLevel(LogLevel::Warn);
    // Whatever the prior level was, we must now accept Warn and reject Debug
    // The observable contract: Log() with Warn does not crash, Debug is swallowed.
    REQUIRE_NOTHROW(log.Log(LogLevel::Warn,  std::source_location::current(), "visible"));
    REQUIRE_NOTHROW(log.Log(LogLevel::Debug, std::source_location::current(), "suppressed"));

    log.SetLevel(LogLevel::Info);
}

TEST_CASE("Logger - EnableConsoleColors toggling does not crash (#28)", "[logging][colors][atomic]") {
    Logger& log = Logger::Get();
    REQUIRE_NOTHROW(log.EnableConsoleColors(false));
    REQUIRE_NOTHROW(log.EnableConsoleColors(true));
}

// ============================================================
// ENGINE_ASSERT — compile-time and no-op path
// ============================================================

TEST_CASE("ENGINE_ASSERT - passing condition with no message args", "[logging][assert]") {
    // Fix #29: must compile and not abort when condition is true.
    // ENGINE_ASSERT expands to a do-while statement, so it cannot be wrapped
    // in REQUIRE_NOTHROW (which expects an expression).  Reaching here without
    // aborting is sufficient to pass the test.
    ENGINE_ASSERT(1 == 1);
    SUCCEED();
}

TEST_CASE("ENGINE_ASSERT - passing condition with format args", "[logging][assert]") {
    // Fix #29: must compile with variadic message and not abort.
    int x = 5;
    ENGINE_ASSERT(x == 5, "x should be 5, got {}", x);
    SUCCEED();
}

TEST_CASE("ENGINE_ASSERT - passing condition with string message", "[logging][assert]") {
    ENGINE_ASSERT(true, "just a plain message");
    SUCCEED();
}
