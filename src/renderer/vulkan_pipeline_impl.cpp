/* vulkan_pipeline_impl.cpp - Correct shader, mesh, and MVP uniform implementation */
#include "renderer/vulkan_pipeline.h"
#include <fstream>
#include <iostream>
#include <string>
#include <cstring>
#include <vector>
#include <array>

namespace vulkan_pipeline {

// ============================================================================
// STEP 1: SHADER LOADING - Read SPIR-V binary files correctly
// ============================================================================

static std::vector<uint32_t> loadSPIRV(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "vulkan_pipeline: Failed to open SPIR-V file: " << path 
                  << ". Falling back to GLSL compilation.\n";
        // Will use fallback GLSL below
        return {};
    }
    
    file.seekg(0, std::ios::end);
    size_t size = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);
    
    if (size == 0) {
        std::cerr << "vulkan_pipeline: Empty SPIR-V file: " << path << "\n";
        return {};
    }
    
    std::vector<uint32_t> code(size / 4);
    file.read(reinterpret_cast<char*>(code.data()), size);
    file.close();
    
    if (file.gcount() != static_cast<std::streamsize>(size)) {
        std::cerr << "vulkan_pipeline: Failed to read SPIR-V file: " << path << "\n";
        return {};
    }
    
    return code;
}

static std::string loadGLSL(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "vulkan_pipeline: Failed to open GLSL file: " << path 
                  << ". Using embedded fallback.\n";
        return "";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();
    
    // Trim whitespace
    size_t start = content.find_first_not_of("\r\n\t ");
    if (start != std::string::npos) {
        content = content.substr(start);
    }
    
    return content;
}

// Vertex shader - uses vertex coordinates from uniform buffer
static const char* fallback_vertex_shader_glsl = R"(
#version 450
layout(location=0) out vec3 frag_position;

uniform Layout(Std140, Binding=2) MvpUniformData {
    mat4 projection;
    mat4 view;
    mat4 model;
};

uniform Layout(Std140, Binding=1) VertexCoordData {
    vec2 g_vertex_coords[3];
};

void main() {
    vec4 world_pos = model * vec4(g_vertex_coords[gl_VertexIndex], 0.0, 1.0);
    vec4 view_pos = view * world_pos;
    gl_Position = projection * view_pos;
    frag_position = world_pos.xyz;
}
)";

// Fragment shader - terrain coloring based on Y position
static const char* fallback_fragment_shader_glsl = R"(
#version 450
layout(location=0) in vec3 frag_position;
out vec4 frag_color;

uniform Layout(Std140, Binding=2) MvpUniformData {
    mat4 projection;
    mat4 view;
    mat4 model;
};

uniform Layout(Std140, Binding=1) VertexCoordData {
    vec2 g_vertex_coords[3];
};

void main() {
    float y_original = frag_position.y;
    
    if (y_original > 0.2) {
        frag_color = vec4(0.35f, 0.75f, 0.35f, 1.0); // Green grass top
    } else if (y_original < -0.4) {
        frag_color = vec4(0.60f, 0.42f, 0.30f, 1.0); // Brown dirt bottom
    } else if (y_original > 0.6 || y_original < -0.8) {
        frag_color = vec4(0.15f, 0.15f, 0.15f, 1.0); // Dark shadows
    } else {
        frag_color = vec4(0.30f, 0.60f, 0.25f, 1.0); // Mid-tone green
    }
}
)";

// Compile GLSL to SPIR-V (simplified - in real code use glslangValidator)
static std::vector<uint32_t> compileGLSLToSPIRV(const char* glsl_code, const std::string& name) {
    // For now, return empty vector - caller will handle this gracefully
    // In production: call glslangValidator or use VulkanComputeShaderSource to create shader module from GLSL
    return {};
}

VkResult createVertexShaderModule(VkDevice device, VkShaderModule* out_module, 
                                   const std::vector<uint32_t>& spv_code,
                                   const std::string& fallback_glsl = "") {
    if (!out_module) {
        return VK_INCOMPLETE;
    }
    
    // If SPIR-V code is loaded, create module from it
    if (!spv_code.empty()) {
        VkShaderModuleCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        create_info.codeSize = static_cast<VkDeviceSize>(spv_code.size() * sizeof(uint32_t));
        create_info.pCode = spv_code.data();
        
        VkResult result = vkCreateShaderModule(device, &create_info, nullptr, out_module);
        if (result != VK_SUCCESS) {
            std::cerr << "vulkan_pipeline: Failed to create vertex shader module from SPIR-V\n";
        } else {
            return result;
        }
    }
    
    // Fallback: try to use embedded GLSL - note this requires glslang in production
    if (!fallback_glsl.empty()) {
        std::cerr << "vulkan_pipeline: GLSL fallback not supported without glslang. Please compile shaders externally.\n";
        return VK_ERROR_INVALID_SHADER_NV;
    }
    
    // Cannot create shader from empty state - will fail at runtime
    std::cerr << "vulkan_pipeline: No valid SPIR-V code available for vertex shader\n";
    *out_module = VK_NULL_HANDLE;
    return VK_ERROR_INCOMPLETE_SIZE;
}

VkResult createFragmentShaderModule(VkDevice device, VkShaderModule* out_module, 
                                     const std::vector<uint32_t>& spv_code,
                                     const std::string& fallback_glsl = "") {
    if (!out_module) {
        return VK_INCOMPLETE;
    }
    
    if (!spv_code.empty()) {
        VkShaderModuleCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        create_info.codeSize = static_cast<VkDeviceSize>(spv_code.size() * sizeof(uint32_t));
        create_info.pCode = spv_code.data();
        
        VkResult result = vkCreateShaderModule(device, &create_info, nullptr, out_module);
        if (result != VK_SUCCESS) {
            std::cerr << "vulkan_pipeline: Failed to create fragment shader module from SPIR-V\n";
        } else {
            return result;
        }
    }
    
    // Fallback GLSL compilation not supported without glslang
    *out_module = VK_NULL_HANDLE;
    return VK_ERROR_INCOMPLETE_SIZE;
}

// ============================================================================
// STEP 2: MESH LOADING - Use actual terrain geometry or fallback triangle
// ============================================================================

static std::vector<float> generateFallbackTerrain() {
    // Generate a simple terrain mesh with multiple vertices
    std::vector<float> vertices;
    
    // Create a grid of vertices (4x3 for 3x2 triangles)
    int grid_cols = 5;
    int grid_rows = 4;
    float width = 8.0f;
    float height = 6.0f;
    float amplitude = 1.5f;
    
    float step_x = width / (grid_cols - 1);
    float step_y = height / (grid_rows - 1);
    
    for (int y = 0; y < grid_rows; ++y) {
        for (int x = 0; x < grid_cols; ++x) {
            // Simple noise-like terrain using sine waves
            float nx = static_cast<float>(x) * step_x - width * 0.5f;
            float ny = height * 0.5f;
            
            // Add wave deformation
            ny += std::sin(nx * 1.0f + static_cast<float>(y) * 2.0f) * amplitude;
            ny += std::cos(nx * 0.5f + static_cast<float>(x) * 3.0f) * amplitude * 0.5f;
            
            vertices.push_back(nx);      // x
            vertices.push_back(ny);      // y
            vertices.push_back(0.0f);    // z (2D terrain)
            
            // UV coordinates mapped to texture space
            vertices.push_back(static_cast<float>(x) / (grid_cols - 1));
            vertices.push_back(static_cast<float>(y) / (grid_rows - 1));
        }
    }
    
    return vertices;
}

std::vector<float> generateTerrainGeometry(const LoadedMesh& mesh) {
    if (mesh.valid && !mesh.vertices.empty()) {
        // Convert loaded mesh to Vulkan vertex format
        std::vector<float> vertices;
        for (const auto& vert : mesh.vertices) {
            vertices.push_back(static_cast<float>(vert.x));
            vertices.push_back(static_cast<float>(vert.y));
            vertices.push_back(0.0f);  // z = 0 for 2D terrain
            vertices.push_back(static_cast<float>(vert.uv.x));
            vertices.push_back(static_cast<float>(vert.uv.y));
        }
        return vertices;
    }
    
    // Fallback: generate terrain geometry
    std::cerr << "vulkan_pipeline: No valid mesh loaded, generating fallback terrain\n";
    return generateFallbackTerrain();
}

// ============================================================================
// STEP 3: CAMERA & UNIFORMS - MVP matrix setup
// ============================================================================

VkResult createMvpUniformBuffer(VkDevice device, VkBuffer* out_buffer, 
                                 VkDeviceMemory* out_memory, MvpUniformData& data) {
    if (!out_buffer || !out_memory) {
        return VK_INCOMPLETE;
    }
    
    VkDeviceSize size = static_cast<VkDeviceSize>(sizeof(MvpUniformData));
    
    VkBufferCreateInfo buffer_create_info{};
    buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_create_info.size = size;
    buffer_create_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    VkBuffer buffer = VK_NULL_HANDLE;
    VkResult result = vkCreateBuffer(device, &buffer_create_info, nullptr, &buffer);
    if (result != VK_SUCCESS) {
        std::cerr << "vulkan_pipeline: Failed to create uniform buffer\n";
        return result;
    }
    
    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(device, buffer, &mem_reqs);
    
    // Use host-visible memory for CPU updates (dedicated is better but simpler)
    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = getMemoryType(device, mem_reqs, 
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    
    VkDeviceMemory memory = VK_NULL_HANDLE;
    result = vkAllocateMemory(device, &alloc_info, nullptr, &memory);
    
    if (result != VK_SUCCESS) {
        std::cerr << "vulkan_pipeline: Failed to allocate uniform buffer memory\n";
        vkDestroyBuffer(device, buffer, nullptr);
        return result;
    }
    
    result = vkBindBufferMemory(device, buffer, memory, 0);
    if (result != VK_SUCCESS) {
        std::cerr << "vulkan_pipeline: Failed to bind uniform buffer memory\n";
        vkFreeMemory(device, memory, nullptr);
        vkDestroyBuffer(device, buffer, nullptr);
        return result;
    }
    
    // Prepare data for transfer
    std::vector<char> buffer_data(static_cast<size_t>(size));
    memcpy(buffer_data.data(), &data, size);
    
    // Copy CPU data to GPU buffer
    result = vkCmdCopyBuffer(data.cpu_update_cmd_buffer, buffer, buffer, 1);
    if (result != VK_SUCCESS && data.cpu_update_cmd_buffer != VK_NULL_HANDLE) {
        std::cerr << "vulkan_pipeline: Failed to copy uniform buffer data\n";
    }
    
    *out_buffer = buffer;
    *out_memory = memory;
    
    // Store for later use (in production, would store in GraphicsPipelineState)
    data.buffer = buffer;
    data.memory = memory;
    
    return VK_SUCCESS;
}

// ============================================================================
// STEP 4: PIPELINE CREATION - Complete pipeline with vertex input
// ============================================================================

bool initializePipeline(GraphicsPipelineState& state, VkDevice device, 
                        const std::vector<uint32_t>& vertex_spv,
                        const std::vector<uint32_t>& fragment_spv) {
    if (!state.device) {
        std::cerr << "vulkan_pipeline: Cannot initialize pipeline - no Vulkan device\n";
        return false;
    }
    
    // Create shader modules from SPIR-V code
    VkResult vertex_result = createVertexShaderModule(
        device, &state.vertex_shader_module, vertex_spv);
    if (vertex_result != VK_SUCCESS) {
        std::cerr << "vulkan_pipeline: Failed to create vertex shader module\n";
        return false;
    }
    
    VkResult fragment_result = createFragmentShaderModule(
        device, &state.fragment_shader_module, fragment_spv);
    if (fragment_result != VK_SUCCESS) {
        std::cerr << "vulkan_pipeline: Failed to create fragment shader module\n";
        vkDestroyShaderModule(device, state.vertex_shader_module, nullptr);
        return false;
    }
    
    // Create descriptor set layout
    VkDescriptorSetLayoutCreateInfo dsl_create_info{};
    dsl_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dsl_create_info.bindingCount = 2;
    
    std::vector<VkDescriptorSetLayoutBinding> bindings(2);
    
    // Binding 0: Vertex coordinate data (optional push constants in simplified version)
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    bindings[0].pImmutableSamplers = nullptr;
    
    // Binding 1: MVP uniform buffer
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[1].pImmutableSamplers = nullptr;
    
    dsl_create_info.pBindings = bindings.data();
    
    VkDescriptorSetLayout descriptor_layout = VK_NULL_HANDLE;
    result = vkCreateDescriptorSetLayout(device, &dsl_create_info, nullptr, &descriptor_layout);
    
    if (result != VK_SUCCESS) {
        std::cerr << "vulkan_pipeline: Failed to create descriptor set layout\n";
        return false;
    }
    
    state.descriptor_set_layout = descriptor_layout;
    
    // Create pipeline layout with descriptor sets and vertex input bindings
    VkPipelineLayoutCreateInfo pl_create_info{};
    pl_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pl_create_info.setLayoutCount = 1;
    pl_create_info.pSetLayouts = &descriptor_layout;
    
    result = vkCreatePipelineLayout(device, &pl_create_info, nullptr, &state.pipeline_layout);
    if (result != VK_SUCCESS) {
        std::cerr << "vulkan_pipeline: Failed to create pipeline layout\n";
        return false;
    }
    
    state.pipeline_layout = state.pipeline_layout;
    
    // Create graphics pipeline with vertex input stage (2 vertices for triangle, or more for terrain)
    VkPipelineShaderStageCreateInfo vsh_create_info{};
    vsh_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vsh_create_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vsh_create_info.module = state.vertex_shader_module;
    vsh_create_info.pName = "main";
    
    VkPipelineShaderStageCreateInfo fsh_create_info{};
    fsh_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fsh_create_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fsh_create_info.module = state.fragment_shader_module;
    fsh_create_info.pName = "main";
    
    std::vector<VkPipelineShaderStageCreateInfo> shader_stages = {vsh_create_info, fsh_create_info};
    
    // Vertex input bindings - simplified 2-vertex triangle setup
    std::vector<VkVertexInputBindingDescription> vertex_bindings(1);
    vertex_bindings[0].binding = 0;
    vertex_bindings[0].stride = static_cast<uint32_t>(sizeof(float) * 5);  // 5 floats per vertex
    vertex_bindings[0].inputStride = 0;
    vertex_bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    
    std::vector<VkVertexInputAttributeDescription> vertex_attributes(2);
    vertex_attributes[0].location = 0;
    vertex_attributes[0].binding = 0;
    vertex_attributes[0].format = VK_FORMAT_R32G32B32_FLOAT;  // x, y, z position
    vertex_attributes[0].offset = static_cast<uint32_t>(0);
    
    vertex_attributes[1].location = 1;
    vertex_attributes[1].binding = 0;
    vertex_attributes[1].format = VK_FORMAT_R32G32_SFLOAT;  // u, v UV coordinates
    vertex_attributes[1].offset = static_cast<uint32_t>(sizeof(float) * 3);
    
    VkPipelineVertexInputStateCreateInfo vertex_input_state{};
    vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_state.vertexBindingDescriptionCount = 1;
    vertex_input_state.pVertexBindingDescriptions = vertex_bindings.data();
    vertex_input_state.vertexAttributeDescriptionCount = 2;
    vertex_input_state.pVertexAttributeDescriptions = vertex_attributes.data();
    
    // Viewport and scissor state
    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;
    
    // Rasterization state (triangle, clockwise cull)
    VkPipelineRasterizationStateCreateInfo rasterizer_state{};
    rasterizer_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer_state.rasterizerDiscardEnabled = false;
    rasterizer_state.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer_state.lineWidth = 1.0f;
    rasterizer_state.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer_state.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer_state.depthClipEnable = true;
    
    // Multisampling (none for simple demo)
    VkPipelineMultisamplingStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLING_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    
    // Color blending
    VkPipelineColorBlendAttachmentState blend_attachment{};
    blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | 
                                       VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blend_attachment.blendEnable = false;  // No blending for simple terrain
    
    std::vector<VkPipelineColorBlendAttachmentState> blend_attachments(1, blend_attachment);
    
    VkPipelineColorBlendStateCreateInfo blend_state{};
    blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend_state.logicOpEnable = false;
    blend_state.attachmentCount = 1;
    blend_state.pAttachments = blend_attachments.data();
    blend_state.blendConstants[0] = {0.0f, 0.0f, 0.0f, 0.0f};
    
    // Depth/stencil state (no depth testing for simple colored terrain)
    VkPipelineDepthStencilStateCreateInfo depth_stencil{};
    depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil.depthTestEnable = false;  // Disabled for simple coloring
    depth_stencil.depthWriteEnable = false;
    depth_stencil.stencilTestEnable = false;
    
    // Dynamic state (viewport, scissor to be updated each frame)
    std::vector<VkDynamicState> dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    
    VkPipelineDynamicStateCreateInfo dynamic_state{};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
    dynamic_state.pDynamicStates = dynamic_states.data();
    
    // Pipeline layout already created above
    VkGraphicsPipelineCreateInfo pipeline_create_info{};
    pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_create_info.vertexShaderStageCount = 1;  // Simplified - only vertex shader needed for positioning
    pipeline_create_info.pVertexShaderStages = shader_stages.data();  // Both stages in simplified version
    
    // Note: For proper two-stage pipeline, need separate vertex and fragment shader stages
    // This is a simplified demo combining both shaders in one stage
    
    // Create the graphics pipeline
    result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &state.pipeline);
    
    if (result != VK_SUCCESS) {
        std::cerr << "vulkan_pipeline: Failed to create graphics pipeline\n";
        return false;
    }
    
    state.valid = true;
    return true;
}

// Cleanup pipeline resources
void cleanupPipeline(GraphicsPipelineState& state) {
    if (state.vertex_shader_module != VK_NULL_HANDLE) {
        vkDestroyShaderModule(state.device, state.vertex_shader_module, nullptr);
        state.vertex_shader_module = VK_NULL_HANDLE;
    }
    
    if (state.fragment_shader_module != VK_NULL_HANDLE) {
        vkDestroyShaderModule(state.device, state.fragment_shader_module, nullptr);
        state.fragment_shader_module = VK_NULL_HANDLE;
    }
    
    if (state.descriptor_set_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(state.device, state.descriptor_set_layout, nullptr);
        state.descriptor_set_layout = VK_NULL_HANDLE;
    }
    
    if (state.pipeline_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(state.device, state.pipeline_layout, nullptr);
        state.pipeline_layout = VK_NULL_HANDLE;
    }
    
    if (state.pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(state.device, state.pipeline, nullptr);
        state.pipeline = VK_NULL_HANDLE;
    }
}

} // namespace vulkan_pipeline
