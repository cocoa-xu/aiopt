cmake_minimum_required(VERSION 3.24)

project(aiopt
    VERSION 0.1.0
    DESCRIPTION "Natural-language command line parsing for C++, resolved by a local language model"
    LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakeDependentOption)

option(AIOPT_BUILD_EXAMPLES "Build the example programs" ${PROJECT_IS_TOP_LEVEL})
option(AIOPT_BUILD_BENCHMARKS "Build the correctness and latency benchmarks" ${PROJECT_IS_TOP_LEVEL})
option(AIOPT_FETCH_LLAMA "Download llama.cpp when no target is already available" ON)

set(AIOPT_LLAMA_SOURCE_DIR "" CACHE PATH
    "Path to an existing llama.cpp checkout; overrides AIOPT_FETCH_LLAMA")
set(AIOPT_LLAMA_TAG "b10107" CACHE STRING
    "llama.cpp git tag to fetch when no checkout is supplied")

# ---------------------------------------------------------------------------
# llama.cpp
# ---------------------------------------------------------------------------
set(AIOPT_LLAMA_VENDORED FALSE)

if(NOT TARGET llama)
    if(AIOPT_LLAMA_SOURCE_DIR)
        message(STATUS "aiopt: using llama.cpp from ${AIOPT_LLAMA_SOURCE_DIR}")
        set(AIOPT_LLAMA_VENDORED TRUE)
        set(LLAMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(LLAMA_BUILD_SERVER OFF CACHE BOOL "" FORCE)
        set(LLAMA_CURL OFF CACHE BOOL "" FORCE)
        add_subdirectory(${AIOPT_LLAMA_SOURCE_DIR} ${CMAKE_BINARY_DIR}/llama.cpp EXCLUDE_FROM_ALL)
    elseif(AIOPT_FETCH_LLAMA)
        message(STATUS "aiopt: fetching llama.cpp ${AIOPT_LLAMA_TAG}")
        set(AIOPT_LLAMA_VENDORED TRUE)
        include(FetchContent)
        set(LLAMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(LLAMA_BUILD_SERVER OFF CACHE BOOL "" FORCE)
        set(LLAMA_CURL OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(llama_cpp
            GIT_REPOSITORY https://github.com/ggml-org/llama.cpp.git
            GIT_TAG ${AIOPT_LLAMA_TAG}
            GIT_SHALLOW TRUE)
        FetchContent_MakeAvailable(llama_cpp)
    else()
        find_package(llama CONFIG REQUIRED)
    endif()
endif()

# ---------------------------------------------------------------------------
# The library itself
# ---------------------------------------------------------------------------
add_library(aiopt INTERFACE)
add_library(aiopt::aiopt ALIAS aiopt)

target_compile_features(aiopt INTERFACE cxx_std_20)
target_link_libraries(aiopt INTERFACE llama)
target_include_directories(aiopt INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

# Warnings are held to our own translation units. Imposing them on consumers,
# or on llama.cpp, would be rude and would not compile.
add_library(aiopt_strict INTERFACE)
if(MSVC)
    target_compile_options(aiopt_strict INTERFACE /W4 /permissive- /EHsc)
else()
    target_compile_options(aiopt_strict INTERFACE
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
        -Wold-style-cast -Wcast-qual -Wdouble-promotion -Wnull-dereference
        -Wnon-virtual-dtor -Woverloaded-virtual -Wunused)
endif()

# ---------------------------------------------------------------------------
# Installation
#
# An exported target may only depend on targets that are themselves exported.
# When llama.cpp is built inside our tree there is nothing to point an installed
# package at, so installation is offered only against an external llama.
# ---------------------------------------------------------------------------
cmake_dependent_option(AIOPT_INSTALL "Generate install rules" ON
    "NOT AIOPT_LLAMA_VENDORED" OFF)

if(AIOPT_INSTALL)
    include(CMakePackageConfigHelpers)

    install(TARGETS aiopt EXPORT aioptTargets)
    install(DIRECTORY include/aiopt DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(EXPORT aioptTargets
        FILE aioptTargets.cmake
        NAMESPACE aiopt::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/aiopt)

    configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/aioptConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/aioptConfig.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/aiopt)
    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/aioptConfigVersion.cmake
        COMPATIBILITY SameMajorVersion
        ARCH_INDEPENDENT)
    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/aioptConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/aioptConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/aiopt)
elseif(AIOPT_LLAMA_VENDORED)
    message(STATUS "aiopt: install rules disabled because llama.cpp is vendored")
endif()

if(AIOPT_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(AIOPT_BUILD_BENCHMARKS)
    add_subdirectory(bench)
endif()
