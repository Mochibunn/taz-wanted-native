#ifndef VULKAN_TEXTURE_H
#define VULKAN_TEXTURE_H

#include <vulkan/vulkan.h>
#include "../mesh_loader.h"
#include "../texture_loader.h"

namespace blitz {

class VulkanTexture {
public:
    VulkanTexture();
    ~VulkanTexture();
    
    bool initialize(VkDevice _device, VkPhysicalDevice _physical_device);
    void cleanup();
    
    VkImageTiling getTileMode() const;
    
    // Public accessor for imageView (used in pipeline binding)
    VkImageView getImageView() const { return imageView; }

private:
    VkDevice device = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkImage image = VK_NULL_HANDLE;
    VkImageView imageView = VK_NULL_HANDLE;
};

// Helper functions declared here, defined in vulkan_texture.cpp
VkRenderPass createRenderPass(VkDevice deviceHandle, VkPhysicalDevice physicalDeviceHandle);
VkBuffer uploadVertexData(VkDevice deviceHandle, const std::vector<Vertex>& vertices);
VkBuffer uploadIndexData(VkDevice deviceHandle, const std::vector<uint16_t>& indices);

// Global device handle (set in main)
extern VkDevice g_device;

} // namespace blitz

#endif // VULKAN_TEXTURE_H
