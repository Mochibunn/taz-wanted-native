#version 450

layout(location = 0) in vec3 fragPosition;
layout(location = 1) in vec2 fragUV;

layout(binding = 1) uniform sampler2D tex;
layout(location = 0) out vec4 color;

void main() {
    vec4 texel = texture(tex, fragUV);
    color = texel;
}
