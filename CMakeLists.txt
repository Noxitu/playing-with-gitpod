cmake_minimum_required(VERSION 3.0.0)
project(ExampleProject VERSION 0.1.0)

find_package(Vulkan REQUIRED)

add_executable(app "main.cpp")
target_compile_features(app PRIVATE cxx_std_17)
target_link_libraries(app PRIVATE Vulkan::Vulkan)
