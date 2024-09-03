cmake_minimum_required(VERSION 3.9 FATAL_ERROR)

project(sigscan CXX C)

add_library(${PROJECT_NAME} SHARED
    sigscan.cpp)
    
set(HEADLESS 1)

# Require BN_API_PATH to be set
if(NOT DEFINED BN_API_PATH)
    message(FATAL_ERROR "BN_API_PATH is not set. Please specify the path to the Binary Ninja API using -DBN_API_PATH=/path/to/binaryninja/api")
endif()

find_path(
    BN_API_INCLUDE_PATH
    NAMES binaryninjaapi.h
    PATHS ${BN_API_PATH}
    REQUIRED
    NO_DEFAULT_PATH
)

if(NOT BN_API_INCLUDE_PATH)
    message(FATAL_ERROR "Binary Ninja API not found at the specified path. Please check the value of BN_API_PATH.")
endif()

add_subdirectory(${BN_API_PATH} api)

target_link_libraries(${PROJECT_NAME}
    binaryninjaapi)

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_VISIBILITY_PRESET hidden
    CXX_STANDARD_REQUIRED ON
    VISIBILITY_INLINES_HIDDEN ON
    POSITION_INDEPENDENT_CODE ON
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out/bin)

bn_install_plugin(${PROJECT_NAME})