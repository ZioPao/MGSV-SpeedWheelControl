cmake_minimum_required(VERSION 3.20)
project(KeyboardSpeedControl VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(MINHOOK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/MinHook")

# Single source of truth for the mod version: the project() version above feeds
# both the log banner and the DLL VERSIONINFO resource.
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/ksc_version.h"
    @ONLY
)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/KeyboardSpeedControl.rc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/KeyboardSpeedControl.rc"
    @ONLY
)

add_library(KeyboardSpeedControl SHARED
    dllmain.cpp
    speed.cpp
    config.cpp
    logger.cpp
    common.cpp
    scanner.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated/KeyboardSpeedControl.rc
    ${MINHOOK_DIR}/src/hook.c
    ${MINHOOK_DIR}/src/buffer.c
    ${MINHOOK_DIR}/src/trampoline.c
    ${MINHOOK_DIR}/src/hde/hde64.c
)

target_include_directories(KeyboardSpeedControl PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/generated
    ${MINHOOK_DIR}/include
    ${MINHOOK_DIR}/src
    ${MINHOOK_DIR}/src/hde
)

target_compile_definitions(KeyboardSpeedControl PRIVATE
    _CRT_SECURE_NO_WARNINGS
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

if(MSVC)
    target_compile_options(KeyboardSpeedControl PRIVATE /MT /W3 /EHsc /permissive-)
    target_link_options(KeyboardSpeedControl PRIVATE /INCREMENTAL:NO)
endif()

target_link_libraries(KeyboardSpeedControl PRIVATE version)

set_target_properties(KeyboardSpeedControl PROPERTIES
    OUTPUT_NAME "KeyboardSpeedControl"
    PREFIX ""
)
