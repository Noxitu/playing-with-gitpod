#!/bin/bash

# For now as a shell script

(
    cd shaders &&
    ../3rdparties/glslang-build/StandAlone/glslangValidator -G *.glsl
)