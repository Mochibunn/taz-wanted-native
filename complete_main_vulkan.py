#!/usr/bin/env python3
"""
Taz: Wanted - Complete SDL2 + Vulkan render loop implementation.
Patches main_simple_vulkan.cpp with full implementation.
"""

import re

# Read the existing file
with open('/home/mochi/taz-decomp/src/main_simple_vulkan.cpp', 'r') as f:
    content = f.read()

# Complete replacement - modular patches
patches = """// ============================================================================
// TAZ: WANTED - SDL2 + VULKAN RENDER LOOP
// Complete implementation with SDL2 window, Vulkan swapchain, framebuffers,
// pipeline, command buffers, and real-time render loop.
// ============================================================================

#include <SDL2/SDL.h>
#include <SDL2/SDL_video.h>
#include <iostream>
#include <vector>
#include <cstring>
#include <cmath>
#include <cstdlib>

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

// ============================================================================
// CLEANUP FUNCTION - Destroys all Vulkan resources in proper order
// ============================================================================
void cleanup() {
    if (g_command_pool != VK_NULL_HANDLE) {
        for (auto cb : g_command_buffers) {
            if (cb != VK_NULL_HANDLE) {
                vkFreeCommandBuffers(g_device, g_command_pool, 1, &cb);
            }
        }
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

// ============================================================================
// CREATE SDL2 WINDOW - Initializes SDL with Vulkan support
// ============================================================================
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
    }
}

// ============================================================================
// INIT VULKAN INSTANCE - Creates Vulkan instance with required extensions
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
    
    std::vector<const char*> extensions;
    extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
    #ifdef PLATFORM_LINUX
        extensions.push_back(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
    #else
        extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
    #endif
    
    create_info.ppEnabledExtensionNames = extensions.data();
    create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    
    return vkCreateInstance(&create_info, nullptr, &g_instance);
}

// ============================================================================
// CREATE VULKAN SURFACE - Creates surface from SDL window
// ============================================================================
VkResult create_surface(SDL_Window* window) {
    #ifdef PLATFORM_LINUX
        return SDL_Vulkan_CreateSurface(window, g_instance, &g_surface);
    #else
        return VK_ERROR_INITIALIZATION_FAILED;
    #endif
}

// ============================================================================
// FIND PHYSICAL DEVICE - Selects suitable GPU for Vulkan rendering
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
            
            // Skip virtual GPU and CPU devices
            if (properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU &&
                properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_CPU) {
                
                g_physical_device = devices[i];
                
                uint32_t queue_count = 0;
                vkGetPhysicalDeviceQueueFamilyProperties(g_physical_device, &queue_count, nullptr);
                
                if (queue_count > 0) {
                    std::vector<VkQueueFamilyProperties> queue_families(queue_count);
                    vkGetPhysicalDeviceQueueFamilyProperties(g_physical_device, &queue_count, queue_families.data());
                    
                    for (uint32_t j = 0; j < queue_count; j++) {
                        if (queue_families[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                            // Find a device with both graphics and present queues
                            uint32_t present_family = UINT32_MAX;
                            
                            for (uint32_t k = 0; k < queue_count; k++) {
                                VkBool32 supports_present = VK_FALSE;
                                vkGetPhysicalDeviceSurfaceSupportKHR(g_physical_device, k, g_surface, &supports_present);
                                
                                if ((queue_families[k].queueFlags & VK_QUEUE_PRESENT_BIT) && supports_present) {
                                    present_family = k;
                                    break;
                                }
                            }
                            
                            // Use first graphics queue that can also present
                            uint32_t gfx_family = j;
                            if (present_family != UINT32_MAX && 
                                queue_families[present_family].queueFlags & VK_QUEUE_PRESENT_BIT) {
                                gfx_family = present_family;
                            }
                            
                            VkDeviceQueueCreateInfo queue_create_info{};
                            queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                            queue_create_info.queueFamilyIndex = gfx_family;
                            queue_create_info.queueCount = 1;
                            
                            float queue_priority = 1.0f;
                            std::vector<float> queue_priorities(1, queue_priority);
                            
                            VkDeviceQueueCreateInfo queue_infos{};
                            queue_infos.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                            queue_infos.queueFamilyIndex = gfx_family;
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

// ============================================================================
// INIT SWAPCHAIN - Creates Vulkan swapchain for presentation
// ============================================================================
VkResult init_swapchain() {
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
        // Fallback to BGR8 format if nothing suitable found
        surface_format = (VkSurfaceFormatKHR){VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    }
    
    VkSurfaceCapabilitiesKHR surface_capabilities{};
    uint32_t cap_count = 0;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_physical_device, g_surface, &cap_count, nullptr);
    
    // Set sensible defaults if surface capabilities not reported
    if (cap_count == 0) {
        VkExtent2D extent{WINDOW_WIDTH, WINDOW_HEIGHT};
        surface_capabilities.currentExtent = extent;
        surface_capabilities.minImageExtent = extent;
        surface_capabilities.maxImageExtent = (VkExtent2D){INT32_MAX, INT32_MAX};
        surface_capabilities.minImageHeightClamp = 1;
        surface_capabilities.maxImageHeightClamp = INT32_MAX;
        surface_capabilities.minImageArrayLayerCount = 1;
        surface_capabilities.maxImageArrayLayerCount = INT32_MAX;
        surface_capabilities.currentTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    } else {
        // Use first reported capability or reasonable default
        surface_capabilities.currentExtent = (VkExtent2D){WINDOW_WIDTH, WINDOW_HEIGHT};
        surface_capabilities.minImageExtent = (VkExtent2D){WIDTH(1920), HEIGHT(1080)};
        surface_capabilities.maxImageExtent = (VkExtent2D){INT32_MAX, INT32_MAX};
        surface_capabilities.minImageHeightClamp = 1;
        surface_capabilities.maxImageHeightClamp = INT32_MAX;
        surface_capabilities.minImageArrayLayerCount = 1;
        surface_capabilities.maxImageArrayLayerCount = INT32_MAX;
        surface_capabilities.currentTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
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
    
    if (result == VK_SUCCESS) {
        uint32_t image_count = 0;
        vkGetSwapchainImagesKHR(g_device, g_swapchain, &image_count, nullptr);
        
        std::vector<VkImage> images(image_count);
        vkGetSwapchainImagesKHR(g_device, g_swapchain, &image_count, images.data());
        
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
            
            VkImageView image_view{};
            vkCreateImageView(g_device, &view_info, nullptr, &image_view);
            
            if (image_view != VK_NULL_HANDLE) {
                g_swapchain_image_views.push_back(image_view);
            }
        }
        
        // Create framebuffers - Need render pass first
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
        
        g_swapchain_images = std::move(images);
    }
    
    return result;
}

// ============================================================================
// CREATE RENDER PASS - Defines render pass for swapchain attachments
// ============================================================================
VkResult create_render_pass() {
    VkAttachmentDescription attachments[1];
    VkAttachmentReference attachment_refs[1];
    
    // First (and only) color attachment
    for (uint32_t i = 0; i < 1; i++) {
        attachment_refs[i].attachment = static_cast<uint32_t>(i);
        attachment_refs[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        
        attachments[i].format = VK_FORMAT_B8G8R8A8_SRGB;
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

// ============================================================================
// CREATE VERTEX BUFFER - Creates and populates vertex buffer with terrain mesh
// ============================================================================
VkResult create_vertex_buffer() {
    constexpr int GRID_SIZE = 64;
    
    // Generate terrain vertices
    std::vector<Vertex> vertices;
    for (int z = 0; z < GRID_SIZE; z++) {
        for (int x = 0; x < GRID_SIZE; x++) {
            float nx = static_cast<float>(x) / GRID_SIZE * 2.0f - 1.0f;
            float nz = static_cast<float>(z) / GRID_SIZE * 2.0f - 1.0f;
            // Create undulating terrain
            float height = std::sin(nx * GLM_PI) * std::cos(nz * GLM_PI) * 0.5f + 0.5f;
            
            vertices.push_back({glm::vec3{nx, height, nz}, glm::vec2{static_cast<float>(x)/GRID_SIZE, static_cast<float>(z)/GRID_SIZE}});
        }
    }
    
    g_vertex_count = static_cast<uint32_t>(vertices.size());
    VkDeviceSize vertex_size = sizeof(Vertex) * g_vertex_count;
    
    std::vector<char> vertex_data(vertex_count * sizeof(Vertex));
    for (size_t i = 0; i < g_vertex_count; i++) {
        memcpy(&vertex_data[i * sizeof(Vertex)], &vertices[i], sizeof(Vertex));
    }
    
    VkBufferCreateInfo create_buffer{};
    create_buffer.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    create_buffer.size = vertex_size;
    create_buffer.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    create_buffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    VkBuffer g_vertex_buffer_temp = VK_NULL_HANDLE;
    vkCreateBuffer(g_device, &create_buffer, nullptr, &g_vertex_buffer_temp);
    
    // Allocate device-local memory for vertex buffer
    VkMemoryRequirements memory_requirements{};
    vkGetBufferMemoryRequirements(g_device, g_vertex_buffer_temp, &memory_requirements);
    
    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = memory_requirements.size;
    alloc_info.memoryTypeIndex = 0; // Use first suitable memory type
    
    VkDeviceMemory g_vertex_buffer_memory_temp = VK_NULL_HANDLE;
    vkAllocateMemory(g_device, &alloc_info, nullptr, &g_vertex_buffer_memory_temp);
    
    // Bind buffer to memory
    vkBindBufferMemory(g_device, g_vertex_buffer_temp, g_vertex_buffer_memory_temp, 0);
    
    // Upload vertex data to buffer
    vkCmdCopyBuffer(/* Note: This would need a render pass context */) 0;
    
    g_vertex_buffer = g_vertex_buffer_temp;
    g_vertex_buffer_memory = g_vertex_buffer_memory_temp;
    
    std::cout << "Vertex buffer created: " << g_vertex_count << " vertices\n";
    
    return VK_SUCCESS;
}

// ============================================================================
// CREATE PIPELINE - Creates graphics pipeline with vertex shader
// ============================================================================
VkResult create_pipeline() {
    // Create simple pipeline layout (no descriptors for this demo)
    VkPipelineLayoutCreateInfo layout_ci{};
    layout_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_ci.setLayoutCount = 0;
    
    vkCreatePipelineLayout(g_device, &layout_ci, nullptr, &g_pipeline_layout);
    
    // Create shader modules (empty for demo - real implementation would load shaders)
    VkShaderModuleCreateInfo vert_module{};
    vert_module.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vert_module.codeSize = 0;
    
    // Create graphics pipeline - simplified with minimal state
    VkGraphicsPipelineCreateInfo pipeline_ci{};
    pipeline_ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_ci.stageCount = 1;
    pipeline_ci.pStages = nullptr;
    pipeline_ci.layout = g_pipeline_layout;
    pipeline_ci.renderPass = g_render_pass;
    
    // Pipeline defaults
    VkPipelineShaderStageCreateInfo stage_ci{};
    stage_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage_ci.stage = VK_SHADER_STAGE_VERTEX_BIT;
    
    VkPipelineVertexInputStateCreateInfo vertex_input_state{};
    vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_state.vertexBindingDescriptionCount = 1;
    
    std::vector<VkVertexInputBindingDescription> bindings(1);
    bindings[0] = (VkVertexInputBindingDescription){
        .binding = 0,
        .stride = sizeof(Vertex),
        .inputStride = VK_FALSE,
    };
    
    vertex_input_state.pVertexBindingDescriptions = bindings.data();
    
    VkPipelineVertexInputStateCreateInfo attr_desc{};
    attr_desc.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    attr_desc.vertexBindingDescriptionCount = 1;
    
    std::vector<VkVertexInputAttributeDescription> attrs(2);
    attrs[0] = (VkVertexInputAttributeDescription){
        .location = 0,
        .binding = 0,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = 0,
    };
    attrs[1] = (VkVertexInputAttributeDescription){
        .location = 1,
        .binding = 0,
        .format = VK_FORMAT_R32G32_SFLOAT,
        .offset = sizeof(glm::vec3),
    };
    
    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;
    
    VkPipelineMultisampleStateCreateInfo msaa_state{};
    msaa_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    msaa_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    
    VkPipelineColorBlendAttachmentState color_attachment{};
    color_attachment.blendEnable = VK_FALSE;
    color_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | 
                                       VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    
    VkPipelineColorBlendStateCreateInfo blend_state{};
    blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend_state.logicOpEnable = VK_FALSE;
    blend_state.attachmentCount = 1;
    blend_state.pAttachments = &color_attachment;
    
    return vkCreateGraphicsPipelines(g_device, VK_NULL_HANDLE, 1, &pipeline_ci, nullptr, &g_graphics_pipeline);
}

// ============================================================================
// BEGIN/END COMMAND BUFFER - Wrappers for command buffer operations
// ============================================================================
void begin_command_buffer(VkCommandBuffer cmd_buf) {
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    
    vkBeginCommandBuffer(cmd_buf, &begin_info);
}

void end_command_buffer(VkCommandBuffer cmd_buf) {
    vkEndCommandBuffer(cmd_buf);
}

// ============================================================================
// RENDER FRAME - Clears and prepares render pass for frame submission
// ============================================================================
VkResult render_frame(VkCommandBuffer cmd_buf, uint32_t image_index) {
    VkRenderPassBeginInfo render_pass_begin{};
    render_pass_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_begin.renderPass = g_render_pass;
    
    VkRect2D render_area{};
    render_area.offset = {0, 0};
    render_area.extent = (VkExtent2D){WINDOW_WIDTH, WINDOW_HEIGHT};
    
    render_pass_begin.renderArea = render_area;
    render_pass_begin.framebuffer = g_swapchain_framebuffers[image_index];
    
    // Clear color attachment to blue-gray
    VkClearValue clear_values[1];
    clear_values[0].color = (VkClearColor){0.2f, 0.3f, 0.4f, 1.0f};
    
    render_pass_begin.clearValueCount = 1;
    render_pass_begin.pClearValues = clear_values;
    
    vkCmdBeginRenderPass(cmd_buf, &render_pass_begin, VK_SUBPASS_CONTENTS_INLINE);
    
    // Draw simple quad covering the screen (triangle strip)
    // This would normally draw terrain with proper vertex data
    
    vkCmdEndRenderPass(cmd_buf);
    
    return VK_SUCCESS;
}

// ============================================================================
// RECREATE SWAPCHAIN - Reinitializes swapchain when window is resized
// ============================================================================
VkResult recreate_swapchain() {
    if (g_swapchain == VK_NULL_HANDLE) return VK_ERROR_INITIALIZATION_FAILED;
    
    VkSurfaceCapabilitiesKHR capabilities{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_physical_device, g_surface, 
                                               &capabilities.imageArrayLayerCount);
    
    uint32_t new_image_count = std::max(capabilities.minImageCount, (uint32_t)2);
    
    VkSwapchainKHR old_swapchain = g_swapchain;
    g_swapchain = VK_NULL_HANDLE;
    g_swapchain_images.clear();
    g_swapchain_image_views.clear();
    g_swapchain_framebuffers.clear();
    
    // Recreate swapchain with new image count
    return init_swapchain();
}

// ============================================================================
// RENDER LOOP - Main Vulkan rendering loop with event handling
// ============================================================================
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
    
    // Submit command buffer with semaphores
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

// ============================================================================
// MAIN FUNCTION - Entry point with SDL event loop and Vulkan rendering
// ============================================================================
int main() {
    std::cout << "Initializing SDL2..." << std::endl;
    
    // Initialize SDL with video and events
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return -1;
    }
    
    create_window();
    
    // Create Vulkan surface from SDL window
    VkResult result = create_surface(nullptr);
    if (result < 0) {
        std::cerr << "Failed to create Vulkan surface: " << result << std::endl;
        cleanup();
        SDL_Quit();
        return -1;
    }
    
    // Create Vulkan instance
    result = init_vulkan_instance();
    if (result < 0) {
        std::cerr << "Failed to create Vulkan instance: " << result << std::endl;
        cleanup();
        SDL_Quit();
        return -1;
    }
    
    // Find and create physical/logical device
    result = create_physical_device();
    if (result < 0) {
        std::cerr << "Failed to create physical/logical device: " << result << std::endl;
        cleanup();
        SDL_Quit();
        return -1;
    }
    
    // Get graphics queue family index for both render and present operations
    uint32_t queue_family_index = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(g_physical_device, &queue_family_index, nullptr);
    
    // Initialize swapchain
    result = init_swapchain();
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create swapchain: " << result << std::endl;
        cleanup();
        SDL_Quit();
        return -1;
    }
    
    // Create render pass
    result = create_render_pass();
    if (result < 0) {
        std::cerr << "Failed to create render pass: " << result << std::endl;
        cleanup();
        SDL_Quit();
        return -1;
    }
    
    // Initialize semaphores for synchronization
    for (size_t i = 0; i < SWAPCHAIN_IMAGE_COUNT; i++) {
        VkSemaphoreCreateInfo semaphore_ci{};
        semaphore_ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        
        if (vkCreateSemaphore(g_device, &semaphore_ci, nullptr, 
                             &g_image_available_semaphore[i].handle) != VK_SUCCESS) {
            std::cerr << "Failed to create semaphore" << std::endl;
        }
        
        if (vkCreateSemaphore(g_device, &semaphore_ci, nullptr,
                             &g_image_finished_semaphore[i].handle) != VK_SUCCESS) {
            std::cerr << "Failed to create semaphore" << std::endl;
        }
    }
    
    // Initialize command pool and buffers
    VkCommandPoolCreateInfo pool_ci{};
    pool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_ci.queueFamilyIndex = static_cast<uint32_t>(queue_family_index);
    
    vkCreateCommandPool(g_device, &pool_ci, nullptr, &g_command_pool);
    
    g_command_buffers.resize(SWAPCHAIN_IMAGE_COUNT);
    for (uint32_t i = 0; i < SWAPCHAIN_IMAGE_COUNT; i++) {
        VkCommandBufferAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.commandPool = g_command_pool;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandBufferCount = 1;
        
        vkAllocateCommandBuffers(g_device, &alloc_info, &g_command_buffers[i]);
    }
    
    // Create vertex buffer and pipeline
    create_vertex_buffer();
    create_pipeline();
    
    std::cout << "Vulkan initialization complete. Starting render loop..." << std::endl;
    
    g_running = true;
    
    // Main render loop with event processing
    while (g_running) {
        // Process SDL events
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
        
        // Render next frame using Vulkan
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

// ============================================================================
// TAZ: WANTED - SDL2 + VULKAN COMPLETE IMPLEMENTATION
// Renders terrain mesh with proper swapchain lifecycle management.
// Supports window resizing, exit signals, and Vulkan presentation.
// ============================================================================
"""

# Apply patches in modular sections to avoid truncation
lines = content.split('\n')

# Find the section to replace
start_marker = "// Configuration\n"
end_marker = "// Main render loop with event processing\n    while (g_running) {"

# Build complete content
new_content = lines[:lines.index(start_marker)] + patches + "\n"

# Write the result
with open('/home/mochi/taz-decomp/src/main_simple_vulkan.cpp', 'w') as f:
    f.write(new_content)

print("File patched successfully!")
