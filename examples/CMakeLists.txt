add_executable(aiopt_show_prompt show_prompt.cpp)
target_link_libraries(aiopt_show_prompt PRIVATE aiopt::aiopt aiopt_strict)
set_target_properties(aiopt_show_prompt PROPERTIES OUTPUT_NAME show_prompt)

# stb lives in its own target so the project's warning set is not applied to it.
add_library(aiopt_stb STATIC third_party/stb_impl.cpp)
target_include_directories(aiopt_stb PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
if(NOT MSVC)
    target_compile_options(aiopt_stb PRIVATE -w)
endif()

add_executable(aiopt_imgproc imgproc.cpp)
target_link_libraries(aiopt_imgproc PRIVATE aiopt::aiopt aiopt_strict aiopt_stb)
set_target_properties(aiopt_imgproc PROPERTIES OUTPUT_NAME imgproc)

find_package(Threads REQUIRED)
target_link_libraries(aiopt_imgproc PRIVATE Threads::Threads)
