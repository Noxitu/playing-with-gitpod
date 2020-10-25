#version 450
#extension GL_ARB_separate_shader_objects : enable


layout (local_size_x = 32, local_size_y = 32, local_size_z = 1 ) in;


layout(std140, binding = 0) buffer Buffer
{
   vec4 pixels[];
};


void main() 
{
    if(gl_GlobalInvocationID.x >= 128 || gl_GlobalInvocationID.y >= 128)
        return;

    pixels[128 * gl_GlobalInvocationID.y + gl_GlobalInvocationID.x] = vec4(
        gl_GlobalInvocationID.x,
        gl_GlobalInvocationID.y,
        gl_GlobalInvocationID.z,
        pixels[128 * gl_GlobalInvocationID.y + gl_GlobalInvocationID.x]);
}