#version 450

layout(location = 0) out vec4 outColor;

in vec2 fragUV;

void main() {
    // Simple colored output - can be enhanced with actual texture sampling
    outColor = vec4(0.1, 0.2, 0.4, 1.0);  // Teal/burrow-blue
}
