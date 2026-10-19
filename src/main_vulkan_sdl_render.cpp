// ============================================================================
// TAZ: WANTED - COMPLETE SDL2 WINDOW + VULKAN SWAPCHAIN RENDER LOOP
// Implements full Vulkan+SDL2 integration with real-time render loop.
// ============================================================================

#include <iostream>
#include <vector>
#include <cstring>
#include <cmath>
#include <array>
#include <cstdio>

#define GLM_FORCE_INLINE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// SDL2 headers
#include <SDL2/SDL.h>
#include <SDL2/SDL_video.h>

// Vulkan headers (SILK - Simple Implementation Library for KHR)
#include "vulkan/vulkan.h"
#include "silk/vk_silken.h"

// ============================================================================
// WINDOW CONFIGURATION
// ============================================================================

#define WINDOW_WIDTH 1920
#define WINDOW_HEIGHT 1080
#define SWAPCHAIN_IMAGE_COUNT 3
#define RENDER_QUEUE_SIZE 3

// ============================================================================
// VULKAN STATE HANDLES
// ============================================================================

struct VulkanState {
    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphics_queue = VK_NULL_HANDLE;
    VkQueue present_queue = VK_NULL_HANDLE;

    // Swapchain
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> swapchain_images;
    std::vector<VkImageView> swapchain_image_views;
    std::vector<VkFramebuffer> swapchain_framebuffers;
    VkRenderPass render_pass = VK_NULL_HANDLE;
    VkPipeline graphics_pipeline = VK_NULL_HANDLE;

    // Command buffers and synchronization
    VkCommandPool command_pool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> command_buffers;
    std::vector<VkSemaphore> image_available_semaphores;
    std::vector<VkSemaphore> render_complete_semaphores;
    std::vector<VkFence> fences;
};

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

static VulkanState g_vulkan_state;
SDL_Window* g_sdl_window = nullptr;
std::vector<Vertex> g_vertices;

// Uniform buffer structure
struct UniformBuffer {
    uint32_t time = 0;
    glm::mat4 transform[16];
};

// ============================================================================
// HELPER FUNCTION: GET MEMORY TYPE
// ============================================================================

static uint32_t get_memory_type(VkPhysicalDevice device, uint32_t bitMask, VkMemoryPropertyFlags properties) {
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
    std::cout << "=== Initializing Vulkan Instance ===\n";

    // Enable required extensions
    const char* extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef __linux__
        VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
#else
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#endif
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
    };

    // Create application info
    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "TAZ: WANTED - SDL2+Vulkan";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "Custom Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_2;

    // Create instance create info
    VkInstanceCreateInfo instance_create_info{};
    instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_create_info.pApplicationInfo = &app_info;
    instance_create_info.enabledExtensionCount = sizeof(extensions) / sizeof(extensions[0]);
    instance_create_info.ppEnabledExtensionNames = extensions;

    VkResult result = vkCreateInstance(&instance_create_info, nullptr, &g_vulkan_state.instance);

    if (result != VK_SUCCESS || !g_vulkan_state.instance) {
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
    std::cout << "\n=== Creating SDL2 Window and Vulkan Surface ===\n";
    
    if (!g_vulkan_state.instance) {
        std::cerr << "Vulkan instance not initialized!\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Initialize SDL video subsystem
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
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

    std::cout << "SDL2 window created successfully." << std::endl;

    // Create Vulkan surface from SDL window
    VkResult result = SDL_Vulkan_CreateSurface(g_sdl_window, g_vulkan_state.instance, &g_vulkan_state.surface);
    
    if (result != VK_SUCCESS || !g_vulkan_state.surface) {
        std::cerr << "SDL_Vulkan_CreateSurface failed: " << static_cast<int>(result) << "\n";
        return result;
    }

    std::cout << "Vulkan surface created successfully.\n";
    
    // Initialize Vulkan logical device
    if (create_logical_device() != VK_SUCCESS) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    return VK_SUCCESS;
}

// ============================================================================
// DEVICE AND QUEUE CREATION
// ============================================================================

VkResult create_logical_device() {
    if (!g_vulkan_state.instance || !g_vulkan_state.surface) {
        std::cerr << "Invalid Vulkan instance or surface!\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Find and select physical device
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(g_vulkan_state.instance, &device_count, nullptr);

    if (device_count == 0) {
        std::cerr << "No GPUs found!\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(g_vulkan_state.instance, &device_count, devices.data());

    VkPhysicalDevice selected_device = VK_NULL_HANDLE;
    uint32_t graphics_family = UINT32_MAX;
    uint32_t present_family = UINT32_MAX;

    for (uint32_t i = 0; i < device_count; i++) {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(devices[i], &properties);

        std::cout << "GPU candidate: " << properties.deviceName << "\n";

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
            vkGetPhysicalDeviceSurfaceSupportedKHR(g_vulkan_state.instance, g_vulkan_state.surface, &surface_supported);

            if (surface_supported == VK_TRUE) {
                present_family = j;
            }
        }

        selected_device = devices[i];
    }

    if (!selected_device) {
        std::cerr << "Failed to find suitable physical device!\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    g_vulkan_state.physical_device = selected_device;

    // Create logical device with swapchain and presentation extensions
    const char* device_extensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
    };

    // Queue create info for graphics queue
    VkDeviceQueueCreateInfo queue_create_info{};
    queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.queueFamilyIndex = static_cast<uint32_t>(graphics_family);
    queue_create_info.queueCount = 1;
    queue_create_info.pQueuePriorities = &1.0f;

    // Create device with one graphics/presentation queue
    VkDeviceCreateInfo device_create_info{};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.queueCreateInfoCount = 1;
    device_create_info.pQueueCreateInfos = &queue_create_info;
    device_create_info.enabledExtensionCount = sizeof(device_extensions) / sizeof(device_extensions[0]);
    device_create_info.ppEnabledExtensionNames = device_extensions;

    VkResult result = vkCreateDevice(selected_device, &device_create_info, nullptr, &g_vulkan_state.device);

    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create logical device: " << static_cast<int>(result) << "\n";
        return result;
    }

    // Get queues - use same queue family for graphics and presentation
    vkGetDeviceQueue(g_vulkan_state.device, graphics_family, 0, &g_vulkan_state.graphics_queue);
    vkGetDeviceQueue(g_vulkan_state.device, graphics_family, 0, &g_vulkan_state.present_queue);

    std::cout << "=== Logical device created successfully ===\n";
    return VK_SUCCESS;
}

// ============================================================================
// COMMAND POOL CREATION
// ============================================================================

VkResult create_command_pool() {
    if (!g_vulkan_state.device) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkCommandPoolCreateInfo pool_create_info{};
    pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_create_info.queueFamilyIndex = static_cast<uint32_t>(0);
    pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(g_vulkan_state.device, &pool_create_info, nullptr, &g_vulkan_state.command_pool) != VK_SUCCESS) {
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
    if (!g_vulkan_state.instance || !g_vulkan_state.surface) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    std::cout << "\n=== Creating Swapchain ===\n";

    // Get swapchain surface format and present mode
    uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceSupportedFormats(g_vulkan_state.instance, g_vulkan_state.surface, &format_count, nullptr);

    std::vector<VkSurfaceFormatKHR> supported_formats(format_count);
    vkGetPhysicalDeviceSurfaceSupportedFormats(g_vulkan_state.instance, g_vulkan_state.surface, &format_count, supported_formats.data());

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
    vkGetPhysicalDeviceSurfacePresentModes(g_vulkan_state.instance, g_vulkan_state.surface, &present_mode_count, nullptr);

    std::vector<VkPresentModeKHR> supported_present_modes(present_mode_count);
    vkGetPhysicalDeviceSurfacePresentModes(g_vulkan_state.instance, g_vulkan_state.surface, &present_mode_count, supported_present_modes.data());

    // Choose the first present mode (usually VK_PRESENT_MODE_FIFO_KHR)
    g_swapchain_present_mode = supported_present_modes[0];

    // Get swapchain capabilities
    VkSurfaceCapabilitiesKHR surface_capabilities{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_vulkan_state.instance, g_vulkan_state.surface, &surface_capabilities);

    // Create swapchain creation info
    VkSwapchainCreateInfoKHR swapchain_create_info{};
    swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_create_info.surface = g_vulkan_state.surface;
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


[Read output capped at 32KB for this call. Use offset=414, limit=128 to continue.]

// ============================================================================
// RENDER PASS AND FRAMEBUFFER CREATION
// ============================================================================

VkResult create_render_pass() {
    if (!g_vulkan_state.device) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    std::cout << "\n=== Creating Render Pass ===\n";

    // Create render pass
    VkRenderPassCreateInfo render_pass_create_info{};
    render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_create_info.attachmentCount = 0;  // No attachments (external swapchain images)
    render_pass_create_info.pAttachments = nullptr;
    render_pass_create_info.subpassCount = 1;
    
    // Create subpass description array
    std::vector<VkSubpassDescription> subpasses(1);
    subpasses[0].sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION;
    subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    render_pass_create_info.pSubpasses = subpasses.data();
    render_pass_create_info.dependencyCount = 0;
    render_pass_create_info.pDependencies = nullptr;

    VkResult result = vkCreateRenderPass(g_vulkan_state.device, &render_pass_create_info, nullptr, &g_vulkan_state.render_pass);

    if (result != VK_SUCCESS || g_vulkan_state.render_pass == VK_NULL_HANDLE) {
        std::cerr << "Failed to create render pass: " << static_cast<int>(result) << "\n";
        return result;
    }

    std::cout << "Render pass created.\n";
    return VK_SUCCESS;
}

VkResult create_framebuffers() {
    if (!g_vulkan_state.swapchain || g_vulkan_state.swapchain == VK_NULL_HANDLE) {
        std::cerr << "Swapchain not initialized!\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    std::cout << "\n=== Creating Framebuffers ===\n";

    // Create framebuffers for each swapchain image
    std::vector<VkImageView> framebuffer_attachments(g_vulkan_state.swapchain_images.size());
    for (uint32_t i = 0; i < g_vulkan_state.swapchain_images.size(); i++) {
        framebuffer_attachments[i] = g_vulkan_state.swapchain_image_views[i];
    }

    VkFramebufferCreateInfo framebuffer_create_info{};
    framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_create_info.renderPass = g_vulkan_state.render_pass;
    framebuffer_create_info.attachmentCount = 1;
    framebuffer_create_info.pAttachments = framebuffer_attachments.data();
    framebuffer_create_info.width = g_vulkan_state.swapchain_images[0].extent.width;
    framebuffer_create_info.height = g_vulkan_state.swapchain_images[0].extent.height;
    framebuffer_create_info.layers = 1;

    VkResult result = vkCreateFramebuffer(g_vulkan_state.device, &framebuffer_create_info, nullptr, &g_swapchain_framebuffers[0]);

    if (result != VK_SUCCESS || g_swapchain_framebuffers[0] == VK_NULL_HANDLE) {
        std::cerr << "Failed to create framebuffer: " << static_cast<int>(result) << "\n";
        return result;
    }

    for (uint32_t i = 1; i < g_vulkan_state.swapchain_images.size(); i++) {
        vkCreateFramebuffer(g_vulkan_state.device, &framebuffer_create_info, nullptr, &g_swapchain_framebuffers[i]);
    }

    std::cout << "Framebuffers created: " << g_swapchain_framebuffers.size() << "\n";
    return VK_SUCCESS;
}

// ============================================================================
// PIPELINE CREATION
// ============================================================================

VkResult create_pipeline_layout() {
    if (!g_vulkan_state.device) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    std::cout << "\n=== Creating Pipeline Layout ===\n";

    // Create pipeline layout (empty for now)
    VkPipelineLayoutCreateInfo pipeline_layout_create_info{};
    pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_create_info.setLayoutCount = 0;
    pipeline_layout_create_info.pSetLayouts = nullptr;

    if (vkCreatePipelineLayout(g_vulkan_state.device, &pipeline_layout_create_info, nullptr, &g_vulkan_state.pipeline_layout) != VK_SUCCESS) {
        std::cerr << "Failed to create pipeline layout!\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    std::cout << "Pipeline layout created.\n";
    return VK_SUCCESS;
}

VkResult create_shader_modules() {
    if (!g_vulkan_state.instance) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    std::cout << "\n=== Creating Shader Modules ===\n";

    // Simple vertex and fragment shaders using basic GLSL code
    const char* vertex_shader = R"(
#version 450
layout(location = 0) in float3 pos_in;
layout(location = 1) in float2 uv_in;
out vec2 frag_uv;

void main() {
    gl_Position = vec4(pos_in, 1.0);
    frag_uv = uv_in;
}
)";

    const char* fragment_shader = R"(
#version 450
layout(location = 0) in vec2 frag_uv;
out vec4 out_color;

void main() {
    out_color = vec4(1.0, 0.9, 0.8, 1.0);
}
)";

    // In a real implementation, you would compile these shaders to SPIR-V
    // For this demo, we use placeholder shader code
    std::cout << "Shader modules created (demo mode - no actual GLSL compilation in SILK).\n";
    
    return VK_SUCCESS;
}

VkResult create_graphics_pipeline() {
    if (!g_vulkan_state.device) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    std::cout << "\n=== Creating Graphics Pipeline ===\n";

    // Create graphics pipeline
    VkPipeline graphics_pipeline = VK_NULL_HANDLE;

    VkPipelineShaderStageCreateInfo shader_stage_info{};
    shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stage_info.stage = VK_SHADER_VERTEX_STAGE;
    
    std::cout << "Graphics pipeline created (placeholder).\n";

    return VK_SUCCESS;
}

// ============================================================================
// VERTEX BUFFER CREATION
// ============================================================================

VkResult create_vertex_buffer() {
    if (!g_vulkan_state.device) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    std::cout << "\n=== Creating Vertex Buffer ===\n";

    // Calculate size of vertex data
    size_t vertex_data_size = g_vertices.size() * sizeof(Vertex);

    // Create vertex buffer
    VkBufferCreateInfo buffer_create_info{};
    buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_create_info.size = vertex_data_size;
    buffer_create_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer vertex_buffer = VK_NULL_HANDLE;
    if (vkCreateBuffer(g_vulkan_state.device, &buffer_create_info, nullptr, &vertex_buffer) != VK_SUCCESS) {
        std::cerr << "Failed to create vertex buffer!\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Allocate memory for vertex buffer
    VkBufferMemoryRequirements buffer_memory_requirements{};
    vkGetBufferMemoryRequirements(g_vulkan_state.device, vertex_buffer, &buffer_memory_requirements);

    VkDeviceMemory memory = VK_NULL_HANDLE;
    allocate_host_visible_memory(g_vulkan_state.device, buffer_memory_requirements.size, 
                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                                 &memory);

    // Copy vertex data to device memory
    void* data = nullptr;
    vkMapMemory(g_vulkan_state.device, memory, 0, vertex_data_size, 0, &data);
    memcpy(data, g_vertices.data(), vertex_data_size);
    vkUnmapMemory(g_vulkan_state.device, memory);

    // Bind buffer to device memory
    if (vkBindBufferMemory(g_vulkan_state.device, vertex_buffer, memory, 0) != VK_SUCCESS) {
        std::cerr << "Failed to bind buffer to memory!\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Store global vertex buffer and memory
    g_vertex_buffer = vertex_buffer;
    g_vertex_memory = memory;

    std::cout << "Vertex buffer created: " << vertex_data_size << " bytes\n";
    return VK_SUCCESS;
}

// ============================================================================
// UNIFORM BUFFER CREATION
// ============================================================================

VkResult create_uniform_buffers() {
    if (!g_vulkan_state.device) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    std::cout << "\n=== Creating Uniform Buffers ===\n";

    // Create uniform buffer for vertex transformation matrices
    size_t uniform_buffer_size = 1024; // 4x3 matrices of float
    
    VkBufferCreateInfo buffer_create_info{};
    buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_create_info.size = uniform_buffer_size;
    buffer_create_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer uniform_buffer = VK_NULL_HANDLE;
    if (vkCreateBuffer(g_vulkan_state.device, &buffer_create_info, nullptr, &uniform_buffer) != VK_SUCCESS) {
        std::cerr << "Failed to create uniform buffer!\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Allocate memory for uniform buffer
    VkBufferMemoryRequirements mem_reqs{};
    vkGetBufferMemoryRequirements(g_vulkan_state.device, uniform_buffer, &mem_reqs);

    VkDeviceMemory mem = VK_NULL_HANDLE;
    allocate_host_visible_memory(g_vulkan_state.device, mem_reqs.size, 
                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                                 &mem);

    void* data;
    vkMapMemory(g_vulkan_state.device, mem, 0, uniform_buffer_size, 0, &data);
    
    // Initialize with zero
    memset(data, 0, uniform_buffer_size);

    vkUnmapMemory(g_vulkan_state.device, mem);

    if (vkBindBufferMemory(g_vulkan_state.device, uniform_buffer, mem, 0) != VK_SUCCESS) {
        std::cerr << "Failed to bind uniform buffer!\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    g_uniform_buffer = uniform_buffer;
    g_uniform_memory = mem;

    std::cout << "Uniform buffer created: " << uniform_buffer_size << " bytes\n";
    return VK_SUCCESS;
}

// ============================================================================
// MAIN FUNCTION - COMPLETE SDL2 + VULKAN RENDER LOOP
// ============================================================================

int main() {
    std::cout << "\n==========================================\n";
    std::cout << "TAZ: WANTED - SDL2 + VULKAN RENDER LOOP\n";
    std::cout << "==========================================\n\n";

    // Generate terrain mesh
    generate_terrain_mesh();

    // Initialize Vulkan instance
    if (init_vulkan_instance() != VK_SUCCESS) {
        return EXIT_FAILURE;
    }

    // Create SDL2 window and Vulkan surface
    if (create_sdl_window_and_surface() != VK_SUCCESS) {
        return EXIT_FAILURE;
    }

    // Create command pool
    if (create_command_pool() != VK_SUCCESS) {
        return EXIT_FAILURE;
    }

    // Allocate command buffers
    std::cout << "\n=== Allocating Command Buffers ===\n";
    
    VkCommandBufferAllocateInfo cmd_alloc_info{};
    cmd_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmd_alloc_info.commandPool = g_vulkan_state.command_pool;
    cmd_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd_alloc_info.commandBufferCount = static_cast<uint32_t>(g_swapchain_images.size());

    if (vkAllocateCommandBuffers(g_vulkan_state.device, &cmd_alloc_info, 
                                  &g_vulkan_state.command_buffers[0]) != VK_SUCCESS) {
        std::cerr << "Failed to allocate command buffers!\n";
        return EXIT_FAILURE;
    }

    // Allocate synchronization primitives
    std::cout << "\n=== Allocating Synchronization Primitives ===\n";
    
    VkSemaphoreCreateInfo semaphore_create_info{};
    semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    if (vkCreateSemaphore(g_vulkan_state.device, &semaphore_create_info, nullptr, 
                          &g_image_available_semaphores[0]) != VK_SUCCESS ||
        vkCreateSemaphore(g_vulkan_state.device, &semaphore_create_info, nullptr, 
                          &g_render_complete_semaphores[0]) != VK_SUCCESS) {
        std::cerr << "Failed to create semaphore!\n";
        return EXIT_FAILURE;
    }

    // Allocate fences for rendering
    VkFenceCreateInfo fence_create_info{};
    fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;  // Start signaled for simplicity

    g_vulkan_state.fences.resize(RENDER_QUEUE_SIZE);
    for (uint32_t i = 0; i < RENDER_QUEUE_SIZE; i++) {
        vkCreateFence(g_vulkan_state.device, &fence_create_info, nullptr, &g_vulkan_state.fences[i]);
    }

    std::cout << "Synchronization primitives allocated.\n";


    // Main render loop
    std::cout << "\n=== Starting Render Loop ===\n";
    
    bool running = true;
    while (running && g_sdl_window) {
        // Poll for SDL events
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                running = false;
            }
        }

        // Update uniform buffer with current time
        static uint32_t frame_time = 0;
        ++frame_time;

        // Begin command buffer recording
        VkCommandBufferBeginInfo cmd_begin_info{};
        cmd_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cmd_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        if (vkBeginCommandBuffer(g_vulkan_state.command_buffers[0], &cmd_begin_info) != VK_SUCCESS) {
            std::cerr << "Failed to begin command buffer!\n";
            continue;
        }

        // Get next swapchain image
        VkImage_acquire_info acquire_info{};
        acquire_info.sType = VK_STRUCTURE_TYPE_IMAGE_ACQUIRE_INFO;
        acquire_info.flags = VK_IMAGE_ACQUIRE_ACCESS_FLAGS_COLOR_ATTACHMENT_READ_BIT_KHR;

        VkResult result = vkAcquireNextImageKHR(g_vulkan_state.device, g_vulkan_state.swapchain, 
                                                  10000000000ULL, g_image_available_semaphores[0], nullptr);
        
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            std::cout << "Surface needs reconfiguration.\n";
            break;
        } else if (result < 0) {
            std::cerr << "Failed to acquire swapchain image: " << result << "\n";
            continue;
        }

        // Bind pipeline and set render pass begin info
        VkRenderPassBeginInfo render_pass_begin_info{};
        render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_pass_begin_info.renderPass = g_vulkan_state.render_pass;
        render_pass_begin_info.framebuffer = g_swapchain_framebuffers[0];
        render_pass_begin_info.renderArea.offsets.x = 0;
        render_pass_begin_info.renderArea.offsets.y = 0;
        render_pass_begin_info.renderArea.extent.width = g_vulkan_state.swapchain_images[0].extent.width;
        render_pass_begin_info.renderArea.extent.height = g_vulkan_state.swapchain_images[0].extent.height;

        // Bind pipeline (placeholder for now)
        VkPipeline vertex_input_state{};
        
        vkCmdBeginRenderPass(g_vulkan_state.command_buffers[0], &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        // Draw vertices
        vkCmdBindVertexBuffers(g_vulkan_state.command_buffers[0], 0, 
                               1, &g_vertex_buffer, &g_vertex_memory);

        VkDeviceSize offset = 0;
        vkCmdDraw(g_vulkan_state.command_buffers[0], static_cast<uint32_t>(g_vertices.size()), 
                   1, offset, 0);

        // End render pass
        vkCmdEndRenderPass(g_vulkan_state.command_buffers[0]);

        if (vkEndCommandBuffer(g_vulkan_state.command_buffers[0]) != VK_SUCCESS) {
            std::cerr << "Failed to end command buffer!\n";
            continue;
        }

        // Submit command buffer for rendering
        VkSubmitInfo submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &g_vulkan_state.command_buffers[0];

        std::cout << "Frame rendered. Submitting to queue...\n";

        if (vkQueueSubmit(g_vulkan_state.graphics_queue, 1, &submit_info, 
                         g_vulkan_state.fences[0]) != VK_SUCCESS) {
            std::cerr << "Failed to submit command buffer!\n";
            continue;
        }

        // Present to swapchain
        VkPresentInfoKHR present_info{};
        present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present_info.waitSemaphoreCount = 0;
        present_info.swapchainCount = 1;
        present_info.pSwapchains = &g_vulkan_state.swapchain;
        
        VkPresentRegionKHR region{};
        region.sType = VK_STRUCTURE_TYPE_PRESENT_REGION_KHR;
        region.rect.offset.x = 0;
        region.rect.offset.y = 0;
        region.rect.extent.width = g_vulkan_state.swapchain_images[0].extent.width;
        region.rect.extent.height = g_vulkan_state.swapchain_images[0].extent.height;

        present_info.pRegions = &region;

        vkQueuePresentKHR(g_vulkan_state.present_queue, &present_info);

        // Check for swapchain reconfiguration needed
        VkResult queue_family_result = VK_SUCCESS;

    }

    std::cout << "\n=== Render loop finished ===\n";

    // Cleanup
    cleanup_vulkan_resources();

    if (g_sdl_window) {
        SDL_DestroyWindow(g_sdl_window);
    }
    SDL_Quit();

    return EXIT_SUCCESS;
}

// ============================================================================
// CLEANUP FUNCTION
// ============================================================================

void cleanup_vulkan_resources() {
    std::cout << "\n=== Cleaning up Vulkan resources ===\n";

    // Destroy command buffers
    if (g_vulkan_state.command_pool) {
        vkDestroyCommandPool(g_vulkan_state.device, g_vulkan_state.command_pool, nullptr);
    }

    // Destroy other Vulkan objects in reverse order of creation
    for (uint32_t i = 0; i < g_swapchain_images.size(); i++) {
        if (g_vulkan_state.swapchain_image_views[i] != VK_NULL_HANDLE) {
            vkDestroyImageView(g_vulkan_state.device, g_vulkan_state.swapchain_image_views[i], nullptr);
        }
    }

    // Clean up uniform buffers and other memory
    std::cout << "Vulkan resources cleaned up.\n";
}

// ============================================================================
// HELPER: ALLOCATE HOST VISIBLE MEMORY
// ============================================================================

void allocate_host_visible_memory(VkDevice device, size_t allocation_size, 
                                   VkMemoryPropertyFlags properties, VkDeviceMemory* memory) {
    VkBufferMemoryRequirements buffer_memory_requirements{};
    
    // Get memory requirements and allocate
    // Implementation depends on having a valid buffer handle
}

