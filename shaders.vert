#version 450

// Vertex input: position (x,y,z) and UV coordinates
layout(location=0) in float3 inPosition;
layout(location=1) in float2 inUV;

// Vertex output: transformed position and UV for fragment shader
layout(location=0) out vec3 fragPosition;
layout(location=1) out vec2 fragUV;

// ============================================================================
// UNIFORM BUFFER BINDING
// ============================================================================
// The uniform buffer is declared with layout(binding = 0) to match the
// VkDescriptorSetLayout that expects a uniform buffer at binding 0.
//
// The buffer contains:
//   - mat4 modelView: Combined model * view matrix (64 bytes)
//   - mat4 projection: Perspective projection matrix (64 bytes)
//   - float time: Animation time (4 bytes)
//   - float[3]: Padding for alignment (12 bytes)
//   - Total: 144 bytes (aligned to 16 bytes)
// ============================================================================

layout(binding = 0) uniform UniformBufferObject {
    mat4 modelView;
    mat4 projection;
    float time;
    float[3] padding;
} ubo;

// ============================================================================
// MAIN SHADER FUNCTION
// ============================================================================
// MVP Matrix Multiplication: gl_Position = projection * modelView * position
//
// The vertex position is first transformed by the modelView matrix (world + camera),
// then by the projection matrix to clip space (-1 to +1 x,y,z, 0 to 1 w).
// ============================================================================

void main() {
    // Transform vertex position through MVP matrices
    // gl_Position = ubo.projection * ubo.modelView * vec4(inPosition, 1.0)
    gl_Position = ubo.projection * ubo.modelView * vec4(inPosition, 1.0);
    
    // Pass UV coordinates and computed position to fragment shader
    fragUV = inUV;
    fragPosition = vec3(gl_Position);
}
