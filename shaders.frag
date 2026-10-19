#version 450

layout(location=0) in vec3 fragPosition;
layout(location=1) in vec2 fragUV;

layout(location=0) out vec4 fragColor;

// ============================================================================
// TERRAIN COLORING BASED ON HEIGHT
// ============================================================================
// The terrain is colored based on the Y component of the transformed position.
// This creates a visual gradient that shows terrain elevation.
// ============================================================================

void main() {
    // Normalize height to 0.0 - 1.0 range for shader logic
    float normalizedHeight = fragPosition.y * 0.001 + 0.5;
    
    // Create interesting color gradient with sine waves
    // The sine functions add organic variation to the color
    float r = clamp(0.6 + sin(fragPosition.x * 0.01) * 0.2, 0.3, 0.9);
    float g = clamp(0.3 + cos(fragPosition.z * 0.01) * 0.2, 0.1, 0.6);
    float b = clamp(0.8 + sin(normalizedHeight * 3.14159) * 0.2, 0.4, 1.0);
    
    // Base color with alpha
    fragColor = vec4(r, g, b, 1.0);
    
    // Optional: darken edges for depth perception
    float depth = abs(fragPosition.z * 0.01);
    fragColor *= 1.0 - depth * 0.1;
}
