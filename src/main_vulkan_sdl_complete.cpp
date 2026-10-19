/* TAZ: WANTED - SDL2 + Vulkan Render Loop */
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <array>
#include <vector>
#include <cstdlib>

#define WINDOW_WIDTH 1024
#define WINDOW_HEIGHT 768
#define WINDOW_TITLE "TAZ: WANTED - Vulkan SDL2"

namespace {
VkInstance g_instance;
VkSurfaceKHR g_surface;
VkPhysicalDevice g_physical_device;
VkDevice g_device;
VkQueue g_queue;
VkSwapchainKHR g_swapchain;
std::vector<VkImage> g_swapchain_images;
std::vector<VkImageView> g_swapchain_image_views;

struct FrameData {
    VkCommandBuffer command_buffer;
} g_frame_data[3];

uint32_t g_current_frame = 0;
bool g_running = true;
int g_exit_code = 0;

SDL_Window* g_sdl_window = nullptr;

VkSurfaceFormatKHR g_surface_format{VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
VkPresentModeKHR g_present_mode = VK_PRESENT_MODE_FIFO_KHR;

struct VertexBuffer {
    VkBuffer buffer;
    VkDeviceMemory memory;
} g_vertex_buffer{};

constexpr float GROUND_LEVEL = 0.0f;
const std::vector<float> GROUND_VERTICES = {
    -1.0f, GROUND_LEVEL,  // bottom-left
     1.0f, GROUND_LEVEL,  // bottom-right
    -1.0f, GROUND_LEVEL,  // duplicate
     1.0f, GROUND_LEVEL   // duplicate
};

} /* anonymous namespace */

static VkResult init_vulkan_instance() {
    uint32_t layer_count = 0;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    
    uint32_t ext_count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &ext_count, nullptr);
    
    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "TAZ: WANTED";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "SDL2";
    app_info.engineVersion = VK_MAKE_VERSION(2, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_2;
    
    VkInstanceCreateInfo inst_create_info{};
    inst_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    inst_create_info.pApplicationInfo = &app_info;
    
    if (vkCreateInstance(&inst_create_info, nullptr, &g_instance) != VK_SUCCESS) {
        return VK_ERROR_INCOMPATIBLE_DRIVER;
    }
    
    return VK_SUCCESS;
}

static VkResult create_sdl_window() {
    g_sdl_window = SDL_CreateWindow(WINDOW_TITLE, WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    if (!g_sdl_window) {
        return VK_ERROR_INCOMPATIBLE_DRIVER;
    }
    
    VkSurfaceCreateInfoKHR surf_create{};
    surf_create.sType = VK_STRUCTURE_TYPE_SURFACE_CREATE_INFO_KHR;
    surf_create.surface = SDL_GetWindowID(g_sdl_window);
    surf_create.instance = g_instance;
    
    if (vkCreateSurfaceKHR(g_instance, nullptr, &surf_create, &g_surface) != VK_SUCCESS) {
        return VK_ERROR_SURFACE_LOST_KHR;
    }
    
    return VK_SUCCESS;
}

static VkResult select_device() {
    uint32_t queue_family_count = 0;
    vkEnumeratePhysicalDeviceQueueFamilyProperties(g_physical_device, &queue_family_count, nullptr);
    
    std::vector<VkQueueFamilyProperties> queues(queue_family_count);
    vkEnumeratePhysicalDeviceQueueFamilyProperties(g_physical_device, &queue_family_count, queues.data());
    
    uint32_t graphics_family = 0;
    for (uint32_t i = 0; i < queue_family_count; i++) {
        if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphics_family = i;
            break;
        }
    }
    
    float priorities[1] = {1.0f};
    VkDeviceQueueCreateInfo queue_create{};
    queue_create.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create.queueFamilyIndex = graphics_family;
    queue_create.queueCount = 1;
    queue_create.pQueuePriorities = priorities;
    
    VkDeviceCreateInfo device_create{};
    device_create.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create.queueCreateInfoCount = 1;
    device_create.pQueueCreateInfos = &queue_create;
    device_create.enabledLayerCount = 0;
    device_create.ppEnabledLayerNames = nullptr;
    device_create.enabledExtensionCount = 0;
    
    if (vkCreateDevice(g_physical_device, &device_create, nullptr, &g_device) != VK_SUCCESS) {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    
    vkGetDeviceQueue(g_device, graphics_family, 0, &g_queue);
    return VK_SUCCESS;
}

static VkResult create_swapchain() {
    uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(g_physical_device, g_surface, &format_count, nullptr);
    
    std::vector<VkSurfaceFormatKHR> formats(format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(g_physical_device, g_surface, &format_count, formats.data());
    
    VkSurfaceFormatKHR format = VK_NULL_HANDLE;
    if (format_count > 0) {
        format = formats[0];
        if (format.format == VK_FORMAT_UNDEFINED) {
            format = (VkSurfaceFormatKHR){VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
        } else {
            g_surface_format = format;
        }
    } else {
        return VK_ERROR_INCOMPATIBLE_DRIVER;
    }
    
    uint32_t present_mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(g_physical_device, g_surface, &present_mode_count, nullptr);
    std::vector<VkPresentModeKHR> present_modes(present_mode_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(g_physical_device, g_surface, &present_mode_count, present_modes.data());
    
    VkSwapchainCreateInfoKHR swap_create{};
    swap_create.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swap_create.surface = g_surface;
    swap_create.minImageCount = 2;
    swap_create.imageFormat = g_surface_format.format;
    swap_create.imageColorSpace = g_surface_format.colorSpace;
    swap_create.imageExtent = {WINDOW_WIDTH, WINDOW_HEIGHT};
    swap_create.imageArrayLayers = 1;
    swap_create.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swap_create.queueFamilyIndexCount = 1;
    swap_create.pQueueFamilyIndices = &graphics_family;
    swap_create.presentMode = g_present_mode;
    swap_create.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swap_create.initialImageAspect = VK_IMAGE_ASPECT_COLOR_BIT;
    swap_create.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swap_create.clipDepth = VK_FALSE;
    swap_create.transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    
    if (vkCreateSwapchainKHR(g_device, &swap_create, nullptr, &g_swapchain) != VK_SUCCESS) {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    
    uint32_t image_count = 0;
    vkGetSwapchainImagesKHR(g_device, g_swapchain, &image_count, nullptr);
    g_swapchain_images.resize(image_count);
    vkGetSwapchainImagesKHR(g_device, g_swapchain, &image_count, g_swapchain_images.data());
    
    for (uint32_t i = 0; i < image_count; i++) {
        VkImageViewCreateInfo view_create{};
        view_create.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_create.image = g_swapchain_images[i];
        view_create.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_create.format = g_surface_format.format;
        view_create.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_create.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_create.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_create.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_create.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_create.subresourceRange.baseMipLevel = 0;
        view_create.subresourceRange.levelCount = 1;
        
        if (vkCreateImageView(g_device, &view_create, nullptr, &g_swapchain_image_views[i]) != VK_SUCCESS) {
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }
    
    return VK_SUCCESS;
}

static VkResult setup_vertex_buffer() {
    VkBufferCreateInfo buf_create{};
    buf_create.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buf_create.size = GROUND_VERTICES.size() * sizeof(float);
    buf_create.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    
    if (vkCreateBuffer(g_device, &buf_create, nullptr, &g_vertex_buffer.buffer) != VK_SUCCESS) {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    
    VkMemoryRequirements mem_req{};
    vkGetBufferMemoryRequirements(g_device, g_vertex_buffer.buffer, &mem_req);
    
    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_req.size;
    alloc_info.memoryTypeIndex = 0;
    
    if (vkAllocateMemory(g_device, &alloc_info, nullptr, &g_vertex_buffer.memory) != VK_SUCCESS) {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    
    vkBindBufferMemory(g_device, g_vertex_buffer.buffer, g_vertex_buffer.memory, 0);
    
    memcpy(g_vertex_buffer.buffer, GROUND_VERTICES.data(), GROUND_VERTICES.size() * sizeof(float));
    
    return VK_SUCCESS;
}

static VkResult create_command_pools_and_buffers() {
    VkCommandPoolCreateInfo pool_create{};
    pool_create.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_create.queueFamilyIndex = 0;
    
    VkCommandPool command_pool;
    if (vkCreateCommandPool(g_device, &pool_create, nullptr, &command_pool) != VK_SUCCESS) {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    
    for (uint32_t i = 0; i < 3; i++) {
        VkCommandBufferAllocateInfo buf_alloc{};
        buf_alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        buf_alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        buf_alloc.commandPool = command_pool;
        buf_alloc.commandBufferCount = 1;
        
        if (vkAllocateCommandBuffers(g_device, &buf_alloc, &g_frame_data[i].command_buffer) != VK_SUCCESS) {
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }
    
    return VK_SUCCESS;
}

static VkResult create_render_pass() {
    VkRenderingInfo render_info{};
    render_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
    render_info.colorAttachmentCount = 1;
    render_info.pColorAttachments = &VkRenderingAttachmentInfo{0};
    
    return VK_SUCCESS;
}

static void submit_command_buffer() {
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_KHR;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &g_frame_data[g_current_frame].command_buffer;
    
    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 0;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &g_swapchain;
    present_info.pImageIndices = &0;
    
    VkSemaphore acquire_semaphore = VK_NULL_HANDLE;
    
    if (vkAcquireNextImageKHR(g_device, g_swapchain, UINT64_MAX, acquire_semaphore, VK_NULL_HANDLE, 0) == VK_ERROR_OUT_OF_DATE_KHR) {
        return;
    }
    
    vkQueueSubmit(g_queue, 1, &submit, VK_NULL_HANDLE);
    vkQueuePresentKHR(g_queue, &present_info);
}

static void render_frame() {
    VkCommandBuffer command_buffer = g_frame_data[g_current_frame].command_buffer;
    
    VkClearValue clear_color = {VkClearColorValue{0.0f, 0.0f, 0.0f, 1.0f}};
    std::vector<VkClearValue> clear_values(1, clear_color);
    
    VkRenderPassBeginInfo render_pass_begin{};
    render_pass_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO_KHR;
    render_pass_begin.renderPass = VK_NULL_HANDLE;
    render_pass_begin.framebuffer = VK_NULL_HANDLE;
    render_pass_begin.renderArea.offsets.x = 0;
    render_pass_begin.renderArea.offsets.y = 0;
    render_pass_begin.renderArea.extent.width = WINDOW_WIDTH;
    render_pass_begin.renderArea.extent.height = WINDOW_HEIGHT;
    render_pass_begin.clearValueCount = 1;
    render_pass_begin.pClearValues = clear_values.data();
    
    vkCmdBeginRenderPass(command_buffer, &render_pass_begin, VK_SUBPASS_CONTENTS_INLINE);
    
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, g_swapchain_graphics_pipeline);
    
    VkViewport viewport{};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = WINDOW_WIDTH;
    viewport.height = WINDOW_HEIGHT;
    
    vkCmdSetScissor(command_buffer, 0, &VkRect2D{0, 0, WINDOW_WIDTH, WINDOW_HEIGHT});
}

static void setup_vertex_buffer_and_bind() {
    VkDeviceSize offsets[1] = {0};
    vkCmdBindVertexBuffers(g_frame_data[g_current_frame].command_buffer, 0, 1, &g_vertex_buffer.buffer, offsets);
}

int main(int /*argc*/, char** /*argv*/) {
    if (init_vulkan_instance() != VK_SUCCESS) return 1;
    
    if (create_sdl_window() != VK_SUCCESS) return 1;
    
    g_physical_device = g_instance;
    select_device();
    
    if (create_swapchain() != VK_SUCCESS) return 1;
    
    if (setup_vertex_buffer() != VK_SUCCESS) return 1;
    
    create_command_pools_and_buffers();
    
    while (g_running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) g_running = false;
        }
        
        submit_command_buffer();
    }
    
    cleanup();
    return g_exit_code;
}

static void cleanup() {
    vkDeviceWaitIdle(g_device);
    vkDestroySurfaceKHR(g_instance, g_surface, nullptr);
    vkDestroyDevice(g_device, nullptr);
    vkDestroyInstance(g_instance, nullptr);
    
    if (g_sdl_window) SDL_DestroyWindow(g_sdl_window);
}
