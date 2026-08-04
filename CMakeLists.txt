cmake_minimum_required(VERSION 3.28)

project(VehicleExplosionCrashGuard VERSION 1.0.0 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "VehicleExplosionCrashGuard is Windows-only.")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "VehicleExplosionCrashGuard must be built as x64.")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

include(FetchContent)

# Pin both dependencies so a later upstream change cannot silently alter the build.
set(RED4EXT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    red4ext_sdk
    GIT_REPOSITORY https://github.com/WopsS/RED4ext.SDK.git
    GIT_TAG 1.0.0
    GIT_SHALLOW TRUE
    GIT_SUBMODULES_RECURSE TRUE
)
FetchContent_MakeAvailable(red4ext_sdk)

set(SAFETYHOOK_FETCH_ZYDIS ON CACHE BOOL "" FORCE)
set(SAFETYHOOK_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SAFETYHOOK_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SAFETYHOOK_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(SAFETYHOOK_BUILD_MODULE OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    safetyhook
    GIT_REPOSITORY https://github.com/cursey/safetyhook.git
    GIT_TAG v0.7.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(safetyhook)

add_library(${PROJECT_NAME} SHARED
    src/Main.cpp
    src/CrashGuard.cpp
    src/CrashGuard.hpp
    src/BuildTarget.hpp
    src/Diagnostics.cpp
    src/Diagnostics.hpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    RED4ext::SDK
    safetyhook::safetyhook
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /permissive- /EHsc /Zc:__cplusplus)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    PREFIX ""
    OUTPUT_NAME "VehicleExplosionCrashGuard"
)

set(DIST_PLUGIN_DIR "${CMAKE_SOURCE_DIR}/dist/red4ext/plugins/VehicleExplosionCrashGuard")
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DIST_PLUGIN_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:${PROJECT_NAME}>"
            "${DIST_PLUGIN_DIR}/VehicleExplosionCrashGuard.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/README_NEXUS.md"
            "${DIST_PLUGIN_DIR}/README.md"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
            "${DIST_PLUGIN_DIR}/THIRD_PARTY_NOTICES.md"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/LICENSE"
            "${DIST_PLUGIN_DIR}/LICENSE.txt"
    VERBATIM
)
