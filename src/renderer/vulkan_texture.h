/* vulkan_texture.h - Vulkan texture staging and upload routines. */
#ifndef VULKAN_TEXTURE_H
#define VULKAN_TEXTURE_H

#include <vulkan/vulkan.h>
#include <vector>

/**
 * @brief Manages Vulkan image creation, memory allocation, and texture data upload.
 * 
 * This class provides utilities for:
 * - Creating VkImage objects with device memory
 * - Setting up staging buffers for CPU->GPU transfers
 * - Uploading pixel data via staging buffers
 * - Transitioning image layouts for various operations
 */
class VulkanTexture {
public:
    /**
     * @brief Construct a VulkanTexture instance.
     * 
     * @param _device The Vulkan logical device.
     * @param _physical_device The physical GPU device.
     */
    VulkanTexture(VkDevice _device, VkPhysicalDevice _physical_device);

    /**
     * @brief Destructor - cleanup is minimal at this stage.
     */
    ~VulkanTexture();

    /**
     * @brief Create an image with device memory allocation.
     * 
     * @param device The Vulkan logical device (can be same as device_).
     * @param phys_dev Physical device to use.
     * @param format Pixel format (e.g., VK_FORMAT_B8G8R8A8_UNORM).
     * @param type Image type (VK_IMAGE_TYPE_2D for 2D textures).
     * @param samples Sample count (VK_SAMPLE_COUNT_1_BIT by default).
     * @param mip_count Number of mipmap levels (0 means single level).
     * @param array_layers Number of array layers.
     * @param tiling Image tiling (VK_IMAGE_TILING_DEVICE for device local memory, VK_IMAGE_TILING_LINEAR for linear).
     * @param mem_flags Memory property flags for allocation.
     * @return VkResult Result of image creation and binding.
     */
    VkResult createImageWithDeviceMemory(
        VkDevice device,
        VkPhysicalDevice phys_dev,
        VkFormat format,
        VkImageType type,
        VkSampleCountFlagBits samples,
        uint32_t mip_count,
        uint32_t array_layers,
        VkImageTiling tiling,
        VkMemoryPropertyFlags mem_flags);

    /**
     * @brief Upload data from a staging buffer into the image.
     * 
     * Assumes the staging buffer already contains data and is bound.
     * 
     * @param cmd_buf The command buffer to record commands in.
     * @param data Pointer to source data.
     * @param size Size of data in bytes.
     * @param layout Target layout (must be VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL).
     * @return true if successful, false otherwise.
     */
    bool uploadTextureData(VkCommandBuffer cmd_buf, const void* data, VkDeviceSize size, VkImageLayout layout);

    /**
     * @brief Transition the image layout for prepare/finish operations.
     * 
     * Handles common transitions between layouts.
     * 
     * @param cmd_buf The command buffer to record commands in.
     * @param img The image handle (VK_NULL_HANDLE is allowed as placeholder).
     * @param format Pixel format of the image.
     * @param aspect Image aspect mask (e.g., VK_IMAGE_ASPECT_COLOR_BIT).
     * @param src_layout Source layout to transition from.
     * @param dst_layout Destination layout to transition to.
     * @return true if successful, false otherwise.
     */
    bool transitionImageLayout(
        VkCommandBuffer cmd_buf,
        VkImage img,
        VkFormat format,
        VkImageAspectFlags aspect,
        VkImageLayout src_layout,
        VkImageLayout dst_layout);

    /**
     * @brief Create a staging buffer for CPU->GPU transfers.
     * 
     * This is typically used as an intermediate buffer for copying data to an image.
     * The staging buffer uses transfer source memory usage.
     * 
     * @param device The Vulkan logical device.
     * @param phys_dev Physical device to query memory properties from.
     * @param size Size of the buffer in bytes.
     * @return VkBuffer The created buffer handle, or VK_NULL_HANDLE on failure.
     */
    VkBuffer createStagingBuffer(VkDevice device, VkPhysicalDevice phys_dev, uint64_t size);

    /**
     * @brief Upload via staging buffer: allocate memory, bind buffer, copy to image.
     * 
     * This is the recommended workflow for CPU->GPU texture uploads:
     * 1. Create staging buffer with uploaded data
     * 2. Transition target image to TRANSFER_DST_OPTIMAL
     * 3. CopyBufferToImage
     * 4. Transition target image to final layout (e.g., SHADER_READ_ONLY_OPTIMAL)
     * 
     * @param cmd_buf The command buffer to record commands in.
     * @param data Source texture data from CPU.
     * @param data_size Size of the data in bytes.
     * @param img Target image handle.
     * @param format Pixel format (used for layout transition).
     * @param layout Target image layout after upload.
     * @return true if successful, false otherwise.
     */
    bool uploadViaStagingBuffer(
        VkCommandBuffer cmd_buf,
        const void* data,
        VkDeviceSize data_size,
        VkImage img,
        VkFormat format,
        VkImageLayout layout);

    /**
     * @brief Map an image for CPU read/write (if supported by device).
     * 
     * Note: This requires the underlying memory to be host-visible and host-coherent.
     * After mapping, you can directly access image data via pointer.
     * 
     * @param cmd_buf Not used in this implementation (memory is already mapped).
     * @param img The image handle.
     * @param write_only Whether to map for read/write or read-only.
     * @param out_data Output pointer to mapped data.
     * @param out_size Output size of the mapped region.
     * @return true if mapping succeeded, false otherwise.
     */
    bool mapImage(VkCommandBuffer cmd_buf, VkImage img, bool write_only, void** out_data, VkDeviceSize* out_size);

    /**
     * @brief Unmap an image memory region.
     * 
     * @param cmd_buf Not used in this implementation.
     * @param img The image handle.
     * @param flags Memory map flags (typically not used).
     * @return true if successful (always returns true as unmap is done via vkUnmapMemory).
     */
    bool unmapImage(VkCommandBuffer cmd_buf, VkImage img, VkMemoryMapFlags flags);

    /**
     * @brief Get a human-readable format block size for debugging.
     * 
     * @param format The Vulkan pixel format.
     * @return uint32_t Block size in bytes per pixel (for common formats).
     */
    static uint32_t getFormatBlockSize(VkFormat format);

private:
    /**
     * @brief Find a suitable memory type index based on bit properties mask.
     * 
     * Linear search through memory types to find one with required flags.
     * 
     * @param required_property_flags Bitmask of required property flags.
     * @param props Memory properties of the physical device.
     * @return Memory type index, or -1 if no suitable type found.
     */
    uint32_t findMemoryType(uint32_t required_property_flags, const VkPhysicalDeviceMemoryProperties& props);

    /**
     * @brief Find a transfer-capable memory type (host-visible + coherent).
     * 
     * Prefer host-visible and host-coherent memory for CPU transfers.
     * Falls back to any host-visible memory if no coherent type available.
     * 
     * @param props Memory properties of the physical device.
     * @return Memory type index, or -1 if no suitable type found.
     */
    uint32_t findTransferMemoryType(const VkPhysicalDeviceMemoryProperties& props);

    VkDevice device_;
    VkPhysicalDevice physical_device_;
};

#endif // VULKAN_TEXTURE_H
