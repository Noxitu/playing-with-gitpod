cmake_minimum_required(VERSION 3.0.0)

project(ExampleProject 
    VERSION 0.1.0
    LANGUAGES CXX
)

find_package(Vulkan REQUIRED)
message("Vulkan_GLSLC_EXECUTABLE = ${Vulkan_GLSLC_EXECUTABLE}")

add_executable(app 
    "src/main.cpp"
    "src/utils.h"
    "src/validation_layer.h"
)

target_include_directories(app PRIVATE "src/")
target_compile_features(app PRIVATE cxx_std_17)
target_link_libraries(app PRIVATE Vulkan::Vulkan)
