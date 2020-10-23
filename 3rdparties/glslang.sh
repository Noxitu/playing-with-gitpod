#!/bin/bash

# Get Sources
git clone https://github.com/KhronosGroup/glslang --depth=1

# Configure
mkdir glslang-build
(cd glslang-build/ && cmake -DCMAKE_BUILD_TYPE=Release ../glslang)

# Build
cmake --build glslang-build/ -- -j 8

# Done
echo "Built: 3rdparties/glslang-build/StandAlone/glslangValidator"