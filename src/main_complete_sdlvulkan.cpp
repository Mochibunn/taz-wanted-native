// Taz Wanted - SDL2 + Vulkan Full Render Pass & Clear Screen Engine
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include <iostream>
#include <vector>
#include <cmath>
#include <stdexcept>
#include <algorithm>

struct Vertex {
    float position[3];
    float normal[3];
    float texCoord[2];
};

static std::vector<Vertex> terrain_vertices;

// Global Vulkan Handles
static VkInstance g_vulkan_instance = VK_NULL_HANDLE;
static VkSurfaceKHR g_surface = VK_NULL_HANDLE;
static VkPhysicalDevice g_physical_device = VK_NULL_HANDLE;
static VkDevice g_device = VK_NULL_HANDLE;
static VkQueue g_graphics_queue = VK_NULL_HANDLE;
static VkSwapchainKHR g_swapchain = VK_NULL_HANDLE;
static std::vector<VkImage> g_swapchain_images;
static VkFormat g_swapchain_image_format = VK_FORMAT_B8G8R8A8_UNORM;
static VkExtent2D g_swapchain_extent = {800, 600};

static std::vector<VkImageView> g_swapchain_image_views;
static VkRenderPass g_render_pass = VK_NULL_HANDLE;
static std::vector<VkFramebuffer> g_swapchain_framebuffers;
static VkCommandPool g_command_pool = VK_NULL_HANDLE;
static VkCommandBuffer g_command_buffer = VK_NULL_HANDLE;

uint32_t generate_terrain_vertices() {
    const float width = 10.0f;
    const int num_tiles_x = 8;
    
    terrain_vertices.clear();
    float step_x = width / static_cast<float>(num_tiles_x);
    
    for (int i = 0; i < num_tiles_x; i++) {
        float x = -width / 2.0f + step_x * i;
        
        Vertex v1;
        v1.position[0] = x;
        v1.position[1] = std::sin(x) * 0.5f;
        v1.position[2] = 0.0f;
        
        terrain_vertices.push_back(v1);
    }
    
    return terrain_vertices.size();
}

bool init_vulkan_context(SDL_Window* window) {
    // 1. Get required SDL Vulkan extensions
    unsigned int extension_count = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(window, &extension_count, nullptr)) {
        std::cerr << "Failed to get SDL Vulkan extension count: " << SDL_GetError() << "\n";
        return false;
    }

    std::vector<const char*> extensions(extension_count);
    if (!SDL_Vulkan_GetInstanceExtensions(window, &extension_count, extensions.data())) {
        std::cerr << "Failed to get SDL Vulkan extensions: " << SDL_GetError() << "\n";
        return false;
    }

    // 2. Create Vulkan Instance
    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Taz Wanted Native";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "TazEngine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    create_info.ppEnabledExtensionNames = extensions.data();
    create_info.enabledLayerCount = 0;

    if (vkCreateInstance(&create_info, nullptr, &g_vulkan_instance) != VK_SUCCESS) {
        std::cerr << "Failed to create Vulkan instance!\n";
        return false;
    }
    std::cout << "✅ Vulkan Instance successfully created!\n";

    // 3. Create Window Surface via SDL
    if (!SDL_Vulkan_CreateSurface(window, g_vulkan_instance, &g_surface)) {
        std::cerr << "Failed to create Vulkan window surface: " << SDL_GetError() << "\n";
        return false;
    }
    std::cout << "✅ Vulkan Window Surface successfully created!\n";

    // 4. Select Physical Device (GPU)
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(g_vulkan_instance, &device_count, nullptr);
    if (device_count == 0) {
        std::cerr << "Failed to find GPUs with Vulkan support!\n";
        return false;
    }

    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(g_vulkan_instance, &device_count, devices.data());
    g_physical_device = devices[0];
    std::cout << "✅ Physical GPU selected successfully!\n";

    // 5. Create Logical Device & Queue with Swapchain Extension
    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_create_info = {};
    queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.queueFamilyIndex = 0;
    queue_create_info.queueCount = 1;
    queue_create_info.pQueuePriorities = &queue_priority;

    const std::vector<const char*> device_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    VkDeviceCreateInfo device_create_info = {};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.queueCreateInfoCount = 1;
    device_create_info.pQueueCreateInfos = &queue_create_info;
    device_create_info.enabledExtensionCount = static_cast<uint32_t>(device_extensions.size());
    device_create_info.ppEnabledExtensionNames = device_extensions.data();
    device_create_info.enabledLayerCount = 0;

    if (vkCreateDevice(g_physical_device, &device_create_info, nullptr, &g_device) != VK_SUCCESS) {
        std::cerr << "Failed to create logical Vulkan device!\n";
        return false;
    }

    vkGetDeviceQueue(g_device, 0, 0, &g_graphics_queue);
    std::cout << "✅ Logical Vulkan Device & Queue initialized successfully!\n";

    // 6. Create Swapchain
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_physical_device, g_surface, &capabilities);

    uint32_t image_count = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount) {
        image_count = capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR swapchain_create_info = {};
    swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_create_info.surface = g_surface;
    swapchain_create_info.minImageCount = image_count;
    swapchain_create_info.imageFormat = g_swapchain_image_format;
    swapchain_create_info.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    swapchain_create_info.imageExtent = g_swapchain_extent;
    swapchain_create_info.imageArrayLayers = 1;
    swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_create_info.preTransform = capabilities.currentTransform;
    swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_create_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapchain_create_info.clipped = VK_TRUE;
    swapchain_create_info.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(g_device, &swapchain_create_info, nullptr, &g_swapchain) != VK_SUCCESS) {
        std::cerr << "Failed to create Vulkan swapchain!\n";
        return false;
    }

    uint32_t actual_image_count = 0;
    vkGetSwapchainImagesKHR(g_device, g_swapchain, &actual_image_count, nullptr);
    g_swapchain_images.resize(actual_image_count);
    vkGetSwapchainImagesKHR(g_device, g_swapchain, &actual_image_count, g_swapchain_images.data());

    std::cout << "✅ Vulkan Swapchain successfully created with " << actual_image_count << " images!\n";

    // 7. Create Image Views
    g_swapchain_image_views.resize(g_swapchain_images.size());
    for (size_t i = 0; i < g_swapchain_images.size(); i++) {
        VkImageViewCreateInfo view_info = {};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = g_swapchain_images[i];
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = g_swapchain_image_format;
        view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT
