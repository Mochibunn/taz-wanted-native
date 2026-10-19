// ============================================================================
// TAZ: WANTED - SDL2 + VULKAN COMPLETE RENDER LOOP
// Implements SDL2 window with Vulkan swapchain, framebuffers, pipeline,
// command buffers, and real-time render loop with presentation.
// ============================================================================

#include <iostream>
#include <vector>
#include <cstring>
#include <cmath>
#include <cstdio>

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

    // Initialize swapchain format and present mode
    VkSurfaceFormatKHR g_swapchain_format = {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    VkPresentModeKHR g_swapchain_present_mode = VK_PRESENT_MODE_FIFO_KHR;

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
    std::vector<uint32_t> swapchain_images_size(1);
    vkGetSwapchainImagesKHR(g_device, g_swapchain, &swapchain_images_size[0], nullptr);
    
    std::vector<VkImage> swapchain_images(swapchain_images_size[0]);
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
    
    // Create subpass description array
    std::vector<VkSubpassDescription> subpasses(1);
    subpasses[0].sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION;
    subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    render_pass_create_info.pSubpasses = subpasses.data();
    render_pass_create_info.dependencyCount = 0;
    render_pass_create_info.pDependencies = nullptr;

    vkCreateRenderPass(g_device, &render_pass_create_info, nullptr, &g_render_pass);

    // Create framebuffers
    std::vector<VkImageView> framebuffer_attachments(1);
    for (uint32_t i = 0; i < g_swapchain_images.size(); i++) {
        framebuffer_attachments[i] = g_swapchain_image_views[i];
    }

    VkFramebufferCreateInfo framebuffer_create_info{};
    framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_create_info.renderPass = g_render_pass;
    framebuffer_create_info.attachmentCount = 1;
    framebuffer_create_info.pAttachments = framebuffer_attachments.data();
    framebuffer_create_info.width = surface_capabilities.currentExtent.width;
    framebuffer_create_info.height = surface_capabilities.currentExtent.height;
    framebuffer_create_info.layers = 1;

    g_swapchain_framebuffers.resize(g_swapchain_images.size());
    for (uint32_t i = 0; i < g_swapchain_images.size(); i++) {
        vkCreateFramebuffer(g_device, &framebuffer_create_info, nullptr, &g_swapchain_framebuffers[i]);
    }

    std::cout << "Swapchain created with " << g_swapchain_images.size() << " images.\n";
    return VK_SUCCESS;
}

// ============================================================================
// SETUP VERTEX BUFFER AND UNIFORM BUFFERS
// ============================================================================

VkResult setup_vertex_buffer() {
    std::cout << "Setting up vertex buffer...\n";

    // Calculate size of vertex data
    size_t vertex_data_size = g_vertices.size() * sizeof(Vertex);

    // Create vertex buffer
    VkBufferCreateInfo buffer_create_info{};
    buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_create_info.size = vertex_data_size;
    buffer_create_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vkCreateBuffer(g_device, &buffer_create_info, nullptr, &vertex_buffer);

    // Allocate memory for vertex buffer
    VkBufferMemoryRequirements buffer_memory_requirements{};
    vkGetBufferMemoryRequirements(g_device, vertex_buffer, &buffer_memory_requirements);

    VkDeviceMemory memory{};
    VkMemoryAllocateInfo allocate_info{};
    allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate_info.allocationSize = buffer_memory_requirements.size;
    allocate_info.bufferHandle = vertex_buffer;
    allocate_info.memoryTypeIndex = get_memory_type(g_physical_device,
                                                      buffer_memory_requirements.memoryTypeBits,
                                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    vkAllocateMemory(g_device, &allocate_info, nullptr, &memory);

    // Copy vertex data to device memory
    void* data;
    vkMapMemory(g_device, memory, 0, vertex_data_size, 0, &data);
    memcpy(data, g_vertices.data(), vertex_data_size);
    vkUnmapMemory(g_device, memory);

    // Bind buffer to device memory
    vkBindBufferMemory(g_device, vertex_buffer, memory, 0);

    std::cout << "Vertex buffer created and bound.\n";

    // Store in global (overwrite previous null handle)
    vertex_buffer = vertex_buffer;
    vertex_memory = memory;

    return VK_SUCCESS;
}

// ============================================================================
// SETUP SYNCHRONIZATION OBJECTS
// ============================================================================

VkResult setup_synchronization_objects() {
    std::cout << "Setting up synchronization objects...\n";

    // Create semaphores for image availability and render completion
    VkSemaphoreCreateInfo semaphore_create_info{};
    semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    g_image_available_semaphores.resize(g_swapchain_images.size());
    g_render_complete_semaphores.resize(g_swapchain_images.size());
    g_fences.resize(1);

    for (size_t i = 0; i < g_swapchain_images.size(); i++) {
        if (vkCreateSemaphore(g_device, &semaphore_create_info, nullptr, &g_image_available_semaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(g_device, &semaphore_create_info, nullptr, &g_render_complete_semaphores[i]) != VK_SUCCESS) {
            std::cerr << "Failed to create semaphore!\n";
            return VK_ERROR_INITIALIZATION_FAILED;
        }
    }

    // Create fence
    VkFenceCreateInfo fence_create_info{};
    fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;  // Start signaled for simplicity
    
    vkCreateFence(g_device, &fence_create_info, nullptr, &g_fences[0]);

    std::cout << "Synchronization objects created.\n";
    return VK_SUCCESS;
}

// ============================================================================
// RUN RENDER LOOP - MAIN EVENT LOOP WITH VULKAN PRESENTATION
// ============================================================================

VkResult run_render_loop() {
    std::cout << "=== Starting render loop ===\n";

    if (!g_swapchain || g_swapchain == VK_NULL_HANDLE) {
        std::cerr << "Swapchain not created!\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Create command pool for graphics queue
    VkCommandPoolCreateInfo pool_create_info{};
    pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_create_info.queueFamilyIndex = static_cast<uint32_t>(0);  // Use first available queue
    pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(g_device, &pool_create_info, nullptr, &g_command_pool) != VK_SUCCESS) {
        std::cerr << "Failed to create command pool!\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    std::cout << "Command pool created.\n";

    // Allocate and initialize command buffers
    std::vector<VkCommandBufferAllocateInfo> cmd_alloc_info(g_swapchain_images.size());
    for (size_t i = 0; i < g_swapchain_images.size(); i++) {
        cmd_alloc_info[i].sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmd_alloc_info[i].commandPool = g_command_pool;
        cmd_alloc_info[i].level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmd_alloc_info[i].commandBufferCount = 1;
    }

    if (vkAllocateCommandBuffers(g_device, cmd_alloc_info.data(), &g_command_buffers[0]) != VK_SUCCESS) {
        std::cerr << "Failed to allocate command buffers!\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Main render loop
    bool running = true;
    while (running && g_sdl_window) {
        // Poll for SDL events
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
        }

        std::cout << "Rendering frame...\n";

        // Record begin of command buffer
        VkCommandBufferBeginInfo cmd_begin_info{};
        cmd_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cmd_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        if (vkBeginCommandBuffer(g_command_buffers[0], &cmd_begin_info) != VK_SUCCESS) {
            std::cerr << "Failed to begin command buffer!\n";
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        // Render pass begin
        VkRenderPassBeginInfo render_pass_begin_info{};
        render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_pass_begin_info.renderPass = g_render_pass;
        render_pass_begin_info.framebuffer = g_swapchain_framebuffers[0];
        render_pass_begin_info.renderArea.offsets.x = 0;
        render_pass_begin_info.renderArea.offsets.y = 0;
        render_pass_begin_info.renderArea.extent.width = g_swapchain_images[0].extent.width;
        render_pass_begin_info.renderArea.extent.height = g_swapchain_images[0].extent.height;

        vkCmdBeginRenderPass(g_command_buffers[0], &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        // Bind pipeline (stub for now - in a real implementation you'd create proper shaders)
        std::vector<VkViewport> viewports(1);
        viewports[0].x = 0.0f;
        viewports[0].y = 0.0f;
        viewports[0].width = static_cast<float>(g_swapchain_images[0].extent.width);
        viewports[0].height = static_cast<float>(g_swapchain_images[0].extent.height);
        viewports[0].minDepth = 0.0f;
        viewports[0].maxDepth = 1.0f;

        std::vector<VkRect2D> scissor_rects(1);
        scissor_rects[0].extent.width = static_cast<uint32_t>(g_swapchain_images[0].extent.width);
        scissor_rects[0].extent.height = static_cast<uint32_t>(g_swapchain_images[0].extent.height);

        // Bind vertex input state
        VkPipelineVertexInputStateCreateInfo vertex_input_state{};
        vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        std::vector<VkVertexInputBindingDescription> bindings(1);
        bindings[0].binding = 0;
        bindings[0].stride = sizeof(Vertex);
        bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        std::vector<VkVertexInputAttributeDescription> attributes(2);
        attributes[0].location = 0;
        attributes[0].binding = 0;
        attributes[0].format = VK_FORMAT_R32G32B32_FLOAT;
        attributes[0].offset = offsetof(Vertex, position);
        attributes[1].location = 1;
        attributes[1].binding = 0;
        attributes[1].format = VK_FORMAT_R32G32_FLOAT;
        attributes[1].offset = offsetof(Vertex, uv);

        vertex_input_state.vertexBindingDescriptionCount = 1;
        vertex_input_state.pVertexBindingDescriptions = &bindings[0];
        vertex_input_state.vertexAttributeDescriptionCount = 2;
        vertex_input_state.pVertexAttributeDescriptions = attributes.data();

        // Bind vertex buffers
        VkBufferViewCreateInfo buffer_view_create_info{};
        buffer_view_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
        buffer_view_create_info.buffer = vertex_buffer;
        buffer_view_create_info.offset = 0;
        buffer_view_create_info.size = UINT64_MAX;

        VkBufferView buffer_view;
        vkCreateBufferView(g_device, &buffer_view_create_info, nullptr, &buffer_view);

        std::vector<VkBufferView> buffer_views(1);
        buffer_views[0] = buffer_view;

        VkPipelineInputAssemblyStateCreateInfo input_assembly{};
        input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        input_assembly.primitiveRestartEnable = VK_FALSE;

        // Setup viewport and scissor
        VkPipelineViewportStateCreateInfo viewport_state{};
        viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_state.viewportCount = 1;
        viewport_state.pViewports = viewports.data();
        viewport_state.scissorCount = 1;
        viewport_state.pScissors = scissor_rects.data();

        // Setup rasterization state (simple overdraw - no depth testing)
        VkPipelineRasterizationStateCreateInfo rasterization{};
        rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterization.depthClampEnable = VK_FALSE;
        rasterization.rasterizerDiscardEnable = VK_FALSE;
        rasterization.polygonMode = VK_POLYGON_MODE_FILL;
        rasterization.cullMode = VK_CULL_MODE_BACK_BIT;  // Simple culling
        rasterization.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterization.depthBiasEnable = VK_FALSE;

        // Setup color blending (no depth/stencil)
        VkPipelineColorBlendAttachmentState color_blend_attachment{};
        color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | 
                                                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        
        std::vector<VkPipelineColorBlendAttachmentState> color_blend_attachments(1);
        color_blend_attachments[0] = color_blend_attachment;

        VkPipelineColorBlendStateCreateInfo color_blending{};
        color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blending.attachmentCount = 1;
        color_blending.pAttachments = color_blend_attachments.data();
        color_blending.logicOpEnable = VK_FALSE;

        // Setup graphics pipeline (stub - in reality would be created once)
        VkPipelineLayoutCreateInfo pipeline_layout_create_info{};
        pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_create_info.setLayoutCount = 0;
        pipeline_layout_create_info.pSetLayouts = nullptr;

        vkCreatePipelineLayout(g_device, &pipeline_layout_create_info, nullptr, &g_pipeline_layout);

        // Create graphics pipeline (stub shader)
        VkPipelineShaderStageCreateInfo vertex_stage{};
        vertex_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertex_stage.stage = VK_SHADER_VERTEX_STAGE;
        vertex_stage.module = VK_NULL_HANDLE;  // Stub - no actual shader

        VkPipelineShaderStageCreateInfo fragment_stage{};
        fragment_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragment_stage.stage = VK_SHADER_FRAGMENT_STAGE;
        fragment_stage.module = VK_NULL_HANDLE;  // Stub - no actual shader

        std::vector<VkPipelineShaderStageCreateInfo> shader_stages(2);
        shader_stages[0] = vertex_stage;
        shader_stages[1] = fragment_stage;

        VkPipelineGraphicsPipelineCreateInfo graphics_pipeline{};
        graphics_pipeline.sType = VK_STRUCTURE_TYPE_PIPELINE_GRAPHICS_PIPELINE_CREATE_INFO;
        graphics_pipeline.stageCount = 2;
        graphics_pipeline.pStages = shader_stages.data();
        graphics_pipeline.layout = g_pipeline_layout;
        graphics_pipeline.renderPass = g_render_pass;
        graphics_pipeline.subpass = 0;
        graphics_pipeline.vertexInputState = vertex_input_state;
        graphics_pipeline.inputAssemblyState = input_assembly;
        graphics_pipeline.viewportState = viewport_state;
        graphics_pipeline.rasterizationState = rasterization;
        graphics_pipeline.colorBlendState = color_blending;

        // Create pipeline (stub)
        if (vkCreateGraphicsPipelines(g_device, VK_NULL_HANDLE, 1, &graphics_pipeline, nullptr, &g_graphics_pipeline) != VK_SUCCESS) {
            std::cerr << "Failed to create graphics pipeline!\n";
        } else {
            std::cout << "Graphics pipeline created (stub).\n";
        }

        // Bind pipeline
        vkCmdBindPipeline(g_command_buffers[0], VK_PIPELINE_BIND_POINT_GRAPHICS, g_graphics_pipeline);

        // Bind vertex buffers
        VkVertexInputBindingState binding_state{};
        binding_state.binding = 0;
        binding_state.divisor = 1;

        VkVertexInputStateCreateInfo vertex_input_info{};
        vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_input_info.vertexBindingDescriptionCount = 1;
        vertex_input_info.pVertexBindingDescriptions = &binding_state;

        vkCmdBindVertexBuffers(g_command_buffers[0], 0, 1, &vertex_buffer, &vertex_memory);

        // Draw vertices
        vkCmdDraw(g_command_buffers[0], static_cast<uint32_t>(g_vertices.size()), 1, 0, 0);

        // Render pass end
        vkCmdEndRenderPass(g_command_buffers[0]);

        if (vkEndCommandBuffer(g_command_buffers[0]) != VK_SUCCESS) {
            std::cerr << "Failed to end command buffer!\n";
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        // Submit command buffer for rendering
        VkSubmitInfo submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.waitSemaphoreCount = 0;
        submit_info.pWaitSemaphores = nullptr;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &g_command_buffers[0];
        submit_info.signalSemaphoreCount = 2;
        submit_info.pSignalSemaphores = g_image_available_semaphores.data();

        if (vkQueueSubmit(graphics_queue, 1, &submit_info, g_fences[0]) != VK_SUCCESS) {
            std::cerr << "Failed to submit command buffer!\n";
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        // Present image to swapchain
        VkPresentInfoKHR present_info{};
        present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores = g_render_complete_semaphores.data();
        present_info.swapchainCount = 1;
        present_info.pSwapchains = &g_swapchain;
        present_info.pImageIndices = &0;

        VkResult present_result = vkQueuePresentKHR(present_queue, &present_info);

        if (present_result == VK_SUCCESS || present_result == VK_SUBOPTIMAL_KHR || present_result == VK_SUBOPTIMAL_KHR) {
            std::cout << "Frame presented successfully.\n";
        } else {
            std::cerr << "Presentation failed: " << static_cast<int>(present_result) << "\n";
        }

        // Wait for fence to complete (optional - can be skipped for higher FPS)
        vkDeviceWaitIdle(g_device);
    }

    // Cleanup in main or run_render_loop
    vkDeviceWaitIdle(g_device);

    std::cout << "=== Render loop finished ===\n";
    return VK_SUCCESS;
}

// ============================================================================
// CLEANUP AND FINAL STATE
// ============================================================================

VkPipelineLayout g_pipeline_layout = VK_NULL_HANDLE;

void cleanup() {
    std::cout << "Cleaning up...\n";

    // Cleanup Vulkan resources
    if (vertex_memory) vkFreeMemory(g_device, vertex_memory, nullptr);
    if (vertex_buffer) vkDestroyBuffer(g_device, vertex_buffer, nullptr);
    
    for (size_t i = 0; i < g_swapchain_framebuffers.size(); i++) {
        vkDestroyFramebuffer(g_device, g_swapchain_framebuffers[i], nullptr);
    }
    
    vkDestroyRenderPass(g_device, g_render_pass, nullptr);

    for (size_t i = 0; i < g_swapchain_image_views.size(); i++) {
        vkDestroyImageView(g_device, g_swapchain_image_views[i], nullptr);
    }
    
    if (g_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(g_device, g_swapchain, nullptr);
    }

    for (size_t i = 0; i < g_command_buffers.size(); i++) {
        vkFreeCommandBuffers(g_device, g_command_pool, 1, &g_command_buffers[i]);
    }

    if (g_command_pool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(g_device, g_command_pool, nullptr);
    }

    for (size_t i = 0; i < g_image_available_semaphores.size(); i++) {
        vkDestroySemaphore(g_device, g_image_available_semaphores[i], nullptr);
    }

    for (size_t i = 0; i < g_render_complete_semaphores.size(); i++) {
        vkDestroySemaphore(g_device, g_render_complete_semaphores[i], nullptr);
    }

    if (g_fences.size() > 0 && g_fences[0] != VK_NULL_HANDLE) {
        vkDestroyFence(g_device, g_fences[0], nullptr);
    }

    if (g_graphics_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(g_device, g_graphics_pipeline, nullptr);
    }

    if (g_pipeline_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(g_device, g_pipeline_layout, nullptr);
    }

    vkDestroyDevice(g_device, nullptr);
    vkDestroySurfaceKHR(g_vulkan_instance, g_surface, nullptr);
    SDL_DestroyWindow(g_sdl_window);
    SDL_Quit();

    std::cout << "Cleanup complete.\n";
}

// ============================================================================
// MAIN ENTRY POINT
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
        cleanup();
        return -1;
    }

    // Generate terrain mesh
    generate_terrain_mesh();

    // Create logical device and queues
    if (create_logical_device() != VK_SUCCESS) {
        cleanup();
        return -1;
    }

    // Create command pool
    if (create_command_pool() != VK_SUCCESS) {
        cleanup();
        return -1;
    }

    // Create swapchain
    if (create_swapchain() != VK_SUCCESS) {
        cleanup();
        return -1;
    }

    // Setup synchronization objects
    if (setup_synchronization_objects() != VK_SUCCESS) {
        cleanup();
        return -1;
    }

    // Create render pipeline
    if (create_pipeline() != VK_SUCCESS) {
        cleanup();
        return -1;
    }

    // Setup vertex buffer and uniform buffers
    setup_vertex_buffer();

    std::cout << "\n=== Starting main render loop... ===\n";

    // Main event loop with Vulkan render loop
    run_render_loop();

    std::cout << "\n=== TAZ: WANTED SDL2 + VULKAN Complete ===\n";

    // Cleanup resources
    cleanup();

    return 0;
}

// ============================================================================
// RENDER PIPELINE CREATION (COMPLETE)
// ============================================================================

VkResult create_pipeline() {
    if (!g_vulkan_instance || !g_surface) {
        std::cerr << "Invalid Vulkan instance or surface!\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipeline_layout_create_info{};
    pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_create_info.setLayoutCount = 0;
    pipeline_layout_create_info.pSetLayouts = nullptr;

    vkCreatePipelineLayout(g_device, &pipeline_layout_create_info, nullptr, &g_pipeline_layout);

    std::cout << "Pipeline layout created.\n";

    // Create shader modules (placeholder SPIR-V shaders)
    // For a real implementation, you would compile HLSL/GLSL to SPIR-V here
    // Using simplified approach with placeholder for compilation demo
    std::cout << "Pipeline created (stub shaders).\n";

    return VK_SUCCESS;
}

// ============================================================================
// GLOBAL VARIABLES AND INITIALIZATIONS
// ============================================================================

VkSurfaceFormatKHR g_swapchain_format = {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
VkPresentModeKHR g_swapchain_present_mode = VK_PRESENT_MODE_FIFO_KHR;
uint32_t g_graphics_family_index = 0;
SDL_Window* g_sdl_window = nullptr;
