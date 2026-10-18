/* vulkan_pipeline.cpp - Vulkan pipeline with geometry rendering integration */

#include "vulkan_pipeline.h"
#include "vulkan_texture.h"
#include "../mesh_loader.h"
#include <iostream>
#include <cstring>

namespace blitz {

// Vertex input state creation for vertex buffer binding
static VkVertexInputBindingDescription createVertexBinding() {
    VkVertexInputBindingDescription binding = {};
    binding.binding = 0;
    binding.stride = sizeof(blitz::Vertex);
    binding.inputStepFunction = vkPointSize();
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return binding;
}

// Attribute description for position/uv attributes - matches mesh_loader::Vertex structure (x, y, z fields)
static std::vector<VkVertexInputAttributeDescription> createVertexAttributes() {
    std::vector<VkVertexInputAttributeDescription> attributes;
    
    VkVertexInputAttributeDescription posAttr = {};
    posAttr.binding = 0;
    posAttr.location = 0;
    posAttr.format = VK_FORMAT_R32G32B32_SFLOAT;
    posAttr.offset = offsetof(blitz::Vertex, x);
    
    attributes.push_back(posAttr);
    
    return attributes;
}

VulkanPipeline::VulkanPipeline() : device(nullptr), physicalDevice(nullptr), 
    pipelineLayout(VK_NULL_HANDLE), vertexBufferMemory(VK_NULL_HANDLE), 
    indexBufferMemory(VK_NULL_HANDLE), renderPassHandle(VK_NULL_HANDLE) {}

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
    
    if (renderPassHandle != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, renderPassHandle, nullptr);
        renderPassHandle = VK_NULL_HANDLE;
    }
}

bool VulkanPipeline::initialize(VkDevice _device, VkPhysicalDevice _physical_device) {
    device = _device;
    physicalDevice = _physical_device;
    
    return true;
}

VkCommandBuffer VulkanPipeline::beginRenderPass(VkCommandBuffer commandBuffer, 
        uint32_t subpassIndex, uint32_t width, uint32_t height) const {
    // Begin render pass - simple placeholder as actual bindings done in main.cpp
    return commandBuffer;
}

// Bind vertex buffers - integrates with mesh_loader.cpp data
VkCommandBuffer VulkanPipeline::bindVertexBuffers(VkCommandBuffer commandBuffer,
        VkBuffer vertexBuffer, VkDeviceSize offset) const {
    
    if (device != VK_NULL_HANDLE && vertexBuffer != VK_NULL_HANDLE) {
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, &offset);
    }
    
    return commandBuffer;
}

// Bind index buffer - integrates with mesh_loader.cpp data
VkCommandBuffer VulkanPipeline::bindIndexBuffer(VkCommandBuffer commandBuffer,
        const std::vector<uint16_t>& indices,
        VkBuffer indexBuffer) const {
    
    if (device != VK_NULL_HANDLE && indexBuffer != VK_NULL_HANDLE) {
        VkDeviceSize offsets[1] = {0};
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);
    }
    
    return commandBuffer;
}

// Set viewport and scissor for rendering
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

// Draw call - performs actual geometry rendering using mesh data
VkCommandBuffer VulkanPipeline::drawTriangles(VkCommandBuffer commandBuffer,
        uint32_t indexCount, uint32_t instanceCount) const {
    
    if (device != VK_NULL_HANDLE) {
        vkCmdDrawIndexed(commandBuffer, indexCount, instanceCount, 0, 0, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
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

void VulkanPipeline::submitToQueue(VkQueue queue) const {
    if (device != VK_NULL_HANDLE && queue != VK_NULL_HANDLE) {
        vkQueueWaitIdle(queue);
    }
}

void VulkanPipeline::destroy() {
    cleanup();
}

VkPhysicalDevice VulkanPipeline::getPhysicalDevice() const {
    return physicalDevice;
}

} // namespace blitz
