cmake_minimum_required(VERSION 3.20)
project(SpeedWheel VERSION 1.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(MINHOOK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/MinHook")

# Single source of truth for the mod version: the project() version above feeds
# both the log banner and the DLL VERSIONINFO resource.
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/sw_version.h"
    @ONLY
)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/SpeedWheel.rc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/SpeedWheel.rc"
    @ONLY
)

add_library(SpeedWheel SHARED
    dllmain.cpp
    speed.cpp
    gate.cpp
    config.cpp
    logger.cpp
    common.cpp
    scanner.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated/SpeedWheel.rc
    ${MINHOOK_DIR}/src/hook.c
    ${MINHOOK_DIR}/src/buffer.c
    ${MINHOOK_DIR}/src/trampoline.c
    ${MINHOOK_DIR}/src/hde/hde64.c
)

target_include_directories(SpeedWheel PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/generated
    ${MINHOOK_DIR}/include
    ${MINHOOK_DIR}/src
    ${MINHOOK_DIR}/src/hde
)

target_compile_definitions(SpeedWheel PRIVATE
    _CRT_SECURE_NO_WARNINGS
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

if(MSVC)
    target_compile_options(SpeedWheel PRIVATE /MT /W3 /EHsc /permissive-)
    target_link_options(SpeedWheel PRIVATE /INCREMENTAL:NO)
endif()

target_link_libraries(SpeedWheel PRIVATE version)

set_target_properties(SpeedWheel PROPERTIES
    OUTPUT_NAME "SpeedWheel"
    PREFIX ""
)
