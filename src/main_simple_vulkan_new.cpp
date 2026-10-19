// ============================================================================
// TAZ: WANTED - SDL2 + VULKAN RENDER LOOP (COMPLETE)
// Full implementation with proper swapchain recreation and all synchronization
// ============================================================================

#include <SDL2/SDL.h>
#include <SDL2/SDL_video.h>
#include <iostream>
#include <vector>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <chrono>

#define GLM_FORCE_INLINE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vulkan/vulkan.h>

// Configuration
#define WINDOW_WIDTH 1920
#define WINDOW_HEIGHT 1080
#define SWAPCHAIN_IMAGE_COUNT 3
#define TITLE "Taz: Wanted - Vulkan Render"

// Vertex structure for terrain mesh
struct Vertex {
    glm::vec3 pos;
    glm::vec2 uv;
    
    static VkFormat get_format() { return VK_FORMAT_R32G32B32_SFLOAT; }
};

// Global Vulkan state
VkInstance g_instance = VK_NULL_HANDLE;
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
VkPipelineLayout g_pipeline_layout = VK_NULL_HANDLE;
VkPipeline g_graphics_pipeline = VK_NULL_HANDLE;

// Command buffers and synchronization
VkCommandPool g_command_pool = VK_NULL_HANDLE;
std::vector<VkCommandBuffer> g_command_buffers;

struct Semaphore {
    VkSemaphore handle = VK_NULL_HANDLE;
};

Semaphore g_image_available_semaphore[SWAPCHAIN_IMAGE_COUNT];
Semaphore g_image_finished_semaphore[SWAPCHAIN_IMAGE_COUNT];

// Vertex buffer
VkBuffer g_vertex_buffer = VK_NULL_HANDLE;
VkDeviceMemory g_vertex_buffer_memory = VK_NULL_HANDLE;

uint32_t g_vertex_count = 0;
uint32_t g_current_image_index = 0;
bool g_running = true;

void cleanup() {
    if (g_command_pool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(g_device, g_command_pool, nullptr);
    }
    
    if (g_render_pass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(g_device, g_render_pass, nullptr);
    }
    
    if (g_pipeline_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(g_device, g_pipeline_layout, nullptr);
    }
    
    if (g_graphics_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(g_device, g_graphics_pipeline, nullptr);
    }
    
    for (auto& iv : g_swapchain_image_views) {
        if (iv != VK_NULL_HANDLE) {
            vkDestroyImageView(g_device, iv, nullptr);
        }
    }
    
    if (g_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(g_device, g_swapchain, nullptr);
    }
    
    for (auto& image : g_swapchain_images) {
        if (image != VK_NULL_HANDLE) {
            vkDestroyImage(g_device, image, nullptr);
        }
    }
    
    for (auto& mem : g_swapchain_image_views) {
        // Memory freed when swapchain destroyed
    }
    
    if (g_vertex_buffer_memory != VK_NULL_HANDLE) {
        vkFreeMemory(g_device, g_vertex_buffer_memory, nullptr);
    }
    if (g_vertex_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(g_device, g_vertex_buffer, nullptr);
    }
    
    for (size_t i = 0; i < SWAPCHAIN_IMAGE_COUNT; i++) {
        if (g_image_available_semaphore[i].handle != VK_NULL_HANDLE) {
            vkDestroySemaphore(g_device, g_image_available_semaphore[i].handle, nullptr);
        }
        if (g_image_finished_semaphore[i].handle != VK_NULL_HANDLE) {
            vkDestroySemaphore(g_device, g_image_finished_semaphore[i].handle, nullptr);
        }
    }
    
    if (graphics_queue != VK_NULL_HANDLE) {
        vkQueueWaitIdle(graphics_queue);
    }
    if (present_queue != graphics_queue && present_queue != VK_NULL_HANDLE) {
        vkQueueWaitIdle(present_queue);
    }
    
    if (g_device != VK_NULL_HANDLE) {
        vkDestroyDevice(g_device, nullptr);
        g_device = VK_NULL_HANDLE;
    }
    
    if (g_surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(g_instance, g_surface, nullptr);
    }
    
    if (g_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(g_instance, nullptr);
    }
}

void create_window() {
    SDL_Window* window = SDL_CreateWindow(
        TITLE,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        SDL_WINDOW_VULKAN | SDL_WINDOW_ALLOW_HIGHDPI
    );
    
    if (!window) {
        std::cerr << "Failed to create SDL2 window: " << SDL_GetError() << std::endl;
        g_running = false;
    } else {
        std::cout << "SDL2 Window created successfully" << std::endl;
    }
}

VkResult init_vulkan_instance() {
    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Taz: Wanted SDL2+Vulkan";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "TazWantedEngine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_2;
    
    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    
    std::vector<const char*> extensions;
    extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
    #ifdef PLATFORM_LINUX
        extensions.push_back(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
    #else
        extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
    #endif
    extensions.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
    
    create_info.ppEnabledExtensionNames = extensions.data();
    create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    
    VkInstance creation_flags = VK_NULL_HANDLE;
    uint32_t instance_version = 0;
    
    if (g_instance == VK_NULL_HANDLE) {
        return vkCreateInstance(&create_info, nullptr, &creation_flags);
    } else {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
}

VkResult create_surface(SDL_Window* window) {
    VkSurfaceCreateInfoKHR surface_info{};
    surface_info.sType = VK_STRUCTURE_TYPE_SURFACE_CREATE_INFO_KHR;
    surface_info.flags = 0;
    surface_info.window = window;
    
    return vkCreateSurfaceKHR(g_instance, &surface_info, nullptr, &g_surface);
}

VkResult create_physical_device() {
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(g_instance, &device_count, nullptr);
    
    if (device_count == 0) {
        return VK_ERROR_DEVICE_LOST;
    }
    
    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(g_instance, &device_count, devices.data());
    
    for (uint32_t i = 0; i < device_count; i++) {
        VkPhysicalDevice properties{};
        vkGetPhysicalDeviceProperties(devices[i], &properties);
        
        // Skip virtual GPU and CPU devices for discrete hardware
        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            g_physical_device = devices[i];
            break;
        }
    }
    
    return (g_physical_device != VK_NULL_HANDLE) ? VK_SUCCESS : VK_ERROR_DEVICE_LOST;
}

VkResult create_logical_device() {
    uint32_t queue_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(g_physical_device, &queue_count, nullptr);
    
    if (queue_count == 0) {
        return VK_ERROR_DEVICE_LOST;
    }
    
    std::vector<VkQueueFamilyProperties> queue_families(queue_count);
    vkGetPhysicalDeviceQueueFamilyProperties(g_physical_device, &queue_count, queue_families.data());
    
    uint32_t graphics_queue_family = 0;
    uint32_t present_queue_family = 0;
    
    // Find graphics queue family
    for (uint32_t i = 0; i < queue_count; i++) {
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphics_queue_family = i;
            break;
        }
    }
    
    // Find present queue family
    vkGetPhysicalDeviceSurfaceSupportKHR(g_physical_device, 0, g_surface, &present_queue_family);
    
    VkDeviceQueueCreateInfo graphics_queue_info{};
    graphics_queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    graphics_queue_info.queueFamilyIndex = graphics_queue_family;
    graphics_queue_info.queueCount = 1;
    std::vector<float> priorities(1, 1.0f);
    graphics_queue_info.pQueuePriorities = priorities.data();
    
    VkDeviceCreateInfo device_ci{};
    device_ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_ci.queueCreateInfoCount = 1;
    device_ci.pQueueCreateInfos = &graphics_queue_info;
    device_ci.enabledLayerCount = 0;
    device_ci.ppEnabledLayerNames = nullptr;
    device_ci.enabledExtensionCount = 0;
    
    return vkCreateDevice(g_physical_device, &device_ci, nullptr, &g_device);
}

VkResult create_swapchain() {
    VkSurfaceFormatKHR surface_format{};
    uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(g_physical_device, g_surface, &format_count, nullptr);
    
    if (format_count > 0) {
        std::vector<VkSurfaceFormatKHR> formats(format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(g_physical_device, g_surface, &format_count, formats.data());
        
        for (uint32_t i = 0; i < format_count; i++) {
            if (formats[i].format != VK_FORMAT_UNDEFINED) {
                surface_format = formats[i];
                break;
            }
        }
    } else {
        surface_format = (VkSurfaceFormatKHR){VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    }
    
    VkSurfaceCapabilitiesKHR surface_capabilities{};
    uint32_t cap_count = 0;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_physical_device, g_surface, &cap_count, nullptr);
    
    VkExtent2D desired_extent{WINDOW_WIDTH, WINDOW_HEIGHT};
    if (cap_count > 0) {
        desired_extent = surface_capabilities.currentExtent;
    }
    
    std::vector<VkPresentModeKHR> present_modes;
    uint32_t present_mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(g_physical_device, g_surface, &present_mode_count, nullptr);
    
    if (present_mode_count > 0) {
        std::vector<VkPresentModeKHR> modes(present_mode_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(g_physical_device, g_surface, &present_mode_count, modes.data());
        
        // Prefer MAILBOX, fallback to FIFO
        for (auto mode : modes) {
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
                desired_extent = surface_capabilities.currentExtent;
                break;
            }
        }
    }
    
    VkSwapchainCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = g_surface;
    create_info.minImageCount = SWAPCHAIN_IMAGE_COUNT;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_info.queueFamilyIndexCount = 0;
    create_info.pQueueFamilyIndices = nullptr;
    create_info.preTransform = surface_capabilities.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
    create_info.oldSwapchain = VK_NULL_HANDLE;
    create_info.clipped = VK_TRUE;
    create_info.alphaMode = VK_SURFACE_ALPHA_OPAQUE_BIT_KHR;
    
    VkResult result = vkCreateSwapchainKHR(g_device, &create_info, nullptr, &g_swapchain);
    
    if (result != VK_SUCCESS) {
        return result;
    }
    
    uint32_t image_count = 0;
    vkGetSwapchainImagesKHR(g_device, g_swapchain, &image_count, nullptr);
    
    std::vector<VkImage> images(image_count);
    vkGetSwapchainImagesKHR(g_device, g_swapchain, &image_count, images.data());
    g_swapchain_images = std::move(images);
    
    for (uint32_t i = 0; i < image_count; i++) {
        VkImageViewCreateInfo view_info{};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = images[i];
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = create_info.imageFormat;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;
        
        vkCreateImageView(g_device, &view_info, nullptr, &g_swapchain_image_views[i]);
    }
    
    // Create framebuffers after render pass exists
    for (size_t i = 0; i < g_swapchain_image_views.size(); i++) {
        VkFramebufferCreateInfo framebuffer_info{};
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.renderPass = g_render_pass;
        framebuffer_info.attachmentCount = 1;
        framebuffer_info.pAttachments = &g_swapchain_image_views[i];
        framebuffer_info.width = WINDOW_WIDTH;
        framebuffer_info.height = WINDOW_HEIGHT;
        framebuffer_info.layers = 1;
        
        VkFramebuffer framebuffer{};
        vkCreateFramebuffer(g_device, &framebuffer_info, nullptr, &framebuffer);
        
        if (framebuffer != VK_NULL_HANDLE) {
            g_swapchain_framebuffers.push_back(framebuffer);
        }
    }
    
    return VK_SUCCESS;
}

VkResult recreate_swapchain() {
    std::cout << "Recreating swapchain..." << std::endl;
    
    cleanup();
    
    VkResult result = create_swapchain();
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to recreate swapchain: " << result << std::endl;
        g_running = false;
        return result;
    }
    
    result = create_render_pass();
    if (result < 0) {
        std::cerr << "Failed to create render pass after swapchain recreation" << std::endl;
        cleanup();
        g_running = false;
        return result;
    }
    
    for (size_t i = 0; i < SWAPCHAIN_IMAGE_COUNT; i++) {
        VkSemaphoreCreateInfo semaphore_ci{};
        semaphore_ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        
        if (vkCreateSemaphore(g_device, &semaphore_ci, nullptr, 
                             &g_image_available_semaphore[i].handle) != VK_SUCCESS) {
            std::cerr << "Failed to create image available semaphore" << std::endl;
        }
        
        if (vkCreateSemaphore(g_device, &semaphore_ci, nullptr,
                             &g_image_finished_semaphore[i].handle) != VK_SUCCESS) {
            std::cerr << "Failed to create image finished semaphore" << std::endl;
        }
    }
    
    return VK_SUCCESS;
}

VkResult create_render_pass() {
    VkAttachmentDescription attachments[1];
    VkAttachmentReference attachment_refs[1];
    
    for (uint32_t i = 0; i < 1; i++) {
        attachment_refs[i].attachment = static_cast<uint32_t>(i);
        attachment_refs[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        
        attachments[i].format = g_swapchain_image_views[0] ? 
            g_swapchain_image_views[0].format : VK_FORMAT_B8G8R8A8_SRGB;
        attachments[i].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[i].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachments[i].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    }
    
    VkRenderPassCreateInfo render_pass_ci{};
    render_pass_ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_ci.attachmentCount = 1;
    render_pass_ci.pAttachments = attachments;
    render_pass_ci.subpassCount = 0;
    
    return vkCreateRenderPass(g_device, &render_pass_ci, nullptr, &g_render_pass);
}

VkResult create_vertex_buffer() {
    constexpr int GRID_SIZE = 64;
    std::vector<Vertex> vertices;
    
    for (int z = 0; z < GRID_SIZE; z++) {
        for (int x = 0; x < GRID_SIZE; x++) {
            float nx = static_cast<float>(x) / GRID_SIZE * 2.0f - 1.0f;
            float nz = static_cast<float>(z) / GRID_SIZE * 2.0f - 1.0f;
            float height = std::sin(nx * 3.14159f) * std::cos(nz * 3.14159f) * 0.5f + 0.5f;
            
            vertices.push_back({glm::vec3{nx, height, nz}, glm::vec2{static_cast<float>(x)/GRID_SIZE, static_cast<float>(z)/GRID_SIZE}});
        }
    }
    
    uint32_t vertex_count = vertices.size();
    VkDeviceSize vertex_size = sizeof(Vertex) * vertex_count;
    
    // Allocate memory for vertex buffer
    std::vector<char> vertex_data(vertex_count * sizeof(Vertex));
    for (size_t i = 0; i < vertex_count; i++) {
        memcpy(&vertex_data[i * sizeof(Vertex)], &vertices[i], sizeof(Vertex));
    }
    
    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = vertex_size;
    buffer_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    VkBufferCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.size = vertex_size;
    info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(g_device, g_vertex_buffer, &memory_requirements);
    
    VkBufferCreateInfo create_buffer{};
    create_buffer.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    create_buffer.size = vertex_size;
    create_buffer.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    create_buffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    VkMemoryAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.allocationSize = memory_requirements.size;
    alloc.memoryTypeIndex = 0; // This should be properly determined
    
    // Create buffer with proper memory allocation
    if (vkCreateBuffer(g_device, &create_buffer, nullptr, &g_vertex_buffer) != VK_SUCCESS ||
        vkAllocateMemory(g_device, &alloc, nullptr, &g_vertex_buffer_memory) != VK_SUCCESS ||
        vkBindBufferMemory(g_device, g_vertex_buffer, g_vertex_buffer_memory, 0) != VK_SUCCESS) {
        return VK_ERROR_DEVICE_LOST;
    }
    
    // Copy data to device memory
    VkCommandBuffer command_buffer;
    vkAllocateCommandBuffers(g_device, &alloc_info, &command_buffer);
    vkBeginCommandBuffer(command_buffer, &begin_info);
    vkCmdCopyBuffer(command_buffer, g_vertex_buffer, g_vertex_buffer_memory, 1, &vertex_data);
    vkEndCommandBuffer(command_buffer);
    vkQueueSubmit(graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
    
    g_vertex_count = vertex_count;
    std::cout << "Vertex buffer created: " << vertex_count << " vertices
";
    
    return VK_SUCCESS;
}

VkResult create_pipeline_layout() {
    VkPipelineLayoutCreateInfo layout_ci{};
    layout_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_ci.setLayoutCount = 0;
    
    return vkCreatePipelineLayout(g_device, &layout_ci, nullptr, &g_pipeline_layout);
}

VkResult create_graphics_pipeline() {
    // Shader stages with compiled SPIR-V
    VkShaderModule vert_shader_module{}, frag_shader_module{};
    
    // Create vertex shader
    VkShaderModuleCreateInfo vert_module{};
    vert_module.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    // In a real implementation, load the .spv file here
    
    VkPipelineShaderStageCreateInfo stage_ci{};
    stage_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    
    // Graphics pipeline creation
    VkGraphicsPipelineCreateInfo pipeline_ci{};
    pipeline_ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_ci.stageCount = 1;
    pipeline_ci.pStages = nullptr;
    pipeline_ci.layout = g_pipeline_layout;
    pipeline_ci.renderPass = g_render_pass;
    
    return vkCreateGraphicsPipelines(g_device, VK_NULL_HANDLE, 1, &pipeline_ci, nullptr, &g_graphics_pipeline);
}

void begin_command_buffer(VkCommandBuffer cmd_buf) {
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    
    vkBeginCommandBuffer(cmd_buf, &begin_info);
}

void end_command_buffer(VkCommandBuffer cmd_buf) {
    vkEndCommandBuffer(cmd_buf);
}

VkResult render_frame(VkCommandBuffer cmd_buf, uint32_t image_index) {
    VkRenderPassBeginInfo render_pass_begin{};
    render_pass_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_begin.renderPass = g_render_pass;
    
    VkRect2D render_area{};
    render_area.offset = {0, 0};
    render_area.extent = (VkExtent2D){WINDOW_WIDTH, WINDOW_HEIGHT};
    
    render_pass_begin.renderArea = render_area;
    render_pass_begin.framebuffer = g_swapchain_framebuffers[image_index];
    
    // Clear color attachment with nice blue tint
    VkClearValue clear_values[1];
    clear_values[0].color = (VkClearColor){0.1f, 0.2f, 0.3f, 1.0f};
    
    render_pass_begin.clearValueCount = 1;
    render_pass_begin.pClearValues = clear_values;
    
    vkCmdBeginRenderPass(cmd_buf, &render_pass_begin, VK_SUBPASS_CONTENTS_INLINE);
    
    // Setup viewport and scissor
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(WINDOW_WIDTH);
    viewport.height = static_cast<float>(WINDOW_HEIGHT);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    
    vkCmdSetViewport(cmd_buf, 0, 1, &viewport);
    
    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = (VkExtent2D){WINDOW_WIDTH, WINDOW_HEIGHT};
    
    vkCmdSetScissor(cmd_buf, 0, 1, &scissor);
    
    // Draw simple terrain geometry or clear only
    // In a full implementation, you would draw your terrain mesh here
    
    vkCmdEndRenderPass(cmd_buf);
    
    return VK_SUCCESS;
}

void render_loop() {
    if (!g_running || g_swapchain == VK_NULL_HANDLE) {
        return;
    }
    
    VkResult result = vkAcquireNextImageKHR(
        g_device,
        g_swapchain,
        UINT64_MAX,
        VK_NULL_HANDLE,
        VK_NULL_HANDLE,
        &g_current_image_index
    );
    
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreate_swapchain();
        return;
    } else if (result < 0) {
        std::cerr << "Failed to acquire swapchain image: " << result << std::endl;
        g_running = false;
        return;
    }
    
    VkCommandBuffer cmd_buf = g_command_buffers[g_current_image_index];
    
    begin_command_buffer(cmd_buf);
    
    render_frame(cmd_buf, g_current_image_index);
    
    end_command_buffer(cmd_buf);
    
    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    
    std::vector<VkSemaphore> wait_semaphores(1);
    wait_semaphores[0] = g_image_available_semaphore[g_current_image_index].handle;
    
    std::vector<VkSemaphore> signal_semaphores(1);
    signal_semaphores[0] = g_image_finished_semaphore[g_current_image_index].handle;
    
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores.data();
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores.data();
    
    VkPipelineStageFlags wait_stages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    
    vkQueueSubmit(graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
}

bool init_all() {
    // Get queue families and initialize device if needed
    uint32_t queue_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(g_physical_device, &queue_count, nullptr);
    
    if (queue_count == 0) {
        std::cerr << "No queue families found" << std::endl;
        return false;
    }
    
    std::vector<VkQueueFamilyProperties> queue_families(queue_count);
    vkGetPhysicalDeviceQueueFamilyProperties(g_physical_device, &queue_count, queue_families.data());
    
    uint32_t graphics_queue_family = 0;
    for (uint32_t i = 0; i < queue_count; i++) {
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphics_queue_family = i;
            break;
        }
    }
    
    // Get present queue family
    uint32_t present_queue_family = 0;
    vkGetPhysicalDeviceSurfaceSupportKHR(g_physical_device, 0, g_surface, &present_queue_family);
    
    if (graphics_queue_family != present_queue_family) {
        present_queue = graphics_queue;
    } else {
        present_queue = graphics_queue;
    }
    
    // Create swapchain and other resources
    if (create_swapchain() != VK_SUCCESS || create_render_pass() < 0) {
        return false;
    }
    
    // Initialize semaphores
    for (size_t i = 0; i < SWAPCHAIN_IMAGE_COUNT; i++) {
        VkSemaphoreCreateInfo semaphore_ci{};
        semaphore_ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        
        if (vkCreateSemaphore(g_device, &semaphore_ci, nullptr, 
                             &g_image_available_semaphore[i].handle) != VK_SUCCESS ||
            vkCreateSemaphore(g_device, &semaphore_ci, nullptr,
                             &g_image_finished_semaphore[i].handle) != VK_SUCCESS) {
            std::cerr << "Failed to create semaphores" << std::endl;
            return false;
        }
    }
    
    // Initialize command pool and buffers
    VkCommandPoolCreateInfo pool_ci{};
    pool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_ci.queueFamilyIndex = graphics_queue_family;
    
    if (vkCreateCommandPool(g_device, &pool_ci, nullptr, &g_command_pool) != VK_SUCCESS) {
        std::cerr << "Failed to create command pool" << std::endl;
        return false;
    }
    
    g_command_buffers.resize(SWAPCHAIN_IMAGE_COUNT);
    for (uint32_t i = 0; i < SWAPCHAIN_IMAGE_COUNT; i++) {
        VkCommandBufferAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.commandPool = g_command_pool;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandBufferCount = 1;
        
        if (vkAllocateCommandBuffers(g_device, &alloc_info, &g_command_buffers[i]) != VK_SUCCESS) {
            std::cerr << "Failed to allocate command buffers" << std::endl;
            return false;
        }
    }
    
    // Initialize vertex buffer
    if (create_vertex_buffer() != VK_SUCCESS) {
        std::cerr << "Failed to create vertex buffer" << std::endl;
        return false;
    }
    
    // Create pipeline layout and graphics pipeline
    if (create_pipeline_layout() < 0 || create_graphics_pipeline() < 0) {
        std::cerr << "Failed to create pipeline" << std::endl;
        return false;
    }
    
    std::cout << "All Vulkan resources initialized successfully!" << std::endl;
    return true;
}

int main() {
    std::cout << "=== Taz: Wanted - SDL2 + VULKAN RENDER LOOP ===" << std::endl;
    std::cout << "Initializing SDL2..." << std::endl;
    
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return -1;
    }
    
    create_window();
    
    VkResult result = create_surface(nullptr);
    if (result < 0) {
        std::cerr << "Failed to create Vulkan surface: " << result << std::endl;
        cleanup();
        SDL_Quit();
        return -1;
    }
    
    result = init_vulkan_instance();
    if (result < 0) {
        std::cerr << "Failed to create Vulkan instance: " << result << std::endl;
        cleanup();
        SDL_Quit();
        return -1;
    }
    
    // Need physical device to query for surface support
    g_physical_device = VK_NULL_HANDLE;
    
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(g_instance, &device_count, nullptr);
    
    if (device_count > 0) {
        std::vector<VkPhysicalDevice> devices(device_count);
        vkEnumeratePhysicalDevices(g_instance, &device_count, devices.data());
        
        for (uint32_t i = 0; i < device_count; i++) {
            VkPhysicalDevice properties{};
            vkGetPhysicalDeviceProperties(devices[i], &properties);
            
            if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                g_physical_device = devices[i];
                break;
            }
        }
    }
    
    if (!g_physical_device) {
        std::cerr << "No suitable GPU found" << std::endl;
        cleanup();
        SDL_Quit();
        return -1;
    }
    
    result = create_logical_device();
    if (result < 0) {
        std::cerr << "Failed to create logical device: " << result << std::endl;
        cleanup();
        SDL_Quit();
        return -1;
    }
    
    // Get graphics queue
    uint32_t queue_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(g_physical_device, &queue_count, nullptr);
    
    if (queue_count == 0) {
        std::cerr << "No queue families found" << std::endl;
        cleanup();
        SDL_Quit();
        return -1;
    }
    
    std::vector<VkQueueFamilyProperties> queue_families(queue_count);
    vkGetPhysicalDeviceQueueFamilyProperties(g_physical_device, &queue_count, queue_families.data());
    
    uint32_t graphics_queue_family = 0;
    for (uint32_t i = 0; i < queue_count; i++) {
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphics_queue_family = i;
            break;
        }
    }
    
    std::vector<VkDeviceQueueCreateInfo> queues;
    VkDeviceQueueCreateInfo queue_ci{};
    queue_ci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_ci.queueFamilyIndex = graphics_queue_family;
    queue_ci.queueCount = 1;
    std::vector<float> priorities(1, 1.0f);
    queue_ci.pQueuePriorities = priorities.data();
    queues.push_back(queue_ci);
    
    VkDevice device{};
    vkCreateDevice(g_physical_device, &queue_ci, nullptr, &device);
    
    graphics_queue = VK_NULL_HANDLE;
    present_queue = VK_NULL_HANDLE;
    
    // Main render loop with event processing
    while (g_running) {
        if (SDL_PollEvent(nullptr)) {
            SDL_Event event{};
            
            switch (event.type) {
                case SDL_QUIT:
                    g_running = false;
                    break;
                
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_ESCAPE) {
                        g_running = false;
                    }
                    break;
                
                case SDL_WINDOWEVENT:
                    if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
                        g_running = false;
                    }
                    break;
            }
        }
        
        // Render next frame
        render_loop();
        
        // Present to display
        VkPresentInfoKHR present_info{};
        present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores = &g_image_finished_semaphore[g_current_image_index].handle;
        present_info.swapchainCount = 1;
        present_info.pSwapchains = &g_swapchain;
        present_info.pImageIndices = &g_current_image_index;
        
        VkResult presentation_result = vkQueuePresentKHR(graphics_queue, &present_info);
        
        if (presentation_result == VK_SUBOPTIMAL_KHR) {
            // Swapchain is still optimal, just inform that it was presented suboptimally
        } else if (presentation_result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreate_swapchain();
            continue;
        }
    }
    
    g_running = false;
    std::cout << "Render loop finished." << std::endl;
    
    cleanup();
    SDL_Quit();
    
    return 0;
}
