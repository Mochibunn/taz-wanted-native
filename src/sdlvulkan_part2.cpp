// ============================================================================
// TAZ: WANTED - SDL2 + VULKAN RENDER LOOP - PART 2
// Swapchain creation, render pass, pipeline setup
// ============================================================================

#include <iostream>
#include <vector>

extern "C" {
#include <SDL2/SDL.h>
#include <SDL2/SDL_video.h>
#include <vulkan/vulkan.h>
}

// External global state (declared in part 1)
SDL_Window* window_ptr = nullptr;
VkInstance g_vulkan_instance = VK_NULL_HANDLE;
VkSurfaceKHR g_surface = VK_NULL_HANDLE;
VkPhysicalDevice g_physical_device = VK_NULL_HANDLE;
VkDevice g_device = VK_NULL_HANDLE;

// ============================================================================
// STEP 3: SWAPCHAIN CREATION
// ============================================================================
static VkResult create_swapchain() {
    std::cout << "=== Creating Vulkan Swapchain ===\n";
    
    // Query surface format
    uint32_t surface_format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(g_physical_device, g_surface, nullptr, &surface_format_count);
    
    if (surface_format_count == 0) {
        std::cerr << "Failed to get surface formats\n";
        return VK_ERROR_INCOMPATIBLE_DEVICE_ORATION;
    }

    std::vector<VkSurfaceFormatKHR> surface_formats(surface_format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(g_physical_device, g_surface, &surface_format_count, surface_formats.data());
    
    // Use first format or fallback to B8G8R8A8_UNORM
    uint32_t chosen_format_index = 0;
    if (surface_formats[chosen_format_index].format != VK_FORMAT_UNDEFINED) {
        ; // Good - use first format
    } else {
        std::cerr << "Surface format undefined, using B8G8R8A8_UNORM\n";
        chosen_format_index = 0; // Will be overridden below
    }

    VkSurfaceFormatKHR chosen_format = surface_formats[chosen_format_index];
    
    // Handle case where first format is UNDEFINED (driver wants us to specify)
    if (chosen_format.format == VK_FORMAT_UNDEFINED) {
        chosen_format.format = VK_FORMAT_B8G8R8A8_UNORM;
        chosen_format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    }

    // Query surface image formats
    uint32_t image_count = 0;
    vkGetPhysicalDeviceSurfaceImageFormatsKHR(g_physical_device, g_surface, nullptr, &image_count, nullptr);

    std::vector<VkFormat> image_formats(image_count);
    vkGetPhysicalDeviceSurfaceImageFormatsKHR(g_physical_device, g_surface, &image_count, image_formats.data());

    // Query swapchain capabilities
    VkSurfaceCapabilitiesKHR surface_capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_physical_device, g_surface, &surface_capabilities);

    // Determine image extent - use min/max clamped to supported values
    VkExtent2D swapchain_extent = surface_capabilities.currentExtent;
    
    // For windowed mode, we can use the min/max/extent as desired
    if (surface_capabilities.currentExtent.width == UINT32_MAX) {
        // Windowed mode: extent is clamped
        uint32_t width = std::clamp(static_cast<uint32_t>(WINDOW_WIDTH), 
                                    surface_capabilities.minImageExtent.width,
                                    surface_capabilities.maxImageExtent.width);
        uint32_t height = std::clamp(static_cast<uint32_t>(WINDOW_HEIGHT),
                                     surface_capabilities.minImageExtent.height,
                                     surface_capabilities.maxImageExtent.height);
        swapchain_extent.width = width;
        swapchain_extent.height = height;
    }

    // Determine image array size
    uint32_t image_array_size = std::max(SWAPCHAIN_IMAGE_COUNT, 
                                         static_cast<uint32_t>(surface_capabilities.minImageCount));
    image_array_size = std::min(image_array_size, surface_capabilities.maxImageCount);

    // Create swapchain
    VkSwapchainCreateInfoKHR swapchain_create{};
    swapchain_create.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_create.surface = g_surface;
    swapchain_create.minImageCount = image_array_size;
    swapchain_create.imageFormat = chosen_format.format;
    swapchain_create.imageColorSpace = chosen_format.colorSpace;
    swapchain_create.imageExtent = swapchain_extent;
    swapchain_create.imageArrayLayers = 1;
    swapchain_create.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    
    // Handle queue families - combine graphics and present if same family
    VkQueueFamilyProperties queue_props;
    uint32_t graphics_family_idx = 0, present_family_idx = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(g_physical_device, &device_count, nullptr);
    std::vector<VkQueueFamilyProperties> qfamilies(16);
    vkGetPhysicalDeviceQueueFamilyProperties(g_physical_device, &device_count, qfamilies.data());
    
    for (uint32_t i = 0; i < device_count; ++i) {
        if ((qfamilies[i].flags & VK_QUEUE_GRAPHICS_BIT) != 0) graphics_family_idx = i;
        if ((qfamilies[i].queueFlags & VK_QUEUE_PRESENT_BIT) != 0) present_family_idx = i;
    }
    
    std::vector<VkSwapchainKHR> dependencies; // Empty for first swapchain
    
    VkBool32 present_mode_ext = VK_FALSE;
    uint32_t present_mode_idx = 0;
    
#ifdef VK_USE_PLATFORM_WIN32_KHR
    // For Windows: we need to specify present mode
    if (present_family_idx == graphics_family_idx) {
        swapchain_create.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchain_create.preTransform = surface_capabilities.currentTransform;
        swapchain_create.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapchain_create.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    } else {
        // Different queues - need sharing
        swapchain_create.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchain_create.queueFamilyIndexCount = 2;
        swapchain_create.pQueueFamilyIndices = new VkUint32[]{graphics_family_idx, present_family_idx};
        swapchain_create.preTransform = surface_capabilities.currentTransform;
        swapchain_create.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapchain_create.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    }
#endif

    swapchain_create.clipped = VK_TRUE;
    swapchain_create.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    
    std::cout << "Swapchain info:\n";
    std::cout << "  Format: " << chosen_format.format << "\n";
    std::cout << "  Color space: " << (uint32_t)chosen_format.colorSpace << "\n";
    std::cout << "  Extent: " << swapchain_extent.width << "x" << swapchain_extent.height << "\n";
    std::cout << "  Array layers: " << image_array_size << "\n";
    std::cout << "  Usage: " << (uint32_t)swapchain_create.imageUsage << "\n";

    VkResult result = vkCreateSwapchainKHR(g_device, &swapchain_create, nullptr, &g_swapchain);
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create swapchain: " << result << "\n";
        return result;
    }

    std::cout << "✅ Swapchain created successfully\n";
    
    // Query swapchain images
    uint32_t swapchain_image_count = 0;
    vkGetSwapchainImagesKHR(g_device, g_swapchain, nullptr, &swapchain_image_count, nullptr);
    
    if (swapchain_image_count == 0) {
        std::cerr << "Failed to get swapchain images\n";
        return VK_ERROR_INCOMPATIBLE_DEVICE_ORATION;
    }

    std::cout << "✅ Swapchain has " << swapchain_image_count << " images\n";

    // Create swapchain images
    g_swapchain_images.resize(swapchain_image_count);
    vkGetSwapchainImagesKHR(g_device, g_swapchain, nullptr, &swapchain_image_count, g_swapchain_images.data());
    
    std::cout << "✅ Retrieved " << g_swapchain_images.size() << " swapchain images\n";

    // Create image views for each swapchain image
    VkImageViewCreateInfo view_create{};
    view_create.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_create.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    view_create.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_create.subresourceRange.levelCount = 1;
    view_create.subresourceRange.layerCount = 1;
    view_create.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    for (uint32_t i = 0; i < swapchain_image_count; ++i) {
        view_create.image = g_swapchain_images[i];
        view_create.format = chosen_format.format;
        
        VkImageView temp_view;
        vkCreateImageView(g_device, &view_create, nullptr, &temp_view);
        g_swapchain_image_views.push_back(temp_view);
    }

    std::cout << "✅ Created " << g_swapchain_image_views.size() << " image views\n";

    return VK_SUCCESS;
}
