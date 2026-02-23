add_executable(UnitTests
    test_math.cpp
    test_types.cpp
    test_ecs.cpp
    test_logging.cpp
)

target_include_directories(UnitTests PRIVATE ${ENGINE_TEST_INCLUDES})

target_link_libraries(UnitTests PRIVATE
    EngineCore
    EngineGameplay   # ECS
    Catch2::Catch2WithMain
)

if(MSVC)
    target_compile_options(UnitTests PRIVATE /Zc:preprocessor /W4)
endif()

catch_discover_tests(UnitTests)
