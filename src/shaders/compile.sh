#!/bin/bash

# For now as a shell script

(
    echo "Compiling shaders into .spv files..."
    cd src/shaders &&
    ../../3rdparties/glslang-build/StandAlone/glslangValidator -G *.glsl
)

echo "Converting shaders into .spv.h files..."
for filename in src/shaders/*.spv
do
    xxd -include "$filename" "$filename.h"
done