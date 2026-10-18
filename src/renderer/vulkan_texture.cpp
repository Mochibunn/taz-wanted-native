/* vulkan_texture.cpp - Vulkan texture loading and geometry helpers */

#include "vulkan_texture.h"
#include <iostream>
#include <cstring>

namespace blitz {

VulkanTexture::VulkanTexture() : device(nullptr), physicalDevice(nullptr) {}

VulkanTexture::~VulkanTexture() {
    cleanup();
}

void VulkanTexture::cleanup() {
    if (imageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, imageView, nullptr);
        imageView = VK_NULL_HANDLE;
    }
    
    if (image != VK_NULL_HANDLE) {
        vkDestroyImage(device, image, nullptr);
        image = VK_NULL_HANDLE;
    }
}

bool VulkanTexture::initialize(VkDevice _device, VkPhysicalDevice _physical_device) {
    device = _device;
    physicalDevice = _physical_device;
    
    std::cout << "VulkanTexture: Initialized with device handle" << std::endl;
    return true;
}

VkImageTiling VulkanTexture::getTileMode() const {
    return VK_IMAGE_TILING_OPTIMAL;
}

bool createTextureImage(VkImage* outImage, VkDevice deviceHandle, BlitzTechTexture* texture) {
    (void)outImage;
    (void)deviceHandle;
    (void)texture;
    std::cout << "createTextureImage: Texture creation placeholder" << std::endl;
    return true;
}

uint32_t findMemoryType(VkPhysicalDevice physicalDeviceHandle, uint32_t typeFilter, 
                        VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProps = {};
    vkGetPhysicalDeviceMemoryProperties(physicalDeviceHandle, &memProps);
    
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && 
            (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    
    return VK_MAX_MEMORY_TYPES - 1;
}

VkRenderPass createRenderPass(VkDevice deviceHandle, VkPhysicalDevice physicalDeviceHandle) {
    (void)deviceHandle;
    (void)physicalDeviceHandle;
    std::cout << "createRenderPass: Placeholder render pass creation" << std::endl;
    return VK_NULL_HANDLE;
}

VkBuffer uploadVertexData(VkDevice deviceHandle, VkPhysicalDevice physicalDeviceHandle, const std::vector<Vertex>& vertices) {
    static bool created = false;
    
    if (!created && !vertices.empty()) {
        VkDeviceSize bufferSize = vertices.size() * sizeof(Vertex);
        
        VkBufferCreateInfo bufInfo = {};
        bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufInfo.size = bufferSize;
        bufInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        
        VkBuffer vertexBuffer = VK_NULL_HANDLE;
        vkCreateBuffer(deviceHandle, &bufInfo, nullptr, &vertexBuffer);
        
        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(deviceHandle, vertexBuffer, &memReqs);
        
        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = findMemoryType(physicalDeviceHandle, memReqs.memoryTypeBits, 
                                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        
        VkDeviceMemory vertexBufferMemory;
        vkAllocateMemory(deviceHandle, &allocInfo, nullptr, &vertexBufferMemory);
        vkBindBufferMemory(deviceHandle, vertexBuffer, vertexBufferMemory, 0);
        
        void* mapped = nullptr;
        vkMapMemory(deviceHandle, vertexBufferMemory, 0, bufferSize, 0, &mapped);
        std::memcpy(mapped, vertices.data(), bufferSize);
        vkUnmapMemory(deviceHandle, vertexBufferMemory);
        
        created = true;
        return vertexBuffer;
    }
    
    return VK_NULL_HANDLE;
}

VkBuffer uploadIndexData(VkDevice deviceHandle, VkPhysicalDevice physicalDeviceHandle, const std::vector<uint16_t>& indices) {
    static bool created = false;
    
    if (!created && !indices.empty()) {
        VkDeviceSize bufferSize = indices.size() * sizeof(uint16_t);
        
        VkBufferCreateInfo bufInfo = {};
        bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufInfo.size = bufferSize;
        bufInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        
        VkBuffer indexBuffer = VK_NULL_HANDLE;
        vkCreateBuffer(deviceHandle, &bufInfo, nullptr, &indexBuffer);
        
        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(deviceHandle, indexBuffer, &memReqs);
        
        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = findMemoryType(physicalDeviceHandle, memReqs.memoryTypeBits, 
                                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        
        VkDeviceMemory indexBufferMemory;
        vkAllocateMemory(deviceHandle, &allocInfo, nullptr, &indexBufferMemory);
        vkBindBufferMemory(deviceHandle, indexBuffer, indexBufferMemory, 0);
        
        void* mapped = nullptr;
        vkMapMemory(deviceHandle, indexBufferMemory, 0, bufferSize, 0, &mapped);
        std::memcpy(mapped, indices.data(), bufferSize);
        vkUnmapMemory(deviceHandle, indexBufferMemory);
        
        created = true;
        return indexBuffer;
    }
    
    return VK_NULL_HANDLE;
}

} // namespace blitz
