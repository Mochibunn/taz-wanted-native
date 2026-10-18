#version 450

// Fragment shader for basic color output
// Can be extended to use texture from texture_loader

layout(location = 0) in vec2 frag_uv;

out vec4 frag_color;

void main() {
    // Simple gradient based on UV coordinates
    float r = 0.1 + 0.9 * frag_uv.x;
    float g = 0.2 + 0.8 * frag_uv.y;
    float b = 0.4 + 0.6 * (frag_uv.x + frag_uv.y) * 0.5;
    
    frag_color = vec4(r, g, b, 1.0);
}
