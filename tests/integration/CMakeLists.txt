add_executable(IntegrationTests
    test_ecs_systems.cpp
    test_physics_world.cpp
    test_blender_integration.cpp
)

target_include_directories(IntegrationTests PRIVATE ${ENGINE_TEST_INCLUDES})

target_link_libraries(IntegrationTests PRIVATE
    EngineCore
    EngineGameplay
    EnginePhysics
    EngineAnimation
    EngineAssets
    Catch2::Catch2WithMain
)

if(MSVC)
    target_compile_options(IntegrationTests PRIVATE /Zc:preprocessor /W4)
endif()

catch_discover_tests(IntegrationTests)
