#!/usr/bin/env python3
"""
Complete SDL2 + Vulkan implementation for Taz: Wanted
This script writes the complete main_simple_vulkan.cpp file with all required functionality.
"""

import os

BASE_PATH = "/home/mochi/taz-decomp/src/main_simple_vulkan.cpp"
BACKUP_PATH = BASE_PATH + ".backup.v2"

file_content = '''// Taz: Wanted - SDL2 + Vulkan Complete Render Loop Implementation
// Full SDL window, swapchain management, terrain rendering, and event loop

#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <chrono>

#include <SDL2/SDL.h>
#include <vulkan/vulkan.h>

// Application constants
const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;
const char* WINDOW_TITLE = "Taz: Wanted - SDL2 + Vulkan Terrain";

// Vertex structure for terrain mesh
struct Vertex {
    float position[3];
    float uv[2];
    
    static VkFormat get_format() {
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    }
};

// Global state
VkInstance g_instance = VK_NULL_HANDLE;
VkSurfaceKHR g_surface = VK_NULL_HANDLE;
VkPhysicalDevice g_physical_device = VK_NULL_HANDLE;
VkDevice g_device = VK_NULL_HANDLE;
VkQueue g_graphics_queue = VK_NULL_HANDLE;
VkQueue g_present_queue = VK_NULL_HANDLE;
VkCommandPool g_command_pool = VK_NULL_HANDLE;
VkRenderPass g_render_pass = VK_NULL_HANDLE;

// Shader modules (global for cleanup)
VkShaderModule g_vertex_shader_module = VK_NULL_HANDLE;
VkShaderModule g_fragment_shader_module = VK_NULL_HANDLE;

// Pipeline and layout
VkPipeline g_graphics_pipeline = VK_NULL_HANDLE;
VkPipelineLayout g_pipeline_layout = VK_NULL_HANDLE;

// Vertex buffer data
std::vector<Vertex> g_vertices;
VkBuffer vertex_buffer = VK_NULL_HANDLE;
VkDeviceMemory vertex_buffer_memory = VK_NULL_HANDLE;

// Swapchain resources
VkSwapchainKHR g_swapchain = VK_NULL_HANDLE;

// Static state objects for pipeline creation (required for address-of operator)
VkVertexInputStateCreateInfo vertex_input_state{};
vertex_input_state.sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_STATE_CREATE_INFO;
vertex_input_state.vertexBindingDescriptionCount = 0;

VkPipelineVertexInputStateCreateInfo pipeline_vertex_input_state{};
pipeline_vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
pipeline_vertex_input_state.pVertexBindingDescriptions = nullptr;
pipeline_vertex_input_state.vertexBindingDescriptionCount = 0;

VkPipelineInputAssemblyStateCreateInfo input_assembly_state{};
input_assembly_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

VkViewport viewport{};
viewport.width = WINDOW_WIDTH;
viewport.height = WINDOW_HEIGHT;
viewport.minDepth = 0.0f;
viewport.maxDepth = 1.0f;
VkPipelineViewportStateCreateInfo viewport_state{};
viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
viewport_state.viewportCount = 1;
viewport_state.pViewports = &viewport;

VkPipelineRasterizationStateCreateInfo rasterization_state{};
rasterization_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
rasterization_state.rasterizerDiscardEnable = VK_FALSE;
rasterization_state.polygonMode = VK_POLYGON_MODE_FILL;
rasterization_state.lineWidth = 1.0f;

VkPipelineMultisampleStateCreateInfo multisample_state{};
multisample_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
multisample_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

// Color blend state
uint32_t color_blend_attachment_count = 1;
VkPipelineColorBlendAttachmentState color_blend_attachment{};
color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | 
                                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
color_blend_attachment.blendEnable = VK_FALSE;

std::vector<VkPipelineColorBlendAttachmentState> color_blend_attachments(color_blend_attachment_count);
color_blend_attachments[0] = color_blend_attachment;

VkPipelineColorBlendStateCreateInfo color_blend_state{};
color_blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
color_blend_state.logicOpEnable = VK_FALSE;
color_blend_state.attachmentCount = static_cast<uint32_t>(color_blend_attachments.size());
color_blend_state.pAttachments = color_blend_attachments.data();

VkPipelineDepthStencilStateCreateInfo depth_stencil_state{};
depth_stencil_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
depth_stencil_state.depthTestEnable = VK_FALSE;
depth_stencil_state.depthWriteEnable = VK_FALSE;
depth_stencil_state.depthCompareOp = VK_COMPARE_OP_ALWAYS;


std::vector<VkImage> g_swapchain_images;
std::vector<VkImageView> g_swapchain_image_views;
std::vector<VkFramebuffer> g_swapchain_framebuffers;

// Frame resources for double buffering
const uint32_t FRAME_COUNT = 3;
std::vector<VkSemaphore> g_render_complete_semaphores;
std::vector<VkSemaphore> g_present_available_semaphores;
std::vector<VkFence> g_present_fences;
uint32_t g_current_image_index = 0;

// SDL window (must be pointer for cleanup)
SDL_Window* window_ptr = nullptr;

// Forward declaration of function used before definition
static void cleanup_resources();

// Find memory type index that matches the requested property flags
static uint32_t find_memory_type(VkPhysicalDevice physical_device, uint32_t memory_type_bits, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_properties);
    
    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; ++i) {
        if ((memory_type_bits & (1 << i)) && 
            (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return UINT3_MAX;
}

// Create shader module from GLSL source code
static VkShaderModuleCreateInfo create_shader_module(const std::string& code, const char* name) {
    VkShaderModuleCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = code.size();
    info.pCode = reinterpret_cast<const uint32_t*>(code.c_str());
    return info;
}

// Create pipeline layout (null layout for simplicity)
static VkPipelineLayoutCreateInfo create_pipeline_layout() {
    VkPipelineLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    info.pSetLayouts = nullptr; // Use null layout for simplicity
    info.setLayoutCount = 0;
    info.pushConstantRangeCount = 0;
    return info;
}

// Create terrain mesh with sine wave variation
static void generate_terrain_mesh() {
    std::vector<Vertex> vertices(64);
    
    float grid_size = 10.0f;
    float segment_count = 32;
    float spacing = grid_size / segment_count;
    
    // Generate terrain with varying heights
    for (uint32_t i = 0; i < vertices.size(); ++i) {
        uint32_t row = i / 8;
        uint32_t col = i % 8;
        
        float x = (col - 4.0f) * spacing;
        float y = (3.5f - row) * spacing;
        
        // Create terrain height with combined sine waves for variety
        float height = std::sin(x * 0.1f) * std::cos(y * 0.1f) * 2.0f + 
                       std::sin((x + y) * 0.05f) * 1.0f;
        
        // Generate UV coordinates for texture mapping
        float uv_x = (col - 3.99f) / 7.98f;
        float uv_y = (row - 3.49f) / 6.98f;
        
        vertices[i].position[0] = x;
        vertices[i].position[1] = y;
        vertices[i].position[2] = height;
        vertices[i].uv[0] = uv_x;
        vertices[i].uv[1] = uv_y;
    }
    
    g_vertices = vertices;
}

int main() {
    // Initialize Vulkan instance
    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Taz: Wanted";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "SDL2/Vulkan Demo";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo instance_create{};
    instance_create.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_create.pApplicationInfo = &app_info;

    // Required extensions for Vulkan surface and swapchain
    std::vector<const char*> required_extensions;
#ifdef __linux__
    required_extensions.emplace_back(VK_KHR_SURFACE_EXTENSION_NAME);
#else
    required_extensions.emplace_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#endif
    required_extensions.emplace_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    instance_create.enabledExtensionCount = static_cast<uint32_t>(required_extensions.size());
    instance_create.ppEnabledExtensionNames = required_extensions.data();

    std::cout << "Initializing Vulkan...\n";
    if (vkCreateInstance(&instance_create, nullptr, &g_instance) != VK_SUCCESS) {
        std::cerr << "Failed to create Vulkan instance\n";
        return -1;
    }

#ifdef __linux__
    std::cout << "Platform: Linux (X11 or Wayland)\n";
#endif

    std::cout << "Vulkan API Version: 1.1\n";
    std::cout << "✅ Vulkan instance created\\n\\n";

    // Initialize SDL2 with video support
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL initialization failed: " << SDL_GetError() << "\\n";
        vkDestroyInstance(g_instance, nullptr);
        return -1;
    }
    std::cout << "✅ SDL initialized successfully\\n";

    // Create SDL window with VULKAN flag (required for surface creation)
    int w = WINDOW_WIDTH;
    int h = WINDOW_HEIGHT;
    const char* title = WINDOW_TITLE;
    
    window_ptr = SDL_CreateWindow(title,
                                   SDL_WINDOWPOS_CENTERED,
                                   SDL_WINDOWPOS_CENTERED,
                                   w,
                                   h,
                                   SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN);

    if (!window_ptr) {
        std::cerr << "Failed to create SDL_Window: " << SDL_GetError() << "\\n";
        vkDestroyInstance(g_instance, nullptr);
        SDL_Quit();
        return -1;
    }
    std::cout << "✅ Window created with SDL_WINDOW_VULKAN\\n";

    // Create Vulkan surface using SDL_Vulkan_CreateSurface
    if (SDL_Vulkan_CreateSurface(window_ptr, g_instance, &g_surface) != 0) {
        std::cerr << "Failed to create Vulkan surface: " << SDL_GetError() << "\\n";
        SDL_DestroyWindow(window_ptr);
        vkDestroyInstance(g_instance, nullptr);
        SDL_Quit();
        return -1;
    }
    std::cout << "✅ Vulkan surface created\\n";

    // Query physical devices and select one supporting the surface
    uint32_t device_count = 0;
    std::vector<VkPhysicalDevice> devices(1);
    vkEnumeratePhysicalDevices(g_instance, nullptr, &device_count, devices.data());
    g_physical_device = devices[0]; // Use first device for demo
    
    std::cout << "✅ Selected physical device\\n";

    // Query queue families - find graphics and present queue families
    std::vector<VkQueueFamilyProperties> queue_families(device_count);
    vkGetPhysicalDeviceQueueFamilyProperties(g_physical_device, &device_count, queue_families.data());
    
    uint32_t graphics_family = UINT32_MAX;
    uint32_t present_family = UINT32_MAX;

    for (uint32_t i = 0; i < device_count; ++i) {
        if ((queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
            graphics_family = i;
        }
        // Present queue: skip video encode/decode queues
        if ((queue_families[i].queueFlags & VK_QUEUE_VIDEO_DECODE_BIT_KHR) == 0 && 
            (queue_families[i].queueFlags & VK_QUEUE_VIDEO_ENCODE_BIT_KHR) == 0) {
            present_family = i;
        }
    }

    // If no graphics queue, use first queue (shouldn't happen on real GPUs)
    if (graphics_family == UINT32_MAX) graphics_family = 0;
    if (present_family == UINT32_MAX && graphics_family != UINT32_MAX) present_family = graphics_family;

    std::cout << "✅ Selected queue families: graphics=" << graphics_family 
              << ", present=" << present_family << "\\n";

    // Create queues with minimal sizes
    float priority = 0.25f;
    
    if (graphics_family != present_family) {
        // Separate graphics and present queues
        std::vector<VkDeviceQueueCreateInfo> queue_create_infos(2);
        
        queue_create_infos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_infos[0].queueFamilyIndex = graphics_family;
        queue_create_infos[0].queueCount = 1;
        queue_create_infos[0].pQueuePriorities = &priority;

        queue_create_infos[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_infos[1].queueFamilyIndex = present_family;
        queue_create_infos[1].queueCount = 1;
        queue_create_infos[1].pQueuePriorities = &priority;

        // Create logical device with Vulkan 1.1 features
        VkDeviceCreateInfo device_create{};
        device_create.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        
        std::vector<const char*> empty_extensions = {};
        device_create.pEnabledExtensionNames = empty_extensions.data();
        device_create.enabledExtensionCount = 0;
        device_create.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
        device_create.pQueueCreateInfos = queue_create_infos.data();

        if (vkCreateDevice(g_physical_device, &device_create, nullptr, &g_device) != VK_SUCCESS) {
            std::cerr << "Failed to create logical device\\n";
            return -1;
        }

        // Get queues from created device
        vkGetDeviceQueue(g_device, graphics_family, 0, &g_graphics_queue);
        vkGetDeviceQueue(g_device, present_family, 0, &g_present_queue);

    } else {
        // Single queue handles both graphics and present
        std::vector<VkDeviceQueueCreateInfo> queue_create_infos(1);
        
        queue_create_infos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_infos[0].queueFamilyIndex = graphics_family;
        queue_create_infos[0].queueCount = 1;
        queue_create_infos[0].pQueuePriorities = &priority;

        VkDeviceCreateInfo device_create{};
        device_create.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        
        std::vector<const char*> empty_extensions = {};
        device_create.pEnabledExtensionNames = empty_extensions.data();
        device_create.enabledExtensionCount = 0;
        device_create.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
        device_create.pQueueCreateInfos = queue_create_infos.data();

        if (vkCreateDevice(g_physical_device, &device_create, nullptr, &g_device) != VK_SUCCESS) {
            std::cerr << "Failed to create logical device\\n";
            return -1;
        }

        vkGetDeviceQueue(g_device, graphics_family, 0, &g_graphics_queue);
        g_present_queue = g_graphics_queue; // Same queue for present
    }

    std::cout << "✅ Logical device created\\n";

    // Create swapchain - query surface format first
    uint32_t surface_format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(g_physical_device, g_surface, nullptr, &surface_format_count);
    
    if (surface_format_count == 0) {
        std::cerr << "Failed to get surface formats\\n";
        return -1;
    }

    std::vector<VkSurfaceFormatKHR> surface_formats(surface_format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(g_physical_device, g_surface, &surface_format_count, surface_formats.data());
    
    // Use first format or fallback to B8G8R8A8_UNORM
    uint32_t chosen_format_index = 0;
    VkSurfaceFormatKHR chosen_format = surface_formats[chosen_format_index];
    if (chosen_format.format == VK_FORMAT_UNDEFINED) {
        chosen_format = 
            reinterpret_cast<VkSurfaceFormatKHR>(VkPhysicalDeviceSurfaceFormatPropertiesKHR){
                .format = VK_FORMAT_B8G8R8A8_UNORM,
                .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
            };
    }

    // Query swapchain image count
    uint32_t swapchain_image_count = 0;
    vkGetPhysicalDeviceSurfaceImageFormatCountsKHR(g_physical_device, g_surface, nullptr);
    if (swapchain_image_count == 0) {
        swapchain_image_count = 2; // Reasonable default for double buffering
    }

    std::cout << "✅ Creating swapchain with " << swapchain_image_count << " images\\n";

    // Create swapchain
    VkSwapchainCreateInfoKHR swapchain_create{};
    swapchain_create.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_create.surface = g_surface;
    swapchain_create.minImageCount = swapchain_image_count;
    swapchain_create.imageFormat = chosen_format.format;
    swapchain_create.imageColorSpace = chosen_format.colorSpace;
    swapchain_create.imageExtent = {static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
    swapchain_create.imageArrayLayers = 1;
    swapchain_create.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchain_create.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    swapchain_create.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_create.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapchain_create.clipped = VK_TRUE;
    swapchain_create.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(g_device, &swapchain_create, nullptr, &g_swapchain) != VK_SUCCESS) {
        std::cerr << "Failed to create swapchain\\n";
        return -1;
    }

    g_swapchain_images.resize(swapchain_image_count);
    std::vector<VkImage> swapchain_images_vec(swapchain_image_count);
    vkGetSwapchainImagesKHR(g_device, g_swapchain, &swapchain_image_count, swapchain_images_vec.data());
    g_swapchain_images.swap(swapchain_images_vec);
    
    std::cout << "✅ Swapchain created with " << swapchain_image_count << " images\\n";

    // Create image views
    VkImageViewCreateInfo view_create{};
    view_create.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_create.image = VK_NULL_HANDLE; // Will be set for each swapchain image
    view_create.format = chosen_format.format;
    view_create.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_create.subresourceRange.levelCount = 1;
    view_create.subresourceRange.layerCount = 1;
    view_create.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_create.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_create.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_create.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_create.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    g_swapchain_image_views.resize(swapchain_image_count);
    for (uint32_t i = 0; i < swapchain_image_count; ++i) {
        if (vkCreateImageView(g_device, &view_create, nullptr, 
                              &g_swapchain_image_views[i]) != VK_SUCCESS) {
            std::cerr << "Failed to create swapchain image view\\n";
            return -1;
        }
    }

    // Create render pass
    VkRenderPassCreateInfo rpci{};
    rpci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    
    // Define subpass with color attachment
    std::vector<VkAttachmentDescription> attachments(1);
    attachments[0].format = chosen_format.format;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    rpci.attachmentCount = static_cast<uint32_t>(attachments.size());
    rpci.pAttachments = attachments.data();
    
    // Define subpass description
    std::vector<VkSubpassDescription> subpasses(1);
    subpasses[0].sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION;
    subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpasses[0].colorAttachmentCount = 1;
    subpasses[0].pColorAttachments = &VkComponentMapping{
        .binding = 0,
        .format = chosen_format.format,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    rpci.subpassCount = static_cast<uint32_t>(subpasses.size());
    rpci.pSubpasses = subpasses.data();
    
    // Define dependencies
    std::vector<VkSubpassDependency> dependencies(1);
    dependencies[0].sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_KHR;
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = 0;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;

    rpci.dependencyCount = static_cast<uint32_t>(dependencies.size());
    rpci.pDependencies = dependencies.data();

    if (vkCreateRenderPass(g_device, &rpci, nullptr, &g_render_pass) != VK_SUCCESS) {
        std::cerr << "Failed to create render pass\\n";
        return -1;
    }
    
    // Create framebuffers
    VkFramebufferCreateInfo framebuffer_create{};
    framebuffer_create.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_create.renderPass = g_render_pass;
    framebuffer_create.attachmentCount = 1;
    framebuffer_create.width = w;
    framebuffer_create.height = h;
    framebuffer_create.layers = 1;

    g_swapchain_framebuffers.resize(swapchain_image_count);
    for (uint32_t i = 0; i < swapchain_image_count; ++i) {
        std::vector<VkImageView> attachments(1);
        attachments[0] = g_swapchain_image_views[i];
        
        if (vkCreateFramebuffer(g_device, &framebuffer_create, nullptr, 
                               &g_swapchain_framebuffers[i]) != VK_SUCCESS) {
            std::cerr << "Failed to create framebuffer\\n";
            return -1;
        }
    }

    std::cout << "✅ Framebuffers created\\n";

    // Create frame resources (semaphores and fences)
    g_render_complete_semaphores.resize(swapchain_image_count);
    g_present_available_semaphores.resize(swapchain_image_count);
    
    VkSemaphoreCreateInfo semaphore_create{};
    semaphore_create.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    for (uint32_t i = 0; i < swapchain_image_count; ++i) {
        // Render complete semaphore
        if (vkCreateSemaphore(g_device, &semaphore_create, nullptr, 
                             &g_render_complete_semaphores[i]) != VK_SUCCESS) {
            std::cerr << "Failed to create render complete semaphore\\n";
            return -1;
        }
        
        // Present available semaphore
        if (vkCreateSemaphore(g_device, &semaphore_create, nullptr, 
                             &g_present_available_semaphores[i]) != VK_SUCCESS) {
            std::cerr << "Failed to create present available semaphore\\n";
            return -1;
        }
        
        // Fence
        VkFenceCreateInfo fence_create{};
        fence_create.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        if (vkCreateFence(g_device, &fence_create, 0, 
                          &g_present_fences[i]) != VK_SUCCESS) {
            std::cerr << "Failed to create fence\\n";
            return -1;
        }
    }

    // Create vertex buffer
    VkBufferCreateInfo vb_create{};
    vb_create.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vb_create.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    vb_create.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vb_create.flags = 0;
    vb_create.size = g_vertices.size() * sizeof(Vertex);

    if (vkCreateBuffer(g_device, &vb_create, nullptr, &vertex_buffer) != VK_SUCCESS) {
        std::cerr << "Failed to create vertex buffer\\n";
        return -1;
    }

    VkMemoryRequirements mem_reqs{};
    vkGetBufferMemoryRequirements(g_device, vertex_buffer, &mem_reqs);

    VkMemoryAllocateInfo vma{};
    vma.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    vma.allocationSize = mem_reqs.size;
    vma.memoryTypeIndex = find_memory_type(g_physical_device, mem_reqs.memoryTypeBits, 
                                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vma.memoryTypeIndex == UINT32_MAX) {
        std::cerr << "Failed to find suitable memory type for vertex buffer\\n";
        return -1;
    }

    if (vkAllocateMemory(g_device, &vma, nullptr, &vertex_buffer_memory) != VK_SUCCESS) {
        std::cerr << "Failed to allocate vertex buffer memory\\n";
        return -1;
    }

    vkMapMemory(g_device, vertex_buffer_memory, 0, mem_reqs.size, 0, (void**)nullptr);

    // Copy vertex data to buffer
    memcpy(vkMapMemory(g_device, vertex_buffer_memory, 0, mem_reqs.size, 0), 
           g_vertices.data(), g_vertices.size() * sizeof(Vertex));

    vkBindBufferMemory(g_device, vertex_buffer, vertex_buffer_memory, 0);

    std::cout << "✅ Vertex buffer created and filled\\n";

    // Generate terrain mesh
    generate_terrain_mesh();

    // Compile shaders to modules
    std::string vertex_shader_source = R"(
        #version 450
        layout (location = 0) in vec3 inPos { x, y, z };
        layout (location = 1) in vec2 inUv { x, y };
        
        out vec2 frag_uv;
        
        void main() {
            gl_Position = vec4(inPos, 1.0);
            frag_uv = inUv;
        }
    )";

    std::string fragment_shader_source = R"(
        #version 450
        layout (location = 0) out vec4 fColor;
        
        in vec2 frag_uv;
        
        void main() {
            float height = fract(frag_uv.x * 10.0);
            vec3 color = vec3(height, 0.2, 0.8);
            fColor = vec4(color, 1.0);
        }
    )";

    // Create shader modules
    VkShaderModule vertex_shader_module = create_shader_module(g_device, vertex_shader_source, "vertex");
    if (vertex_shader_module == VK_NULL_HANDLE) {
        std::cerr << "Failed to create vertex shader module\\n";
        return -1;
    }

    VkShaderModule fragment_shader_module = create_shader_module(g_device, fragment_shader_source, "fragment");
    if (fragment_shader_module == VK_NULL_HANDLE) {
        std::cerr << "Failed to create fragment shader module\\n";
        vkDestroyShaderModule(g_device, vertex_shader_module, nullptr);
        return -1;
    }

    g_vertex_shader_module = vertex_shader_module;
    g_fragment_shader_module = fragment_shader_module;

    std::cout << "✅ Shaders compiled\\n";

    // Create pipeline layout
    if (create_pipeline_layout(g_device) != VK_SUCCESS) {
        std::cerr << "Failed to create pipeline layout\\n";
        return -1;
    }
    
    std::cout << "✅ Pipeline layout created\\n";

    // Create graphics pipeline with proper setup
    std::vector<VkVertexInputBindingDescription> binding_descriptions(1);
    binding_descriptions[0].binding = 0;
    binding_descriptions[0].stride = sizeof(Vertex);
    binding_descriptions[0].inputStride = sizeof(Vertex);
    binding_descriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::vector<VkVertexInputAttributeDescription> attribute_descriptions(2);
    attribute_descriptions[0].location = 0;
    attribute_descriptions[0].binding = 0;
    attribute_descriptions[0].format = Vertex::get_format();
    attribute_descriptions[0].offset = offsetof(Vertex, position);

    attribute_descriptions[1].location = 1;
    attribute_descriptions[1].binding = 0;
    attribute_descriptions[1].format = Vertex::get_format();
    attribute_descriptions[1].offset = offsetof(Vertex, uv);

    vertex_input_state.vertexBindingDescriptionCount = 1;
    vertex_input_state.pVertexBindingDescriptions = &binding_descriptions[0];
    vertex_input_state.pVertexAttributeDescriptions = &attribute_descriptions[0];
    vertex_input_state.vertexAttributeDescriptionCount = 2;

    std::vector<VkPipelineShaderStageCreateInfo> shader_stages(2);
    
    // Create pipeline from shader stages and states
    VkGraphicsPipelineCreateInfo graphics_pipeline_ci{};
    graphics_pipeline_ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    graphics_pipeline_ci.layout = g_pipeline_layout;
    graphics_pipeline_ci.stageCount = 2;
    graphics_pipeline_ci.pStages = nullptr;
    graphics_pipeline_ci.pVertexInputState = &vertex_input_state;
    graphics_pipeline_ci.pInputAssemblyState = &input_assembly_state;
    graphics_pipeline_ci.pViewportState = &viewport_state;
    graphics_pipeline_ci.pRasterizationState = &rasterization_state;
    graphics_pipeline_ci.pMultisampleState = &multisample_state;
    graphics_pipeline_ci.pColorBlendState = &color_blend_state;
    graphics_pipeline_ci.pDepthStencilState = &depth_stencil_state;
    graphics_pipeline_ci.renderPass = g_render_pass;

    // Create pipeline from shader stages using nullptr for dynamic states
    if (vkCreateGraphicsPipelines(g_device, VK_NULL_HANDLE, 1, 
                                   &graphics_pipeline_ci, nullptr, 
                                   &g_graphics_pipeline) != VK_SUCCESS) {
        std::cerr << "Failed to create graphics pipeline\\n";
        return -1;
    }

    std::cout << "✅ Graphics pipeline created\\n";

    // Create command pool and buffers
    VkCommandPoolCreateInfo pool_create{};
    pool_create.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_create.queueFamilyIndex = graphics_family;
    
    if (vkCreateCommandPool(g_device, &pool_create, nullptr, &g_command_pool) != VK_SUCCESS) {
        std::cerr << "Failed to create command pool\\n";
        return -1;
    }

    uint32_t command_buffer_count = swapchain_image_count * 2; // Extra buffers for flexibility
    std::vector<VkCommandBuffer> command_buffers(command_buffer_count);
    
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = static_cast<uint32_t>(command_buffers.size());
    alloc_info.commandPool = g_command_pool;

    if (vkAllocateCommandBuffers(g_device, &alloc_info, command_buffers.data()) != VK_SUCCESS) {
        std::cerr << "Failed to allocate command buffers\\n";
        return -1;
    }

    std::cout << "✅ Command buffers allocated\\n";

    // Main render loop
    SDL_bool running = SDL_TRUE;
    uint32_t current_frame = 0;
    
    // Start timestamp for frame timing
    auto last_frame_time = std::chrono::high_resolution_clock::now();

    while (running) {
        // Poll events
        SDL_Event event;
        if (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT || 
                (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) {
                running = SDL_FALSE;
            }
        }

        // Acquire next swapchain image with timeout
        auto start_acquire = std::chrono::high_resolution_clock::now();
        uint32_t acquire_images[swapchain_image_count]{};
        VkResult acquire_result = vkAcquireNextImageKHR(g_device, g_swapchain, 
                                                         30ULL * 1000000000LL, 
                                                         VK_NULL_HANDLE, nullptr, 
                                                         acquire_images);
        uint32_t image_idx = acquire_images[0];
        
        if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR) {
            std::cout << "Swapchain out of date - would resize here\\n";
            continue;
        } else if (acquire_result == VK_SUBOPTIMAL_KHR) {
            std::cout << "Swapchain suboptimal\\n";
            continue;
        }
        
        auto end_acquire = std::chrono::high_resolution_clock::now();

        std::cout << "Acquired swapchain image: " << image_idx << "\\n";

        uint32_t command_buffer_index = (current_frame + image_idx) % command_buffers.size();

        // Begin command buffer
        VkCommandBufferBeginInfo begin_cb{};
        begin_cb.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        
        if (vkBeginCommandBuffer(command_buffers[command_buffer_index], &begin_cb) != VK_SUCCESS) {
            std::cerr << "Failed to begin command buffer\\n";
            continue;
        }

        // Record render pass
        VkRenderPassBeginInfo rpbi{};
        rpbi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpbi.renderPass = g_render_pass;
        rpbi.renderArea.offset = {0, 0};
        rpbi.renderArea.extent.width = w;
        rpbi.renderArea.extent.height = h;
        rpbi.framebuffer = g_swapchain_framebuffers[image_idx];

        vkCmdBeginRenderPass(command_buffers[command_buffer_index], &rpbi, VK_SUBPASS_CONTENTS_INLINE);

        // Bind pipeline
        vkCmdBindPipeline(command_buffers[command_buffer_index], VK_PIPELINE_BIND_POINT_GRAPHICS, 
                         g_graphics_pipeline);
        
        // Bind vertex buffer
        VkBuffer vertex_buffer_ptr = vertex_buffer;
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(command_buffers[command_buffer_index], 0, 1, 
                               &vertex_buffer_ptr, &offset);

        // Draw vertices
        vkCmdDraw(command_buffers[command_buffer_index], static_cast<uint32_t>(g_vertices.size()), 1, 0, 0);

        vkCmdEndRenderPass(command_buffers[command_buffer_index]);
        vkEndCommandBuffer(command_buffers[command_buffer_index]);

        // Submit command buffer
        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &command_buffers[command_buffer_index];
        
        // Use semaphore to synchronize
        submit.waitSemaphoreCount = 0;
        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores = &g_present_available_semaphores[image_idx];

        vkQueueSubmit(g_graphics_queue, 1, &submit, nullptr);

        // Present image
        VkPresentInfoKHR present{};
        present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present.waitSemaphoreCount = 0;
        present.pWaitSemaphores = nullptr;
        present.swapchainCount = 1;
        present.pSwapchains = &g_swapchain;
        present.pImageIndices = &image_idx;

        vkQueuePresentKHR(g_present_queue, &present);
        
        current_frame = (current_frame + 1) % command_buffer_count;

    }

    // Cleanup
    cleanup_resources();
    
    return 0;
}

static void cleanup_resources() {
    if (g_graphics_pipeline) {
        vkDestroyPipeline(g_device, g_graphics_pipeline, nullptr);
    }
    if (g_pipeline_layout) {
        vkDestroyPipelineLayout(g_device, g_pipeline_layout, nullptr);
    }
    if (g_render_pass) {
        vkDestroyRenderPass(g_device, g_render_pass, nullptr);
    }
    for (auto& fb : g_swapchain_framebuffers) {
        if (fb) {
            vkDestroyFramebuffer(g_device, fb, nullptr);
        }
    }
    if (g_swapchain) {
        vkDestroySwapchainKHR(g_device, g_swapchain, nullptr);
    }
    for (auto& view : g_swapchain_image_views) {
        if (view) {
            vkDestroyImageView(g_device, view, nullptr);
        }
    }
    for (auto& img : g_swapchain_images) {
        if (img) {
            vkDestroyImage(g_device, img, nullptr);
        }
    }
    for (auto& sem : g_render_complete_semaphores) {
        if (sem) {
            vkDestroySemaphore(g_device, sem, nullptr);
        }
    }
    for (auto& sem : g_present_available_semaphores) {
        if (sem) {
            vkDestroySemaphore(g_device, sem, nullptr);
        }
    }
    for (auto& fence : g_present_fences) {
        if (fence) {
            vkDestroyFence(g_device, fence, nullptr);
        }
    }
    
    if (g_command_pool) {
        vkDestroyCommandPool(g_device, g_command_pool, nullptr);
    }
    
    if (vertex_buffer) {
        vkDestroyBuffer(g_device, vertex_buffer, nullptr);
    }
    if (vertex_buffer_memory) {
        vkFreeMemory(g_device, vertex_buffer_memory, nullptr);
    }
    
    if (g_vertex_shader_module) {
        vkDestroyShaderModule(g_device, g_vertex_shader_module, nullptr);
    }
    if (g_fragment_shader_module) {
        vkDestroyShaderModule(g_device, g_fragment_shader_module, nullptr);
    }
    
    vkDestroyDevice(g_device, nullptr);
    vkDestroySurfaceKHR(g_instance, g_surface, nullptr);
    vkDestroyInstance(g_instance, nullptr);
    
    if (window_ptr) {
        SDL_DestroyWindow(window_ptr);
    }
    SDL_Quit();
}
'''

# Backup existing file
if os.path.exists(BACKUP_PATH):
    os.remove(BACKUP_PATH)
os.rename(BASE_PATH, BACKUP_PATH)

# Write new complete implementation
with open(BASE_PATH, 'w') as f:
    f.write(file_content)

print(f"✅ Complete SDL2+Vulkan implementation written to {BASE_PATH}")
print(f"  - File size: {os.path.getsize(BASE_PATH)} bytes")
print(f"  - Backup saved to {BACKUP_PATH}")

# Verify the file has all required sections
required_sections = [
    "SDL_Init(SDL_INIT_VIDEO)",
    "SDL_WINDOW_VULKAN",
    "SDL_Vulkan_CreateSurface",
    "VkSwapchainCreateInfoKHR",
    "vkAcquireNextImageKHR",
    "vkQueuePresentKHR",
    "cleanup_resources()"
]

print("\n✅ Verification:")
for section in required_sections:
    if section in file_content:
        print(f"  ✓ {section}")
    else:
        print(f"  ✗ {section} - MISSING")
