cmake_minimum_required(VERSION 3.23 FATAL_ERROR)

add_library(opencl-stub OBJECT)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src OPENCL_STUB_SRCS)
set(OCL_STUB_INC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
file(GLOB_RECURSE OCL_STUB_HEADERS ${OCL_STUB_INC_DIR}/*)

target_sources(opencl-stub
  PRIVATE ${OPENCL_STUB_SRCS}

  PUBLIC FILE_SET HEADERS
  BASE_DIRS ${OCL_STUB_INC_DIR}
  FILES ${OCL_STUB_HEADERS}
)

target_compile_definitions(
  opencl-stub PUBLIC CL_TARGET_OPENCL_VERSION=300
)

target_include_directories(
  opencl-stub
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include/CL
)

target_compile_options(opencl-stub
  PRIVATE
  -std=c++17 -Wall -Werror -Wno-unused-parameter
)