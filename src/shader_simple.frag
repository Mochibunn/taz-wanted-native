// Simple fragment shader - renders background color and text
#version 330 core

out vec4 FragColor;

uniform vec4 uColor;        // Background color
uniform float uTime;        // Time for animations
uniform int uShowText;      // 1 = show text, 0 = solid color

in vec2 TexCoord;

void main() {
    // Default background color (blue)
    FragColor = vec4(0.1, 0.3, 0.6, 1.0);
    
    // Can add text here using a texture atlas
    // For now, just show a colored triangle
    if (TexCoord.y > 0.5) {
        FragColor = vec4(1.0, 0.8, 0.2, 1.0); // Orange top half
    }
}
