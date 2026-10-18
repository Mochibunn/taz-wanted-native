#version 450

// Fragment shader that samples the simple position-based color
layout(location = 0) in vec3 fragColor;
layout(location = 0) out vec4 outColor;

void main() {
    // Output the computed color directly (texture sampling can be added later)
    outColor = vec4(fragColor, 1.0);
}
