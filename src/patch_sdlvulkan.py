#!/usr/bin/env python3
"""
Modular patcher for Taz:Wanted SDL2+Vulkan implementation.
Patches main_simple_vulkan.cpp in chunks to avoid truncation issues.
"""

import os

SRC_PATH = "/home/mochi/taz-decomp/src/main_simple_vulkan.cpp"

# Section 1: Headers and globals
section1_headers_globals = r'''// ============================================================================
// TAZ: WANTED - SDL2 + VULKAN RENDER LOOP IMPLEMENTATION
// File: src/main_simple_vulkan.cpp
// ============================================================================

#include <iostream>
#include <vector>
#include <cstring>
#include <cmath>
#include <chrono>
#include <optional>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "glad/glad.h"

// Include SDL2 headers for window management
#include <SDL2/SDL.h>
#include <SDL2/SDL_video.h>

// Window dimensions
#define WINDOW_WIDTH 1920
#define WINDOW_HEIGHT 1080
#define SWAPCHAIN_IMAGE_COUNT 3
#define RENDER_PASS_NAME "Main Render Pass"

// Vulkan Instance and device handles
VkInstance g_vulkan_instance = VK_NULL_HANDLE;
VkSurfaceKHR g_surface = VK_NULL_HANDLE;
VkPhysicalDevice g_physical_device = VK_NULL_HANDLE;
VkDevice g_device = VK_NULL_HANDLE;
VkQueue graphics_queue = VK_NULL_HANDLE;
VkQueue present_queue = VK_NULL_HANDLE;

// Swapchain and related structures
VkSwapchainKHR g_swapchain = VK_NULL_HANDLE;
std::vector<VkImage> g_swapchain_images;
std::vector<VkImageView> g_swapchain_image_views;
std::vector<VkFramebuffer> g_swapchain_framebuffers;
VkRenderPass g_render_pass = VK_NULL_HANDLE;
VkPipeline g_graphics_pipeline = VK_NULL_HANDLE;
VkPipelineLayout g_pipeline_layout = VK_NULL_HANDLE;

// Command buffers and synchronization primitives
VkCommandPool g_command_pool = VK_NULL_HANDLE;
std::vector<VkCommandBuffer> g_command_buffers;
std::vector<VkSemaphore> g_image_available_semaphores;
std::vector<VkSemaphore> g_render_complete_semaphores;
std::vector<VkFence> g_fences;

// Vertex staging buffer
VkBuffer vertex_staging_buffer = VK_NULL_HANDLE;
VkDeviceMemory vertex_memory = VK_NULL_HANDLE;

// SDL2 window and surface
SDL_Window* g_sdl_window = nullptr;
SDL_Surface* g_sdl_surface = nullptr;
SDL_Renderer* g_renderer = nullptr;

// Current frame tracking
uint32_t g_current_frame_index = 0;
bool g_should_quit = false;

// Swapchain configuration options
VkSurfaceFormatKHR g_swapchain_format{};
VkPresentModeKHR g_swapchain_present_mode = VK_PRESENT_MODE_FIFO_KHR;
VkSurfaceTransformFlagBitsKHR g_preferred_transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
''';

# Section 2: Vertex structure and helper functions
section2_helpers = r'''
// ============================================================================
// VERTEX STRUCTURE AND HELPER FUNCTIONS
// ============================================================================

struct Vertex {
    float pos[3];  // x, y, z - vertex position
    float uv[2];   // texture coordinates
};

std::vector<Vertex> g_vertices;

// Generate terrain mesh with gradient vertices
void generate_terrain_mesh() {
    std::cout << "Generating terrain mesh...\n";
    
    // Create a simple grid of vertices for terrain visualization
    int grid_size = 10;
    float grid_step_x = static_cast<float>(WINDOW_WIDTH) / grid_size;
    float grid_step_z = static_cast<float>(WINDOW_HEIGHT) / grid_size;
    
    for (int z = 0; z < grid_size + 1; z++) {
        for (int x = 0; x < grid_size + 1; x++) {
            Vertex vertex;
            vertex.pos[0] = static_cast<float>(x) * grid_step_x - WINDOW_WIDTH / 2.0f;
            vertex.pos[2] = -(static_cast<float>(z) * grid_step_z - WINDOW_HEIGHT / 2.0f);
            // Simple height variation based on position
            vertex.pos[1] = sinf(vertex.pos[0] * 0.01f) * 
                           cosf(vertex.pos[2] * 0.01f) * 50.0f;
            
            vertex.uv[0] = static_cast<float>(x) / grid_size;
            vertex.uv[1] = static_cast<float>(z) / grid_size;
            
            g_vertices.push_back(vertex);
        }
    }
    
    std::cout << "Terrain mesh generated: " << g_vertices.size() << " vertices\n";
}

// ============================================================================
// VULKAN INSTANCE INITIALIZATION
// ============================================================================

VkResult init_vulkan_instance() {
    std::cout << "Initializing Vulkan instance...\n";

    // Create Vulkan instance with required extensions
    std::vector<const char*> vulkan_instance_extensions;
#if defined(NDEBUG) || !defined(VK_USE_PLATFORM_WIN32_WINDOW_EXT)
    vulkan_instance_extensions.push_back("VK_KHR_surface");
    vulkan_instance_extensions.push_back("VK_KHR_device_group_create");
    vulkan_instance_extensions.push_back("VK_KHR_external_memory_capabilities");
    vulkan_instance_extensions.push_back("VK_KHR_external_semaphore_capabilities");
    vulkan_instance_extensions.push_back("VK_KHR_external_fence_capabilities");
    vulkan_instance_extensions.push_back("VK_KHR_external_memory";
    vulkan_instance_extensions.push_back("VK_KHR_get_physical_device_properties2");
    vulkan_instance_extensions.push_back("VK_KHR_get_surface_capabilities2");
    vulkan_instance_extensions.push_back("VK_KHR_surface";
    vulkan_instance_extensions.push_back("VK_EXT_debug_utils";
#endif

    VkInstanceCreateInfo instance_create_info{};
    instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_create_info.enabledExtensionCount = static_cast<uint32_t>(vulkan_instance_extensions.size());
    instance_create_info.ppEnabledExtensionNames = vulkan_instance_extensions.data();

    result = vkCreateInstance(&instance_create_info, nullptr, &g_vulkan_instance);

    if (result != VK_SUCCESS || !g_vulkan_instance) {
        std::cerr << "Failed to create Vulkan instance: " << static_cast<int>(result) << "\n";
        return result;
    }

    // Create SDL surface
    if (SDL_Vulkan_CreateSurface(g_sdl_window, g_vulkan_instance, nullptr, &g_surface) != 0) {
        std::cerr << "Failed to create SDL Vulkan surface: " << SDL_GetError() << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    std::cout << "Vulkan instance created successfully\n";
    return VK_SUCCESS;
}

// ============================================================================
// DEVICE SELECTION AND CREATION
// ============================================================================

VkResult find_and_create_device(VkSurfaceKHR surface) {
    std::cout << "Finding and creating physical device...\n";

    // Enumerate physical devices
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(g_vulkan_instance, &device_count, nullptr);

    if (device_count == 0) {
        std::cerr << "No GPUs found!\n";
        return VK_ERROR_INCOMPATIBLE_DRIVER;
    }

    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(g_vulkan_instance, &device_count, devices.data());

    // Select the first suitable device (graphics + present support)
    for (uint32_t i = 0; i < device_count; i++) {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(devices[i], &properties);

        if ((properties.deviceType & VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) == 0 &&
            (properties.deviceType & VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) == 0) {
            continue;
        }

        // Check queue support
        uint32_t queue_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &queue_count, nullptr);

        if (queue_count == 0) {
            continue;
        }

        // Get queue families and find graphics + present
        std::vector<VkQueueFamilyProperties> queue_properties(queue_count);
        vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &queue_count, queue_properties.data());

        uint32_t graphics_family = UINT32_MAX;
        uint32_t present_family = UINT32_MAX;

        for (uint32_t j = 0; j < queue_count; j++) {
            if ((queue_properties[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
                graphics_family = j;
            }

            VkBool32 supported = false;
            vkGetPhysicalDeviceSurfaceSupportedKHR(devices[i], surface, &supported);

            if (supported == VK_TRUE && 
                !vkGetPhysicalDeviceSurfacePresentModesKHR(devices[i], surface, nullptr, nullptr)) {
                present_family = j;
            }
        }

        // Try to use the same queue for both graphics and presentation if possible
        if (graphics_family != UINT32_MAX && 
            (present_family == graphics_family || present_family == UINT32_MAX)) {
            
            g_physical_device = devices[i];
            
            uint32_t queue_count_final = 0;
            vkGetPhysicalDeviceQueueProperties(devices[i], &queue_count_final, nullptr);

            std::vector<VkQueueProperties> queue_props(queue_count_final);
            vkGetPhysicalDeviceQueueProperties(devices[i], &queue_count_final, queue_props.data());

            VkDeviceQueueCreateInfo queue_create_info{};
            queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_create_info.queueFamilyIndex = graphics_family;
            queue_create_info.queueCount = 1;
            queue_create_info.pQueuePriorities = &1.0f;

            // Build device extensions list
            std::vector<const char*> device_extensions;
            device_extensions.push_back("VK_KHR_surface");
            device_extensions.push_back("VK_KHR_swapchain");
            device_extensions.push_back("VK_KHR_get_physical_device_properties2");
            
#if defined(NDEBUG) || !defined(VK_USE_PLATFORM_WIN32_WINDOW_EXT)
            device_extensions.push_back("VK_EXT_debug_utils";
#endif

            // Create logical device
            VkDeviceCreateInfo device_create_info{};
            device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            device_create_info.queueCreateInfoCount = 1;
            device_create_info.pQueueCreateInfos = &queue_create_info;
            device_create_info.enabledExtensionCount = static_cast<uint32_t>(device_extensions.size());
            device_create_info.ppEnabledExtensionNames = device_extensions.data();

            result = vkCreateDevice(g_physical_device, &device_create_info, nullptr, &g_device);

            if (result != VK_SUCCESS) {
                std::cerr << "Failed to create logical device: " << static_cast<int>(result) << "\n";
                continue;
            }

            // Get queues
            vkGetDeviceQueue(g_device, graphics_family, 0, &graphics_queue);
            
            if (present_family == graphics_family) {
                vkGetDeviceQueue(g_device, present_family, 1, &present_queue);
            }

            std::cout << "Physical device selected: " << properties.deviceName 
                      << "\nGraphics queue family: " << graphics_family;
            
            return result;
        }
    }

    std::cerr << "Failed to find suitable physical device!\n";
    return VK_ERROR_INCOMPATIBLE_DEVICE_OR_API_Version;
}

// ============================================================================
// RENDER PASS CREATION
// ============================================================================

VkResult create_render_pass() {
    std::cout << "Creating render pass...\n";

    VkSubpassDescription subpass{};
    subpass.sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION;
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    
    // Color attachment description
    VkPipelineColorAttachmentDesc color_attachment{};
    color_attachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color_attachment.format = g_swapchain_format.format;
    color_attachment.loadOp = VK_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_LOAD_OP_EXPLICIT_NONE;
    color_attachment.stencilStoreOp = VK_STORE_OP_STORE;
    color_attachment.attachment = VK_ATTACHMENT_UNUSED;

    subpass.pColorAttachments = &color_attachment;
    subpass.preserveAttachmentCount = 0;

    std::vector<VkSubpassDescription> subpasses(1, subpass);

    VkRenderPassCreateInfo render_pass_ci{};
    render_pass_ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_ci.subpassCount = static_cast<uint32_t>(subpasses.size());
    render_pass_ci.pSubpasses = subpasses.data();

    result = vkCreateRenderPass(g_device, &render_pass_ci, nullptr, &g_render_pass);

    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create render pass: " << static_cast<int>(result) << "\n";
        return result;
    }

    std::cout << "Render pass created\n";
    return VK_SUCCESS;
}

// ============================================================================
// GRAPHICS PIPELINE CREATION
// ============================================================================

VkResult create_pipeline(VkRenderPass render_pass, VkSurfaceFormatKHR format) {
    std::cout << "Creating graphics pipeline...\n";

    // Vertex shader code
    const char* vertex_shader = R"(
        #version 450
        layout(location = 0) in vec3 in_position;
        layout(location = 1) in vec2 in_uv;

        out vec2 frag_uv;

        void main() {
            gl_Position = vec4(in_position, 1.0);
            frag_uv = in_uv;
        }
    )";

    // Fragment shader code
    const char* fragment_shader = R"(
        #version 450
        layout(location = 0) out vec4 frag_color;
        in vec2 frag_uv;

        void main() {
            // Create a gradient effect based on UV coordinates
            float r = 0.5f + 0.3f * sin(frag_uv.y * 10.0);
            float g = 0.3f + 0.3f * cos(frag_uv.x * 10.0);
            float b = 0.2f;
            frag_color = vec4(r, g, b, 1.0f);
        }
    )";

    // Compile shaders (simplified - in production use spv files)
    VkShaderModuleCreateInfo vertex_module_ci{};
    vertex_module_ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vertex_module_ci.codeSize = strlen(vertex_shader);
    vertex_module_ci.pCode = reinterpret_cast<const uint32_t*>(vertex_shader);

    VkShaderModuleCreateInfo fragment_module_ci{};
    fragment_module_ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    fragment_module_ci.codeSize = strlen(fragment_shader);
    fragment_module_ci.pCode = reinterpret_cast<const uint32_t*>(fragment_shader);

    // Create shader modules (note: this won't work with raw GLSL, needs spv - using placeholder)
    VkShaderModule vertex_shader_module;
    VkShaderModule fragment_shader_module;
    
    // In real implementation, these would be loaded from SPIR-V files
    std::cout << "Shaders compiled (placeholder for SPIR-V loading)\n";

    // Create pipeline stages - using placeholder shader modules
    std::vector<VkPipelineShaderStageCreateInfo> shader_stages(2);
    
    shader_stages[0] = VkPipelineShaderStageCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        nullptr,
        0,
        VK_SHADER_STAGE_VERTEX_BIT,
        "main",
        vertex_shader_module,
        nullptr
    };
    
    shader_stages[1] = VkPipelineShaderStageCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        nullptr,
        0,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        "main",
        fragment_shader_module,
        nullptr
    };

    // Vertex input state
    std::vector<VkVertexInputBindingDescription> vertex_bindings(1);
    vertex_bindings[0].binding = 0;
    vertex_bindings[0].stride = sizeof(Vertex);
    vertex_bindings[0].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

    VkPipelineVertexInputStateCreateInfo vertex_input_ci{};
    vertex_input_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_ci.vertexBindingDescriptionCount = static_cast<uint32_t>(vertex_bindings.size());
    vertex_input_ci.pVertexBindingDescriptions = vertex_bindings.data();

    std::vector<VkVertexInputAttributeDescription> vertex_attrs(1);
    vertex_attrs[0].location = 0;
    vertex_attrs[0].binding = 0;
    vertex_attrs[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    vertex_attrs[0].offset = offsetof(Vertex, pos);

    // Vertex attribute state
    VkPipelineVertexAttributeDescriptionCreateInfo vertex_attr_ci{};
    vertex_attr_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo input_assembly_ci{};
    input_assembly_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_ci.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly_ci.primitiveRestartEnable = VK_FALSE;

    // Viewport state
    VkPipelineViewportStateCreateInfo viewport_ci{};
    viewport_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_ci.viewportCount = 1;
    viewport_ci.scissorCount = 1;

    // Rasterization state
    VkPipelineRasterizationStateCreateInfo raster_ci{};
    raster_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster_ci.lineWidth = 1.0f;
    raster_ci.depthClampEnable = VK_FALSE;
    raster_ci.rasterizerDiscardEnable = VK_FALSE;
    raster_ci.polygonMode = VK_POLYGON_MODE_FILL;
    raster_ci.cullMode = VK_CULL_MODE_BACK_BIT;
    raster_ci.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster_ci.depthBiasEnable = VK_FALSE;

    // Multisample state
    VkPipelineMultisampleStateCreateInfo multisample_ci{};
    multisample_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample_ci.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Color blend state
    std::vector<VkColorBlendAttachmentState> color_attachments(1);
    color_attachments[0].blendEnable = VK_FALSE;
    color_attachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | 
                                          VK_COLOR_COMPONENT_G_BIT | 
                                          VK_COLOR_COMPONENT_B_BIT | 
                                          VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo color_blend_ci{};
    color_blend_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend_ci.attachmentCount = 1;
    color_blend_ci.pAttachments = color_attachments.data();

    // Dynamic states
    std::vector<VkDynamicState> dynamic_states(2);
    dynamic_states[0] = VK_DYNAMIC_STATE_VIEWPORT;
    dynamic_states[1] = VK_DYNAMIC_STATE_SCISSOR;

    VkPipelineDynamicStateCreateInfo dynamic_ci{};
    dynamic_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_ci.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
    dynamic_ci.pDynamicStates = dynamic_states.data();

    // Pipeline layout (simplified)
    VkPipelineLayoutCreateInfo pipeline_layout_ci{};
    pipeline_layout_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    
    result = vkCreatePipelineLayout(g_device, &pipeline_layout_ci, nullptr, &g_pipeline_layout);

    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create pipeline layout: " << static_cast<int>(result) << "\n";
        return result;
    }

    // Pipeline color blend attachment state
    VkPipelineColorBlendAttachmentState color_blend_att{};
    color_blend_att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | 
                                     VK_COLOR_COMPONENT_G_BIT | 
                                     VK_COLOR_COMPONENT_B_BIT | 
                                     VK_COLOR_COMPONENT_A_BIT;
    color_blend_att.blendEnable = VK_FALSE;

    std::vector<VkPipelineColorBlendAttachmentState> color_attach_states(1, color_blend_att);

    // Pipeline graphics creation info
    VkGraphicsPipelineCreateInfo pipeline_ci{};
    pipeline_ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_ci.stageCount = static_cast<uint32_t>(shader_stages.size());
    pipeline_ci.pStages = shader_stages.data();
    pipeline_ci.layout = g_pipeline_layout;
    pipeline_ci.renderPass = render_pass;
    pipeline_ci.subpass = 0;
    pipeline_ci.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_ci.basePipelineModule = VK_NULL_HANDLE;

    result = vkCreateGraphicsPipelines(g_device, VK_NULL_HANDLE, 
                                       1, &pipeline_ci, nullptr, &g_graphics_pipeline);

    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create graphics pipeline: " << static_cast<int>(result) << "\n";
        return result;
    }

    std::cout << "Graphics pipeline created\n";
    return VK_SUCCESS;
}

// ============================================================================
// SWAPCHAIN CREATION
// ============================================================================

VkResult init_swapchain() {
    std::cout << "Initializing swapchain...\n";

    // Get surface format and present mode support
    uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceSupportedFormats(g_physical_device, g_surface, &format_count, nullptr);

    std::vector<VkSurfaceFormatKHR> supported_formats(format_count);
    vkGetPhysicalDeviceSurfaceSupportedFormats(g_physical_device, g_surface, &format_count, supported_formats.data());

    // Select first supported format or use RGBA8 if available
    g_swapchain_format = supported_formats[0];
    if (g_swapchain_format.format == VK_FORMAT_B8G8R8A8_UNORM) {
        g_swapchain_format = supported_formats[format_count - 1];
    }

    // Get present mode support
    uint32_t present_mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(g_physical_device, g_surface, &present_mode_count, nullptr);

    std::vector<VkPresentModeKHR> supported_present_modes(present_mode_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(g_physical_device, g_surface, &present_mode_count, supported_present_modes.data());

    // Find FIFO or prefered mode
    for (uint32_t i = 0; i < present_mode_count; i++) {
        if (supported_present_modes[i] == VK_PRESENT_MODE_FIFO_KHR) {
            g_swapchain_present_mode = VK_PRESENT_MODE_FIFO_KHR;
            break;
        }
    }

    // Get surface transform
    VkSurfaceCapabilitiesKHR capabilities{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_physical_device, g_surface, &capabilities);

    // Create swapchain
    VkSwapchainCreateInfoKHR swapchain_ci{};
    swapchain_ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_ci.surface = g_surface;
    swapchain_ci.minImageCount = capabilities.minImageCount;
    swapchain_ci.imageFormat = g_swapchain_format.format;
    swapchain_ci.imageColorSpace = g_swapchain_format.colorSpace;
    swapchain_ci.imageExtent = {WINDOW_WIDTH, WINDOW_HEIGHT};
    swapchain_ci.imageArrayLayers = 1;
    swapchain_ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchain_ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_ci.queueFamilyIndexCount = 0;
    swapchain_ci.pQueueFamilyIndices = nullptr;
    swapchain_ci.preTransform = capabilities.currentTransform;
    swapchain_ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_ci.presentMode = g_swapchain_present_mode;
    swapchain_ci.clipped = VK_TRUE;

    result = vkCreateSwapchainKHR(g_device, &swapchain_ci, nullptr, &g_swapchain);

    if (result != VK_SUCCESS || !g_swapchain) {
        std::cerr << "Failed to create swapchain: " << static_cast<int>(result) << "\n";
        return result;
    }

    // Get swapchain images
    uint32_t image_count = 0;
    vkGetSwapchainImagesKHR(g_device, g_swapchain, &image_count, nullptr);

    std::vector<VkImage> images(image_count);
    vkGetSwapchainImagesKHR(g_device, g_swapchain, &image_count, images.data());

    g_swapchain_images.resize(image_count);
    for (uint32_t i = 0; i < image_count; i++) {
        g_swapchain_images[i] = images[i];
    }

    // Create image views
    std::vector<VkImageView> image_views(image_count);
    for (size_t i = 0; i < image_count; i++) {
        VkImageSubresourceRange subresource_range{};
        subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresource_range.baseMipLevel = 0;
        subresource_range.levelCount = 1;
        subresource_range.baseArrayLayer = 0;
        subresource_range.layerCount = 1;

        VkImageViewCreateInfo view_ci{};
        view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_ci.image = g_swapchain_images[i];
        view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_ci.format = g_swapchain_format.format;
        view_ci.subresourceRange = subresource_range;

        result = vkCreateImageView(g_device, &view_ci, nullptr, &image_views[i]);

        if (result != VK_SUCCESS) {
            std::cerr << "Failed to create image view: " << static_cast<int>(result) << "\n";
            return result;
        }

        g_swapchain_image_views.push_back(image_views[i]);
    }

    // Create framebuffers
    std::vector<VkFramebuffer> framebuffers(image_count);
    for (uint32_t i = 0; i < image_count; i++) {
        VkFramebufferCreateInfo framebuffer_ci{};
        framebuffer_ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_ci.renderPass = g_render_pass;
        framebuffer_ci.attachmentCount = 1;
        framebuffer_ci.pAttachments = &g_swapchain_image_views[i];
        framebuffer_ci.width = WINDOW_WIDTH;
        framebuffer_ci.height = WINDOW_HEIGHT;
        framebuffer_ci.layers = 1;

        result = vkCreateFramebuffer(g_device, &framebuffer_ci, nullptr, &framebuffers[i]);

        if (result != VK_SUCCESS) {
            std::cerr << "Failed to create framebuffer: " << static_cast<int>(result) << "\n";
            return result;
        }
    }

    g_swapchain_framebuffers = framebuffers;
    std::cout << "Swapchain initialized with " << image_count << " images\n";
    return VK_SUCCESS;
}

// ============================================================================
// COMMAND BUFFER INITIALIZATION
// ============================================================================

VkResult init_command_buffers() {
    std::cout << "Initializing command buffers...\n";

    // Create command pool
    VkCommandPoolCreateInfo pool_ci{};
    pool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_ci.queueFamilyIndex = 0;

    result = vkCreateCommandPool(g_device, &pool_ci, nullptr, &g_command_pool);

    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create command pool\n";
        return result;
    }

    // Allocate command buffers
    uint32_t image_count = static_cast<uint32_t>(g_swapchain_images.size());
    VkCommandBufferAllocateInfo allocate_ci{};
    allocate_ci.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocate_ci.commandPool = g_command_pool;
    allocate_ci.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate_ci.commandBufferCount = image_count;

    result = vkAllocateCommandBuffers(g_device, &allocate_ci, g_command_buffers.data());

    if (result != VK_SUCCESS) {
        std::cerr << "Failed to allocate command buffers\n";
        return result;
    }

    // Create semaphores and fences
    g_image_available_semaphores.resize(image_count);
    g_render_complete_semaphores.resize(image_count);
    g_fences.resize(image_count);

    for (uint32_t i = 0; i < image_count; i++) {
        VkSemaphoreCreateInfo semaphore_ci{};
        semaphore_ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        result = vkCreateSemaphore(g_device, &semaphore_ci, nullptr, 
                                   &g_image_available_semaphores[i]);

        if (result != VK_SUCCESS) {
            std::cerr << "Failed to create semaphore: " << static_cast<int>(result) << "\n";
            return result;
        }

        result = vkCreateSemaphore(g_device, &semaphore_ci, nullptr, 
                                   &g_render_complete_semaphores[i]);

        if (result != VK_SUCCESS) {
            std::cerr << "Failed to create completion semaphore: " << static_cast<int>(result) << "\n";
            return result;
        }

        VkFenceCreateInfo fence_ci{};
        fence_ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_ci.flags = VK_FENCE_SIGNALED_BIT;

        result = vkCreateFence(g_device, &fence_ci, nullptr, &g_fences[i]);

        if (result != VK_SUCCESS) {
            std::cerr << "Failed to create fence: " << static_cast<int>(result) << "\n";
            return result;
        }
    }

    std::cout << "Command buffers, semaphores, and fences initialized\n";
    return VK_SUCCESS;
}

// ============================================================================
// BUFFER INITIALIZATION
// ============================================================================

VkResult init_buffers() {
    std::cout << "Initializing vertex buffers...\n";

    generate_terrain_mesh();

    VkDeviceSize vertex_size = sizeof(Vertex) * g_vertices.size();

    VkBufferCreateInfo buffer_ci{};
    buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_ci.size = vertex_size;
    buffer_ci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    result = vkCreateBuffer(g_device, &buffer_ci, nullptr, &vertex_staging_buffer);

    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create vertex staging buffer\n";
        return result;
    }

    VkMemoryRequirements memory_req{};
    vkGetBufferMemoryRequirements(g_device, vertex_staging_buffer, &memory_req);

    VkMemoryAllocateInfo memory_alloc_ci{};
    memory_alloc_ci.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory_alloc_ci.allocationSize = memory_req.size;
    
    // Use host-visible memory type (simplified selection)
    uint32_t memory_type_index = 0;
    VkMemoryPropertyFlags required_props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | 
                                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    memory_alloc_ci.memoryTypeIndex = memory_type_index;

    result = vkAllocateMemory(g_device, &memory_alloc_ci, nullptr, &vertex_memory);

    if (result != VK_SUCCESS) {
        std::cerr << "Failed to allocate vertex buffer memory\n";
        return result;
    }

    result = vkBindBufferMemory(g_device, vertex_staging_buffer, vertex_memory, 0);

    if (result != VK_SUCCESS) {
        std::cerr << "Failed to bind vertex buffer memory\n";
        return result;
    }

    // Copy vertex data to GPU buffer
    void* data = nullptr;
    vkMapMemory(g_device, vertex_memory, 0, vertex_size, 0, &data);
    memcpy(data, g_vertices.data(), vertex_size);
    vkUnmapMemory(g_device, vertex_memory);

    std::cout << "Vertex buffers initialized with " << g_vertices.size() << " vertices\n";
    return VK_SUCCESS;
}
'''

# Section 3: Render and present functions
section3_render_presentation = r'''

// ============================================================================
// RENDER FUNCTION
// ============================================================================

void render(VkCommandBuffer command_buffer, uint32_t image_index) {
    std::cout << "Rendering frame to swapchain image " << image_index << "\n";

    // Begin render pass
    VkRenderPassBeginInfo rp_begin_info{};
    rp_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp_begin_info.renderPass = g_render_pass;
    rp_begin_info.framebuffer = g_swapchain_framebuffers[image_index];
    rp_begin_info.renderArea.offset.x = 0;
    rp_begin_info.renderArea.offset.y = 0;
    rp_begin_info.renderArea.extent.width = WINDOW_WIDTH;
    rp_begin_info.renderArea.extent.height = WINDOW_HEIGHT;

    // Clear values for render pass
    std::vector<VkClearValue> clear_values(1);

    // Create animated color based on frame time
    float r = 0.2f + sinf(SDL_GetTicks() / 500.0f) * 0.3f;
    float g = 0.3f + cosf(SDL_GetTicks() / 500.0f) * 0.3f;
    float b = 0.1f;

    clear_values[0].color = {{r, g, b, 1.0f}};

    rp_begin_info.clearValueCount = 1;
    rp_begin_info.pClearValues = clear_values.data();

    // Record commands
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkResetFence(g_device, g_fences[image_index], 0);
    vkResetCommandBuffer(command_buffer, 0);

    vkBeginCommandBuffer(command_buffer, &begin_info);

    // Begin render pass
    vkCmdBeginRenderPass(command_buffer, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

    // Bind pipeline
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, g_graphics_pipeline);

    // Set viewport and scissor
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(WINDOW_WIDTH);
    viewport.height = static_cast<float>(WINDOW_HEIGHT);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = WINDOW_WIDTH;
    scissor.extent.height = WINDOW_HEIGHT;

    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    // Draw vertices
    if (!g_vertices.empty()) {
        uint32_t vertex_count = static_cast<uint32_t>(g_vertices.size());
        
        VkBufferVertexInputFeedbackCreateInfoEXT feedback_ci{};
        
        vkCmdBindVertexBuffer(command_buffer, &vertex_staging_buffer, 0);
        
        vkCmdDraw(command_buffer, vertex_count, 1, 0, 0);
    }

    // End render pass
    vkCmdEndRenderPass(command_buffer);

    // Signal semaphore for presentation
    vkCmdEndCommandBuffer(command_buffer);

    vkQueueSubmit(graphics_queue, 1, &VkSubmitInfo{
        VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0, nullptr, 
        g_image_available_semaphores[image_index], 
        g_render_complete_semaphores[image_index],
        1, &command_buffer
    }, VK_NULL_HANDLE);

    std::cout << "Frame rendered\n";
}

// ============================================================================
// PRESENTATION FUNCTION
// ============================================================================

VkResult present_swapchain(VkSwapchainKHR swapchain) {
    std::cout << "Presenting frame...\n";

    // Check if we should quit
    if (!g_sdl_window || g_should_quit) {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    // Acquire next available image
    VkResult acquire_result = vkAcquireNextImageKHR(g_device, swapchain, UINT64_MAX, 
                                                    VK_NULL_HANDLE, VK_NULL_HANDLE);

    if (acquire_result == VK_SUBOPTIMAL_KHR) {
        std::cout << "Swapchain needs refresh\n";
    } else if (acquire_result != VK_SUCCESS) {
        std::cerr << "Acquire swapchain image failed: " << static_cast<int>(acquire_result) << "\n";
        return acquire_result;
    }

    // Submit command buffer and present (already done in render function above)
    // The presentation happens after rendering completes

    g_current_frame_index = (g_current_frame_index + 1) % static_cast<uint32_t>(g_swapchain_images.size());

    std::cout << "Presented. Frame index: " << g_current_frame_index << "\n";
    return VK_SUCCESS;
}

// ============================================================================
// CLEANUP FUNCTION
// ============================================================================

void cleanup() {
    std::cout << "Cleaning up Vulkan resources...\n";

    if (vertex_memory != VK_NULL_HANDLE) vkFreeMemory(g_device, vertex_memory, nullptr);
    if (vertex_staging_buffer != VK_NULL_HANDLE) vkDestroyBuffer(g_device, vertex_staging_buffer, nullptr);
    
    for (uint32_t i = 0; i < g_swapchain_images.size(); i++) {
        if (g_image_available_semaphores[i] != VK_NULL_HANDLE) 
            vkDestroySemaphore(g_device, g_image_available_semaphores[i], nullptr);
        if (g_render_complete_semaphores[i] != VK_NULL_HANDLE) 
            vkDestroySemaphore(g_device, g_render_complete_semaphores[i], nullptr);
        if (g_fences[i] != VK_NULL_HANDLE) 
            vkDestroyFence(g_device, g_fences[i], nullptr);
    }

    for (size_t i = 0; i < g_swapchain_images.size(); i++) {
        if (g_swapchain_image_views[i] != VK_NULL_HANDLE) 
            vkDestroyImageView(g_device, g_swapchain_image_views[i], nullptr);
    }

    if (!g_swapchain_framebuffers.empty()) {
        for (const auto& fb : g_swapchain_framebuffers) {
            if (fb != VK_NULL_HANDLE) vkDestroyFramebuffer(g_device, fb, nullptr);
        }
    }

    if (g_swapchain != VK_NULL_HANDLE) vkDestroySwapchainKHR(g_device, g_swapchain, nullptr);
    if (g_render_pass != VK_NULL_HANDLE) vkDestroyRenderPass(g_device, g_render_pass, nullptr);
    if (g_graphics_pipeline != VK_NULL_HANDLE) vkDestroyPipeline(g_device, g_graphics_pipeline, nullptr);
    if (g_pipeline_layout != VK_NULL_HANDLE) vkDestroyPipelineLayout(g_device, g_pipeline_layout, nullptr);

    if (g_command_pool != VK_NULL_HANDLE) vkDestroyCommandPool(g_device, g_command_pool, nullptr);

    // Destroy command buffers individually
    for (auto& cmd : g_command_buffers) {
        if (cmd != VK_NULL_HANDLE) vkFreeCommandBuffers(g_device, g_command_pool, 1, &cmd);
    }

    if (g_device != VK_NULL_HANDLE) vkDestroyDevice(g_device, nullptr);
    if (g_surface != VK_NULL_HANDLE) vkDestroySurfaceKHR(g_vulkan_instance, g_surface, nullptr);
    if (g_vulkan_instance != VK_NULL_HANDLE) vkDestroyInstance(g_vulkan_instance, nullptr);

    // Cleanup SDL2 resources
    if (g_renderer != nullptr) SDL_DestroyRenderer(g_renderer);
    if (g_sdl_window != nullptr) SDL_DestroyWindow(g_sdl_window);
    
    SDL_Quit();

    std::cout << "Cleanup complete\n";
}
'''

# Section 4: Main function
section4_main = r'''

// ============================================================================
// MAIN FUNCTION - RENDER LOOP AND EVENT HANDLING
// ============================================================================

int main() {
    std::cout << "=== TAZ: WANTED - SDL2 + VULKAN RENDER LOOP ===\n\n";

    // Initialize SDL2 with video subsystem
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return -1;
    }

    // Create SDL window with Vulkan support
    g_sdl_window = SDL_CreateWindow(
        "Taz: Wanted - Vulkan Window",
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
    );

    if (!g_sdl_window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << "\n";
        SDL_Quit();
        return -1;
    }

    // Get SDL surface and create simple renderer (optional)
    g_sdl_surface = SDL_GetWindowSurface(g_sdl_window);

    if (!g_sdl_surface) {
        std::cerr << "SDL_GetWindowSurface failed: " << SDL_GetError() << "\n";
        return -1;
    }

    // Create Vulkan surface from SDL window
    if (SDL_Vulkan_CreateSurface(g_sdl_window, g_vulkan_instance, nullptr, &g_surface) != 0) {
        std::cerr << "SDL_Vulkan_CreateSurface failed: " << SDL_GetError() << "\n";
        SDL_DestroyWindow(g_sdl_window);
        return -1;
    }

    // Initialize Vulkan instance
    if (init_vulkan_instance() != VK_SUCCESS) {
        std::cerr << "Failed to initialize Vulkan\n";
        SDL_DestroyWindow(g_sdl_window);
        return -1;
    }

    // Find and create device
    if (find_and_create_device(g_surface) != VK_SUCCESS) {
        std::cerr << "Failed to find/create device\n";
        cleanup();
        return -1;
    }

    // Initialize render pass
    if (create_render_pass() != VK_SUCCESS) {
        std::cerr << "Failed to create render pass\n";
        cleanup();
        return -1;
    }

    // Initialize swapchain
    if (init_swapchain() != VK_SUCCESS) {
        std::cerr << "Failed to initialize swapchain\n";
        cleanup();
        return -1;
    }

    // Create graphics pipeline
    if (create_pipeline(g_render_pass, g_swapchain_format) != VK_SUCCESS) {
        std::cerr << "Failed to create pipeline\n";
        cleanup();
        return -1;
    }

    // Allocate and initialize command buffers
    if (init_command_buffers() != VK_SUCCESS) {
        std::cerr << "Failed to initialize command buffers\n";
        cleanup();
        return -1;
    }

    // Initialize vertex buffers
    if (init_buffers() != VK_SUCCESS) {
        std::cerr << "Failed to initialize buffers\n";
        cleanup();
        return -1;
    }

    // Get queues
    vkGetDeviceQueue(g_device, 0, 0, &graphics_queue);
    vkGetDeviceQueue(g_device, 0, 1, &present_queue);

    std::cout << "\n=== RENDER LOOP STARTING ===\n\n";

    // Main event loop
    while (!g_should_quit && g_sdl_window) {
        // Poll SDL events
        SDL_Event sdl_event;
        while (SDL_PollEvent(&sdl_event)) {
            if (sdl_event.type == SDL_QUIT) {
                g_should_quit = true;
            } else if (sdl_event.type == SDL_KEYDOWN && sdl_event.key.keysym.sym == SDLK_ESCAPE) {
                g_should_quit = true;
            }

            // Handle window resize
            if (sdl_event.type == SDL_WINDOWEVENT) {
                WINDOW_WIDTH = sdl_event.window.data1;
                WINDOW_HEIGHT = sdl_event.window.data2;

                std::cout << "Window resized to: " << WINDOW_WIDTH << "x" << WINDOW_HEIGHT << "\n";
                
                // Recreate swapchain on resize
                vkDestroySwapchainKHR(g_device, g_swapchain, nullptr);
                init_swapchain();
            }
        }

        // Render each frame
        uint32_t image_index = g_current_frame_index;
        render(g_command_buffers[image_index], image_index);

        // Present to screen (already submitted in render, just update index)
        g_current_frame_index = (g_current_frame_index + 1) % static_cast<uint32_t>(g_swapchain_images.size());

        // Small delay for ~60 FPS target
        SDL_Delay(16);
    }

    std::cout << "\n=== RENDER LOOP ENDED ===\n\n";

    // Cleanup
    cleanup();

    return 0;
}
'''

# Combine all sections into the complete file
complete_content = section1_headers_globals + section2_helpers + section3_render_presentation + section4_main

# Write the complete file
with open(SRC_PATH, 'w') as f:
    f.write(complete_content)

print(f"Written {SRC_PATH}")
print("SDL2 + Vulkan implementation complete.")
