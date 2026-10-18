#ifndef VULKAN_PIPELINE_H
#define VULKAN_PIPELINE_H

#include <vulkan/vulkan.h>
#include "../mesh_loader.h"  // For Vertex struct
#include "../transforms.h"   // For SceneGraphNode, CameraFrustum, TransformUniforms, Matrix4
#include "vulkan_texture.h"
#include <vector>
#include <glm/mat4x4.hpp>

namespace blitz {

// Helper function to find suitable memory type (must be defined before use)
uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeBits, uint32_t properties);

// Vertex and fragment shader SPIR-V bytecode helpers
VkShaderModule createVertexShaderModule(VkDevice device);
VkShaderModule createFragmentShaderModule(VkDevice device);

class VulkanPipeline {
public:
    VulkanPipeline();
    ~VulkanPipeline();
    
    bool initialize(VkDevice _device, VkPhysicalDevice _physical_device);
    
    // Create the graphics pipeline from shaders with MVP support
    bool create(const char* vertexShaderSource, const char* fragmentShaderSource);

    // Bind graphics pipeline before drawing (uses vkCmdBindPipeline)
    // Note: vkCmdBindPipeline is a void function, returns nothing
    void bind(VkCommandBuffer commandBuffer) const;
    
    // Get the compiled pipeline handle for validation
    const VkPipeline* getPipelineHandle() const {
        return pipelineHandle != VK_NULL_HANDLE ? &pipelineHandle : nullptr;
    }

private:
    VkDevice device = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline pipelineHandle = VK_NULL_HANDLE;  // Created pipeline instance
    VkBuffer uniformBuffer = VK_NULL_HANDLE;
    VkDeviceMemory uniformBufferMemory = VK_NULL_HANDLE;
    VkRenderPass renderPassHandle = VK_NULL_HANDLE;
    
    // Shader module storage for graphics pipeline
    VkShaderModule vertexShaderModule_ = VK_NULL_HANDLE;
    VkShaderModule fragmentShaderModule_ = VK_NULL_HANDLE;
};

} // namespace blitz

#endif // VULKAN_PIPELINE_H
