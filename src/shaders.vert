#version 450

// Vertex input layout: position (x,y,z) and UV texture coordinates (u,v)
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec2 a_texCoord;

// Output to fragment shader
layout(location = 0) out vec3 fragColor;

// Uniform buffer structure for MVP matrices
uniform LayoutLocation {
    mat4 ubo_projectionMatrix;
    mat4 ubo_viewMatrix;
    mat4 ubo_modelMatrix;
} ubo_transform;

void main() {
    // Apply Model-View-Projection matrix transformation
    // Transform: MVP = Projection * View * Model
    // Apply in correct order (right-to-left multiplication)
    vec4 transformedPosition = vec4(a_position, 1.0);
    
    // First apply model matrix (object transform)
    transformedPosition = ubo_transform.ubo_modelMatrix * transformedPosition;
    
    // Then apply view matrix (camera transform)
    transformedPosition = ubo_transform.ubo_viewMatrix * transformedPosition;
    
    // Finally apply projection matrix (camera frustum)
    transformedPosition = ubo_transform.ubo_projectionMatrix * transformedPosition;
    
    gl_Position = transformedPosition;
    
    // Create a simple colored material based on vertex position (no textures yet)
    float red = sin(a_position.x * 3.14159) * 0.5 + 0.5;
    float green = cos(a_position.y * 3.14159) * 0.5 + 0.5;
    float blue = sin(a_position.z * 3.14159) * 0.5 + 0.5;
    
    fragColor = vec3(red, green, blue);
}
