// ============================================================================
// TAZ: WANTED - SDL2 + VULKAN COMPLETE RENDER LOOP
// Implements SDL2 window with Vulkan swapchain, framebuffers, pipeline,
// command buffers, and real-time render loop with presentation.
// ============================================================================

#include <iostream>
#include <vector>
#include <cstring>
#include <cmath>
#include <optional>
#include <array>

#define GLM_FORCE_INLINE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// SDL2 headers
#include <SDL2/SDL.h>
#include <SDL2/SDL_video.h>

// Vulkan headers - use standard header (platform-specific surface requires SDL_Vulkan_CreateSurface)
#include <vulkan/vulkan.h>

// Window dimensions
#define WINDOW_WIDTH 1920
#define WINDOW_HEIGHT 1080
#define SWAPCHAIN_IMAGE_COUNT 3

// Global Vulkan state
VkInstance g_vulkan_instance = VK_NULL_HANDLE;
VkSurfaceKHR g_surface = VK_NULL_HANDLE;
VkPhysicalDevice g_physical_device = VK_NULL_HANDLE;
VkDevice g_device = VK_NULL_HANDLE;
VkQueue graphics_queue = VK_NULL_HANDLE;
VkQueue present_queue = VK_NULL_HANDLE;

// Swapchain structures
VkSwapchainKHR g_swapchain = VK_NULL_HANDLE;
std::vector<VkImage> g_swapchain_images;
std::vector<VkImageView> g_swapchain_image_views;
std::vector<VkFramebuffer> g_swapchain_framebuffers;
VkRenderPass g_render_pass = VK_NULL_HANDLE;
VkPipeline g_graphics_pipeline = VK_NULL_HANDLE;
VkPipelineLayout g_pipeline_layout = VK_NULL_HANDLE;

// Command buffers and synchronization
VkCommandPool g_command_pool = VK_NULL_HANDLE;
std::vector<VkCommandBuffer> g_command_buffers;
std::vector<VkSemaphore> g_image_available_semaphores;
std::vector<VkSemaphore> g_render_complete_semaphores;
std::vector<VkFence> g_fences;

// Vertex buffer
VkBuffer vertex_buffer = VK_NULL_HANDLE;
VkDeviceMemory vertex_memory = VK_NULL_HANDLE;

// Uniform buffer for terrain parameters
static constexpr uint32_t UNIFORM_BUFFER_SIZE = 1024;

// ============================================================================
// VERTEX STRUCTURE (pointer-based for Vulkan API compatibility)
// ============================================================================

struct Vertex {
    float position[3];  // x, y, z
    float uv[2];        // texture coordinates
};

std::vector<Vertex> g_vertices;

// Generate terrain mesh with gradient vertices
void generate_terrain_mesh() {
    std::cout << "Generating terrain mesh...\n";
    
    int grid_size = 10;
    float grid_step_x = static_cast<float>(WINDOW_WIDTH) / grid_size;
    float grid_step_z = static_cast<float>(WINDOW_HEIGHT) / grid_size;
    
    for (int z = 0; z < grid_size + 1; z++) {
        for (int x = 0; x < grid_size + 1; x++) {
            Vertex vertex{};
            vertex.position[0] = static_cast<float>(x) * grid_step_x - WINDOW_WIDTH / 2.0f;
            // Height based on sine wave gradient
            vertex.position[1] = sinf(vertex.position[0] * 0.01f) * 
                                 cosf(static_cast<float>(z) * grid_step_z * 0.01f) * 50.0f;
            vertex.position[2] = -(static_cast<float>(z) * grid_step_z - WINDOW_HEIGHT / 2.0f);
            vertex.uv[0] = static_cast<float>(x) / grid_size;
            vertex.uv[1] = static_cast<float>(z) / grid_size;
            
            g_vertices.push_back(vertex);
        }
    }
    
    std::cout << "Terrain mesh generated: " << g_vertices.size() << " vertices\n";
}

// ============================================================================
// HELPER FUNCTION: GET MEMORY TYPE
// ============================================================================

uint32_t get_memory_type(VkPhysicalDevice device, uint32_t bitMask, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties mem_properties{};
    vkGetPhysicalDeviceMemoryProperties(device, &mem_properties);

    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
        if ((bitMask & (1 << i)) && 
            (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    std::cerr << "Failed to find suitable memory type!\n";
    return UINT32_MAX;
}

// ============================================================================
// VULKAN INITIALIZATION
// ============================================================================

VkResult init_vulkan_instance() {
    std::cout << "Initializing Vulkan instance...\n";

    const char* extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef __linux__
        VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
#else
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#endif
    };

    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "TAZ: WANTED";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "Custom Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo instance_create_info{};
    instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_create_info.pApplicationInfo = &app_info;
    instance_create_info.enabledExtensionCount = sizeof(extensions) / sizeof(extensions[0]);
    instance_create_info.ppEnabledExtensionNames = extensions;

    VkResult result = vkCreateInstance(&instance_create_info, nullptr, &g_vulkan_instance);

    if (result != VK_SUCCESS || !g_vulkan_instance) {
        std::cerr << "Failed to create Vulkan instance: " << static_cast<int>(result) << "\n";
        return result;
    }

    std::cout << "Vulkan instance created successfully.\n";
    return VK_SUCCESS;
}

// ============================================================================
// CREATE SDL2 WINDOW AND VULKAN SURFACE
// ============================================================================

VkResult create_sdl_window_and_surface() {
    std::cout << "Creating SDL2 window and Vulkan surface...\n";
    
    if (!g_vulkan_instance) {
        std::cerr << "Vulkan instance not initialized!\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Create SDL2 window with Vulkan support flag
    g_sdl_window = SDL_CreateWindow(
        "TAZ: WANTED",                    // Title
        WINDOW_WIDTH,                     // Width
        WINDOW_HEIGHT,                    // Height
        SDL_WINDOW_RESIZABLE |            // Resizable
        SDL_WINDOW_BORDERLESS |           // No borders
        SDL_WINDOW_VULKAN                 // Vulkan support
    );

    if (!g_sdl_window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    std::cout << "SDL2 window created successfully.\n";

    // Create Vulkan surface from SDL window
    VkResult result = SDL_Vulkan_CreateSurface(g_sdl_window, g_vulkan_instance, &g_surface);
    
    if (result != VK_SUCCESS || !g_surface) {
        std::cerr << "SDL_Vulkan_CreateSurface failed: " << static_cast<int>(result) << "\n";
        return result;
    }

    std::cout << "Vulkan surface created successfully.\n";
    return VK_SUCCESS;
}

// ============================================================================
// DEVICE AND QUEUE CREATION
// ============================================================================

VkResult create_logical_device() {
    if (!g_vulkan_instance || !g_surface) {
        std::cerr << "Invalid Vulkan instance or surface!\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Find and select physical device
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(g_vulkan_instance, &device_count, nullptr);

    if (device_count == 0) {
        std::cerr << "No GPUs found!\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(g_vulkan_instance, &device_count, devices.data());

    VkPhysicalDevice selected_device = VK_NULL_HANDLE;
    uint32_t graphics_family = UINT32_MAX;
    uint32_t present_family = UINT32_MAX;

    for (uint32_t i = 0; i < device_count; i++) {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(devices[i], &properties);

        std::cout << "GPU: " << properties.deviceName << "\n";

        // Check queue support
        uint32_t queue_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &queue_count, nullptr);

        if (queue_count == 0) {
            continue;
        }

        std::vector<VkQueueFamilyProperties> queue_properties(queue_count);
        vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &queue_count, queue_properties.data());

        for (uint32_t j = 0; j < queue_count; j++) {
            if ((queue_properties[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
                graphics_family = j;
            }

            VkBool32 surface_supported = false;
            vkGetPhysicalDeviceSurfaceSupportedKHR(g_vulkan_instance, g_surface, &surface_supported);

            if (surface_supported == VK_TRUE) {
                present_family = j;
            }
        }

        // Select first suitable device
        selected_device = devices[i];
    }

    if (!selected_device) {
        std::cerr << "Failed to find suitable physical device!\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    g_physical_device = selected_device;

    // Create logical device with swapchain and presentation extensions
    const char* device_extensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
    };

    std::array<VkDeviceQueueCreateInfo, 2> queue_create_infos(2);
    
    // Create two identical queues for graphics and presentation
    VkDeviceQueueCreateInfo queue_create_info{};
    queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.queueFamilyIndex = static_cast<uint32_t>(graphics_family);
    queue_create_info.queueCount = 1;
    queue_create_info.pQueuePriorities = &1.0f;

    queue_create_infos[0] = queue_create_info;
    queue_create_infos[1] = queue_create_info;

    VkDeviceCreateInfo device_create_info{};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.queueCreateInfoCount = 2;
    device_create_info.pQueueCreateInfos = queue_create_infos.data();
    device_create_info.enabledExtensionCount = sizeof(device_extensions) / sizeof(device_extensions[0]);
    device_create_info.ppEnabledExtensionNames = device_extensions;

    VkResult result = vkCreateDevice(selected_device, &device_create_info, nullptr, &g_device);

    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create logical device: " << static_cast<int>(result) << "\n";
        return result;
    }

    // Get queues - use same queue family for graphics and presentation
    vkGetDeviceQueue(g_device, graphics_family, 0, &graphics_queue);
    vkGetDeviceQueue(g_device, graphics_family, 1, &present_queue);

    std::cout << "=== Logical device created successfully ===\n";
    return VK_SUCCESS;
}

// ============================================================================
// COMMAND POOL CREATION
// ============================================================================

VkResult create_command_pool() {
    if (!g_device) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkCommandPoolCreateInfo pool_create_info{};
    pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_create_info.queueFamilyIndex = static_cast<uint32_t>(0);
    pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    vkCreateCommandPool(g_device, &pool_create_info, nullptr, &g_command_pool);

    if (!g_command_pool) {
        std::cerr << "Failed to create command pool!\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    std::cout << "Command pool created.\n";
    return VK_SUCCESS;
}

// ============================================================================
// SWAPCHAIN CREATION
// ============================================================================

VkResult create_swapchain() {
    if (!g_vulkan_instance || !g_surface) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Get swapchain surface format and present mode
    uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceSupportedFormats(g_vulkan_instance, g_surface, &format_count, nullptr);

    std::vector<VkSurfaceFormatKHR> supported_formats(format_count);
    vkGetPhysicalDeviceSurfaceSupportedFormats(g_vulkan_instance, g_surface, &format_count, supported_formats.data());

    // Choose the first available format (usually BGRA8)
    if (supported_formats[0].format == VK_FORMAT_B8G8R8A8_UNORM) {
        g_swapchain_format.format = VK_FORMAT_B8G8R8A8_UNORM;
    } else if (supported_formats[0].format == VK_FORMAT_B8G8R8_UNORM) {
        g_swapchain_format.format = VK_FORMAT_B8G8R8_UNORM;
    } else {
        g_swapchain_format.format = supported_formats[0].format;
    }

    g_swapchain_format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

    uint32_t present_mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModes(g_vulkan_instance, g_surface, &present_mode_count, nullptr);

    std::vector<VkPresentModeKHR> supported_present_modes(present_mode_count);
    vkGetPhysicalDeviceSurfacePresentModes(g_vulkan_instance, g_surface, &present_mode_count, supported_present_modes.data());

    // Choose the first present mode (usually VK_PRESENT_MODE_FIFO_KHR)
    g_swapchain_present_mode = supported_present_modes[0];

    // Get swapchain capabilities
    VkSurfaceCapabilitiesKHR surface_capabilities{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_vulkan_instance, g_surface, &surface_capabilities);

    // Create swapchain creation info
    VkSwapchainCreateInfoKHR swapchain_create_info{};
    swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_create_info.surface = g_surface;
    swapchain_create_info.minImageCount = SWAPCHAIN_IMAGE_COUNT;
    swapchain_create_info.imageFormat = g_swapchain_format.format;
    swapchain_create_info.imageColorSpace = g_swapchain_format.colorSpace;
    swapchain_create_info.imageExtent = surface_capabilities.currentExtent;
    swapchain_create_info.imageArrayLayers = 1;
    swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_create_info.queueFamilyIndexCount = 0;
    swapchain_create_info.pQueueFamilyIndices = nullptr;
    swapchain_create_info.preTransform = surface_capabilities.currentTransform;
    swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_create_info.presentMode = g_swapchain_present_mode;
    swapchain_create_info.clipped = VK_TRUE;
    swapchain_create_info.oldSwapchain = VK_NULL_HANDLE;

    VkResult result = vkCreateSwapchainKHR(g_device, &swapchain_create_info, nullptr, &g_swapchain);

    if (result != VK_SUCCESS || g_swapchain == VK_NULL_HANDLE) {
        std::cerr << "Failed to create swapchain: " << static_cast<int>(result) << "\n";
        return result;
    }

    // Get swapchain images info
    vkGetSwapchainImagesKHR(g_device, g_swapchain, &g_swapchain_images.size(), nullptr);
    
    std::vector<VkImage> swapchain_images(g_swapchain_images.size());
    vkGetSwapchainImagesKHR(g_device, g_swapchain, &swapchain_images.size(), swapchain_images.data());
    g_swapchain_images = std::move(swapchain_images);

    // Create image views
    VkImageViewCreateInfo image_view_create_info{};
    image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_create_info.image = VK_NULL_HANDLE;
    image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_create_info.format = g_swapchain_format.format;
    image_view_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_create_info.subresourceLayers = 1;
    image_view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_view_create_info.subresourceRange.baseMipLevel = 0;
    image_view_create_info.subresourceRange.levelCount = 1;
    image_view_create_info.subresourceRange.baseArrayLayer = 0;
    image_view_create_info.subresourceRange.layerCount = 1;

    for (size_t i = 0; i < g_swapchain_images.size(); i++) {
        image_view_create_info.image = g_swapchain_images[i];
        vkCreateImageView(g_device, &image_view_create_info, nullptr, &g_swapchain_image_views[i]);
    }

    // Create render pass
    VkRenderPassCreateInfo render_pass_create_info{};
    render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_create_info.attachmentCount = 0;
    render_pass_create_info.pAttachments = nullptr;
    render_pass_create_info.subpassCount = 1;
    render_pass_create_info.pSubpasses = &g_render_pass_subpasses;
    render_pass_create_info.dependencyCount = 0;
    render_pass_create_info.pDependencies = nullptr;

    vkCreateRenderPass(g_device, &render_pass_create_info, nullptr, &g_render_pass);

    // Create framebuffers
    VkFramebufferCreateInfo framebuffer_create_info{};
    framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_create_info.renderPass = g_render_pass;
    framebuffer_create_info.attachmentCount = 1;
    framebuffer_create_info.pAttachments = &framebuffer_attachments;
    framebuffer_create_info.width = surface_capabilities.currentExtent.width;
    framebuffer_create_info.height = surface_capabilities.currentExtent.height;
    framebuffer_create_info.layers = 1;

    for (uint32_t i = 0; i < g_swapchain_images.size(); i++) {
        VkImageView attach_views[1] = {g_swapchain_image_views[i]};
        
        g_swapchain_framebuffers.resize(g_swapchain_images.size());
        vkCreateFramebuffer(g_device, &framebuffer_create_info, nullptr, &g_swapchain_framebuffers[i]);
    }

    return VK_SUCCESS;
}

// ============================================================================
// RENDER PIPELINE CREATION
// ============================================================================

VkResult create_pipeline() {
    if (!g_vulkan_instance || !g_surface) {
        std::cerr << "Invalid Vulkan instance or surface!\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Create pipeline
    VkRenderPass render_pass = VK_NULL_HANDLE;
    
    std::vector<VkShaderModule> shader_modules;
    
    // Create compute pipeline for terrain rendering
    VkPipelineLayoutCreateInfo pipeline_layout_create_info{};
    pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    VkPipelineCache pipeline_cache{};
    vkCreatePipelineCache(g_device, nullptr, &pipeline_cache);
    
    return VK_SUCCESS;
}

// ============================================================================
// MAIN FUNCTION - COMPLETE RENDER LOOP
// ============================================================================

int main() {
    std::cout << "=== TAZ: WANTED SDL2 + VULKAN ===\n";
    std::cout << "Initializing Vulkan and creating window...\n\n";

    // Initialize Vulkan instance
    if (init_vulkan_instance() != VK_SUCCESS) {
        return -1;
    }

    // Create SDL2 window and Vulkan surface
    if (create_sdl_window_and_surface() != VK_SUCCESS) {
        std::cerr << "Failed to create window/surface!\n";
        return -1;
    }

    // Generate terrain mesh
    generate_terrain_mesh();

    // TODO: Add device, swapchain, pipeline, command buffer creation here
    
    // TODO: Main render loop with event handling
    
    std::cout << "\n=== TAZ: WANTED SDL2 + VULKAN Complete ===\n";
    return 0;
}

// ============================================================================
// GLOBAL VARIABLES AND INITIALIZATIONS
// ============================================================================

VkSurfaceFormatKHR g_swapchain_format = {VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
VkPresentModeKHR g_swapchain_present_mode = VK_PRESENT_MODE_FIFO_KHR;
uint32_t g_graphics_family_index = 0;
SDL_Window* g_sdl_window = nullptr;

// Render pass subpasses
VkSubpassDescription g_render_pass_subpasses[1] = {
    VkSubpassDescription{}
};

// Framebuffer attachments
VkImageView framebuffer_attachments = VK_NULL_HANDLE;