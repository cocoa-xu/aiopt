add_executable(aiopt_show_prompt show_prompt.cpp)
target_link_libraries(aiopt_show_prompt PRIVATE aiopt::aiopt aiopt_strict)
set_target_properties(aiopt_show_prompt PROPERTIES OUTPUT_NAME show_prompt)

add_executable(aiopt_imgproc imgproc.cpp)
target_link_libraries(aiopt_imgproc PRIVATE aiopt::aiopt aiopt_strict)
set_target_properties(aiopt_imgproc PROPERTIES OUTPUT_NAME imgproc)
