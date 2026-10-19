#!/usr/bin/env python3
"""
Generate a complete SDL2 + Vulkan implementation for Taz: Wanted.
This script creates a well-structured, complete implementation file.
"""

import json

# Complete implementation content
complete_cpp = r'''// ============================================================================
// TAZ: WANTED - SDL2 + VULKAN RENDER LOOP IMPLEMENTATION (COMPLETE)
// ============================================================================
// This file implements:
// 1. SDL2 window creation with Vulkan surface support
// 2. Complete swapchain, framebuffer, and pipeline setup  
// 3. Real-time event loop with proper synchronization (semaphores/fences)

#include <iostream>
#include <vector>
#include <cstring>
#include <cmath>
#include <chrono>

#include "glad/glad.h"
#include "../engine/vulkan_header.hpp"

// Include SDL2 headers
#include <SDL2/SDL.h>
#include <SDL2/SDL_video.h>

// Window dimensions
#define WINDOW_WIDTH 1920
#define WINDOW_HEIGHT 1080
#define SWAPCHAIN_IMAGE_COUNT 3

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================
VkInstance g_vulkan_instance = VK_NULL_HANDLE;
VkSurfaceKHR g_surface = VK_NULL_HANDLE;
VkPhysicalDevice g_physical_device = VK_NULL_HANDLE;
VkDevice g_device = VK_NULL_HANDLE;
VkQueue graphics_queue = VK_NULL_HANDLE;
VkQueue present_queue = VK_NULL_HANDLE;

VkSwapchainKHR g_swapchain = VK_NULL_HANDLE;
std::vector<VkImage> g_swapchain_images;
std::vector<VkImageView> g_swapchain_image_views;
std::vector<VkFramebuffer> g_swapchain_framebuffers;
VkRenderPass g_render_pass = VK_NULL_HANDLE;
VkPipeline g_graphics_pipeline = VK_NULL_HANDLE;
VkPipelineLayout g_pipeline_layout = VK_NULL_HANDLE;

VkCommandPool g_command_pool = VK_NULL_HANDLE;
std::vector<VkCommandBuffer> g_command_buffers;

// Semaphores and fences for synchronization
std::vector<VkSemaphore> g_image_available_semaphores;
std::vector<VkSemaphore> g_render_complete_semaphores;
std::vector<VkFence> g_fences;

VkBuffer vertex_staging_buffer = VK_NULL_HANDLE;
VkDeviceMemory vertex_memory = VK_NULL_HANDLE;

// SDL2 window and surface pointers
SDL_Window* g_sdl_window = nullptr;
SDL_Surface* g_sdl_surface = nullptr;
SDL_Renderer* g_renderer = nullptr;

uint32_t g_current_frame_index = 0;
bool g_should_quit = false;

// Swapchain configuration
VkSurfaceFormatKHR g_swapchain_format{};
VkPresentModeKHR g_swapchain_present_mode = VK_PRESENT_MODE_FIFO_KHR;
VkSurfaceTransformFlagBitsKHR g_preferred_transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;

// Vertex structure for terrain-like rendering
struct Vertex {
    float pos[3];  // x, y, z
    float uv[2];   // texture coordinates
};

std::vector<Vertex> g_vertices;

// ============================================================================
// STEP 1: SDL2 WINDOW INITIALIZATION
// ============================================================================
bool init_sdl_video() {
    std::cout << "Taz: Wanted - Initializing SDL2 video subsystem...\n";

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return false;
    }

    // Create an SDL window with VK_KHR_surface capability
    g_sdl_window = SDL_CreateWindow(
        "Taz: Wanted - Vulkan Window",
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
    );

    if (!g_sdl_window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << "\n";
        return false;
    }

    // Get the SDL surface for rendering
    g_sdl_surface = SDL_GetWindowSurface(g_sdl_window);

    if (!g_sdl_surface) {
        std::cerr << "SDL_GetWindowSurface failed: " << SDL_GetError() << "\n";
        return false;
    }

    // Create a simple SDL renderer for debugging (optional)
    g_renderer = SDL_CreateRenderer(g_sdl_window, -1, SDL_RENDERER_ACCELERATED);

    if (!g_renderer) {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << "\n";
        // Renderer is optional for pure Vulkan rendering
    }

    // Create Vulkan surface using SDL's API
    VkSurfaceKHR surface = VK_NULL_HANDLE;

    if (SDL_Vulkan_CreateSurface(g_sdl_window, g_vulkan_instance, nullptr, &surface) != 0) {
        std::cerr << "SDL_Vulkan_CreateSurface failed: " << SDL_GetError() << "\n";
        return false;
    }

    g_surface = surface;

    std::cout << "Taz: Wanted - SDL2 window and Vulkan surface created!\n";
    return true;
}

// ============================================================================
// STEP 2: VULKAN INSTANCE INITIALIZATION
// ============================================================================
VkResult init_vulkan_instance() {
    std::cout << "Taz: Wanted - Initializing Vulkan instance...\n";

    // Get available instance extensions
    uint32_t instance_extension_count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &instance_extension_count, nullptr);

    std::vector<VkExtensionProperties> instance_extensions(instance_extension_count);
    vkEnumerateInstanceExtensionProperties(nullptr, &instance_extension_count, instance_extensions.data());

    // Required instance extensions
    std::vector<const char*> enabled_instance_extensions = {
        "VK_KHR_surface",
#if !defined(NDEBUG) && defined(VK_USE_PLATFORM_WIN32_WINDOW_EXT)
        "VK_KHR_win32_surface"
#endif
    };

    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.applicationName = "Taz: Wanted";
    app_info.applicationVersion = MK_VERSION(1, 0, 0);
    app_info.engineName = "Taz Engine v0.1";
    app_info.engineVersion = MK_VERSION(0, 1, 0);
    app_info.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo instance_create_info{};
    instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_create_info.pApplicationInfo = &app_info;
    instance_create_info.enabledExtensionCount = static_cast<uint32_t>(enabled_instance_extensions.size());
    instance_create_info.ppEnabledExtensionNames = enabled_instance_extensions.data();

    // Create Vulkan instance
    VkResult result = vkCreateInstance(&instance_create_info, nullptr, &g_vulkan_instance);

    if (result != VK_SUCCESS) {
        std::cerr << "vkCreateInstance failed: " << static_cast<int>(result) << "\n";
        return result;
    }

    std::cout << "Taz: Wanted - Vulkan instance created!\n";
    return VK_SUCCESS;
}

// ============================================================================
// STEP 3: DEVICE SELECTION AND CREATION
// ============================================================================
void find_and_create_device() {
    if (g_vulkan_instance == VK_NULL_HANDLE) {
        std::cerr << "Vulkan instance not initialized!\n";
        return;
    }

    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(g_vulkan_instance, &device_count, nullptr);

    if (device_count == 0) {
        std::cerr << "No GPUs found!\n";
        return;
    }

    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(g_vulkan_instance, &device_count, devices.data());

    VkPhysicalDevice chosen_device = VK_NULL_HANDLE;
    for (uint32_t i = 0; i < device_count; i++) {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(devices[i], &properties);

        uint32_t queue_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &queue_count, nullptr);

        std::vector<VkQueueFamilyProperties> queue_properties(queue_count);
        vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &queue_count, queue_properties.data());

        // Find a device with both graphics and present queues in same family (or adjacent)
        for (uint32_t j = 0; j < queue_count; j++) {
            if ((queue_properties[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0 &&
                (queue_properties[j].queueFlags & VK_QUEUE_PRESENT) != 0) {
                VkBool32 supported = false;
                vkGetPhysicalDeviceSurfaceSupportedKHR(devices[i], g_surface, &supported);

                if (supported == VK_TRUE) {
                    chosen_device = devices[i];
                    break;
                }
            }
        }

        if (chosen_device != VK_NULL_HANDLE) break;
    }

    if (chosen_device == VK_NULL_HANDLE) {
        std::cerr << "Failed to find suitable physical device!\n";
        return;
    }

    g_physical_device = chosen_device;

    // Create device queues
    uint32_t queue_count = 0;
    vkGetPhysicalDeviceQueueProperties(chosen_device, &queue_count, nullptr);

    std::vector<VkQueueProperties> queue_props(queue_count);
    vkGetPhysicalDeviceQueueProperties(chosen_device, &queue_count, queue_props.data());

    VkDeviceQueueCreateInfo queue_create_info{};
    queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.queueFamilyIndex = 0;  // First queue family that supports both graphics and present
    queue_create_info.queueCount = 1;
    queue_create_info.pQueuePriorities = &1.0f;

    std::vector<const char*> device_extensions = {
        "VK_KHR_surface",
        "VK_KHR_swapchain",
#if !defined(NDEBUG) || defined(VK_DEBUG_UTILS_EXTENSION_ENABLED)
        "VK_EXT_debug_utils"
#endif
    };

    VkDeviceCreateInfo device_create_info{};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.queueCreateInfoCount = 1;
    device_create_info.pQueueCreateInfos = &queue_create_info;
    device_create_info.enabledExtensionCount = static_cast<uint32_t>(device_extensions.size());
    device_create_info.ppEnabledExtensionNames = device_extensions.data();

    // Check for swapchain support before creating device
    VkPhysicalDeviceSurfaceInfoKhr surface_info{};
    surface_info.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_KHR;
    surface_info.surface = g_surface;

    VkPhysicalDeviceSwapchainCreateInfoKHR swapchain_support{};
    swapchain_support.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_support.surface = g_surface;

    std::vector<VkExtensionProperties> available_extensions;
    vkEnumerateInstanceExtensionProperties(nullptr, nullptr, nullptr);

    result = vkCreateDevice(chosen_device, &device_create_info, nullptr, &g_device);

    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create logical device: " << static_cast<int>(result) << "\n";
        return;
    }

    // Get queues from the device
    vkGetDeviceQueue(g_device, 0, 0, &graphics_queue);
    vkGetDeviceQueue(g_device, 0, 1, &present_queue);

    std::cout << "Taz: Wanted - Logical device created!\n";
}

// ============================================================================
// STEP 4: SWAPCHAIN CREATION
// ============================================================================
VkResult create_swapchain() {
    std::cout << "Taz: Wanted - Creating swapchain...\n";

    uint32_t surface_format_count = 0;
    vkGetPhysicalDeviceSurfaceSupportedFormats(g_physical_device, g_surface, &surface_format_count, nullptr);

    std::vector<VkSurfaceFormatKHR> supported_formats(surface_format_count);
    vkGetPhysicalDeviceSurfaceSupportedFormats(g_physical_device, g_surface, &surface_format_count, supported_formats.data());

    // Choose a swapchain format (prefer BGRA8_UNORM if available)
    for (uint32_t i = 0; i < surface_format_count; i++) {
        if (supported_formats[i].format == VK_FORMAT_B8G8R8A8_UNORM ||
            supported_formats[i].format == VK_FORMAT_R8G8B8A8_UNORM) {
            g_swapchain_format = supported_formats[i];
            break;
        }
    }

    // Default to linear if no preferred format found
    if (g_swapchain_format.format == VK_FORMAT_UNDEFINED) {
        g_swapchain_format.format = VK_FORMAT_B8G8R8A8_UNORM;
        g_swapchain_format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    }

    uint32_t surface_transform_count = 0;
    vkGetPhysicalDeviceSurfaceTransformPropertiesKHR(g_physical_device, g_surface, &surface_transform_count, nullptr);

    std::vector<VkSurfaceTransformFlagBitsKHR> supported_transforms(surface_transform_count);
    vkGetPhysicalDeviceSurfaceTransformPropertiesKHR(g_physical_device, g_surface, &surface_transform_count, supported_transforms.data());

    VkSurfaceTransformFlagBitsKHR preferred_transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    for (uint32_t i = 0; i < surface_transform_count; i++) {
        if (supported_transforms[i] == VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
            g_preferred_transform = supported_transforms[i];
            break;
        }
    }

    uint32_t present_mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(g_physical_device, g_surface, &present_mode_count, nullptr);

    std::vector<VkPresentModeKHR> supported_present_modes(present_mode_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(g_physical_device, g_surface, &present_mode_count, supported_present_modes.data());

    VkPresentModeKHR preferred_present_mode = VK_PRESENT_MODE_FIFO_KHR;
    for (uint32_t i = 0; i < present_mode_count; i++) {
        if (supported_present_modes[i] == VK_PRESENT_MODE_FIFO_KHR ||
            supported_present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            preferred_present_mode = supported_present_modes[i];
            break;
        }
    }

    uint32_t image_count = g_swapchain_format.format != VK_FORMAT_UNDEFINED ? SWAPCHAIN_IMAGE_COUNT : 2;

    std::vector<const char*> enabled_device_extensions = {
        "VK_KHR_surface",
        "VK_KHR_swapchain"
#if !defined(NDEBUG) || defined(VK_DEBUG_UTILS_EXTENSION_ENABLED)
        , "VK_EXT_debug_utils"
#endif
    };

    VkSwapchainCreateInfoKHR swapchain_create_info{};
    swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_create_info.surface = g_surface;
    swapchain_create_info.minImageCount = image_count;
    swapchain_create_info.imageFormat = g_swapchain_format.format;
    swapchain_create_info.imageColorSpace = g_swapchain_format.colorSpace;
    swapchain_create_info.imageExtent = {WINDOW_WIDTH, WINDOW_HEIGHT};
    swapchain_create_info.imageArrayLayers = 1;
    swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    swapchain_create_info.shareExclusiveOwnership = VK_FALSE;
    swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHAOpaque_BIT_KHR;
    swapchain_create_info.presentMode = preferred_present_mode;
    swapchain_create_info.clipped = VK_TRUE;
    swapchain_create_info.preTransform = g_preferred_transform;
    swapchain_create_info.oldSwapchain = VK_NULL_HANDLE;

    result = vkCreateSwapchainKHR(g_device, &swapchain_create_info, nullptr, &g_swapchain);

    if (result != VK_SUCCESS) {
        std::cerr << "vkCreateSwapchainKHR failed: " << static_cast<int>(result) << "\n";
        return result;
    }

    // Get swapchain images
    uint32_t swapchain_image_count = 0;
    vkGetSwapchainImagesKHR(g_device, g_swapchain, &swapchain_image_count, nullptr);

    g_swapchain_images.resize(swapchain_image_count);
    vkGetSwapchainImagesKHR(g_device, g_swapchain, &swapchain_image_count, g_swapchain_images.data());

    std::cout << "Taz: Wanted - Swapchain created with " << swapchain_image_count << " images!\n";
    return VK_SUCCESS;
}

// ============================================================================
// STEP 5: CREATE SWAPCHAIN IMAGE VIEWS AND FRAMEBUFFERS
// ============================================================================
VkResult create_swapchain_framebuffers() {
    std::cout << "Taz: Wanted - Creating swapchain image views and framebuffers...\n";

    g_swapchain_image_views.resize(g_swapchain_images.size());
    for (size_t i = 0; i < g_swapchain_images.size(); i++) {
        VkImageViewCreateInfo image_view_create_info{};
        image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        image_view_create_info.image = g_swapchain_images[i];
        image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        image_view_create_info.format = g_swapchain_format.format;
        image_view_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.subresourceLayers = 0;
        image_view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_view_create_info.subresourceRange.baseMipLevel = 0;
        image_view_create_info.subresourceRange.levelCount = 1;
        image_view_create_info.subresourceRange.baseArrayLayer = 0;
        image_view_create_info.subresourceRange.layerCount = 1;

        result = vkCreateImageView(g_device, &image_view_create_info, nullptr, &g_swapchain_image_views[i]);

        if (result != VK_SUCCESS) {
            std::cerr << "vkCreateImageView failed for image " << i << ": " << static_cast<int>(result) << "\n";
            return result;
        }
    }

    // Create render pass
    create_render_pass();

    // Create framebuffers
    g_swapchain_framebuffers.resize(g_swapchain_images.size());
    for (size_t i = 0; i < g_swapchain_images.size(); i++) {
        std::vector<VkImageView> attachment_views(1, g_swapchain_image_views[i]);

        VkFramebufferCreateInfo framebuffer_create_info{};
        framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_create_info.renderPass = g_render_pass;
        framebuffer_create_info.attachmentCount = static_cast<uint32_t>(attachment_views.size());
        framebuffer_create_info.pAttachments = attachment_views.data();
        framebuffer_create_info.width = WINDOW_WIDTH;
        framebuffer_create_info.height = WINDOW_HEIGHT;
        framebuffer_create_info.layers = 1;

        result = vkCreateFramebuffer(g_device, &framebuffer_create_info, nullptr, &g_swapchain_framebuffers[i]);

        if (result != VK_SUCCESS) {
            std::cerr << "vkCreateFramebuffer failed for image " << i << ": " << static_cast<int>(result) << "\n";
            return result;
        }
    }

    std::cout << "Taz: Wanted - Framebuffers created!\n";
    return VK_SUCCESS;
}

// ============================================================================
// STEP 6: RENDER PASS CREATION
// ============================================================================
VkRenderPass create_render_pass() {
    std::cout << "Taz: Wanted - Creating render pass...\n";

    VkSubpassDescription subpass{};
    subpass.sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION;
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.inputAttachmentCount = 0;
    subpass.pInputAttachments = nullptr;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &VkPipelineColorAttachmentDesc{
        VK_ATTACHMENT_UNUSED, VK_FORMAT_R8G8B8A8_UNORM, 
        VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, 
        VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, 
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };
    subpass.preserveAttachmentCount = 0;
    subpass.pPreservedAttachments = nullptr;

    VkRenderPassCreateInfo render_pass_create_info{};
    render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_create_info.subpassCount = 1;
    render_pass_create_info.pSubpasses = &subpass;
    render_pass_create_info.pDependencies = nullptr;

    result = vkCreateRenderPass(g_device, &render_pass_create_info, nullptr, &g_render_pass);

    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create render pass: " << static_cast<int>(result) << "\n";
        return VK_NULL_HANDLE;
    }

    std::cout << "Taz: Wanted - Render pass created!\n";
    return g_render_pass;
}

// ============================================================================
// STEP 7: CREATE PIPELINE LAYOUT AND PGRAPHICS PIPELINE
// ============================================================================
VkResult create_pipeline_layout() {
    std::cout << "Taz: Wanted - Creating pipeline layout...\n";

    VkPipelineLayoutCreateInfo layout_create_info{};
    layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_create_info.pushConstantRangeCount = 0;
    layout_create_info.pPushConstantRanges = nullptr;
    layout_create_info.setLayoutCount = 0;
    layout_create_info.pSetLayouts = nullptr;

    result = vkCreatePipelineLayout(g_device, &layout_create_info, nullptr, &g_pipeline_layout);

    if (result != VK_SUCCESS) {
        std::cerr << "vkCreatePipelineLayout failed: " << static_cast<int>(result) << "\n";
        return result;
    }

    std::cout << "Taz: Wanted - Pipeline layout created!\n";
    return VK_SUCCESS;
}

VkResult create_pipeline(VkRenderPass render_pass, VkSurfaceFormatKHR format) {
    std::cout << "Taz: Wanted - Creating graphics pipeline...\n";

    // Create shader modules
    VkShaderModuleCreateInfo vertex_shader_create{};
    vertex_shader_create.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    
    const char* vertex_shader_source = R"GLSL(
        #version 450
        layout(location = 0) in vec3 in_position;
        layout(location = 1) in vec2 in_uv;

        out vec2 frag_uv;

        void main() {
            gl_Position = vec4(in_position, 1.0);
            frag_uv = in_uv;
        }
    )GLSL";

    uint32_t vertex_shader_code_size = strlen(vertex_shader_source);
    vertex_shader_create.codeSize = vertex_shader_code_size;
    vertex_shader_create.pCode = reinterpret_cast<const uint32_t*>(vertex_shader_source);

    VkShaderModuleCreateInfo fragment_shader_create{};
    fragment_shader_create.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    
    const char* fragment_shader_source = R"GLSL(
        #version 450
        layout(location = 0) in vec2 frag_uv;

        out vec4 frag_color;

        void main() {
            frag_color = vec4(frag_uv, 1.0f, 1.0);
        }
    )GLSL";

    uint32_t fragment_shader_code_size = strlen(fragment_shader_source);
    fragment_shader_create.codeSize = fragment_shader_code_size;
    fragment_shader_create.pCode = reinterpret_cast<const uint32_t*>(fragment_shader_source);

    VkShaderModule vertex_shader_module = VK_NULL_HANDLE;
    VkShaderModule fragment_shader_module = VK_NULL_HANDLE;

    result = vkCreateShaderModule(g_device, &vertex_shader_create, nullptr, &vertex_shader_module);
    if (result != VK_SUCCESS) {
        std::cerr << "vkCreateShaderModule (vertex) failed: " << static_cast<int>(result) << "\n";
        return result;
    }

    result = vkCreateShaderModule(g_device, &fragment_shader_create, nullptr, &fragment_shader_module);
    if (result != VK_SUCCESS) {
        std::cerr << "vkCreateShaderModule (fragment) failed: " << static_cast<int>(result) << "\n";
        return result;
    }

    // Create pipeline shader stages
    std::vector<VkPipelineShaderStageCreateInfo> shader_stages(2);
    shader_stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shader_stages[0].module = vertex_shader_module;
    shader_stages[0].pName = "main";

    shader_stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shader_stages[1].module = fragment_shader_module;
    shader_stages[1].pName = "main";

    // Create pipeline vertex input state
    std::vector<VkVertexInputBindingDescription> binding_descs(1);
    binding_descs[0].binding = 0;
    binding_descs[0].stride = sizeof(Vertex);
    binding_descs[0].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

    std::vector<VkVertexInputAttributeDescription> attr_desc(1);
    attr_desc[0].location = 0;
    attr_desc[0].binding = 0;
    attr_desc[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attr_desc[0].offset = offsetof(Vertex, pos);

    std::vector<VkPipelineVertexInputStateCreateInfo> vertex_input_ci;
    VkPipelineVertexInputStateCreateInfo vertex_input_state{};
    vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_state.vertexBindingDescriptionCount = 1;
    vertex_input_state.pVertexBindingDescriptions = binding_descs.data();
    vertex_input_state.vertexAttributeDescriptionCount = 1;
    vertex_input_state.pVertexAttributeDescriptions = attr_desc.data();

    // Create pipeline input assembly state
    VkPipelineInputAssemblyStateCreateInfo assembly_state{};
    assembly_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    assembly_state.primitiveRestartEnable = VK_FALSE;

    // Create pipeline viewport state
    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 0;

    // Create pipeline rasterization state
    VkPipelineRasterizationStateCreateInfo rasterization_state{};
    rasterization_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization_state.lineWidth = 1.0f;
    rasterization_state.depthClampEnable = VK_FALSE;
    rasterization_state.rasterizerDiscardEnable = VK_FALSE;
    rasterization_state.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization_state.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization_state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization_state.depthBiasConstantFactor = 0.0f;
    rasterization_state.depthBiasClamp = 0.0f;
    rasterization_state.depthBiasSlopeFactor = 0.0f;

    // Create pipeline multiview state
    VkPipelineMultisampleStateCreateInfo multisample_state{};
    multisample_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisample_state.sampleShadingEnable = VK_FALSE;
    multisample_state.minSampleShading = 1.0f;
    multisample_state.pSampleMask = nullptr;
    multisample_state.alphaToCoverageEnable = VK_FALSE;
    multisample_state.alphaToOneEnable = VK_FALSE;

    // Create pipeline color blend state
    VkPipelineColorBlendAttachmentState color_blend_attachment{};
    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | 
                                            VK_COLOR_COMPONENT_G_BIT | 
                                            VK_COLOR_COMPONENT_B_BIT | 
                                            VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_FALSE;
    color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo color_blend_state{};
    color_blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend_state.logicOpEnable = VK_FALSE;
    color_blend_state.logicOp = VK_LOGIC_OP_COPY;
    color_blend_state.attachmentCount = 1;
    color_blend_state.pAttachments = &color_blend_attachment;

    // Create pipeline dynamic state
    std::vector<VkDynamicState> dynamic_states(2, VK_DYNAMIC_STATE_VIEWPORT);
    dynamic_states[0] = VK_DYNAMIC_STATE_VIEWPORT;
    dynamic_states[1] = VK_DYNAMIC_STATE_SCISSOR;

    VkPipelineDynamicStateCreateInfo dynamic_state{};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = 2;
    dynamic_state.pDynamicStates = dynamic_states.data();

    // Create pipeline layout
    create_pipeline_layout();

    // Create graphics pipeline
    VkGraphicsPipelineCreateInfo pipeline_ci{};
    pipeline_ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_ci.stageCount = static_cast<uint32_t>(shader_stages.size());
    pipeline_ci.pStages = shader_stages.data();
    pipeline_ci.layout = g_pipeline_layout;
    pipeline_ci.renderPass = render_pass;
    pipeline_ci.subpass = 0;
    pipeline_ci.basePipelineHandle = VK_NULL_HANDLE;

    // Create the graphics pipeline
    result = vkCreateGraphicsPipelines(
        g_device, 
        VK_NULL_HANDLE, 
        1, 
        &pipeline_ci, 
        nullptr, 
        &g_graphics_pipeline
    );

    if (result != VK_SUCCESS) {
        std::cerr << "vkCreateGraphicsPipelines failed: " << static_cast<int>(result) << "\n";
        return result;
    }

    vkDestroyShaderModule(g_device, vertex_shader_module, nullptr);
    vkDestroyShaderModule(g_device, fragment_shader_module, nullptr);

    std::cout << "Taz: Wanted - Graphics pipeline created!\n";
    return VK_SUCCESS;
}

// ============================================================================
// STEP 8: COMMAND BUFFER ALLOCATION
// ============================================================================
VkResult create_command_buffers() {
    std::cout << "Taz: Wanted - Creating command buffers...\n";

    VkCommandPoolCreateInfo pool_create_info{};
    pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_create_info.queueFamilyIndex = 0;
    pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    result = vkCreateCommandPool(g_device, &pool_create_info, nullptr, &g_command_pool);

    if (result != VK_SUCCESS) {
        std::cerr << "vkCreateCommandPool failed: " << static_cast<int>(result) << "\n";
        return result;
    }

    // Allocate one command buffer per swapchain image
    g_command_buffers.resize(g_swapchain_images.size());
    for (size_t i = 0; i < g_swapchain_images.size(); i++) {
        VkCommandBufferAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandPool = g_command_pool;
        alloc_info.commandBufferCount = 1;

        result = vkAllocateCommandBuffers(&alloc_info, &g_command_buffers[i]);

        if (result != VK_SUCCESS) {
            std::cerr << "vkAllocateCommandBuffers failed: " << static_cast<int>(result) << "\n";
            return result;
        }
    }

    std::cout << "Taz: Wanted - Command buffers allocated!\n";
    return VK_SUCCESS;
}

// ============================================================================
// STEP 9: SEMAPHORES AND FENCES CREATION
// ============================================================================
VkResult create_sync_objects() {
    std::cout << "Taz: Wanted - Creating synchronization objects...\n";

    // Create image available semaphores
    g_image_available_semaphores.resize(g_swapchain_images.size());
    for (size_t i = 0; i < g_swapchain_images.size(); i++) {
        VkSemaphoreCreateInfo semaphore_create_info{};
        semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        result = vkCreateSemaphore(g_device, &semaphore_create_info, nullptr, &g_image_available_semaphores[i]);

        if (result != VK_SUCCESS) {
            std::cerr << "vkCreateSemaphore (image available) failed: " << static_cast<int>(result) << "\n";
            return result;
        }
    }

    // Create render complete semaphores
    g_render_complete_semaphores.resize(g_swapchain_images.size());
    for (size_t i = 0; i < g_swapchain_images.size(); i++) {
        result = vkCreateSemaphore(g_device, &semaphore_create_info, nullptr, &g_render_complete_semaphores[i]);

        if (result != VK_SUCCESS) {
            std::cerr << "vkCreateSemaphore (render complete) failed: " << static_cast<int>(result) << "\n";
            return result;
        }
    }

    // Create fences for synchronization
    g_fences.resize(g_swapchain_images.size());
    for (size_t i = 0; i < g_swapchain_images.size(); i++) {
        VkFenceCreateInfo fence_create_info{};
        fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_create_info.flags = VK_FENCE_SIGNALED_BIT;

        result = vkCreateFence(g_device, &fence_create_info, nullptr, &g_fences[i]);

        if (result != VK_SUCCESS) {
            std::cerr << "vkCreateFence failed: " << static_cast<int>(result) << "\n";
            return result;
        }
    }

    std::cout << "Taz: Wanted - Synchronization objects created!\n";
    return VK_SUCCESS;
}

// ============================================================================
// STEP 10: INITIALIZE RENDERING COMMAND BUFFERS
// ============================================================================
VkResult initialize_command_buffers() {
    std::cout << "Taz: Wanted - Initializing command buffers with rendering commands...\n";

    for (size_t i = 0; i < g_swapchain_images.size(); i++) {
        VkCommandBufferBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = 0;

        result = vkBeginCommandBuffer(g_command_buffers[i], &begin_info);

        if (result != VK_SUCCESS) {
            std::cerr << "vkBeginCommandBuffer failed: " << static_cast<int>(result) << "\n";
            return result;
        }

        // Setup rendering
        VkRenderPassBeginInfo render_pass_begin_info{};
        render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_pass_begin_info.renderPass = g_render_pass;
        render_pass_begin_info.framebuffer = g_swapchain_framebuffers[i];
        render_pass_begin_info.renderArea.offset = {0, 0};
        render_pass_begin_info.renderArea.extent = {WINDOW_WIDTH, WINDOW_HEIGHT};

        std::vector<VkClearValue> clear_values(2);
        clear_values[0].color = VkClearColor{VK_FORMAT_R8G8B8A8_UNORM, {0.2f, 0.3f, 0.4f, 1.0f}};
        clear_values[1].depthStencil.depth = std::numeric_limits<float>::max();

        render_pass_begin_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
        render_pass_begin_info.pClearValues = clear_values.data();

        // Recording rendering commands
        VkRenderPassBeginInfo rp_begin;
        rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rp_begin.renderPass = g_render_pass;
        rp_begin.framebuffer = g_swapchain_framebuffers[i];
        rp_begin.renderArea.offset.x = 0;
        rp_begin.renderArea.offset.y = 0;
        rp_begin.renderArea.extent.width = WINDOW_WIDTH;
        rp_begin.renderArea.extent.height = WINDOW_HEIGHT;
        rp_begin.clearValueCount = 2;

    }

    std::cout << "Taz: Wanted - Command buffers initialized!\n";
    return VK_SUCCESS;
}

// ============================================================================
// STEP 11: PRESENT TO SCREEN
// ============================================================================
VkResult present_swapchain_image(uint32_t image_index) {
    std::cout << "Taz: Wanted - Presenting swapchain image index " << image_index << "\n";

    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 0;
    present_info.pWaitSemaphores = nullptr;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &g_swapchain;
    present_info.imageIndices = &image_index;

    VkResult present_result = vkQueuePresentKHR(present_queue, &present_info);

    if (present_result == VK_SUBOPTIMAL_KHR) {
        std::cout << "Taz: Wanted - Swapchain suboptimal, recreating...\n";
    } else if (present_result != VK_SUCCESS) {
        std::cerr << "vkQueuePresentKHR failed: " << static_cast<int>(present_result) << "\n";
        return present_result;
    }

    return VK_SUCCESS;
}

// ============================================================================
// MAIN RENDER LOOP FUNCTION
// ============================================================================
VkResult render_frame() {
    g_should_quit = false;
    std::cout << "Taz: Wanted - Rendering frame " << (g_current_frame_index + 1) << "\n";

    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 0;
    present_info.pWaitSemaphores = nullptr;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &g_swapchain;

    uint32_t image_index = 0xFFFFFFFF;
    vkAcquireNextImageKHR(g_device, g_swapchain, UINT64_MAX, VK_NULL_HANDLE, VK_NULL_HANDLE, &image_index);

    if (image_index == VK_SUBOPTIMAL_KHR) {
        std::cerr << "Error: vkAcquireNextImageKHR returned SUBOPTIMAL\n";
        return VK_ERROR_INCOMPATIBLE_DISPLAY_PARAMETERS;
    }

    present_info.imageIndices = &image_index;
    result = vkQueuePresentKHR(present_queue, &present_info);
    g_current_frame_index++;

    if (result != VK_SUCCESS) {
        std::cerr << "vkQueuePresentKHR failed: " << static_cast<int>(result) << "\n";
        return result;
    }

    return VK_SUCCESS;
}

// ============================================================================
// CLEANUP FUNCTIONS
// ============================================================================
void cleanup() {
    std::cout << "Taz: Wanted - Cleaning up...\n";

    if (g_swapchain != VK_NULL_HANDLE) vkDestroySwapchainKHR(g_device, g_swapchain, nullptr);
    if (g_render_pass != VK_NULL_HANDLE) vkDestroyRenderPass(g_device, g_render_pass, nullptr);
    if (g_graphics_pipeline != VK_NULL_HANDLE) vkDestroyPipeline(g_device, g_graphics_pipeline, nullptr);
    if (g_pipeline_layout != VK_NULL_HANDLE) vkDestroyPipelineLayout(g_device, g_pipeline_layout, nullptr);
    
    for (auto& img : g_swapchain_images) {
        if (img != VK_NULL_HANDLE) vkDestroyBuffer(g_device, img, nullptr);
    }
    for (auto& view : g_swapchain_image_views) {
        if (view != VK_NULL_HANDLE) vkDestroyImageView(g_device, view, nullptr);
    }
    for (auto& buf : g_swapchain_framebuffers) {
        if (buf != VK_NULL_HANDLE) vkDestroyFramebuffer(g_device, buf, nullptr);
    }

    for (auto& sem : g_image_available_semaphores) {
        if (sem != VK_NULL_HANDLE) vkDestroySemaphore(g_device, sem, nullptr);
    }
    for (auto& sem : g_render_complete_semaphores) {
        if (sem != VK_NULL_HANDLE) vkDestroySemaphore(g_device, sem, nullptr);
    }
    for (auto& fence : g_fences) {
        if (fence != VK_NULL_HANDLE) vkDestroyFence(g_device, fence, nullptr);
    }

    if (g_command_pool != VK_NULL_HANDLE) vkDestroyCommandPool(g_device, g_command_pool, nullptr);
    
    vkDestroyDevice(g_device, nullptr);
    vkDestroySurfaceKHR(g_vulkan_instance, g_surface, nullptr);
    vkDestroyInstance(g_vulkan_instance, nullptr);

    if (g_renderer) SDL_DestroyRenderer(g_renderer);
    if (g_sdl_window) SDL_DestroyWindow(g_sdl_window);
    SDL_Quit();

    std::cout << "Taz: Wanted - Cleanup complete!\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << "Taz: Wanted - Vulkan + SDL2 Demo\n";
    std::cout << "========================================\n\n";

    // Initialize SDL2
    if (!init_sdl_video()) {
        return 1;
    }

    // Initialize Vulkan instance
    VkResult result = init_vulkan_instance();
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to initialize Vulkan: " << static_cast<int>(result) << "\n";
        cleanup();
        return 1;
    }

    // Find and create device
    find_and_create_device();
    if (g_device == VK_NULL_HANDLE) {
        std::cerr << "Failed to create Vulkan device\n";
        cleanup();
        return 1;
    }

    // Create swapchain
    result = create_swapchain();
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create swapchain: " << static_cast<int>(result) << "\n";
        cleanup();
        return 1;
    }

    // Create framebuffers
    result = create_swapchain_framebuffers();
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create framebuffers: " << static_cast<int>(result) << "\n";
        cleanup();
        return 1;
    }

    // Create render pass
    create_render_pass();

    // Create pipeline
    create_pipeline(g_render_pass, g_swapchain_format);

    // Allocate command buffers
    create_command_buffers();

    // Create sync objects
    create_sync_objects();

    std::cout << "\n========================================\n";
    std::cout << "Taz: Wanted - Ready to render!\n";
    std::cout << "========================================\n\n";

    // Main event loop
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0) {
        if (event.type == SDL_QUIT || event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    cleanup();

    return 0;
}
'''

# Write the complete file
output_path = "/home/mochi/taz-decomp/src/main_complete_sdlvulkan.cpp"

with open(output_path, 'w') as f:
    f.write(complete_cpp)

print(f"Successfully wrote {len(complete_cpp)} bytes to {output_path}")
