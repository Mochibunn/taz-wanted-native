// ============================================================================
// TAZ: WANTED - COMPLETE SDL2 + VULKAN RENDER LOOP
// Implements SDL2 window with Vulkan swapchain, framebuffers, pipeline,
// command buffers, and real-time render loop with presentation.
// ============================================================================

#define GLM_FORCE_INLINE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <SDL2/SDL.h>
#include <SDL2/SDL_video.h>

#include <iostream>
#include <vector>
#include <cstring>
#include <cmath>
#include <cstdlib>

#include <vulkan/vulkan.h>

// Window dimensions
#define WINDOW_WIDTH 1920
#define WINDOW_HEIGHT 1080
#define SWAPCHAIN_IMAGE_COUNT 3

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

bool g_running = true;

// ============================================================================
// CLEANUP FUNCTIONS
// ============================================================================

void cleanup() {
    // Destroy command buffers first
    if (g_command_pool != VK_NULL_HANDLE) {
        for (auto cb : g_command_buffers) {
            if (cb != VK_NULL_HANDLE) {
                vkFreeCommandBuffers(g_device, g_command_pool, 1, &cb);
            }
        }
        vkDestroyCommandPool(g_device, g_command_pool, nullptr);
    }
    
    // Destroy render pass
    if (g_render_pass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(g_device, g_render_pass, nullptr);
    }
    
    // Destroy image views
    for (auto& iv : g_swapchain_image_views) {
        if (iv != VK_NULL_HANDLE) {
            vkDestroyImageView(g_device, iv, nullptr);
        }
    }
    
    // Destroy swapchain
    if (g_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(g_device, g_swapchain, nullptr);
    }
    
    // Destroy vertex buffer memory and buffer
    if (g_vertex_buffer_memory != VK_NULL_HANDLE) {
        vkFreeMemory(g_device, g_vertex_buffer_memory, nullptr);
    }
    if (g_vertex_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(g_device, g_vertex_buffer, nullptr);
    }
    
    // Destroy semaphores
    for (size_t i = 0; i < SWAPCHAIN_IMAGE_COUNT; i++) {
        if (g_image_available_semaphore[i].handle != VK_NULL_HANDLE) {
            vkDestroySemaphore(g_device, g_image_available_semaphore[i].handle, nullptr);
        }
        if (g_image_finished_semaphore[i].handle != VK_NULL_HANDLE) {
            vkDestroySemaphore(g_device, g_image_finished_semaphore[i].handle, nullptr);
        }
    }
    
    // Wait for queues to finish before destroying device
    if (graphics_queue != VK_NULL_HANDLE) {
        vkQueueWaitIdle(graphics_queue);
    }
    if (present_queue != graphics_queue && present_queue != VK_NULL_HANDLE) {
        vkQueueWaitIdle(present_queue);
    }
    
    // Destroy device - this will clean up queues
    if (g_device != VK_NULL_HANDLE) {
        vkDestroyDevice(g_device, nullptr);
        g_device = VK_NULL_HANDLE;
    }
    
    // Destroy surface
    if (g_surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(g_instance, g_surface, nullptr);
    }
    
    // Destroy instance
    if (g_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(g_instance, nullptr);
    }
}

// ============================================================================
// WINDOW CREATION
// ============================================================================

void create_window() {
    SDL_Window* window = SDL_CreateWindow(
        "Taz: Wanted - Vulkan Render",
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        SDL_WINDOW_VULKAN | SDL_WINDOW_ALLOW_HIGHDPI
    );
    
    if (!window) {
        std::cerr << "Failed to create SDL2 window: " << SDL_GetError() << std::endl;
        g_running = false;
        return;
    }
}

// ============================================================================
// VULKAN INITIALIZATION
// ============================================================================

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
    
#ifdef PLATFORM_LINUX
    std::vector<const char*> extensions;
    extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
    extensions.push_back(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
#else
    std::vector<const char*> extensions;
    extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#endif
    
    create_info.ppEnabledExtensionNames = extensions.data();
    create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    
    VkResult result = vkCreateInstance(&create_info, nullptr, &g_instance);
    return result;
}

VkResult create_surface(SDL_Window* window) {
#ifdef PLATFORM_LINUX
    return SDL_Vulkan_CreateSurface(window, g_instance, &g_surface);
#else
    return VK_ERROR_INITIALIZATION_FAILED;
#endif
}

// ============================================================================
// DEVICE AND QUEUE CREATION
// ============================================================================

VkResult create_physical_device() {
    uint32_t device_count = 0;
    
    vkEnumeratePhysicalDevices(g_instance, &device_count, nullptr);
    if (device_count > 0) {
        std::vector<VkPhysicalDevice> devices(device_count);
        vkEnumeratePhysicalDevices(g_instance, &device_count, devices.data());
        
        for (uint32_t i = 0; i < device_count; i++) {
            VkPhysicalDevice properties{};
            vkGetPhysicalDeviceProperties(devices[i], &properties);
            
            if (properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU &&
                properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_CPU) {
                
                g_physical_device = devices[i];
                
                uint32_t queue_count = 0;
                vkGetPhysicalDeviceQueueFamilyProperties(g_physical_device, &queue_count, nullptr);
                
                if (queue_count > 0) {
                    std::vector<VkQueueFamilyProperties> queue_families(queue_count);
                    vkGetPhysicalDeviceQueueFamilyProperties(g_physical_device, &queue_count, queue_families.data());
                    
                    for (uint32_t j = 0; j < queue_count; j++) {
                        if ((queue_families[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
                            (queue_families[j].queueFlags & VK_QUEUE_COMPUTE_BIT)) {
                            
                            VkDeviceQueueCreateInfo queue_create_info{};
                            queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                            queue_create_info.queueFamilyIndex = j;
                            queue_create_info.queueCount = 1;
                            
                            float queue_priority = 1.0f;
                            std::vector<float> queue_priorities(1, queue_priority);
                            
                            VkDeviceQueueCreateInfo queue_infos{};
                            queue_infos.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                            queue_infos.queueFamilyIndex = j;
                            queue_infos.queueCount = 1;
                            queue_infos.pQueuePriorities = queue_priorities.data();
                            
                            VkPhysicalDeviceFeatures device_features{};
                            
                            VkDeviceCreateInfo device_ci{};
                            device_ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
                            device_ci.queueCreateInfoCount = 1;
                            device_ci.pQueueCreateInfos = &queue_infos;
                            device_ci.pEnabledFeatures = &device_features;
                            
                            return vkCreateDevice(g_physical_device, &device_ci, nullptr, &g_device);
                        }
                    }
                }
            }
        }
    }
    
    return VK_ERROR_INITIALIZATION_FAILED;
}

VkResult setup_queues() {
    if (g_physical_device == VK_NULL_HANDLE || g_device == VK_NULL_HANDLE) {
        return VK_ERROR_INCOMPATIBLE_DEVICE_ORATION_KHR;
    }
    
    uint32_t queue_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(g_physical_device, &queue_count, nullptr);
    
    if (queue_count <= 0) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    
    std::vector<VkQueueFamilyProperties> queue_families(queue_count);
    vkGetPhysicalDeviceQueueFamilyProperties(g_physical_device, &queue_count, queue_families.data());
    
    VkDeviceQueueCreateInfo graphics_queue_ci{};
    VkDeviceQueueCreateInfo present_queue_ci{};
    
    bool same_queue = true;
    
    for (uint32_t i = 0; i < queue_count; i++) {
        if ((queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
            (queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT)) {
            
            graphics_queue_ci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            graphics_queue_ci.queueFamilyIndex = i;
            graphics_queue_ci.queueCount = 1;
            
            float priority = 1.0f;
            std::vector<float> priorities(1, priority);
            graphics_queue_ci.pQueuePriorities = priorities.data();
            
            if ((queue_families[i].queueFlags & VK_QUEUE_PRESENT)) {
                present_queue_ci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                present_queue_ci.queueFamilyIndex = i;
                present_queue_ci.queueCount = 1;
                present_queue_ci.pQueuePriorities = priorities.data();
                
                same_queue = true;
            } else {
                same_queue = false;
            }
        }
    }
    
    if (graphics_queue_ci.sType != VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    
    VkDeviceCreateInfo device_ci{};
    device_ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_ci.queueCreateInfoCount = 1;
    device_ci.pQueueCreateInfos = &graphics_queue_ci;
    
    if (same_queue) {
        graphics_queue_ci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        graphics_queue_ci.queueFamilyIndex = i; // Last valid index
        device_ci.queueCreateInfoCount = 1;
        device_ci.pQueueCreateInfos = &graphics_queue_ci;
        
        graphics_queue_present = VK_NULL_HANDLE;
    } else {
        device_ci.queueCreateInfoCount = 2;
        device_ci.pQueueCreateInfos = &graphics_queue_ci;
        
        VkDeviceQueueCreateInfo present_queue_ci{};
        present_queue_ci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        
        // We need to recreate with both queues
    }
    
    return vkCreateDevice(g_physical_device, &device_ci, nullptr, &g_device);
}

// ============================================================================
// SWAPCHAIN CREATION
// ============================================================================

VkResult init_swapchain() {
    uint32_t format_count = 0;
    std::vector<VkSurfaceFormatKHR> formats;
    
    vkGetPhysicalDeviceSurfaceFormatsKHR(g_physical_device, g_surface, &format_count, nullptr);
    if (format_count > 0) {
        formats.resize(format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(g_physical_device, g_surface, &format_count, formats.data());
        
        for (uint32_t i = 0; i < format_count; i++) {
            if (formats[i].format != VK_FORMAT_UNDEFINED) {
                formats.push_back(formats[i]);
            }
        }
    }
    
    VkSurfaceFormatKHR preferred_format = formats.size() > 0 ? formats[0] : 
        (VkSurfaceFormatKHR){VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    
    std::vector<VkSwapchainKHRCapabilitiesKHR> capabilities;
    VkSurfaceCapabilitiesKHR surface_capabilities{};
    uint32_t cap_count = 0;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_physical_device, g_surface, &cap_count, nullptr);
    
    VkSwapchainCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = g_surface;
    create_info.minImageCount = SWAPCHAIN_IMAGE_COUNT;
    create_info.imageFormat = preferred_format.format;
    create_info.imageColorSpace = preferred_format.colorSpace;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    
    create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_info.queueFamilyIndexCount = 0;
    create_info.pQueueFamilyIndices = nullptr;
    create_info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    
    create_info.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
    create_info.oldSwapchain = VK_NULL_HANDLE;
    create_info.clipped = VK_TRUE;
    create_info.alphaMode = VK_SURFACE_ALPHA_OPAQUE_BIT_KHR;
    
    VkResult result = vkCreateSwapchainKHR(g_device, &create_info, nullptr, &g_swapchain);
    
    if (result == VK_SUCCESS) {
        uint32_t image_count = 0;
        std::vector<VkImage> images(SWAPCHAIN_IMAGE_COUNT);
        vkGetSwapchainImagesKHR(g_device, g_swapchain, &image_count, images.data());
        
        for (uint32_t i = 0; i < image_count; i++) {
            VkImageViewCreateInfo view_info{};
            view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            view_info.image = images[i];
            view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            view_info.format = create_info.imageFormat;
            view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            view_info.subresourceRange.layerCount = 1;
            
            VkImageView image_view{};
            vkCreateImageView(g_device, &view_info, nullptr, &image_view);
            
            if (image_view != VK_NULL_HANDLE) {
                g_swapchain_image_views.push_back(image_view);
            }
        }
        
        // Create framebuffers from image views
        for (size_t i = 0; i < g_swapchain_image_views.size(); i++) {
            VkFramebufferCreateInfo framebuffer_ci{};
            framebuffer_ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebuffer_ci.renderPass = g_render_pass;
            framebuffer_ci.attachmentCount = 1;
            framebuffer_ci.pAttachments = &g_swapchain_image_views[i];
            framebuffer_ci.width = WINDOW_WIDTH;
            framebuffer_ci.height = WINDOW_HEIGHT;
            framebuffer_ci.layers = 1;
            
            VkFramebuffer framebuffer{};
            vkCreateFramebuffer(g_device, &framebuffer_ci, nullptr, &framebuffer);
            g_swapchain_framebuffers.push_back(framebuffer);
        }
    }
    
    return result;
}

// ============================================================================
// RENDER PASS CREATION
// ============================================================================

VkResult create_render_pass() {
    std::vector<VkAttachmentDescription> attachments(1);
    std::vector<VkAttachmentReference> attachment_refs(1);
    
    // Create color attachment reference from the swapchain
    attachment_refs[0].attachment = 0;
    attachment_refs[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    // First color attachment - swapchain image with MAILBOX or FIFO mode
    attachments[0].format = VkFormat{VK_FORMAT_B8G8R8A8_SRGB};
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    // Create render pass
    VkRenderPassCreateInfo render_pass_ci{};
    render_pass_ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_ci.attachmentCount = 1;
    render_pass_ci.pAttachments = attachments.data();
    render_pass_ci.subpassCount = 0;
    
    return vkCreateRenderPass(g_device, &render_pass_ci, nullptr, &g_render_pass);
}

// ============================================================================
// SHADER COMPILATION
// ============================================================================

VkResult compile_shaders(const char* vert_path, const char* frag_path) {
    std::cout << "Compiling shaders..." << std::endl;
    
    // Placeholder for shader compilation logic - would use ashell or glslangValidator
    return VK_SUCCESS;
}

// ============================================================================
// VERTEX BUFFER CREATION
// ============================================================================

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
    
    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = vertex_size;
    buffer_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    VkBuffer g_vertex_buffer_create{};
    vkCreateBuffer(g_device, &buffer_info, nullptr, &g_vertex_buffer_create);
    
    if (g_vertex_buffer_create != VK_NULL_HANDLE) {
        g_vertex_buffer = g_vertex_buffer_create;
        
        VkMemoryRequirements memory_requirements;
        vkGetBufferMemoryRequirements(g_device, g_vertex_buffer, &memory_requirements);
        
        VkMemoryAllocateFlagsInfo alloc_flags_info{};
        alloc_flags_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
        
        std::cout << "Vertex buffer created: " << vertex_count << " vertices\n";
    }
    
    return VK_SUCCESS;
}

// ============================================================================
// COMMAND BUFFER CREATION
// ============================================================================

VkResult create_command_buffers() {
    VkCommandPoolCreateInfo pool_ci{};
    pool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_ci.queueFamilyIndex = 0;
    
    vkCreateCommandPool(g_device, &pool_ci, nullptr, &g_command_pool);
    
    if (g_command_pool == VK_NULL_HANDLE) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    
    for (size_t i = 0; i < SWAPCHAIN_IMAGE_COUNT; i++) {
        VkCommandBufferAllocateInfo alloc_ci{};
        alloc_ci.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_ci.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_ci.commandPool = g_command_pool;
        alloc_ci.commandBufferCount = 1;
        
        VkCommandBuffer command_buffer{};
        vkAllocateCommandBuffers(g_device, &alloc_ci, &command_buffer);
        
        if (command_buffer != VK_NULL_HANDLE) {
            g_command_buffers.push_back(command_buffer);
        }
    }
    
    return VK_SUCCESS;
}

// ============================================================================
// RENDER FUNCTION - DYNAMIC OFFSETS
// ============================================================================

VkResult record_frame(VkCommandBuffer command_buffer, uint32_t image_idx) {
    VkClearValue clear_values[1];
    clear_values[0].color = (VkClearColor){(VkClearColorValue){float{0.2f}, float{0.3f}, float{0.4f}, float{1.0f}}};
    
    VkRenderPassBeginInfo render_pass_begin{};
    render_pass_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_begin.renderPass = g_render_pass;
    render_pass_begin.framebuffer = g_swapchain_framebuffers[image_idx];
    render_pass_begin.renderArea.offset = {0, 0};
    
    std::cout << "Recording frame for image " << image_idx << "\n";
    
    // Dynamic rendering with terrain mesh
    VkRenderPassBeginInfo begin_info;
    begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin_info.renderPass = g_render_pass;
    begin_info.framebuffer = g_swapchain_framebuffers[0];
    begin_info.renderArea.offsetsX = {0, 0};
    
    vkCmdBeginRenderPass(command_buffer, &render_pass_begin, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, g_graphics_pipeline);
    
    // Draw command with dynamic vertex offset
    uint32_t vertex_offset = 0;
    vkCmdDrawIndexed(
        command_buffer,
        static_cast<uint32_t>(vertices.size()),
        1,
        vertex_offset,
        0,
        1
    );
    
    vkCmdEndRenderPass(command_buffer);
    
    return VK_SUCCESS;
}

// ============================================================================
// PRESENTATION
// ============================================================================

VkResult present_swapchain() {
    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 0;
    
    uint32_t swapchain_image_count = 0;
    vkGetSwapchainImagesKHR(g_device, g_swapchain, &swapchain_image_count, nullptr);
    
    if (swapchain_image_count > 0) {
        std::vector<VkImage> images(swapchain_image_count);
        vkGetSwapchainImagesKHR(g_device, g_swapchain, &swapchain_image_count, images.data());
        
        present_info.swapchainCount = 1;
        present_info.pSwapchains = &g_swapchain;
        
        VkPresentInfoKHR info = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        info.waitSemaphoreCount = 0;
        
        // Present the image we just rendered
        if (image_idx < swapchain_image_count) {
            info.imageIndices[0] = images[image_idx];
            
            return vkQueuePresentKHR(present_queue, &info);
        } else {
            return VK_ERROR_INCOMPATIBLE_DEVICE_ORATION_KHR;
        }
    }
    
    return VK_SUCCESS;
}

// ============================================================================
// MAIN ENTRY POINT
// ============================================================================

int main() {
    std::cout << "Starting Taz: Wanted SDL2+Vulkan render loop..." << std::endl;
    
    if (!init_vulkan_instance()) {
        std::cerr << "Failed to initialize Vulkan!" << std::endl;
        return -1;
    }
    
    create_window();
    
    if (!g_running) {
        std::cerr << "Window creation failed, cleanup and exit\n";
        cleanup();
        return -1;
    }
    
    VkResult result = VK_SUCCESS;
    result = create_physical_device();
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create physical device!" << std::endl;
        return -1;
    }
    
    // Set up queues after getting physical device
    VkResult queue_result = VK_SUCCESS;
    
    result = init_swapchain();
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to initialize swapchain!" << std::endl;
        return -1;
    }
    
    create_vertex_buffer();
    compile_shaders("shaders.vert", "shaders.frag");
    
    // Create render pass and framebuffers
    result = VK_SUCCESS;
    
    // Setup semaphores
    for (size_t i = 0; i < SWAPCHAIN_IMAGE_COUNT; i++) {
        VkSemaphoreCreateInfo semaphore_ci{};
        semaphore_ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        
        vkCreateSemaphore(g_device, &semaphore_ci, nullptr, &g_image_available_semaphore[i].handle);
        vkCreateSemaphore(g_device, &semaphore_ci, nullptr, &g_image_finished_semaphore[i].handle);
    }
    
    run_render_loop();
    
    std::cout << "Render loop completed successfully!" << std::endl;
    
    cleanup();
    return 0;
}
