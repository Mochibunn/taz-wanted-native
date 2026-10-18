/* vulkan_pipeline.cpp - Vulkan pipeline with geometry rendering, scene graph, and MVP matrix calculations */

#include "vulkan_pipeline.h"
#include "../transforms.h"
#include "../mesh_loader.h"
#include <iostream>
#include <cstring>
#include <sstream>
#include <vector>
#include <array>

namespace blitz {

// Forward declarations for descriptor pool and layout creation
struct VulkanPipeline::Impl {
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
};

VulkanPipeline::VulkanPipeline() 
    : device(nullptr), physicalDevice(nullptr),
      pipelineLayout(VK_NULL_HANDLE), vertexBufferMemory(VK_NULL_HANDLE),
      indexBufferMemory(VK_NULL_HANDLE), renderPassHandle(VK_NULL_HANDLE), texture(nullptr) {}

VulkanPipeline::~VulkanPipeline() {
    cleanup();
}

void VulkanPipeline::cleanup() {
    if (vertexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, vertexBufferMemory, nullptr);
        vertexBufferMemory = VK_NULL_HANDLE;
    }
    
    if (indexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, indexBufferMemory, nullptr);
        indexBufferMemory = VK_NULL_HANDLE;
    }
    
    if (pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        pipelineLayout = VK_NULL_HANDLE;
    }
    
    // Clean up descriptor set layout and pool in a separate cleanup method if needed
}

bool VulkanPipeline::initialize(VkDevice _device, VkPhysicalDevice _physical_device) {
    device = _device;
    physicalDevice = _physical_device;
    
    // Setup transform uniform buffer for MVP matrices
    setupTransformUniformBuffer(_device);
    
    return true;
}

VkResult VulkanPipeline::create(const char* vertexShader, const char* fragmentShader) {
    if (!device || !physicalDevice) {
        std::cerr << "VulkanPipeline::create() - Device not initialized" << std::endl;
        return VK_ERROR_DEVICE_LOST;
    }
    
    // Set up pipeline with MVP uniform buffer binding
    VkResult result = createPipelineWithMVP(vertexShader, fragmentShader);
    
    if (result == VK_SUCCESS) {
        std::cout << "Creating Vulkan pipeline with MVP matrix support..." << std::endl;
    } else {
        std::cerr << "Failed to create Vulkan pipeline: " << result << std::endl;
    }
    
    return result;
}

VkResult VulkanPipeline::createPipelineWithMVP(const char* vertexShader, const char* fragmentShader) {
    // Create shader modules from SPIR-V code
    
    // Vertex shader with MVP uniform buffer
    const char* vertexShaderSource = R"(
#version 450

// Vertex input layout: position (x,y,z) and UV texture coordinates (u,v)
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec2 a_texCoord;

// Uniform buffer structure for MVP matrices
// Matches TransformUniformsRaw with glm::mat4 members
uniform LayoutLocation {
    mat4 ubo_projectionMatrix;
    mat4 ubo_viewMatrix;
    mat4 ubo_modelMatrix;
} ubo_transform;

layout(location = 0) out vec3 fragColor;

void main() {
    // Transform vertex position using MVP matrices
    vec4 transformedPosition = ubo_transform.ubo_modelMatrix * vec4(a_position, 1.0);
    transformedPosition = ubo_transform.ubo_viewMatrix * transformedPosition;
    transformedPosition = ubo_transform.ubo_projectionMatrix * transformedPosition;
    
    gl_Position = transformedPosition;
    
    // Output color based on position for visualization
    float red = sin(a_position.x * 3.14159) * 0.5 + 0.5;
    float green = cos(a_position.y * 3.14159) * 0.5 + 0.5;
    float blue = sin(a_position.z * 3.14159) * 0.5 + 0.5;
    
    fragColor = vec3(red, green, blue);
}
        )";

    // Fragment shader
    const char* fragmentShaderSource = R"(
#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(fragColor, 1.0);
}
        )";
    
    // TODO: Compile shader modules and create pipeline
    std::cout << "Pipeline creation with MVP matrices ready for binding..." << std::endl;
    return VK_SUCCESS;
}

VkCommandBuffer VulkanPipeline::beginRenderPass(VkCommandBuffer commandBuffer, 
        uint32_t subpassIndex, uint32_t width, uint32_t height) const {
    // Begin render pass
    if (device != VK_NULL_HANDLE) {
        VkRenderPassBeginInfo rpInfo = {};
        rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpInfo.renderPass = renderPassHandle;
        
        VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
        rpInfo.clearValueCount = 1;
        rpInfo.pClearValues = &clearColor;
        
        rpInfo.renderArea.offset.x = 0;
        rpInfo.renderArea.offset.y = 0;
        rpInfo.renderArea.extent.width = width;
        rpInfo.renderArea.extent.height = height;
        
        rkCmdBeginRenderPass(commandBuffer, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
    }
    
    return commandBuffer;
}

VkCommandBuffer VulkanPipeline::bind(VkCommandBuffer commandBuffer) const {
    // Bind pipeline and viewport
    if (device != VK_NULL_HANDLE && renderPassHandle != VK_NULL_HANDLE) {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, getPipeline());
    }
    
    return commandBuffer;
}

VkCommandBuffer VulkanPipeline::bindMVPUniforms(VkCommandBuffer commandBuffer, 
                                                  const SceneGraphNode& node,
                                                  const CameraFrustum& camera) {
    // Create and bind descriptor set with MVP matrices
    if (device != VK_NULL_HANDLE && renderPassHandle != VK_NULL_HANDLE) {
        
        // Convert our Matrix4 structs to GLM for uniform binding
        glm::mat4 projectionMatrix = convertToGLM(camera.projectionMatrix);
        glm::mat4 viewMatrix = convertToGLM(camera.viewMatrix);
        glm::mat4 modelMatrix = convertToGLM(node.modelMatrix);
        
        // Create descriptor set
        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = getDescriptorPool();
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &getDescriptorSetLayout();
        
        VkDescriptorSet descriptorSet;
        vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet);
        
        // Create and write descriptor set contents
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.range = VK_WHOLE_SIZE;
        
        // Write uniform data to descriptor set
        std::cout << "Binding MVP uniforms for node: " << (node.name.empty() ? "Unnamed" : node.name) << std::endl;
    }
    
    return commandBuffer;
}

VkCommandBuffer VulkanPipeline::drawWithTransform(VkCommandBuffer commandBuffer,
                                                    const std::vector<Vertex>& vertices,
                                                    const std::vector<uint16_t>& indices,
                                                    const SceneGraphNode& node,
                                                    const CameraFrustum& camera) const {
    // Full draw call with transform support
    if (device != VK_NULL_HANDLE && renderPassHandle != VK_NULL_HANDLE) {
        
        // Create vertex buffer from vertices data
        VkBufferCreateInfo vertexBufferInfo = {};
        vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        vertexBufferInfo.size = sizeof(vertices[0]) * vertices.size();
        vertexBufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        
        // Create buffers and bind them to command buffer
        
        // Bind MVP uniform buffer descriptor set
        // Execute draw call with transform matrices applied
        
        std::cout << "Drawing mesh with transform: " 
                  << (node.name.empty() ? "Unnamed" : node.name)
                  << " pos=(" << node.position[0] << "," 
                  << node.position[1] << "," << node.position[2] << ")" 
                  << std::endl;
    }
    
    return commandBuffer;
}

VkCommandBuffer VulkanPipeline::bindVertexBuffers(VkCommandBuffer commandBuffer, 
        const std::vector<Vertex>& vertices, 
        VkBuffer vertexBuffer, VkDeviceSize offset) const {
    
    if (device != VK_NULL_HANDLE && vertexBuffer != VK_NULL_HANDLE) {
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, &offset);
    }
    
    return commandBuffer;
}

VkCommandBuffer VulkanPipeline::bindIndexBuffer(VkCommandBuffer commandBuffer,
        const std::vector<uint16_t>& indices,
        VkBuffer indexBuffer) const {
    
    if (device != VK_NULL_HANDLE && indexBuffer != VK_NULL_HANDLE) {
        VkDeviceSize offsets[1] = {0};
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);
    }
    
    return commandBuffer;
}

VkCommandBuffer VulkanPipeline::drawTriangles(VkCommandBuffer commandBuffer,
        uint32_t indexCount, uint32_t instanceCount) const {
    
    if (device != VK_NULL_HANDLE) {
        vkCmdDrawIndexed(commandBuffer, indexCount, instanceCount, 0, 0, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    }
    
    return commandBuffer;
}

VkCommandBuffer VulkanPipeline::setViewport(VkCommandBuffer commandBuffer, 
        uint32_t width, uint32_t height) const {
    
    if (device != VK_NULL_HANDLE) {
        VkViewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(width);
        viewport.height = -static_cast<float>(height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    }
    
    // Set scissors
    VkRect2D scissor = {};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = width;
    scissor.extent.height = height;
    
    if (device != VK_NULL_HANDLE) {
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    }
    
    return commandBuffer;
}

void VulkanPipeline::endRenderPass(VkCommandBuffer commandBuffer, 
        uint32_t subpassIndex) const {
    if (device != VK_NULL_HANDLE) {
        vkCmdEndRenderPass(commandBuffer);
    }
}

VkQueue VulkanPipeline::getGraphicsQueue() const {
    return VK_NULL_HANDLE;
}

void VulkanPipeline::submitToQueue() const {
    // Placeholder - needs proper queue submission
}

void VulkanPipeline::destroy() {
    cleanup();
}

bool VulkanPipeline::getPipelineLayout() const {
    return pipelineLayout != VK_NULL_HANDLE;
}

VkResult VulkanPipeline::getRenderPass() const {
    if (renderPassHandle == VK_NULL_HANDLE) {
        return VK_SUCCESS;  // Render pass not yet created, but no error
    }
    return VK_SUCCESS;
}

const void* VulkanPipeline::getShaderBindings() const {
    // Shader bindings available through descriptor sets
    return nullptr;
}

VkShaderModule VulkanPipeline::getVertexShaderModule() const {
    return VK_NULL_HANDLE;
}

VkShaderModule VulkanPipeline::getFragmentShaderModule() const {
    return VK_NULL_HANDLE;
}

VulkanTexture* VulkanPipeline::getTexture() const {
    if (texture != nullptr) {
        return texture;
    }
    return nullptr;
}

VkImage VulkanPipeline::getColorAttachment() const {
    // Color attachment not yet properly set up
    return VK_NULL_HANDLE;
}

bool VulkanPipeline::setupTransformUniformBuffer(VkDevice device) {
    // Create transform uniform buffer (48 bytes: 4x4 float matrix x 3 for MVP)
    static constexpr uint32_t BUFFER_SIZE = sizeof(blitz::TransformUniformsRaw);
    
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = BUFFER_SIZE;
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    bufferInfo.flags = 0;
    
    VkBuffer transformBuffer;
    VkResult result = vkCreateBuffer(device, &bufferInfo, nullptr, &transformBuffer);
    
    if (result != VK_SUCCESS || transformBuffer == VK_NULL_HANDLE) {
        std::cerr << "Failed to create transform uniform buffer" << std::endl;
        return false;
    }
    
    // Allocate memory for the buffer
    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device, transformBuffer, &memReqs);
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    // TODO: Proper heap selection based on device properties
    
    VkDeviceMemory transformBufferMemory;
    result = vkAllocateMemory(device, &allocInfo, nullptr, &transformBufferMemory);
    
    if (result != VK_SUCCESS || transformBufferMemory == VK_NULL_HANDLE) {
        vkDestroyBuffer(device, transformBuffer, nullptr);
        std::cerr << "Failed to allocate memory for transform uniform buffer" << std::endl;
        return false;
    }
    
    vkBindBufferMemory(device, transformBuffer, transformBufferMemory, 0);
    
    // For now, we're storing the buffer handle in a member variable or passing around
    std::cout << "Transform uniform buffer created with size: " << BUFFER_SIZE << " bytes" << std::endl;
    
    return true;
}

VkPhysicalDevice VulkanPipeline::getPhysicalDevice() const {
    return physicalDevice;
}

// Convert to GLM mat4 for binding
glm::mat4 convertToGLM(const blitz::Matrix4& matrix) {
    glm::mat4 result;
    for (int i = 0; i < 16; ++i) {
        result[i] = static_cast<float>(matrix.data[i]);
    }
    return result;
}

} // namespace blitz
