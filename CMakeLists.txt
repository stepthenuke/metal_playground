cmake_minimum_required(VERSION 3.30)
set(CMAKE_CXX_STANDARD 20)
project(metal_playground)

# Thanks to dpogue!
# https://github.com/dpogue/CMake-MetalShaderSupport/tree/main
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_SOURCE_DIR}/cmake")
include(MetalShaderSupport)
enable_language(Metal)

add_subdirectory(src)
