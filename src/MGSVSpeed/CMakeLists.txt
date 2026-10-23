cmake_minimum_required(VERSION 3.20)
project(MGSVSpeed LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(MINHOOK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/MinHook")

add_library(MGSVSpeed SHARED
    dllmain.cpp
    speed.cpp
    config.cpp
    logger.cpp
    common.cpp
    scanner.cpp
    ${MINHOOK_DIR}/src/hook.c
    ${MINHOOK_DIR}/src/buffer.c
    ${MINHOOK_DIR}/src/trampoline.c
    ${MINHOOK_DIR}/src/hde/hde64.c
)

target_include_directories(MGSVSpeed PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${MINHOOK_DIR}/include
    ${MINHOOK_DIR}/src
    ${MINHOOK_DIR}/src/hde
)

target_compile_definitions(MGSVSpeed PRIVATE
    _CRT_SECURE_NO_WARNINGS
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

if(MSVC)
    target_compile_options(MGSVSpeed PRIVATE /MT /W3 /EHsc /permissive-)
    target_link_options(MGSVSpeed PRIVATE /INCREMENTAL:NO)
endif()

target_link_libraries(MGSVSpeed PRIVATE version)

set_target_properties(MGSVSpeed PROPERTIES
    OUTPUT_NAME "MGSVSpeed"
    PREFIX ""
)
