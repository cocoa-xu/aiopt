add_executable(aiopt_show_prompt show_prompt.cpp)
target_link_libraries(aiopt_show_prompt PRIVATE aiopt::aiopt aiopt_strict)
set_target_properties(aiopt_show_prompt PROPERTIES OUTPUT_NAME show_prompt)

# ---------------------------------------------------------------------------
# The model the examples are built against
#
# Which model to use is a build-time decision, not something the user of a
# finished program should have to know, so the path is compiled in. Point
# AIOPT_EXAMPLE_MODEL at another entry in the table below, or set
# AIOPT_EXAMPLE_MODEL_PATH at a file you already have to skip fetching.
# ---------------------------------------------------------------------------
set(AIOPT_EXAMPLE_MODEL "qwen3-4b" CACHE STRING "Model compiled into the examples")
set_property(CACHE AIOPT_EXAMPLE_MODEL PROPERTY STRINGS qwen3-4b smollm2-360m gemma3-270m)

set(AIOPT_EXAMPLE_MODEL_PATH "" CACHE FILEPATH
    "Use this model file instead of fetching one; overrides AIOPT_EXAMPLE_MODEL")
set(AIOPT_MODEL_CACHE_DIR "$ENV{HOME}/.cache/aiopt/models" CACHE PATH
    "Where fetched models are kept, shared across build directories")

if(AIOPT_EXAMPLE_MODEL STREQUAL "qwen3-4b")
    set(_model_file "Qwen3-4B-Instruct-2507-Q4_K_M.gguf")
    set(_model_repo "unsloth/Qwen3-4B-Instruct-2507-GGUF")
elseif(AIOPT_EXAMPLE_MODEL STREQUAL "smollm2-360m")
    set(_model_file "smollm2-360m-instruct-q8_0.gguf")
    set(_model_repo "HuggingFaceTB/SmolLM2-360M-Instruct-GGUF")
elseif(AIOPT_EXAMPLE_MODEL STREQUAL "gemma3-270m")
    set(_model_file "gemma-3-270m-it-Q4_0.gguf")
    set(_model_repo "unsloth/gemma-3-270m-it-GGUF")
else()
    message(FATAL_ERROR
        "aiopt: unknown AIOPT_EXAMPLE_MODEL '${AIOPT_EXAMPLE_MODEL}'. "
        "Choose one of qwen3-4b, smollm2-360m, gemma3-270m, "
        "or set AIOPT_EXAMPLE_MODEL_PATH to a file of your own.")
endif()

if(AIOPT_EXAMPLE_MODEL_PATH)
    set(_model_final "${AIOPT_EXAMPLE_MODEL_PATH}")
    add_custom_target(aiopt_example_model)
    message(STATUS "aiopt: examples use ${_model_final}")
else()
    set(_model_final "${AIOPT_MODEL_CACHE_DIR}/${_model_file}")
    add_custom_command(
        OUTPUT "${_model_final}"
        COMMAND ${CMAKE_COMMAND}
                -DURL=https://huggingface.co/${_model_repo}/resolve/main/${_model_file}
                -DDEST=${_model_final}
                -P ${PROJECT_SOURCE_DIR}/cmake/download_model.cmake
        COMMENT "aiopt: obtaining ${_model_file}"
        VERBATIM)
    add_custom_target(aiopt_example_model DEPENDS "${_model_final}")
    message(STATUS "aiopt: examples use ${AIOPT_EXAMPLE_MODEL} (${_model_final})")
endif()

# ---------------------------------------------------------------------------
# imgproc
# ---------------------------------------------------------------------------

# stb is fetched rather than committed, so its terms stay with its own project
# and nothing third-party is redistributed here. Pinned to a commit: stb has no
# releases, and SOURCE_SUBDIR names a directory that does not exist so that
# CMake populates the headers without looking for a build system.
include(FetchContent)
FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG 31c1ad37456438565541f4919958214b6e762fb4
    SOURCE_SUBDIR headers-only)
FetchContent_MakeAvailable(stb)

# Built as its own target so the project's warning set is not applied to it.
add_library(aiopt_stb STATIC stb_impl.cpp)
target_include_directories(aiopt_stb PUBLIC ${stb_SOURCE_DIR})
if(NOT MSVC)
    target_compile_options(aiopt_stb PRIVATE -w)
endif()

find_package(Threads REQUIRED)

# Staged beside the binary so the commands in the README can be run from the
# build directory without hunting for a picture first.
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/example.png
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/example.png
            ${CMAKE_CURRENT_BINARY_DIR}/example.png
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/example.png
    COMMENT "aiopt: staging example.png"
    VERBATIM)
add_custom_target(aiopt_example_image DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/example.png)

add_executable(aiopt_imgproc imgproc.cpp)
add_dependencies(aiopt_imgproc aiopt_example_model aiopt_example_image)
target_link_libraries(aiopt_imgproc PRIVATE aiopt::aiopt aiopt_strict aiopt_stb Threads::Threads)
target_compile_definitions(aiopt_imgproc PRIVATE AIOPT_MODEL_PATH="${_model_final}")
set_target_properties(aiopt_imgproc PROPERTIES OUTPUT_NAME imgproc)
