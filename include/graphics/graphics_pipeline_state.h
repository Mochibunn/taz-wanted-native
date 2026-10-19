/* graphics_pipeline_state.h - Graphics pipeline state management */
#ifndef GRAPHICS_PIPELINE_STATE_H
#define GRAPHICS_PIPELINE_STATE_H

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

struct GraphicsPipelineState {
    VkShaderModule vertex_shader_module = VK_NULL_HANDLE;
    VkShaderModule fragment_shader_module = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    
    // Uniform buffers
    VkBuffer mvp_uniform_buffer = VK_NULL_HANDLE;
    VkDeviceMemory mvp_uniform_memory = VK_NULL_HANDLE;
    VkBuffer vertex_coord_buffer = VK_NULL_HANDLE;
    VkDeviceMemory vertex_coord_memory = VK_NULL_HANDLE;
    
    bool valid = false;
    bool initialized = false;
    
    // Pipeline creation callbacks
    VkResult (*createVertexShaderModule)(VkDevice device, VkShaderModule* out_module);
    VkResult (*createFragmentShaderModule)(VkDevice device, VkShaderModule* out_module);
    GraphicsPipelineState (*initializeGraphicsPipeline)(VkDevice device);
    bool (*bindPipeline)(VkCommandBuffer commandBuffer, GraphicsPipelineState& state);
    
    void cleanup(VkDevice device) {
        if (vertex_shader_module != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device, vertex_shader_module, nullptr);
        }
        if (fragment_shader_module != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device, fragment_shader_module, nullptr);
        }
        if (pipeline_layout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
        }
        if (descriptor_set_layout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device, descriptor_set_layout, nullptr);
        }
        if (pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device, pipeline, nullptr);
        }
        if (mvp_uniform_buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, mvp_uniform_buffer, nullptr);
        }
        if (mvp_uniform_memory != VK_NULL_HANDLE) {
            vkFreeMemory(device, mvp_uniform_memory, nullptr);
        }
    }
};

#endif // GRAPHICS_PIPELINE_STATE_H