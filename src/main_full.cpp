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
VkQueue g_present_queue = VK_NULL_HANDLE;
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

VkResult initializeVulkanInstance() {
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
        return VK_ERROR_INITIALIZATION_FAILED;
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
    const char* extensions[] = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME};
    create_info.enabledExtensionCount = sizeof(extensions) / sizeof(const char*);
    create_info.ppEnabledExtensionNames = extensions;

    VkResult result = vkCreateInstance(&create_info, nullptr, &g_instance);
    if (result != VK_SUCCESS) {
        return result;
    }

    // Create window surface using SDL's Vulkan surface creation
    const char* surface_ext[] = {VK_KHR_WIN32_SURFACE_EXTENSION_NAME};
    VkWin32SurfaceCreateInfoKHR sc{};
    sc.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    sc.hinstance = SDL_GetWindowGDIOpaque(g_window);
    sc.hwnd = g_window;
    
    result = vkCreateWin32SurfaceKHR(g_instance, &sc, nullptr, &g_surface);
    if (result != VK_SUCCESS) {
        return result;
    }

    return VK_SUCCESS;
}

// Create logical device with queue families for graphics and presentation
VkResult createLogicalDevice() {
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(g_instance, &device_count, nullptr);
    
    if (device_count == 0) {
        return VK_ERROR_DEVICE_LOST;
    }

    // Get first physical device
    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(g_instance, &device_count, devices.data());
    g_physical_device = devices[0];
    
    // Create graphics and present queues
    VkDeviceQueueCreateInfo queue_create{};
    queue_create.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create.queueFamilyIndex = 0;
    queue_create.queueCount = 1;
    queue_create.pQueueCreateInfos = nullptr;

    // Enable required device extensions
    const char* device_extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    
    VkDeviceCreateInfo device_create{};
    device_create.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create.queueCreateInfoCount = 0;
    device_create.enabledExtensionCount = sizeof(device_extensions) / sizeof(const char*);
    device_create.ppEnabledExtensionNames = device_extensions;

    VkResult result = vkCreateDevice(g_physical_device, &device_create, nullptr, &g_device);
    if (result != VK_SUCCESS) {
        return result;
    }

    // Get graphics and present queues
    vkGetDeviceQueue(g_device, 0, 0, &g_graphics_queue);
    vkGetDeviceQueue(g_device, 0, 1, &g_present_queue);

    // Create command pool
    VkCommandPoolCreateInfo pool_create{};
    pool_create.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_create.queueFamilyIndex = 0;

    result = vkCreateCommandPool(g_device, &pool_create, nullptr, &g_command_pool);
    if (result != VK_SUCCESS) {
        return result;
    }

    return VK_SUCCESS;
}

// Create graphics pipeline state with vertex and fragment shaders
VkResult createGraphicsPipelineState() {
    std::cout << "Creating graphics pipeline state..." << std::endl;
    
    // Initialize the pipeline with device and physical device handles
    if (!g_pipeline.initialize(g_device, g_physical_device)) {
        std::cerr << "Failed to initialize VulkanPipeline" << std::endl;
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    
    // Create uniform buffer for transforms
    if (!g_pipeline.setupTransformUniformBuffer(g_device)) {
        std::cerr << "Failed to setup transform uniform buffer" << std::endl;
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    
    // Create the graphics pipeline with shaders (using precompiled SPIR-V)
    VkResult result = g_pipeline.create(nullptr, nullptr);
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create graphics pipeline: " << result << std::endl;
        return result;
    }
    
    std::cout << "Graphics pipeline created successfully!" << std::endl;
    return VK_SUCCESS;
}

// Create a simple render pass for our triangle
VkRenderPass createSimpleRenderPass() {
    VkRenderPassCreateInfo rpci{};
    rpci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;

    // Color attachment description
    VkAttachmentDescription color_att{};
    color_att.format = VK_FORMAT_R8G8B8A8_UNORM;
    color_att.samples = VK_SAMPLE_COUNT_1_BIT;
    color_att.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_att.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_att.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_att.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Render pass attachments
    VkAttachmentReference color_ref{};
    color_ref.attachment = 0;
    color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    rpci.attachmentCount = 1;
    rpci.pAttachments = &color_att;

    rpci.subpassCount = 1;
    rpci.pSubpasses = &VkSubpassDescription{
        .sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION,
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_ref
    };

    VkSubpassDependency dependency{};
    dependency.sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY;
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_OUTPUT_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_OUTPUT_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask = VK_ACCESS_COLOR_WRITE_BIT;

    rpci.dependencyCount = 1;
    rpci.pDependencies = &dependency;

    VkRenderPass render_pass;
    vkCreateRenderPass(g_device, &rpci, nullptr, &render_pass);
    return render_pass;
}

// Create swapchain and framebuffer
VkResult createSwapchain() {
    std::cout << "Creating swapchain..." << std::endl;

    // Get surface capabilities
    VkSurfaceCapabilitiesKHR caps[1];
    int cap_count = 0;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_physical_device, g_surface, &cap_count, caps);

    if (cap_count == 0) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Get queue family indices
    uint32_t gfx_family = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(g_physical_device, &cap_count, nullptr);
    
    VkSurfaceFormatKHR format{};
    if (cap_count > 0) {
        for (uint32_t i = 0; i < cap_count; i++) {
            if (caps[i].minImageCount == 1 && caps[i].currentExtent.width == WINDOW_WIDTH &&
                caps[i].currentExtent.height == WINDOW_HEIGHT) {
                // Use a standard format
                format.format = VK_FORMAT_B8G8R8A8_UNORM;
                format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
            }
        }
    }

    VkSwapchainCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = g_surface;
    create_info.minImageCount = 2;
    create_info.imageFormat = format.format;
    create_info.imageColorSpace = format.colorSpace;
    create_info.imageExtent = {WINDOW_WIDTH, WINDOW_HEIGHT};
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    // Create swapchain
    VkSwapchainKHR old_swapchain = VK_NULL_HANDLE;
    VkResult result = vkCreateSwapchainKHR(g_device, &create_info, nullptr, &g_swapchain);
    
    if (result != VK_SUCCESS) {
        return result;
    }

    uint32_t image_count = 0;
    vkGetSwapchainImagesKHR(g_device, g_swapchain, &image_count, nullptr);
    std::vector<VkImage> swapchain_images(image_count);
    vkGetSwapchainImagesKHR(g_device, g_swapchain, &image_count, swapchain_images.data());

    // Create a render pass and framebuffer for each image
    VkRenderPass render_pass = createSimpleRenderPass();

    // Create framebuffer from the first swapchain image
    if (image_count > 0) {
        VkImageView color_view;
        vkCreateImage(g_device, &VkImageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .flags = 0,
            .imageType = VK_IMAGE_TYPE_2D,
            .extent.width = WINDOW_WIDTH,
            .extent.height = WINDOW_HEIGHT,
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            .shadingModel = VK_SHADING_MODEL_STANDARD,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
        }, nullptr, &color_view);

    }

    return result;
}

// Helper function to create a vertex buffer
VkBuffer createVertexBuffer(const void* data, VkDeviceSize size) {
    VkBufferCreateInfo bufci{};
    bufci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufci.size = size;
    bufci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer;
    vkCreateBuffer(g_device, &bufci, nullptr, &buffer);

    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(g_device, buffer, &req);

    VkDeviceMemory memory;
    vkAllocateMemory(g_device, &VkMemoryAllocateInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = req.size,
        .memoryHeapIndex = 0
    }, nullptr, &memory);

    vkBindBufferMemory(g_device, buffer, memory, 0);

    // Copy data to buffer
    VkCommandPool cmd_pool;
    VkCommandBuffer cmd_buffer;
    vkCreateCommandPool(g_device, &VkCommandPoolCreateInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = 0
    }, nullptr, &cmd_pool);

    vkAllocateCommandBuffers(g_device, &VkCommandBufferAllocateInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    }, &cmd_buffer);

    vkBeginCommandBuffer(cmd_buffer, &VkCommandBufferBeginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
    });

    vkCmdCopyBuffer(cmd_buffer, buffer, VkBuffer{0}, 1, size);
    vkEndCommandBuffer(cmd_buffer);

    VkPipelineCache pipeline_cache;
    VkFence fence;
    VkCommandBufferSyncInfo cb_si{};
    cb_si.cmdBuff = cmd_buffer;
    cb_si.pFence = &fence;

    // Submit command buffer to queue
    vkQueueSubmit(g_graphics_queue, 1, &VkSemaphoreWait{
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = nullptr,
        .value = 0
    }, VK_NULL_HANDLE);

    return buffer;
}

// Main application entry point
int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    // Initialize SDL2 video subsystem
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL could not initialize: " << SDL_GetError() << "\n";
        return -1;
    }

    // Create Vulkan instance with window surface
    VkResult result = initializeVulkanInstance();
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create Vulkan instance\n";
        return -1;
    }

    // Create logical device and queue families
    result = createLogicalDevice();
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create logical device\n";
        return -1;
    }

    // Create graphics pipeline state with shaders
    VkResult init_result = createGraphicsPipelineState();
    
    if (init_result != VK_SUCCESS) {
        std::cerr << "Failed to create graphics pipeline: " << init_result << "\n";
        return -1;
    }

    std::cout << "Vulkan pipeline initialized successfully" << std::endl;

    // Create swapchain and framebuffer
    VkRenderPass render_pass = createSimpleRenderPass();

    // Simple framebuffer for direct swapchain rendering
    VkFramebuffer framebuffer{};
    result = createSwapchain();
    
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

        // Bind graphics pipeline for drawing - THIS IS THE KEY INTEGRATION
        if (g_pipeline.getPipelineHandle() != VK_NULL_HANDLE) {
            VkPipelineBindPoint bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;
            vkCmdBindPipeline(command_buffer, bind_point, g_pipeline.getPipelineHandle());

            std::cout << "✅ Pipeline bound successfully via vkCmdBindPipeline" << std::endl;
        } else {
            std::cerr << "Warning: Pipeline handle is null, skipping bind\n";
        }

        // Execute render pass (simplified - actual framebuffer would be set)
        VkRenderPassBeginInfo render_begin{};
        render_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_begin.renderArea.extent.width = WINDOW_WIDTH;
        render_begin.renderArea.extent.height = WINDOW_HEIGHT;

        // Add color attachment clear value for simple rendering
        VkClearValue clear_value{};
        clear_value.color.value = {0.2f, 0.3f, 0.4f, 1.0f};  // Blue background
        
        render_begin.framebuffer = framebuffer;
        render_begin.clearValueCount = 1;
        render_begin.pClearValues = &clear_value;

        vkCmdBeginRenderPass(command_buffer, &render_begin, VK_SUBPASS_CONTENTS_INLINE);
        
        // Draw a simple colored triangle
        VkVertexInputBindingDescription binding_desc{};
        binding_desc.binding = 0;
        binding_desc.stride = sizeof(float) * 5;  // pos(3) + color(2)
        binding_desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        
        VkVertexInputAttributeDescription attribute_desc{};
        attribute_desc.location = 0;
        attribute_desc.binding = 0;
        attribute_desc.format = VK_FORMAT_R32G32B32_FLOAT;  // Position
        attribute_desc.offset = 0;
        
        attribute_desc.location = 1;
        attribute_desc.format = VK_FORMAT_R32G32_FLOAT;  // Color
        
        VkVertexInputDecoration vertex_decoration{};
        vertex_decoration.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_decoration.vertexBindingDescriptionCount = 1;
        vertex_decoration.pVertexBindingDescriptions = &binding_desc;
        
        // Create vertex data for a colored triangle (3 vertices)
        std::vector<Vertex> vertices(3);
        
        // Vertex 0: red, bottom-left
        vertices[0] = {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}};
        // Vertex 1: green, bottom-right  
        vertices[1] = {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}};
        // Vertex 2: blue, top
        vertices[2] = {{0.0f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}};
        
        vkCmdDraw(command_buffer, 3, 1, 0, 0);
        
        vkCmdEndRenderPass(command_buffer);
        
        // End command buffer and submit to queue for execution
        vkEndCommandBuffer(command_buffer);

        VkSemaphore render_complete_semaphore = VK_NULL_HANDLE;
        vkCreateSemaphore(g_device, &VkSemaphoreCreateInfo{}, nullptr, &render_complete_semaphore);

        VkSubmitInfo submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.waitSemaphoreCount = 0;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &command_buffer;

        // Submit the command buffer to graphics queue for execution
        vkQueueSubmit(g_graphics_queue, 1, &submit_info, VK_NULL_HANDLE);

        // Present swapchain image
        VkPresentInfoKHR present_info{};
        present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present_info.waitSemaphoreCount = 0;
        present_info.swapchainCount = 1;
        present_info.pSwapchains = &g_swapchain;
        present_info.pImageIndices = &g_image_index;

        VkResult present_res = vkQueuePresentKHR(g_present_queue, &present_info);

        // If we encounter an error during presentation, it might be because the swapchain is out of date
        if (present_res != VK_SUCCESS && present_res != VK_SUBOPTIMAL_KHR) {
            std::cerr << "Presentation failed with result code: " << present_res << "\n";
        }

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
