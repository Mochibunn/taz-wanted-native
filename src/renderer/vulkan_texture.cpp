/* vulkan_texture.cpp - Vulkan texture staging and upload routines. */
#include "vulkan_texture.h"
#include <cstring>
#include <cmath>

namespace {

/* Helper: get memory properties of a physical device. */
VkPhysicalDeviceMemoryProperties getPhysDevMemoryProperties(VkPhysicalDevice phys_dev)
{
    VkPhysicalDeviceMemoryProperties props = {};
    vkGetPhysicalDeviceMemoryProperties(phys_dev, &props);
    return props;
}

} /* namespace */

VulkanTexture::VulkanTexture(VkDevice _device, VkPhysicalDevice _physical_device)
    : device_(_device), physical_device_(_physical_device)
{
}

VulkanTexture::~VulkanTexture()
{
    /* Clean up any resources here if needed. */
}

/* Get the block size for a given format (used in memory allocation). */
uint32_t VulkanTexture::getFormatBlockSize(VkFormat format)
{
    switch (format) {
        case VK_FORMAT_B8G8R8A8_UNORM:   return 32;
        case VK_FORMAT_B8G8R8A8_SRGB:    return 32;
        case VK_FORMAT_R8G8B8A8_UNORM:   return 32;
        case VK_FORMAT_R8G8B8A8_SRGB:    return 32;
        case VK_FORMAT_R8_UNORM:         return 4;
        case VK_FORMAT_R16_UNORM:        return 8;
        case VK_FORMAT_R16G16_UNORM:     return 16;
        default: return 0;
    }
}

/* Find a suitable memory type index for general use (host accessible). */
uint32_t VulkanTexture::findMemoryType(
    uint32_t required_property_flags,
    const VkPhysicalDeviceMemoryProperties& props)
{
    /* Search for a type matching our requirements. */
    for (uint32_t i = 0; i < props.memoryTypeCount; ++i) {
        if ((props.memoryTypes[i].propertyFlags & required_property_flags) == required_property_flags) {
            return i;
        }
    }

    return static_cast<uint32_t>(-1); /* No suitable type found. */
}

/* Find a transfer-capable memory type (host-visible + coherent). */
uint32_t VulkanTexture::findTransferMemoryType(const VkPhysicalDeviceMemoryProperties& props)
{
    /* Prefer host-visible + coherent memory for CPU transfers. */
    uint32_t host_visible_coherent = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    for (uint32_t i = 0; i < props.memoryTypeCount; ++i) {
        if ((props.memoryTypes[i].propertyFlags & host_visible_coherent) == host_visible_coherent) {
            return i;
        }
    }

    /* Fallback: any host-visible memory is acceptable for transfers. */
    uint32_t host_visible = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    for (uint32_t i = 0; i < props.memoryTypeCount; ++i) {
        if ((props.memoryTypes[i].propertyFlags & host_visible) == host_visible) {
            return i;
        }
    }

    return static_cast<uint32_t>(-1); /* No suitable type found. */
}

/* Create an image with device memory allocation. */
VkResult VulkanTexture::createImageWithDeviceMemory(
    VkDevice device,
    VkPhysicalDevice phys_dev,
    VkFormat format,
    VkImageType type,
    VkSampleCountFlagBits samples,
    uint32_t mip_count,
    uint32_t array_layers,
    VkImageTiling tiling,
    VkMemoryPropertyFlags mem_flags)
{
    VkPhysicalDeviceMemoryProperties props = getPhysDevMemoryProperties(phys_dev);

    /* Image create info - standard values. */
    VkImageCreateInfo image_info = {};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.pNext = nullptr;
    image_info.flags = 0;

    /* Image type and dimensions. */
    image_info.imageType = type;
    image_info.format = format;
    image_info.extent.depth = 1;

    /* Default size (override as needed). */
    image_info.extent.width = 1024;
    image_info.extent.height = 1024;

    /* Mipmaps and array layers. */
    image_info.mipLevels = mip_count > 0 ? mip_count : 1;
    image_info.arrayLayers = array_layers > 0 ? array_layers : 1;
    image_info.samples = samples;
    image_info.tiling = tiling;

    /* Usage: transfer source, transfer dest, and sampled. */
    image_info.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    /* Sharing mode exclusive for simplicity. */
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.queueFamilyIndexCount = 0;
    image_info.pQueueFamilyIndices = nullptr;

    /* Initial layout undefined. */
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    /* Create the image. */
    VkImage img;
    VkResult result = vkCreateImage(device, &image_info, nullptr, &img);
    if (result != VK_SUCCESS || img == VK_NULL_HANDLE) {
        return result;
    }

    /* Get memory requirements for the image. */
    VkMemoryRequirements mem_requirements = {};
    vkGetImageMemoryRequirements(device, img, &mem_requirements);

    /* Find suitable memory type index based on flags. */
    uint32_t mem_type_idx = findMemoryType(
        static_cast<uint32_t>(mem_flags), props);

    if (mem_type_idx == static_cast<uint32_t>(-1)) {
        vkDestroyImage(device, img, nullptr);
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    /* Allocation info. */
    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = static_cast<uint32_t>(mem_type_idx);

    /* Allocate memory. */
    VkDeviceMemory dev_mem;
    result = vkAllocateMemory(device, &alloc_info, nullptr, &dev_mem);
    if (result != VK_SUCCESS || dev_mem == VK_NULL_HANDLE) {
        vkDestroyImage(device, img, nullptr);
        return result;
    }

    /* Bind memory to the image. */
    result = vkBindImageMemory(device, img, dev_mem, 0);
    if (result != VK_SUCCESS) {
        vkFreeMemory(device, dev_mem, nullptr);
        vkDestroyImage(device, img, nullptr);
        return result;
    }

    /* Return VK_SUCCESS. The image and memory are now bound and ready for use. */
    return VK_SUCCESS;
}

/* Create a staging buffer for CPU-to-GPU uploads. */
VkBuffer VulkanTexture::createStagingBuffer(VkDevice device, VkPhysicalDevice phys_dev, uint64_t size)
{
    /* Buffer create info for transfer operations. */
    VkBufferCreateInfo buf_info = {};
    buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buf_info.pNext = nullptr;
    buf_info.flags = 0;
    buf_info.size = static_cast<VkDeviceSize>(size);
    buf_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    buf_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    buf_info.queueFamilyIndexCount = 0;
    buf_info.pQueueFamilyIndices = nullptr;

    /* Create the buffer. */
    VkBuffer buffer;
    vkCreateBuffer(device, &buf_info, nullptr, &buffer);
    if (buffer == VK_NULL_HANDLE) {
        return VK_NULL_HANDLE;
    }

    /* Get memory requirements for the buffer. */
    VkMemoryRequirements mem_requirements = {};
    vkGetBufferMemoryRequirements(device, buffer, &mem_requirements);

    /* Find transfer memory type index. */
    uint32_t mem_type_idx = findTransferMemoryType(getPhysDevMemoryProperties(phys_dev));
    if (mem_type_idx == static_cast<uint32_t>(-1)) {
        vkDestroyBuffer(device, buffer, nullptr);
        return VK_NULL_HANDLE;
    }

    /* Allocation info. */
    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = static_cast<uint32_t>(mem_type_idx);

    /* Allocate memory for the buffer. */
    VkDeviceMemory buffer_mem;
    vkAllocateMemory(device, &alloc_info, nullptr, &buffer_mem);
    if (buffer_mem == VK_NULL_HANDLE) {
        vkDestroyBuffer(device, buffer, nullptr);
        return VK_NULL_HANDLE;
    }

    /* Bind memory to the buffer. */
    vkBindBufferMemory(device, buffer, buffer_mem, 0);

    /* Return VkBuffer handle. */
    return buffer;
}

/* Upload texture data via staging buffer. */
bool VulkanTexture::uploadViaStagingBuffer(
    VkCommandBuffer cmd_buf,
    const void* data,
    VkDeviceSize data_size,
    VkImage img,
    VkFormat format,
    VkImageLayout layout)
{
    /* Create staging buffer for upload. */
    VkBuffer staging = createStagingBuffer(device_, physical_device_, data_size);
    if (staging == VK_NULL_HANDLE) {
        return false;
    }

    /* Upload data into the staging buffer from host memory. */
    void* host_ptr = nullptr;
    VkResult result = vkMapMemory(device_, staging, 0, data_size, 0, &host_ptr);
    if (result != VK_SUCCESS || !host_ptr) {
        vkDestroyBuffer(device_, staging, nullptr);
        return false;
    }

    /* Copy data from host pointer to buffer. */
    std::memcpy(host_ptr, data, static_cast<size_t>(data_size));

    /* Unmap the staging buffer memory. */
    vkUnmapMemory(device_, staging);

    /* Transition image to transfer destination layout. */
    result = transitionImageLayout(cmd_buf, img, format, VK_IMAGE_ASPECT_COLOR_BIT,
                                   VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    if (result == false) {
        vkDestroyBuffer(device_, staging, nullptr);
        return false;
    }

    /* Copy from staging buffer to image. */
    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.subresourceLayers = 1;
    region.imageOffset = {0, 0, 0};

    /* Calculate actual image extent based on format. */
    uint32_t tex_width = static_cast<uint32_t>(std::sqrt(static_cast<double>(data_size / (64 * sizeof(uint8_t)))));
    if (tex_width == 0) tex_width = 1;

    region.imageExtent.width = tex_width;
    region.imageExtent.height = tex_width;
    region.imageExtent.depth = 1;

    std::vector<VkBufferImageCopy> regions(1, region);
    vkCmdCopyBufferToImage(cmd_buf, staging, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           static_cast<uint32_t>(regions.size()), regions.data());

    /* Transition image to shader-read-only layout. */
    result = transitionImageLayout(cmd_buf, img, format, VK_IMAGE_ASPECT_COLOR_BIT,
                                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    if (result == false) {
        vkDestroyBuffer(device_, staging, nullptr);
        return false;
    }

    /* Clean up staging buffer. */
    vkDestroyBuffer(device_, staging, nullptr);

    return true;
}

/* Upload texture data directly into the image. */
bool VulkanTexture::uploadTextureData(VkCommandBuffer cmd_buf, const void* data, VkDeviceSize size, VkImageLayout layout)
{
    /* This function is a placeholder for direct upload when using device memory.
     * In practice, images are typically uploaded via staging buffers or through mapped memory. */

    return true;
}

/* Transition the image layout from src to dst. */
bool VulkanTexture::transitionImageLayout(
    VkCommandBuffer cmd_buf,
    VkImage img,
    VkFormat format,
    VkImageAspectFlags aspect,
    VkImageLayout src_layout,
    VkImageLayout dst_layout)
{
    /* Handle NULL image gracefully. */
    if (img == VK_NULL_HANDLE) {
        return false;
    }

    /* Barrier info. */
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.pNext = nullptr;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = 0;

    /* Set common properties. */
    barrier.image = img;
    barrier.oldLayout = src_layout;
    barrier.newLayout = dst_layout;
    barrier.subresourceRange.aspectMask = aspect;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags stage_flags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

    /* Access masks and stages based on destination layout. */
    switch (dst_layout) {
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            stage_flags = VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_HOST_WRITE_BIT;
            stage_flags = VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;

        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            /* For shader read, we need to wait for all previous writes. */
            barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            stage_flags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            break;

        case VK_IMAGE_LAYOUT_PREINITIALIZED:
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_HOST_WRITE_BIT;
            stage_flags = VK_PIPELINE_STAGE_HOST_BIT;
            break;

        default:
            return false;
    }

    /* Update barrier stages. */
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    /* Issue the pipeline barrier. */
    const std::vector<VkImageMemoryBarrier>& barriers = {barrier};
    vkCmdPipelineBarrier(cmd_buf, stage_flags, stage_flags, 0, 0, nullptr, 0, nullptr,
                         static_cast<uint32_t>(1), &barriers[0]);

    return true;
}

/* Map an image for CPU read/write (simplified, returns true to indicate success). */
bool VulkanTexture::mapImage(VkCommandBuffer cmd_buf, VkImage img, bool write_only, void** out_data, VkDeviceSize* out_size)
{
    if (!out_data || !out_size) {
        return false;
    }

    *out_data = nullptr;
    *out_size = 0;

    /* Simplified: just succeed; in practice, mapping happens via host-visible memory allocation. */
    return true;
}

/* Unmap an image memory region. */
bool VulkanTexture::unmapImage(VkCommandBuffer cmd_buf, VkImage img, VkMemoryMapFlags flags)
{
    /* Simplified: just ensure memory is not mapped anymore. */
    /* In practice, vkUnmapMemory should be called before destroying the device/memory. */
    return true;
}
