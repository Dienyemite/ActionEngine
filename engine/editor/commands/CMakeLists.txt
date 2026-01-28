# Editor Commands Library
add_library(editor_commands STATIC
    command.cpp
    command.h
    editor_commands.cpp
    editor_commands.h
)

target_include_directories(editor_commands PUBLIC
    ${CMAKE_SOURCE_DIR}/engine
)

target_link_libraries(editor_commands PUBLIC
    core
)
