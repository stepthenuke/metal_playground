add_subdirectory(metal)

find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3-shared)

add_executable(metal_playground main.cpp)
target_link_libraries(metal_playground PRIVATE METAL_CPP SDL3::SDL3)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/metal-cpp)

add_metal_shader_library(Shaders
   shaders/basic.metal
)


