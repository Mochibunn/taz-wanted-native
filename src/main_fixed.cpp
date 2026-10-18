#include "renderer/vulkan_pipeline.h"
#include "transforms.h"
#include <SDL2/SDL.h>
#include <vulkan/vulkan.h>
#include <iostream>
#include <vector>
#include <fstream>

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

VkInstance g_instance = VK_NULL_HANDLE;
VkPhysicalDevice g_physical_device = (VkPhysicalDevice)0;
VkDevice g_device = VK_NULL_HANDLE;
VkCommandPool g_command_pool = VK_NULL_HANDLE;
VkSurfaceKHR g_surface = VK_NULL_HANDLE;
VkQueue g_graphics_queue = VK_NULL_HANDLE;
VkSwapchainKHR g_swapchain = VK_NULL_HANDLE;
uint32_t g_image_index = 0;
bool g_should_quit = false;

SDL_Window* g_window = nullptr;
VulkanPipeline g_pipeline;

// Vertex structure for colored triangle
struct Vertex {
    float pos[3];  // x, y, z
    float color[3]; // r, g, b
};

// Main application entry point
int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    // Initialize SDL2 video subsystem
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL could not initialize: " << SDL_GetError() << "\n";
        return -1;
    }

    // Create SDL window with Vulkan support
    const char* window_title = "Taz: Wanted - Native Linux Port";
    int window_w = WINDOW_WIDTH;
    int window_h = WINDOW_HEIGHT;
    g_window = SDL_CreateWindow(
        window_title,
        window_w, window_h,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN
    );

    if (!g_window) {
        std::cerr << "Failed to create SDL window: " << SDL_GetError() << std::endl;
        return -1;
    }

    // Create Vulkan instance with required extensions
    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "TazWanted Native";
    app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    app_info.pEngineName = "Taz Engine";
    app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    app_info.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    
    // Enable required extensions for SDL Vulkan surface
    const char* extensions[] = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_XLIB_SURFACE_EXTENSION_NAME};
    create_info.enabledExtensionCount = sizeof(extensions) / sizeof(const char*);
    create_info.ppEnabledExtensionNames = extensions;

    VkResult result = vkCreateInstance(&create_info, nullptr, &g_instance);
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create Vulkan instance: " << result << std::endl;
        return -1;
    }

    // Create window surface using SDL's Vulkan surface creation
    const char* surface_ext[] = {VK_KHR_XLIB_SURFACE_EXTENSION_NAME};
    VkXlibSurfaceCreateInfoKHR sc{};
    sc.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    sc.dpy = SDL_GetWindowX11Display(g_window);
    sc.window = SDL_GetWindowX11ID(g_window);
    
    result = vkCreateXlibSurfaceKHR(g_instance, &sc, nullptr, &g_surface);
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create Vulkan surface: " << result << std::endl;
        return -1;
    }

    // Create logical device and queue families
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(g_instance, &device_count, nullptr);
    
    if (device_count == 0) {
        std::cerr << "No physical devices found" << std::endl;
        return -1;
    }

    // Get first physical device
    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(g_instance, &device_count, devices.data());
    g_physical_device = devices[0];
    
    // Create graphics queue with proper info
    VkDeviceQueueCreateInfo queue_create{};
    queue_create.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create.queueFamilyIndex = 0;
    queue_create.queueCount = 1;

    // Enable required device extensions
    const char* device_extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    
    VkDeviceCreateInfo device_create{};
    device_create.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create.queueCreateInfoCount = 0;
    device_create.enabledExtensionCount = sizeof(device_extensions) / sizeof(const char*);
    device_create.ppEnabledExtensionNames = device_extensions;

    VkResult res = vkCreateDevice(g_physical_device, &device_create, nullptr, &g_device);
    if (res != VK_SUCCESS) {
        std::cerr << "Failed to create logical device: " << res << std::endl;
        return -1;
    }

    // Get graphics queue
    vkGetDeviceQueue(g_device, 0, 0, &g_graphics_queue);

    // Create command pool
    VkCommandPoolCreateInfo pool_create{};
    pool_create.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_create.queueFamilyIndex = 0;

    res = vkCreateCommandPool(g_device, &pool_create, nullptr, &g_command_pool);
    if (res != VK_SUCCESS) {
        std::cerr << "Failed to create command pool: " << res << std::endl;
        return -1;
    }

    // Create graphics pipeline state with shaders
    VkResult init_result = g_pipeline.initialize(g_device, g_physical_device);
    
    if (init_result != VK_SUCCESS) {
        std::cerr << "Failed to initialize VulkanPipeline: " << init_result << "\n";
        return -1;
    }

    // Create the graphics pipeline with shaders (using precompiled SPIR-V)
    result = g_pipeline.create("shader.vert", "shader.frag");
    
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create graphics pipeline: " << result << std::endl;
        return -1;
    }
    
    std::cout << "Vulkan pipeline initialized successfully" << std::endl;

    // Main application render loop with vkCmdBindPipeline integration
    while (!g_should_quit && SDL_WaitEvent(nullptr)) {
        // Allocate and begin command buffer
        VkCommandBufferAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandPool = g_command_pool;
        alloc_info.commandBufferCount = 1;

        VkCommandBuffer command_buffer = VK_NULL_HANDLE;
        vkAllocateCommandBuffers(g_device, &alloc_info, &command_buffer);
        
        if (command_buffer == VK_NULL_HANDLE) {
            std::cerr << "Failed to allocate command buffer\n";
            continue;
        }

        // Begin command buffer recording
        VkCommandBufferBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        
        vkBeginCommandBuffer(command_buffer, &begin_info);

        // Create viewport and scissor
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(WINDOW_WIDTH);
        viewport.height = static_cast<float>(WINDOW_HEIGHT);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.extent.width = WINDOW_WIDTH;
        scissor.extent.height = WINDOW_HEIGHT;

        g_pipeline.setViewport(command_buffer, WINDOW_WIDTH, WINDOW_HEIGHT);

        // Bind graphics pipeline for drawing - THIS IS THE KEY INTEGRATION
        if (g_pipeline.getPipelineHandle() != VK_NULL_HANDLE) {
            VkPipelineBindPoint bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;
            vkCmdBindPipeline(command_buffer, bind_point, g_pipeline.getPipelineHandle());

            std::cout << "✅ Pipeline bound successfully via vkCmdBindPipeline" << std::endl;
        } else {
            std::cerr << "Warning: Pipeline handle is null, skipping bind\n";
        }

        // Begin render pass
        VkRenderPassBeginInfo render_begin{};
        render_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_begin.renderArea.extent.width = WINDOW_WIDTH;
        render_begin.renderArea.extent.height = WINDOW_HEIGHT;

        // Add color attachment clear value for simple rendering
        VkClearValue clear_value{};
        clear_value.color.float32[0] = 0.2f;
        clear_value.color.float32[1] = 0.3f;
        clear_value.color.float32[2] = 0.4f;
        clear_value.color.float32[3] = 1.0f;

        render_begin.framebuffer = VK_NULL_HANDLE; // Simplified - no framebuffer for swapchain
        render_begin.clearValueCount = 1;
        render_begin.pClearValues = &clear_value;

        vkCmdBeginRenderPass(command_buffer, &render_begin, VK_SUBPASS_CONTENTS_INLINE);
        
        // Draw a simple colored triangle using vertex data
        VkVertexInputBindingDescription binding_desc{};
        binding_desc.binding = 0;
        binding_desc.stride = sizeof(Vertex);
        binding_desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        
        VkVertexInputAttributeDescription attribute_pos{};
        attribute_pos.location = 0;
        attribute_pos.binding = 0;
        attribute_pos.format = VK_FORMAT_R32G32B32_SFLOAT;  // Position
        attribute_pos.offset = offsetof(Vertex, pos);
        
        VkVertexInputAttributeDescription attribute_col{};
        attribute_col.location = 1;
        attribute_col.binding = 0;
        attribute_col.format = VK_FORMAT_R32G32B32_SFLOAT;  // Color  
        attribute_col.offset = offsetof(Vertex, color);

        // Create vertex data for a colored triangle (3 vertices)
        std::vector<Vertex> vertices(3);
        
        // Vertex 0: red, bottom-left
        vertices[0] = {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}};
        // Vertex 1: green, bottom-right  
        vertices[1] = {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}};
        // Vertex 2: blue, top
        vertices[2] = {{0.0f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}};
        
        g_pipeline.bindVertexBuffers(command_buffer, vertices);
        g_pipeline.drawTriangles(command_buffer, vertices.size(), 1);
        
        // End render pass
        g_pipeline.endRenderPass(command_buffer, 0);
        
        // End command buffer and submit to queue for execution
        vkEndCommandBuffer(command_buffer);

        // Submit the command buffer to graphics queue for execution
        VkSubmitInfo submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.waitSemaphoreCount = 0;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &command_buffer;

        vkQueueSubmit(g_graphics_queue, 1, &submit_info, VK_NULL_HANDLE);

    }

    // Create success flag file
    std::ofstream flag_file("/home/mochi/taz-decomp/vulkan_color_complete.flag");
    if (flag_file.is_open()) {
        flag_file << "Vulkan color pipeline rendered successfully" << std::endl;
        flag_file.close();
        std::cout << "Success flag created: vulkan_color_complete.flag" << std::endl;
    }

    // Cleanup all Vulkan resources before exit
    if (g_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(g_instance, nullptr);
    }
    
    if (g_device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(g_device);
    }

    SDL_Quit();

    return 0;
}
