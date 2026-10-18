#version 450

// Vertex input layout: position (x,y,z) and UV texture coordinates (u,v)
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec2 a_texCoord;

// Output to fragment shader
layout(location = 0) out vec3 fragColor;

void main() {
    // Pass through position and UVs for potential texture sampling
    gl_Position = vec4(a_position, 1.0);
    
    // Create a simple colored material based on vertex position (no textures yet)
    float red = sin(a_position.x * 3.14159) * 0.5 + 0.5;
    float green = cos(a_position.y * 3.14159) * 0.5 + 0.5;
    float blue = sin(a_position.z * 3.14159) * 0.5 + 0.5;
    
    fragColor = vec3(red, green, blue);
}
