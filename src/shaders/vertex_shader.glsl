#version 450

layout(location = 0) out vec3 fragPosition;
layout(location = 1) out vec2 fragUV;

layout(binding = 0) uniform BufferObject {
    mat4 projection;
    mat4 view;
    mat4 model;
} block;

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inUV;

void main() {
    fragPosition = (block.model * block.view * block.projection) * inPos;
    fragUV = inUV;
    gl_Position = vec4(fragPosition, 1.0);
}
