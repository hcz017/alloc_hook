add_subdirectory(gl_test)
file(GLOB_RECURSE DIR_SRCS 
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/util/*.cpp"
)
add_executable(alloc_hook_test ${DIR_SRCS})

target_link_libraries(alloc_hook_test gtest log opencl-stub gles3jni)
install(TARGETS alloc_hook_test DESTINATION ${CMAKE_INSTALL_PREFIX}/out/bin)