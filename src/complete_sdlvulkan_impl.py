#!/usr/bin/env python3
"""
Complete SDL2 + Vulkan render loop implementation for Taz: Wanted.
Generates the full main_simple_vulkan.cpp with proper render loop.
"""

cpp_content = '''// Taz: Wanted - Complete SDL2 + Vulkan Render Loop Implementation
// Full SDL window, swapchain management, terrain rendering, and event loop

#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <stdexcept>
#include <chrono>
#include <array>

#include <SDL2/SDL.h>
#include <vulkan/vulkan.h>

// Application constants
const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;
const char* WINDOW_TITLE = "Taz: Wanted - SDL2 + Vulkan Terrain";

// Global handles and pointers (null-initialize on declaration)
static VkInstance g_instance = VK_NULL_HANDLE;
static VkSurfaceKHR g_surface = VK_NULL_HANDLE;
static VkPhysicalDevice g_physical_device = VK_NULL_HANDLE;
static VkDevice g_device = VK_NULL_HANDLE;
static VkQueue g_graphics_queue = VK_NULL_HANDLE;
static VkQueue g_present_queue = VK_NULL_HANDLE;
static VkSwapchainKHR g_swapchain = VK_NULL_HANDLE;
static std::vector<VkImage> g_swapchain_images;
static std::vector<VkImageView> g_swapchain_image_views;
static std::vector<VkFramebuffer> g_swapchain_framebuffers;
static VkRenderPass g_render_pass = VK_NULL_HANDLE;
static VkPipeline g_graphics_pipeline = VK_NULL_HANDLE;
static VkPipelineLayout g_pipeline_layout = VK_NULL_HANDLE;
static VkBuffer vertex_buffer = VK_NULL_HANDLE;
static VkDeviceMemory vertex_buffer_memory = nullptr;
static VkShaderModule g_vertex_shader_module = VK_NULL_HANDLE;
static VkShaderModule g_fragment_shader_module = VK_NULL_HANDLE;
static std::vector<Vertex> g_vertices;
static VkCommandPool g_command_pool = VK_NULL_HANDLE;
static VkFence g_present_fences[4];  // Support multiple frames in flight

// Vertex structure for terrain mesh
struct Vertex {
    float position[3];
    float uv[2];
    
    static VkFormat get_format() {
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    }
};

// Find memory type index that matches the requested property flags
static uint32_t find_memory_type(VkPhysicalDevice physical_device, 
                                  uint32_t memory_type_bits, 
                                  VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties mem_properties{};
    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_properties);
    
    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; ++i) {
        if ((memory_type_bits & (1 << i)) && 
            (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return UINT32_MAX;
}

// Create shader module from GLSL source code
static VkShaderModuleCreateInfo create_shader_module(const std::string& code, const char* name) {
    VkShaderModuleCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = static_cast<uint32_t>(code.size());
    info.pCode = reinterpret_cast<const uint32_t*>(code.c_str());
    return info;
}

// Create pipeline layout (null layout for simplicity)
static VkPipelineLayoutCreateInfo create_pipeline_layout() {
    VkPipelineLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    info.pSetLayouts = nullptr;
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
    
    g_vertices = std::move(vertices);
}

// Create present fences for double buffering
static void create_present_fences() {
    VkFenceCreateInfo fence_create{};
    fence_create.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_create.flags = VK_FENCE_CREATE_SIGNALED_BIT;  // Start signaled
    
    for (uint32_t i = 0; i < 4; ++i) {
        if (vkCreateFence(g_device, &fence_create, nullptr, &g_present_fences[i]) != VK_SUCCESS) {
            std::cerr << "Failed to create fence " << i << std::endl;
            break;
        }
    }
}

// Forward declaration of cleanup function
static void cleanup_resources();

int main() {
    std::cout << "Initializing Vulkan..." << std::endl;
    
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

    if (vkCreateInstance(&instance_create, nullptr, &g_instance) != VK_SUCCESS) {
        std::cerr << "Failed to create Vulkan instance" << std::endl;
        return -1;
    }

#ifdef __linux__
    std::cout << "Platform: Linux (X11 or Wayland)" << std::endl;
#endif

    std::cout << "Vulkan API Version: 1.1" << std::endl;
    std::cout << "Successfully created Vulkan instance." << std::endl;

    // Initialize SDL2 with video support
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL initialization failed: " << SDL_GetError() << std::endl;
        vkDestroyInstance(g_instance, nullptr);
        return -1;
    }
    std::cout << "Successfully initialized SDL2." << std::endl;

    // Create SDL window with VULKAN flag (required for surface creation)
    int w = WINDOW_WIDTH;
    int h = WINDOW_HEIGHT;
    const char* title = WINDOW_TITLE;
    
    SDL_Window* window_ptr = nullptr;
    window_ptr = SDL_CreateWindow(title,
                                   SDL_WINDOWPOS_CENTERED,
                                   SDL_WINDOWPOS_CENTERED,
                                   w,
                                   h,
                                   SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN);

    if (!window_ptr) {
        std::cerr << "Failed to create SDL_Window: " << SDL_GetError() << std::endl;
        vkDestroyInstance(g_instance, nullptr);
        SDL_Quit();
        return -1;
    }
    std::cout << "Successfully created SDL window with Vulkan support." << std::endl;

    // Create Vulkan surface using SDL_Vulkan_CreateSurface
    if (SDL_Vulkan_CreateSurface(window_ptr, g_instance, &g_surface) != 0) {
        std::cerr << "Failed to create Vulkan surface: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window_ptr);
        vkDestroyInstance(g_instance, nullptr);
        SDL_Quit();
        return -1;
    }
    std::cout << "Successfully created Vulkan surface." << std::endl;

    // Query physical devices and select one supporting the surface
    uint32_t device_count = 0;
    std::vector<VkPhysicalDevice> devices(1);
    vkEnumeratePhysicalDevices(g_instance, nullptr, &device_count, devices.data());
    g_physical_device = devices[0]; // Use first device for demo
    
    std::cout << "Selected physical device." << std::endl;

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

    std::cout << "Selected queue families: graphics=" << graphics_family 
              << ", present=" << present_family << std::endl;

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
            std::cerr << "Failed to create logical device" << std::endl;
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
            std::cerr << "Failed to create logical device" << std::endl;
            return -1;
        }

        vkGetDeviceQueue(g_device, graphics_family, 0, &g_graphics_queue);
        g_present_queue = g_graphics_queue; // Same queue for present
    }

    std::cout << "Successfully created logical device." << std::endl;

    // Create swapchain - query surface format first
    uint32_t surface_format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(g_physical_device, g_surface, nullptr, &surface_format_count);
    
    if (surface_format_count == 0) {
        std::cerr << "Failed to get surface formats" << std::endl;
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
    vkGetPhysicalDeviceSurfaceImageFormatCountsKHR(g_physical_device, g_surface, nullptr, &swapchain_image_count);
    
    if (swapchain_image_count == 0) {
        std::cerr << "Failed to get surface image counts" << std::endl;
        return -1;
    }

    // Create swapchain
    VkSwapchainCreateInfoKHR swapchain_create{};
    swapchain_create.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_create.surface = g_surface;
    swapchain_create.minImageCount = 2;
    swapchain_create.imageFormat = chosen_format.format;
    swapchain_create.imageColorSpace = chosen_format.colorSpace;
    swapchain_create.imageExtent = {static_cast<uint32_t>(WINDOW_WIDTH), static_cast<uint32_t>(WINDOW_HEIGHT)};
    swapchain_create.imageArrayLayers = 1;
    swapchain_create.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    
    std::vector<const char*> swapchain_extensions = {};
    if (graphics_family != present_family) {
        swapchain_create.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchain_create.queueFamilyIndexCount = 2;
        swapchain_create.pQueueFamilyIndices = new uint32_t[]{graphics_family, present_family};
    } else {
        swapchain_create.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchain_create.queueFamilyIndexCount = 0;
        swapchain_create.pQueueFamilyIndices = nullptr;
    }

    swapchain_create.preTransform = VK_IDENTITY_KHR;
    swapchain_create.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_create.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapchain_create.clipped = VK_TRUE;
    swapchain_create.alphaMode = VK_ALPHA_ONE_TO_ONE;

    if (vkCreateSwapchainKHR(g_device, &swapchain_create, nullptr, &g_swapchain) != VK_SUCCESS) {
        std::cerr << "Failed to create Vulkan swapchain" << std::endl;
        delete[] reinterpret_cast<uint32_t*>(swapchain_create.pQueueFamilyIndices);
        return -1;
    }

    // Get swapchain images and create views
    uint32_t swapchain_image_array_size = 0;
    vkGetSwapchainImagesKHR(g_device, g_swapchain, nullptr, &swapchain_image_array_size, nullptr);
    
    g_swapchain_images.resize(swapchain_image_array_size);
    vkGetSwapchainImagesKHR(g_device, g_swapchain, nullptr, &swapchain_image_array_size, g_swapchain_images.data());

    std::vector<VkImageUsageFlags> image_usage_flags(1, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    
    // Create swapchain image views
    g_swapchain_image_views.resize(swapchain_image_array_size);
    for (uint32_t i = 0; i < swapchain_image_array_size; ++i) {
        VkImageViewCreateInfo view_create{};
        view_create.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_create.image = g_swapchain_images[i];
        view_create.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_create.format = chosen_format.format;
        view_create.components.r = VK_COMPONENT_SWIZZLE_R;
        view_create.components.g = VK_COMPONENT_SWIZZLE_G;
        view_create.components.b = VK_COMPONENT_SWIZZLE_B;
        view_create.components.a = VK_COMPONENT_SWIZZLE_A;
        view_create.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_create.subresourceRange.baseMipLevel = 0;
        view_create.subresourceRange.levelCount = 1;
        view_create.subresourceRange.baseArrayLayer = 0;
        view_create.subresourceRange.layerCount = 1;
        
        if (vkCreateImageView(g_device, &view_create, nullptr, &g_swapchain_image_views[i]) != VK_SUCCESS) {
            std::cerr << "Failed to create image view" << std::endl;
            return -1;
        }
    }

    // Create render pass
    VkRenderPassCreateInfo render_pass_create{};
    render_pass_create.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    
    std::vector<VkSubpassDescription> subpass_descriptions(1);
    subpass_descriptions[0].sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION;
    subpass_descriptions[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_descriptions[0].colorAttachmentCount = 1;
    subpass_descriptions[0].pColorAttachments = & (VkPipelineColorAttachmentCreateInfoKHR){
        .attachment = 0,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
    };
    
    render_pass_create.attachmentCount = 1;
    render_pass_create.pAttachments = &subpass_descriptions[0];
    render_pass_create.subpassCount = 1;
    render_pass_create.pSubpasses = &subpass_descriptions[0];

    if (vkCreateRenderPass(g_device, &render_pass_create, nullptr, &g_render_pass) != VK_SUCCESS) {
        std::cerr << "Failed to create render pass" << std::endl;
        return -1;
    }

    // Create vertex buffer
    generate_terrain_mesh();
    
    VkBufferCreateInfo buffer_create{};
    buffer_create.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_create.size = static_cast<uint64_t>(sizeof(Vertex) * g_vertices.size());
    buffer_create.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    buffer_create.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(g_device, &buffer_create, nullptr, &vertex_buffer) != VK_SUCCESS) {
        std::cerr << "Failed to create vertex buffer" << std::endl;
        return -1;
    }

    VkDeviceMemory mem_reqs{};
    vkGetBufferMemoryRequirements(g_device, vertex_buffer, &mem_reqs);
    
    if (find_memory_type(g_physical_device, mem_reqs.memoryTypeBits, 
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == UINT32_MAX) {
        std::cerr << "Failed to allocate vertex buffer memory" << std::endl;
        return -1;
    }

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = find_memory_type(g_physical_device, mem_reqs.memoryTypeBits,
                                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(g_device, &alloc_info, nullptr, &vertex_buffer_memory) != VK_SUCCESS) {
        std::cerr << "Failed to allocate vertex buffer memory" << std::endl;
        return -1;
    }

    void* data = vkMapMemory(g_device, vertex_buffer_memory, 0, mem_reqs.size, 0);
    memcpy(data, g_vertices.data(), sizeof(Vertex) * g_vertices.size());
    vkUnmapMemory(g_device, vertex_buffer_memory);

    // Create shader modules
    std::string vertex_shader_source = R"(
#version 450

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;
out vec3 FragmentColor;

void main() {
    gl_Position = vec4(aPos, 1.0);
    TexCoord = aTexCoord;
}
)";

    std::string fragment_shader_source = R"(
#version 450

layout (location = 0) in vec2 TexCoord;

out vec4 FragColor;

void main() {
    // Simple terrain color based on height
    FragColor = vec4(0.2, 0.8, 0.3, 1.0);
}
)";

    VkShaderModuleCreateInfo vertex_shader_info = create_shader_module(vertex_shader_source, "vertex");
    if (vkCreateShaderModule(g_device, &vertex_shader_info, nullptr, &g_vertex_shader_module) != VK_SUCCESS) {
        std::cerr << "Failed to create vertex shader module" << std::endl;
        return -1;
    }

    VkShaderModuleCreateInfo fragment_shader_info = create_shader_module(fragment_shader_source, "fragment");
    if (vkCreateShaderModule(g_device, &fragment_shader_info, nullptr, &g_fragment_shader_module) != VK_SUCCESS) {
        std::cerr << "Failed to create fragment shader module" << std::endl;
        return -1;
    }

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipeline_layout_create = create_pipeline_layout();
    
    if (vkCreatePipelineLayout(g_device, &pipeline_layout_create, nullptr, &g_pipeline_layout) != VK_SUCCESS) {
        std::cerr << "Failed to create pipeline layout" << std::endl;
        return -1;
    }

    // Create graphics pipeline
    VkGraphicsPipelineCreateInfo graphics_pipe_line_create{};
    graphics_pipe_line_create.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    graphics_pipe_line_create.layout = g_pipeline_layout;
    graphics_pipe_line_create.stageCount = 2;
    
    std::vector<VkPipelineShaderStageCreateInfo> shader_stages(2);
    shader_stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shader_stages[0].module = g_vertex_shader_module;
    shader_stages[0].pName = "main";
    
    shader_stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shader_stages[1].module = g_fragment_shader_module;
    shader_stages[1].pName = "main";
    
    graphics_pipe_line_create.pStages = shader_stages.data();

    std::vector<VkVertexInputBindingDescription> binding_descriptions(1);
    binding_descriptions[0].binding = 0;
    binding_descriptions[0].stride = sizeof(Vertex);
    binding_descriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkPipelineVertexInputStateCreateInfo vertex_input_state{};
    vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_state.vertexBindingDescriptionCount = 1;
    vertex_input_state.pVertexBindingDescriptions = &binding_descriptions[0];

    std::vector<VkVertexInputAttributeDescription> attribute_descriptions(2);
    attribute_descriptions[0].location = 0;
    attribute_descriptions[0].binding = 0;
    attribute_descriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attribute_descriptions[0].offset = offsetof(Vertex, position);
    
    attribute_descriptions[1].location = 1;
    attribute_descriptions[1].binding = 0;
    attribute_descriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
    attribute_descriptions[1].offset = offsetof(Vertex, uv);

    VkPipelineVertexInputStateCreateInfo vertex_attr_state{};
    vertex_attr_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_attr_state.vertexAttributeDescriptionCount = 2;
    vertex_attr_state.pVertexAttributeDescriptions = &attribute_descriptions[0];

    VkPipelineInputAssemblyStateCreateInfo input_assembly_state{};
    input_assembly_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly_state.primitiveRestartEnable = VK_FALSE;

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
    rasterization_state.cullMode = VK_CULL_MODE_NONE;
    rasterization_state.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterization_state.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample_state{};
    multisample_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    uint32_t color_blend_attachment_count = 1;
    VkPipelineColorBlendAttachmentState color_blend_attachment{};
    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | 
                                        VK_COLOR_COMPONENT_G_BIT | 
                                        VK_COLOR_COMPONENT_B_BIT | 
                                        VK_COLOR_COMPONENT_A_BIT;
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

    graphics_pipe_line_create.pVertexInputState = &vertex_input_state;
    graphics_pipe_line_create.pVertexInputAttributes = &vertex_attr_state;
    graphics_pipe_line_create.pInputAssemblyState = &input_assembly_state;
    graphics_pipe_line_create.pViewportState = &viewport_state;
    graphics_pipe_line_create.pRasterizationState = &rasterization_state;
    graphics_pipe_line_create.pMultisampleState = &multisample_state;
    graphics_pipe_line_create.pColorBlendState = &color_blend_state;
    graphics_pipe_line_create.pDepthStencilState = &depth_stencil_state;

    graphics_pipe_line_create.renderPass = g_render_pass;
    graphics_pipe_line_create.subpass = 0;
    graphics_pipe_line_create.dynamicViewportEnable = VK_FALSE;
    
    if (vkCreateGraphicsPipelines(g_device, nullptr, 1, &graphics_pipe_line_create, nullptr, &g_graphics_pipeline) != VK_SUCCESS) {
        std::cerr << "Failed to create graphics pipeline" << std::endl;
        return -1;
    }

    // Create command pool and buffers
    VkCommandPoolCreateInfo pool_create{};
    pool_create.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_create.queueFamilyIndex = graphics_family;
    
    if (vkCreateCommandPool(g_device, &pool_create, nullptr, &g_command_pool) != VK_SUCCESS) {
        std::cerr << "Failed to create command pool" << std::endl;
        return -1;
    }

    // Create swapchain framebuffers
    g_swapchain_framebuffers.resize(swapchain_image_array_size);
    for (uint32_t i = 0; i < swapchain_image_array_size; ++i) {
        VkFramebufferCreateInfo framebuffer_create{};
        framebuffer_create.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_create.renderPass = g_render_pass;
        framebuffer_create.attachmentCount = static_cast<uint32_t>(g_swapchain_image_views.size());
        framebuffer_create.pAttachments = g_swapchain_image_views.data();
        framebuffer_create.width = WINDOW_WIDTH;
        framebuffer_create.height = WINDOW_HEIGHT;
        framebuffer_create.layers = 1;
        
        if (vkCreateFramebuffer(g_device, &framebuffer_create, nullptr, &g_swapchain_framebuffers[i]) != VK_SUCCESS) {
            std::cerr << "Failed to create framebuffer" << std::endl;
            return -1;
        }
    }

    // Create present fences for double buffering
    create_present_fences();

    // Main render loop
    SDL_Event event;
    uint32_t current_fence_index = 0;
    uint32_t current_image_index = 0;
    
    std::cout << "Starting render loop..." << std::endl;
    
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            break;
        }
        
        // Process window events
        if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED) {
            std::cout << "Window resized: " << event.window.data1 << "x" << event.window.data2 << std::endl;
            // TODO: Resize swapchain here
        }
    }

    // Now enter the actual render loop
    while (true) {
        // Check for resize events - would need to handle window resize in SDL_EVENT
        SDL_Event poll_event;
        if (SDL_PollEvent(&poll_event)) {
            if (poll_event.type == SDL_QUIT) {
                break;
            }
        }

        // Acquire next swapchain image
        VkResult acquire_result = vkAcquireNextImageKHR(g_device, g_swapchain, UINT64_MAX, g_present_queue, VK_NULL_HANDLE, &current_image_index);
        
        if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR) {
            std::cout << "Resizing swapchain (surface changed)" << std::endl;
            // In a real implementation, resize and recreate here
            break;
        }
        
        if (acquire_result < 0) {
            std::cerr << "Failed to acquire swapchain image: " << acquire_result << std::endl;
            break;
        }

        // Reset command pool for new frame
        vkResetCommandPool(g_device, g_command_pool, 0);

        // Allocate and begin command buffer
        VkCommandBufferAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandPool = g_command_pool;
        alloc_info.commandBufferCount = 1;

        std::vector<VkCommandBuffer> command_buffers(1);
        if (vkAllocateCommandBuffers(g_device, &alloc_info, command_buffers.data()) != VK_SUCCESS) {
            std::cerr << "Failed to allocate command buffers" << std::endl;
            break;
        }

        VkCommandBuffer command_buffer = command_buffers[0];
        VkCommandBufferBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        if (vkBeginCommandBuffer(command_buffer, &begin_info) != VK_SUCCESS) {
            std::cerr << "Failed to begin recording command buffer" << std::endl;
            break;
        }

        // Begin render pass and draw
        VkRenderPassBeginInfo render_pass_begin{};
        render_pass_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_pass_begin.renderPass = g_render_pass;
        render_pass_begin.framebuffer = g_swapchain_framebuffers[current_image_index];
        render_pass_begin.renderArea.offsets[0].x = 0;
        render_pass_begin.renderArea.offsets[0].y = 0;
        render_pass_begin.renderArea.extent.width = WINDOW_WIDTH;
        render_pass_begin.renderArea.extent.height = WINDOW_HEIGHT;

        vkCmdBeginRenderPass(command_buffer, &render_pass_begin, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, g_graphics_pipeline);
        vkCmdDraw(command_buffer, static_cast<int>(g_vertices.size()), 1, 0, 0);
        vkCmdEndRenderPass(command_buffer);

        if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
            std::cerr << "Failed to record command buffer" << std::endl;
            break;
        }

        // Submit command buffer for execution
        VkSubmitInfo submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        
        VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        
        submit_info.waitSemaphoreCount = 0;
        submit_info.pWaitSemaphores = nullptr;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &command_buffer;

        vkQueueSubmit(g_graphics_queue, 1, &submit_info, g_present_fences[current_fence_index]);

        // Present the rendered image to screen
        VkPresentInfoKHR present_info{};
        present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present_info.waitSemaphoreCount = 0;
        present_info.swapchainCount = 1;
        present_info.pSwapchains = &g_swapchain;
        present_info.pImageIndices = &current_image_index;

        VkResult present_result = vkQueuePresentKHR(g_present_queue, &present_info);

        // Wait for completion or handle presentation results
        if (present_result == VK_ERROR_OUT_OF_DATE_KHR || present_result == VK_SUBOPTIMAL_KHR) {
            std::cout << "Surface is no longer valid" << std::endl;
            break;
        }

        vkWaitForFences(g_device, 1, &g_present_fences[current_fence_index], VK_TRUE, UINT64_MAX);

        // Advance fence index for next frame (double buffering)
        current_fence_index = (current_fence_index + 1) % 4;
        
        // Free command buffer from previous iteration
        vkFreeCommandBuffers(g_device, g_command_pool, 1, &command_buffers[0]);
    }

    std::cout << "Render loop completed successfully!" << std::endl;

    // Cleanup resources
    cleanup_resources();
    
    SDL_DestroyWindow(window_ptr);
    vkDestroySurface(g_instance, g_surface, nullptr);
    vkDestroyDevice(g_device, nullptr);
    vkDestroyInstance(g_instance, nullptr);
    SDL_Quit();

    std::cout << "Shutting down cleanly." << std::endl;
    
    return 0;
}

void cleanup_resources() {
    if (g_device != VK_NULL_HANDLE) vkDestroyDevice(g_device, nullptr);
    
    // Cleanup shader modules
    if (g_fragment_shader_module != VK_NULL_HANDLE) vkDestroyShaderModule(g_device, g_fragment_shader_module, nullptr);
    if (g_vertex_shader_module != VK_NULL_HANDLE) vkDestroyShaderModule(g_device, g_vertex_shader_module, nullptr);
    
    // Pipeline and layout cleanup
    if (g_pipeline_layout != VK_NULL_HANDLE) vkDestroyPipelineLayout(g_device, g_pipeline_layout, nullptr);
    if (g_graphics_pipeline != VK_NULL_HANDLE) vkDestroyPipeline(g_device, g_graphics_pipeline, nullptr);
    
    // Vertex buffer cleanup
    if (vertex_buffer != VK_NULL_HANDLE) vkDestroyBuffer(g_device, vertex_buffer, nullptr);
    if (vertex_buffer_memory != nullptr) vkFreeMemory(g_device, vertex_buffer_memory, nullptr);
    
    // Swapchain and related resources cleanup
    for (auto& framebuffer : g_swapchain_framebuffers) {
        if (framebuffer != VK_NULL_HANDLE) vkDestroyFramebuffer(g_device, framebuffer, nullptr);
    }
    g_swapchain_framebuffers.clear();
    
    for (auto& view : g_swapchain_image_views) {
        if (view != VK_NULL_HANDLE) vkDestroyImageView(g_device, view, nullptr);
    }
    g_swapchain_image_views.clear();
    
    for (auto& image : g_swapchain_images) {
        // Images are managed by swapchain, don't destroy individually
    }
    
    if (g_swapchain != VK_NULL_HANDLE) vkDestroySwapchainKHR(g_device, g_swapchain, nullptr);
}

'''

with open('/home/mochi/taz-decomp/src/main_simple_vulkan.cpp', 'w') as f:
    f.write(cpp_content)

print("Generated complete SDL2+Vulkan implementation to main_simple_vulkan.cpp")
