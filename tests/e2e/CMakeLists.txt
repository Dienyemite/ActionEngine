add_executable(E2ETests
    test_engine_lifecycle.cpp
)

target_include_directories(E2ETests PRIVATE ${ENGINE_TEST_INCLUDES})

target_link_libraries(E2ETests PRIVATE
    ActionEngine   # Full engine (minus Vulkan window) — links all subsystems
    Catch2::Catch2WithMain
)

if(MSVC)
    target_compile_options(E2ETests PRIVATE /Zc:preprocessor /W4)
endif()

catch_discover_tests(E2ETests)
