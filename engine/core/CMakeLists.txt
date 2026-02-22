add_library(EngineCore STATIC
    types.h
    logging.h
    logging.cpp
    profiler.h
    profiler.cpp
    
    memory/allocators.h
    memory/allocators.cpp
    
    jobs/job_system.h
    jobs/job_system.cpp
    
    containers/sparse_set.h
    
    math/math.h
    math/math.cpp
)

target_include_directories(EngineCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

if(MSVC)
    # /Zc:preprocessor enables conformant C++20 preprocessor required for __VA_OPT__ (#29)
    target_compile_options(EngineCore PRIVATE /W4 /WX /Zc:preprocessor)
else()
    target_compile_options(EngineCore PRIVATE -Wall -Wextra -Werror -msse2)
endif()
