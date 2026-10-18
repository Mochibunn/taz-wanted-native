#version 450

// Vertex shader for basic triangle rendering
// Loads vertex position and UV coordinates from mesh buffer

layout(location = 0) out vec2 frag_uv;

layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec2 in_uv;

void main() {
    gl_Position = vec4(in_pos, 1.0);
    frag_uv = in_uv;
}
