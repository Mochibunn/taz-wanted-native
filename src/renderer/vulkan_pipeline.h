#ifndef VULKAN_PIPELINE_H
#define VULKAN_PIPELINE_H

#include <vulkan/vulkan.h>
#include "../mesh_loader.h"  // For Vertex struct
#include "vulkan_texture.h"
#include <vector>

namespace blitz {

class VulkanPipeline {
public:
    VulkanPipeline();
    ~VulkanPipeline();
    
    bool initialize(VkDevice _device, VkPhysicalDevice _physical_device);
    void cleanup();
    
    // Create the graphics pipeline from shaders
    VkResult create(const char* vertexShader, const char* fragmentShader);
    
    // Begin recording a render pass on command buffer
    VkCommandBuffer beginRenderPass(VkCommandBuffer commandBuffer, 
        uint32_t subpassIndex, uint32_t width, uint32_t height) const;
    
    // Placeholder - full implementation would create pipeline layout
    VkCommandBuffer bind(VkCommandBuffer commandBuffer) const { return commandBuffer; }
    
    VkCommandBuffer setViewport(VkCommandBuffer commandBuffer, 
        uint32_t width, uint32_t height) const;
    
    VkCommandBuffer drawTriangles(VkCommandBuffer commandBuffer,
        size_t vertexCount, size_t instanceCount);
    
    VkCommandBuffer bindVertexBuffers(VkCommandBuffer commandBuffer, 
        const std::vector<Vertex>& vertices, 
        VkBuffer vertexBuffer, VkDeviceSize offset) const;

    VkCommandBuffer bindIndexBuffer(VkCommandBuffer commandBuffer,
        const std::vector<uint16_t>& indices,
        VkBuffer indexBuffer) const;

    void endRenderPass(VkCommandBuffer commandBuffer, 
        uint32_t subpassIndex) const;
    
    // Removed VulkanContext dependency
    void submitToQueue() const;
    
    void destroy();
    
    bool getPipelineLayout() const;
    
    VkResult getRenderPass() const;
    
    const void* getShaderBindings() const;
    
    const VkShaderModule* getVertexShaderModule() const;
    
    const VkShaderModule* getFragmentShaderModule() const;
    
    VulkanTexture* getTexture() const;
    
    VkPhysicalDevice getPhysicalDevice() const;
    
    // Get graphics queue - needs proper implementation with device initialization
    VkQueue getGraphicsQueue() const;
    
    // Get color attachment image - placeholder for render pass
    VkImage getColorAttachment() const;

private:
    VkDevice device = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
    VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;
    
    // Render pass handle for actual render pass creation
    VkRenderPass renderPassHandle = VK_NULL_HANDLE;
    
    // Framebuffer handle (placeholder - needs proper creation)
    VkFramebuffer framebuffer = VK_NULL_HANDLE;

};

} // namespace blitz

#endif // VULKAN_PIPELINE_H
