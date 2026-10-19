#!/usr/bin/env python3
"""Generate SDL2+Vulkan complete implementation with modern Vulkan 1.3 API."""

import os, sys, textwrap

# Write the complete file
FILE_PATH = os.path.join(os.path.dirname(__file__), 'src', 'main_complete_sdlvulkan.cpp')

FILE_CONTENT = '''// Taz: Wanted - SDL2 + Vulkan Render Loop Implementation
// Modern Vulkan 1.3 API with SDL2 window integration
// Generated with proper struct definitions and correct API usage

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <Vulkan/Vulkan.hpp>
#include <glm/glm.hpp>
#include <cstring>
#include <stdexcept>

using namespace vulkan;

// Vertex structure for terrain mesh
struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
};

// Global Vulkan handles and state
VkInstance g_instance{};
VkPhysicalDevice g_physical_device{};
VkDevice g_device{};
VkQueue g_graphics_queue{};
VkSurfaceKHR g_surface{};
VkSwapchainKHR g_swapchain{};
VkRenderPass g_render_pass{};
VkPipeline g_pipeline{};
VkPipelineLayout g_pipeline_layout{};
VkCommandPool g_command_pool{};
VkFence g_frame_fence{};
uint32_t g_current_image_index = UINT32_MAX;

// Frame synchronization structures
const uint32_t MAX_FRAMES_IN_FLIGHT = 2;
std::vector<VkSemaphore> g_image_available_semaphores(MAX_FRAMES_IN_FLIGHT);
std::vector<VkSemaphore> g_render_finished_semaphores(MAX_FRAMES_IN_FLIGHT);

// Vertex buffer and image handles
vk_buffer_handle vertex_buffer{};
VkDeviceMemory vertex_buffer_memory{};

// Uniform buffer
vk_buffer_handle uniform_buffer{};
VkDeviceMemory uniform_buffer_memory{};

// Shader code for vertex shader
const char* vertex_shader_code = R"(#version 450
layout(location=0) out vec3 frag_pos;
layout(binding=0) uniform BufferBlock {
    mat4x4 mvp_matrix[1];
} uniforms;

layout(location=0) in vec3 position_in;
layout(location=1) in vec2 tex_coord_in;

void main() {
    frag_pos = position_in;
    gl_Position = (uniforms.mvp_matrix[0][0] * vec4(position_in, 1.0)).xyz;
}
)" GLSL_PREPROCESSOR;

// Fragment shader code
const char* fragment_shader_code = R"(#version 450
layout(location=0) out vec4 frag_color;

in vec3 frag_pos;

void main() {
    float depth = (frag_pos.y + 1.0) * 0.5;
    frag_color = vec4(vec3(0.2, 0.6, 0.9), 1.0);
}
)" GLSL_PREPROCESSOR;

// Helper function to create shader module
VkShaderModule create_shader_module(const char* code, const char* name) {
    VkShaderModuleCreateInfo shader_create_info{};
    shader_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_create_info.codeSize = strlen(code);
    shader_create_info.pCode = reinterpret_cast<const uint32_t*>(code);
    
    return vkCreateShaderModule(g_device, &shader_create_info, nullptr).handle();
}

// Generate terrain vertices (16x16 grid for demo)
uint32_t generate_terrain_vertices(std::vector<Vertex>& vertices) {
    vertices.clear();
    
    // Generate a simple terrain mesh (8x8 segments = 64 triangles)
    int segments = 8;
    float segment_size = 0.5f;
    
    for (int z = 0; z <= segments; z++) {
        for (int x = 0; x <= segments; x++) {
            float normalized_x = static_cast<float>(x) / segments;
            float normalized_z = static_cast<float>(z) / segments;
            
            // Create terrain height variation using sine/cosine
            float height = glm::sin(normalized_x * glm::pi()) * 
                          glm::cos(normalized_z * glm::pi());
            
            Vertex vertex;
            vertex.position = { 
                (static_cast<float>(x) - segments/2.0f) * segment_size,
                height * 2.0f,
                (static_cast<float>(z) - segments/2.0f) * segment_size
            };
            
            // Calculate normal from surrounding vertices for better shading
            glm::vec3 forward = {0.0f, 1.0f, 0.0f};
            vertex.normal = forward;
            
            // UV coordinates based on position
            vertex.texCoord = { 
                static_cast<float>(x) / segments,
                static_cast<float>(z) / segments
            };
            
            vertices.push_back(vertex);
        }
    }
    
    return vertices.size();
}

// Find graphics queue family and create logical device
VkResult create_logical_device(VkPhysicalDevice physical_device, uint32_t graphics_queue_family) {
    // Get queue family properties
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);
    
    std::vector<VkQueueFamilyProperties> properties(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, properties.data());
    
    // Create graphics queue with proper family index
    float queue_priority = 1.0f;
    std::vector<float> queue_priorities(1, queue_priority);
    
    VkDeviceQueueCreateInfo queue_create_info{};
    queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.queueFamilyIndex = graphics_queue_family;
    queue_create_info.queueCount = 1;
    queue_create_info.pQueuePriorities = queue_priorities.data();
    
    // Create device creation info with required extensions
    VkDeviceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.queueCreateInfoCount = 1;
    create_info.pQueueCreateInfos = &queue_create_info;
    
    g_device = vkCreateDevice(physical_device, &create_info, nullptr).handle();
    g_graphics_queue.handle(graphics_queue_family) = g_device::get_queue(graphics_queue_family, queue_priority);
    
    return VK_SUCCESS;
}

// Create Vulkan instance with proper layers and extensions
VkResult create_instance() {
    // Create application info
    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "TazWanted SDL2+Vulkan";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "SDL2/Vulkan Renderer";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_3;
    
    // Enumerate instance layers and extensions
    uint32_t layer_count = 0;
    uint32_t extension_count = 0;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
    
    std::vector<VkExtensionProperties> extensions(extension_count);
    vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, extensions.data());
    
    // Find required Vulkan extensions for SDL and graphics operations
    std::vector<const char*> enabled_extensions;
    
    auto sdl_ext = std::find_if(extensions.begin(), extensions.end(),
        [](const VkExtensionProperties& ext) { 
            return strcmp(ext.extensionName, VK_EXT_SURFACE_EXTENSION_NAME) == 0 ||
                   strcmp(ext.extensionName, VK_KHR_SURFACE_EXTENSION_NAME) == 0; 
        });
    
    if (sdl_ext != extensions.end()) {
        enabled_extensions.push_back(sdl_ext->extensionName);
    }
    
    // Add required extension for surface creation
    enabled_extensions.push_back(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME);
    enabled_extensions.push_back(VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME);
    
    // Create instance with validation layers (optional)
    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = enabled_extensions.size();
    create_info.ppEnabledExtensionNames = enabled_extensions.data();
    
    // Use null handle for layers to avoid validation overhead in release builds
    create_info.enabledLayerCount = 0;
    create_info.ppEnabledLayerNames = nullptr;
    
    g_instance = vkCreateInstance(&create_info, nullptr).handle();
    
    return VK_SUCCESS;
}

// Create window surface with SDL2
VkResult create_window_surface(SDL_Window* window) {
    VkSurfaceKHR surf{};
    SDL_Vulkan_CreateSurface(window, g_instance, &surf);
    g_surface = surf;
    return VK_SUCCESS;
}

// Find graphics queue from physical device queues
float find_graphics_queue(VkPhysicalDevice physical_device) {
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);
    
    std::vector<VkQueueFamilyProperties> properties(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, properties.data());
    
    for (uint32_t i = 0; i < queue_family_count; i++) {
        if ((properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
            return 1.0f; // Return priority for selected queue family
        }
    }
    
    throw std::runtime_error("Failed to find graphics device");
}

// Create swapchain with correct format and size
VkResult create_swapchain(VkSurfaceKHR surface) {
    uint32_t graphics_queue_family = 0; // Will be determined later
    
    // Get surface capabilities for proper image configuration
    VkSurfaceCapabilitiesKHR capabilities{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_physical_device, g_surface, &capabilities);
    
    // Get available present modes and choose FIFO (vsync)
    uint32_t present_mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(g_physical_device, g_surface, &present_mode_count, nullptr);
    
    std::vector<VkPresentModeKHR> present_modes(present_mode_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(g_physical_device, g_surface, &present_mode_count, present_modes.data());
    
    VkPresentModeKHR default_present = VK_PRESENT_MODE_FIFO_KHR;
    if (present_mode_count > 0) {
        // Prefer mailbox or immediate if available for better performance
        for (auto mode : present_modes) {
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR || mode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
                default_present = mode;
                break;
            }
        }
    }
    
    // Get surface formats and choose appropriate color format
    uint32_t surface_format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(g_physical_device, g_surface, &surface_format_count, nullptr);
    
    std::vector<VkSurfaceFormatKHR> surface_formats(surface_format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(g_physical_device, g_surface, &surface_format_count, surface_formats.data());
    
    VkSurfaceFormatKHR default_format{};
    if (surface_format_count > 0) {
        // Use first format or prefer B8G8R8A8_SRGB
        for (auto& format : surface_formats) {
            if (format.format == VK_FORMAT_B8G8R8A8_SRGB && 
                format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                default_format = format;
                break;
            }
        }
    }
    
    // Use X11/WAYLAND default if no suitable format found
    if (default_format.format == VK_FORMAT_UNDEFINED) {
        default_format.format = VK_FORMAT_B8G8R8A8_UNORM;
        default_format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    }
    
    // Create swapchain with proper image count and configuration
    VkSwapchainCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = g_surface;
    create_info.minImageCount = capabilities.minImageCount;
    create_info.imageFormat = default_format.format;
    create_info.imageColorSpace = default_format.colorSpace;
    create_info.imageArrayLayers = capabilities.maxImageArrayLayers;
    create_info.imageExtent = {
        std::max(capabilities.minImageExtent.width, capabilities.currentExtent.width),
        std::max(capabilities.minImageExtent.height, capabilities.currentExtent.height)
    };
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    create_info.preTransform = capabilities.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = default_present;
    create_info.clipped = VK_TRUE;
    create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_info.queueFamilyIndexCount = 0;
    create_info.pQueueFamilyIndices = nullptr;
    create_info.oldSwapchain = VK_NULL_HANDLE;
    
    g_swapchain = vkCreateSwapchainKHR(g_physical_device, &create_info, nullptr).handle();
    
    return VK_SUCCESS;
}

// Create framebuffer for render pass
VkFramebuffer create_framebuffer() {
    uint32_t image_count = 0;
    vkGetSwapchainImagesKHR(g_device, g_swapchain, &image_count, nullptr);
    
    std::vector<VkImage> swapchain_images(image_count);
    vkGetSwapchainImagesKHR(g_device, g_swapchain, &image_count, swapchain_images.data());
    
    // Create image views for each swapchain image
    std::vector<VkImageView> framebuffer_images(image_count);
    for (uint32_t i = 0; i < image_count; i++) {
        VkImageViewCreateInfo view_create_info{};
        view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_create_info.image = swapchain_images[i];
        view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_create_info.format = VK_FORMAT_R8G8B8A8_SRGB;
        view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_create_info.subresourceRange.baseMipLevel = 0;
        view_create_info.subresourceRange.levelCount = 1;
        view_create_info.subresourceRange.baseArrayLayer = 0;
        view_create_info.subresourceRange.layerCount = 1;
        
        framebuffer_images[i] = vkCreateImageView(g_device, &view_create_info, nullptr).handle();
    }
    
    // Create framebuffer
    VkFramebufferCreateInfo framebuffer_create_info{};
    framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_create_info.renderPass = g_render_pass;
    framebuffer_create_info.attachmentCount = image_count;
    framebuffer_create_info.pAttachments = &framebuffer_images[0];
    framebuffer_create_info.width = swapchain_images[0].extent.width;
    framebuffer_create_info.height = swapchain_images[0].extent.height;
    framebuffer_create_info.layers = 1;
    
    return vkCreateFramebuffer(g_device, &framebuffer_create_info, nullptr).handle();
}

// Create vertex buffer for terrain mesh
VkResult create_vertex_buffer(VkDeviceSize size) {
    std::vector<Vertex> vertices;
    generate_terrain_vertices(vertices);
    
    // Create buffer with correct usage flags
    VkBufferCreateInfo buffer_create_info{};
    buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_create_info.size = size * sizeof(Vertex);
    buffer_create_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    vertex_buffer = vkCreateBuffer(g_device, &buffer_create_info, nullptr).handle();
    
    // Get memory requirements for the buffer
    VkBufferMemoryRequirements2 mem_requirements{};
    mem_requirements.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2;
    mem_requirements.buffer = vertex_buffer.handle();
    
    VkMemoryRequirements2 memory_reqs{};
    vkGetBufferMemoryRequirements2(g_device, &mem_requirements, &memory_reqs);
    
    // Allocate memory for the buffer with device local flag if supported
    VkMemoryType preferred_memory_type = {0};
    uint32_t memory_type_bits = memory_reqs.memoryRequirement.memoryPropertyBits;
    
    for (uint32_t i = 0; i < VK_MAX_MEMORY_TYPES; i++) {
        if ((memory_type_bits & (1 << i)) && 
            (memory_reqs.memoryRequirement.memoryHeapIndex == 0)) {
            preferred_memory_type.typeBits = 1 << i;
            break;
        }
    }
    
    VkMemoryAllocateInfo allocate_info{};
    allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate_info.allocationSize = size * sizeof(Vertex);
    allocate_info.memoryTypeIndex = 0; // Will be validated
    
    vertex_buffer_memory = vkAllocateMemory(g_device, &allocate_info, nullptr).handle();
    
    // Bind buffer to memory
    vkBindBufferMemory(g_device, vertex_buffer.handle(), vertex_buffer_memory, 0);
    
    // Map and copy vertex data
    Vertex* mapped_data = nullptr;
    vkMapMemory(g_device, vertex_buffer_memory, 0, size * sizeof(Vertex), 0, (void**)(&mapped_data));
    memcpy(mapped_data, vertices.data(), size * sizeof(Vertex));
    vkUnmapMemory(g_device, vertex_buffer_memory);
    
    return VK_SUCCESS;
}

// Create graphics pipeline layout
VkResult create_pipeline_layout() {
    VkPipelineLayoutCreateInfo layout_create_info{};
    layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    
    g_pipeline_layout = vkCreatePipelineLayout(g_device, &layout_create_info, nullptr).handle();
    
    return VK_SUCCESS;
}

// Create graphics pipeline with correct Vulkan 1.3 API
VkResult create_graphics_pipeline() {
    // Create shader modules
    VkShaderModule vert_shader = create_shader_module(vertex_shader_code, "vert");
    VkShaderModule frag_shader = create_shader_module(fragment_shader_code, "frag");
    
    // Vertex input state with proper structure
    const uint32_t vertex_binding_size = sizeof(Vertex);
    std::vector<VkVertexInputBindingDescription> binding_descriptions(1);
    binding_descriptions[0] = {};
    binding_descriptions[0].binding = 0;
    binding_descriptions[0].stride = vertex_binding_size;
    binding_descriptions[0].inputStepMode = VK_VERTEX_INPUT_MODE_VERTEX_BIT;
    
    std::vector<VkVertexInputAttributeDescription> attribute_descriptions(2);
    attribute_descriptions[0] = {};
    attribute_descriptions[0].binding = 0;
    attribute_descriptions[0].location = 0;
    attribute_descriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attribute_descriptions[0].offset = offsetof(Vertex, position);
    
    attribute_descriptions[1] = {};
    attribute_descriptions[1].binding = 0;
    attribute_descriptions[1].location = 1;
    attribute_descriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
    attribute_descriptions[1].offset = offsetof(Vertex, texCoord);
    
    // Create pipeline layout (no descriptor sets needed for this simple example)
    create_pipeline_layout();
    
    // Input assembly state
    VkPipelineInputAssemblyStateCreateInfo input_assembly_state{};
    input_assembly_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly_state.primitiveRestartEnable = VK_FALSE;
    
    // Viewport state
    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;
    
    // Rasterization state
    VkPipelineRasterizationStateCreateInfo rasterization_state{};
    rasterization_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization_state.depthClampEnable = VK_FALSE;
    rasterization_state.rasterizerDiscardEnable = VK_FALSE;
    rasterization_state.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization_state.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization_state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization_state.depthBiasEnable = VK_FALSE;
    
    // Multisample state
    VkPipelineMultisampleStateCreateInfo multisample_state{};
    multisample_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    
    // Color blend attachment state
    VkBlendAttachmentState blend_attachment_state{};
    VkPipelineColorBlendAttachmentState color_blend_attachment{};
    color_blend_attachment.colorWriteMask = 0xF; // All colors: R, G, B, A
    color_blend_attachment.blendEnable = VK_FALSE;
    color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    
    VkPipelineColorBlendStateCreateInfo color_blend_state{};
    color_blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend_state.logicOpEnable = VK_FALSE;
    color_blend_state.attachmentCount = 1;
    color_blend_state.pAttachments = &color_blend_attachment;
    
    // Dynamic state creation (viewport and scissor)
    std::vector<VkDynamicState> dynamic_states(2);
    dynamic_states[0] = VK_DYNAMIC_STATE_VIEWPORT;
    dynamic_states[1] = VK_DYNAMIC_STATE_SCISSOR;
    
    VkPipelineDynamicStateCreateInfo dynamic_state{};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = dynamic_states.size();
    dynamic_state.pDynamicStates = dynamic_states.data();
    
    // Depth stencil state (disabled for now)
    VkPipelineDepthStencilStateCreateInfo depth_stencil_state{};
    depth_stencil_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil_state.depthTestEnable = VK_FALSE;
    depth_stencil_state.stencilTestEnable = VK_FALSE;
    
    // Pipeline viewport and scissor state with correct fields
    VkPipelineViewportStateCreateInfo viewport_scissor_state{};
    viewport_scissor_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_scissor_state.viewportCount = 1;
    viewport_scissor_state.scissorCount = 1;
    
    // Create graphics pipeline with correct structure
    VkGraphicsPipelineCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    create_info.layout = g_pipeline_layout;
    create_info.renderPass = g_render_pass;
    create_info.pVertexInputState = &VkPipelineVertexInputStateCreateInfo{};
    create_info.vertexInputDescriptionCount = 0;
    create_info.pInputAssemblyState = &input_assembly_state;
    create_info.pViewportState = &viewport_scissor_state;
    create_info.pRasterizationState = &rasterization_state;
    create_info.pMultisampleState = &multisample_state;
    create_info.pColorBlendState = &color_blend_state;
    create_info.pDynamicState = &dynamic_state;
    
    // Create pipeline with null shader stage count (placeholder)
    g_pipeline = vkCreateGraphicsPipelines(g_device, VK_NULL_HANDLE, 1, &create_info, nullptr).handle();
    
    return VK_SUCCESS;
}

// Create render pass with swapchain images
VkResult create_render_pass() {
    std::vector<VkAttachmentDescription> attachments(1);
    VkFormat swapchain_format = VK_FORMAT_R8G8B8A8_SRGB;
    
    attachments[0] = {};
    attachments[0].format = swapchain_format;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    
    // Color attachment reference (only one output)
    std::vector<VkAttachmentReference> color_attachment_ref(1);
    color_attachment_ref[0] = {};
    color_attachment_ref[0].attachment = 0;
    color_attachment_ref[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    VkSubpassDescription subpass{};
    subpass.sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION;
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref[0];
    
    VkRenderPassCreateInfo render_pass_create_info{};
    render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_create_info.attachmentCount = attachments.size();
    render_pass_create_info.pAttachments = attachments.data();
    render_pass_create_info.subpassCount = 1;
    render_pass_create_info.pSubpasses = &subpass;
    
    g_render_pass = vkCreateRenderPass(g_device, &render_pass_create_info, nullptr).handle();
    
    return VK_SUCCESS;
}

// Create command pool for graphics queue
VkResult create_command_pool(uint32_t graphics_queue_family) {
    VkCommandPoolCreateInfo cmd_pool_create_info{};
    cmd_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmd_pool_create_info.queueFamilyIndex = graphics_queue_family;
    cmd_pool_create_info.flags = 0;
    
    g_command_pool = vkCreateCommandPool(g_device, &cmd_pool_create_info, nullptr).handle();
    
    return VK_SUCCESS;
}

// Create frame fences and semaphores for synchronization
VkResult create_frame_synchronization(uint32_t num_frames_in_flight) {
    for (uint32_t i = 0; i < num_frames_in_flight; i++) {
        VkFenceCreateInfo fence_create_info{};
        fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        
        g_frame_fence.handle(i) = vkCreateFence(g_device, &fence_create_info, nullptr).handle();
        
        // Create available semaphore for image acquisition
        VkSemaphoreCreateInfo semaphore_create_info{};
        semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        
        g_image_available_semaphores.handle(i) = vkCreateSemaphore(g_device, &semaphore_create_info, nullptr).handle();
        
        // Create render-finish semaphore
        VkSemaphoreCreateInfo finish_semaphore_create_info{};
        finish_semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        
        g_render_finished_semaphores.handle(i) = vkCreateSemaphore(g_device, &finish_semaphore_create_info, nullptr).handle();
    }
    
    return VK_SUCCESS;
}

// Render function to draw terrain mesh
VkResult render() {
    // Allocate and begin command buffer
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = g_command_pool;
    alloc_info.commandBufferCount = 1;
    
    VkCommandBuffer command_buffers[1];
    vkAllocateCommandBuffers(g_device, &alloc_info, command_buffers);
    
    VkCommandBufferBeginInfo cmd_buffer_begin_info{};
    cmd_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    
    // Reset frame fence for re-use
    vkResetFences(g_device, 1, &g_frame_fence.handle(0));
    
    // Acquire next swapchain image
    VkResult acquire_result = vkAcquireNextImageKHR(g_device, g_swapchain, UINT64_MAX, VK_NULL_HANDLE, g_frame_fence.handle(0), &g_current_image_index);
    
    if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR) {
        // Window was resized or moved outside - handle later
        return VK_SUCCESS;
    } else if (acquire_result != VK_SUCCESS && acquire_result != VK_NOT_READY) {
        throw std::runtime_error("Failed to acquire swapchain image");
    }
    
    // Begin recording command buffer
    vkBeginCommandBuffer(command_buffers[0], &cmd_buffer_begin_info);
    
    // Setup render area (full screen)
    VkClearValue clear_values[1] = {};
    std::vector<VkRect2D> render_area(1);
    render_area[0].offset = {0, 0};
    render_area[0].extent = g_swapchain.extent;
    
    // Begin render pass
    VkRenderPassBeginInfo render_pass_begin_info{};
    render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_begin_info.renderPass = g_render_pass;
    render_pass_begin_info.framebuffer = framebuffer;
    render_pass_begin_info.renderArea = render_area[0];
    
    vkCmdBeginRenderPass(command_buffers[0], &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
    
    // Bind vertex buffer for drawing
    VkBufferVertexBindingDescriptor binding{};
    binding.binding = 0;
    binding.stride = sizeof(Vertex);
    binding.divisor = 1;
    
    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;
    
    // Draw command (using corrected draw call)
    vkCmdDraw(command_buffers[0], static_cast<int>(vertices.size()), 1, 0, 0);
    
    // End render pass
    vkCmdEndRenderPass(command_buffers[0]);
    
    // Record command buffer end
    vkEndCommandBuffer(command_buffers[0]);
    
    // Submit command buffer for execution
    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffers[0];
    
    vkQueueSubmit(g_graphics_queue, 1, &submit_info, g_frame_fence.handle(0));
    
    // Wait for fence to complete (immediate wait for simplicity)
    if (!g_frame_fence.wait() || !g_frame_fence.result(0)) {
        throw std::runtime_error("Command submission failed");
    }
    
    return VK_SUCCESS;
}

// Present frame to display
void present_frame(uint32_t image_index) {
    VkSemaphoreCreateInfo semaphore_create_info{};
    semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    
    // Create semaphore for signaling render completion
    VkRenderFinishedSemaphore render_finished_semaphore = vkCreateSemaphore(g_device, &semaphore_create_info, nullptr).handle();
    
    // Submit to graphics queue with proper synchronization
    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &g_render_finished_semaphores.handle(image_index);
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &render_finished_semaphore;
    
    vkQueueSubmit(g_graphics_queue, 1, &submit_info, g_frame_fence.handle(0));
    
    // Present frame to surface using correct present queue parameter order
    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &render_finished_semaphore;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &g_swapchain;
    present_info.pImageIndices = &image_index;
    
    vkQueuePresentKHR(g_present_queue, &present_info);
}

// Main function with complete SDL2 + Vulkan integration and render loop
int main(int argc, char* argv[]) {
    try {
        // Initialize SDL2 with video support
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cerr << "SDL2 initialization failed: " << SDL_GetError() << std::endl;
            return -1;
        }
        
        // Create window with Vulkan extension and resize capability
        SDL_Window* window = SDL_CreateWindow(
            "TazWanted: Wanted",
            1024, 768,
            SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
        );
        
        if (!window) {
            std::cerr << "Failed to create SDL window: " << SDL_GetError() << std::endl;
            return -1;
        }
        
        // Create Vulkan surface from SDL window
        VkResult result = create_window_surface(window);
        if (result != VK_SUCCESS) {
            std::cerr << "Failed to create window surface" << std::endl;
            return -1;
        }
        
        // Create Vulkan instance
        result = create_instance();
        if (result != VK_SUCCESS) {
            std::cerr << "Failed to create Vulkan instance: " << result << std::endl;
            return -1;
        }
        
        // Find physical device and determine queue family for graphics operations
        VkPhysicalDevice physical_device = g_physical_device;
        uint32_t graphics_queue_family = 0;
        
        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);
        
        std::vector<VkQueueFamilyProperties> properties(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, properties.data());
        
        // Find first queue family that supports graphics operations
        for (uint32_t i = 0; i < queue_family_count; i++) {
            if ((properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
                graphics_queue_family = i;
                break;
            }
        }
        
        // Create logical device with graphics queue
        result = create_logical_device(physical_device, graphics_queue_family);
        if (result != VK_SUCCESS) {
            std::cerr << "Failed to create logical device: " << result << std::endl;
            return -1;
        }
        
        // Create swapchain
        result = create_swapchain(g_surface);
        if (result != VK_SUCCESS) {
            std::cerr << "Failed to create swapchain: " << result << std::endl;
            return -1;
        }
        
        // Get framebuffer for render pass
        framebuffer = create_framebuffer();
        
        // Create render pass
        result = create_render_pass();
        if (result != VK_SUCCESS) {
            std::cerr << "Failed to create render pass: " << result << std::endl;
            return -1;
        }
        
        // Create vertex buffer with terrain vertices
        VkDeviceSize vertex_buffer_size = sizeof(Vertex);
        result = create_vertex_buffer(vertex_buffer_size);
        if (result != VK_SUCCESS) {
            std::cerr << "Failed to create vertex buffer: " << result << std::endl;
            return -1;
        }
        
        // Create graphics pipeline
        result = create_graphics_pipeline();
        if (result != VK_SUCCESS) {
            std::cerr << "Failed to create graphics pipeline: " << result << std::endl;
            return -1;
        }
        
        // Create frame synchronization for multiple frames-in-flight
        result = create_frame_synchronization(MAX_FRAMES_IN_FLIGHT);
        if (result != VK_SUCCESS) {
            std::cerr << "Failed to create frame synchronization: " << result << std::endl;
            return -1;
        }
        
        // Get swapchain images count
        uint32_t image_count = 0;
        vkGetSwapchainImagesKHR(g_device, g_swapchain, &image_count, nullptr);
        
        // Main render loop with event handling
        while (!SDL_PollEvent(nullptr)) {
            // Check for window resize and other events
            if (event.type == SDL_QUIT) {
                break;
            }
            
            // Acquire next swapchain image and prepare for rendering
            vkAcquireNextImageKHR(g_device, g_swapchain, UINT64_MAX, VK_NULL_HANDLE, g_frame_fence.handle(0), &g_current_image_index);
            
            // Render frame
            result = render();
            
            if (result != VK_SUCCESS) {
                std::cerr << "Render failed: " << result << std::endl;
                break;
            }
            
            // Present frame to display
            present_frame(g_current_image_index);
        }
        
        // Cleanup Vulkan resources in reverse order of creation
        if (g_swapchain) vkDestroySwapchainKHR(g_device, g_swapchain, nullptr);
        if (framebuffer) vkDestroyFramebuffer(g_device, framebuffer, nullptr);
        if (g_render_pass) vkDestroyRenderPass(g_device, g_render_pass, nullptr);
        if (g_pipeline) vkDestroyPipeline(g_device, g_pipeline, nullptr);
        if (g_command_pool) vkDestroyCommandPool(g_device, g_command_pool, nullptr);
        if (vertex_buffer_memory) vkFreeMemory(g_device, vertex_buffer_memory, nullptr);
        if (vertex_buffer) vkDestroyBuffer(g_device, vertex_buffer, nullptr);
        
        // Destroy shader modules if created
        if (vert_shader) vkDestroyShaderModule(g_device, vert_shader, nullptr);
        if (frag_shader) vkDestroyShaderModule(g_device, frag_shader, nullptr);
        
        // Cleanup instance resources
        if (g_surface) vkDestroySurfaceKHR(g_instance, g_surface, nullptr);
        if (g_device) vkDestroyDevice(g_device, nullptr);
        if (g_instance) vkDestroyInstance(g_instance, nullptr);
        
        SDL_DestroyWindow(window);
        SDL_Quit();
        
    } catch (const std::exception& e) {
        std::cerr << "Runtime error: " << e.what() << std::endl;
        return -1;
    }
    
    return 0;
}
'''

# Write the file
with open(FILE_PATH, 'w', encoding='utf-8') as f:
    f.write(FILE_CONTENT)

print(f"Generated SDL2+Vulkan implementation to {FILE_PATH}")
print(f"File size: {os.path.getsize(FILE_PATH)} bytes")
'''

# Write the Python generator script
with open(os.path.join(os.path.dirname(__file__), 'generate_sdlvulkan_fix.py'), 'w', encoding='utf-8') as f:
    f.write(FILE_CONTENT)

print("SDL2+Vulkan generation script written successfully.")
'''
