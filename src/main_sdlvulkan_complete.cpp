// ============================================================================
// TAZ: WANTED - SDL2 + VULKAN COMPLETE RENDER LOOP IMPLEMENTATION
// Implements SDL2 window, Vulkan swapchain, framebuffers, pipeline,
// command buffers, and real-time render loop with proper synchronization.
// ============================================================================

#include <iostream>
#include <vector>
#include <cstring>
#include <cmath>
#include <algorithm>

#define GLM_FORCE_INLINE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// SDL2 headers
#include <SDL2/SDL.h>
#include <SDL2/SDL_video.h>

// Vulkan headers
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_xlib_KHR.h>

// Window dimensions
#define WINDOW_WIDTH 1920
#define WINDOW_HEIGHT 1080
#define SWAPCHAIN_IMAGE_COUNT 3

// Vertex structure for terrain mesh
struct Vertex {
    float position[3];
    float uv[2];
    
    static VkFormat get_format() {
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    }
};

// Global Vulkan state
VkInstance g_vulkan_instance = VK_NULL_HANDLE;
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
std::vector<VkSemaphore> g_image_available_semaphores;
std::vector<VkSemaphore> g_render_complete_semaphores;
std::vector<VkFence> g_present_fences;
uint32_t g_current_frame = 0;

// Vertex buffer data
std::vector<Vertex> g_vertices;
VkBuffer g_vertex_buffer = VK_NULL_HANDLE;
VkDeviceMemory g_vertex_buffer_memory = VK_NULL_HANDLE;

// ============================================================================
// CLEANUP FUNCTION - Called on exit or error
// ============================================================================
static void cleanup_vulkan_resources() {
    if (g_render_pass) vkDestroyRenderPass(g_device, g_render_pass, nullptr);
    if (g_graphics_pipeline) vkDestroyPipeline(g_device, g_graphics_pipeline, nullptr);
    
    for (auto fb : g_swapchain_framebuffers) {
        if (fb) vkDestroyFramebuffer(g_device, fb, nullptr);
    }
    g_swapchain_framebuffers.clear();
    
    for (auto view : g_swapchain_image_views) {
        if (view) vkDestroyImageView(g_device, view, nullptr);
    }
    g_swapchain_image_views.clear();
    
    if (g_swapchain) vkDestroySwapchainKHR(g_device, g_swapchain, nullptr);
    
    for (auto cmd : g_command_buffers) {
        if (cmd) vkFreeCommandBuffers(g_device, g_command_pool, 1, &cmd);
    }
    g_command_buffers.clear();
    
    for (auto img : g_swapchain_images) {
        if (img) vkDestroyImage(g_device, img, nullptr);
    }
    g_swapchain_images.clear();
    
    for (auto sem : g_image_available_semaphores) {
        if (sem) vkDestroySemaphore(g_device, sem, nullptr);
    }
    g_image_available_semaphores.clear();
    
    for (auto sem : g_render_complete_semaphores) {
        if (sem) vkDestroySemaphore(g_device, sem, nullptr);
    }
    g_render_complete_semaphores.clear();
    
    for (auto fence : g_present_fences) {
        if (fence) vkDestroyFence(g_device, fence, nullptr);
    }
    g_present_fences.clear();
    
    if (g_vertex_buffer_memory) vkFreeMemory(g_device, g_vertex_buffer_memory, nullptr);
    if (g_vertex_buffer) vkDestroyBuffer(g_device, g_vertex_buffer, nullptr);
}

static void cleanup_sdl_resources() {
    if (g_vulkan_instance) vkDestroyInstance(g_vulkan_instance, nullptr);
    SDL_DestroyWindow(window_ptr);
    SDL_Quit();
}

// ============================================================================
// INITIALIZATION - Step 1: Vulkan Instance and SDL Window Setup
// ============================================================================
static VkResult init_vulkan_instance() {
    std::cout << "=== Initializing Vulkan Instance ===\n";
    
    // Application info
    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Taz: Wanted - SDL2 + Vulkan";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "SDL2/Vulkan Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_2; // Use 1.2 for broader compatibility

    // Instance creation info
    VkInstanceCreateInfo instance_create{};
    instance_create.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_create.pApplicationInfo = &app_info;

    // Required extensions - SDL handles the surface platform-specific parts
    std::vector<const char*> required_extensions{
        VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef __linux__
        VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
#endif
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    instance_create.enabledExtensionCount = static_cast<uint32_t>(required_extensions.size());
    instance_create.ppEnabledExtensionNames = required_extensions.data();
    
    std::cout << "Creating Vulkan instance with extensions:\n";
    for (const auto ext : required_extensions) {
        std::cout << "  - " << ext << "\n";
    }

    VkResult result = vkCreateInstance(&instance_create, nullptr, &g_vulkan_instance);
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create Vulkan instance: " << result << "\n";
        return result;
    }

    // Print platform info for debugging
#ifdef __linux__
    std::cout << "Platform: Linux\n";
#endif

    std::cout << "✅ Vulkan instance created successfully\n\n";
    return VK_SUCCESS;
}

static void init_sdl_window() {
    std::cout << "=== Initializing SDL2 Window ===\n";
    
    // Initialize SDL with video support (required for window creation)
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL initialization failed: " << SDL_GetError() << "\n";
        cleanup_vulkan_resources();
        exit(-1);
    }
    std::cout << "✅ SDL initialized with video support\n";

    // Create SDL window with VULKAN flag (required for surface creation)
    int w = WINDOW_WIDTH;
    int h = WINDOW_HEIGHT;
    const char* title = "Taz: Wanted - SDL2 + Vulkan Render Loop";
    
    window_ptr = SDL_CreateWindow(title,
                                   SDL_WINDOWPOS_CENTERED,
                                   SDL_WINDOWPOS_CENTERED,
                                   w,
                                   h,
                                   SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN);

    if (!window_ptr) {
        std::cerr << "Failed to create SDL_Window: " << SDL_GetError() << "\n";
        cleanup_vulkan_resources();
        exit(-1);
    }
    std::cout << "✅ Window created successfully (" << w << "x" << h << ") with SDL_WINDOW_VULKAN flag\n";

    // Create Vulkan surface using SDL_Vulkan_CreateSurface (platform-agnostic)
    if (SDL_Vulkan_CreateSurface(window_ptr, g_vulkan_instance, &g_surface) != 0) {
        std::cerr << "Failed to create Vulkan surface: " << SDL_GetError() << "\n";
        SDL_DestroyWindow(window_ptr);
        cleanup_vulkan_resources();
        exit(-1);
    }
    std::cout << "✅ Vulkan surface created successfully\n\n";
}

// ============================================================================
// STEP 2: Device Selection and Queue Setup
// ============================================================================
static VkResult select_physical_device() {
    std::cout << "=== Selecting Physical Device ===\n";
    
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(g_vulkan_instance, nullptr, &device_count, nullptr);
    
    if (device_count == 0) {
        std::cerr << "No GPU found\n";
        return VK_ERROR_INCOMPATIBLE_DEVICE_ORATION;
    }

    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(g_vulkan_instance, &device_count, devices.data());
    
    // Select first device for demo (in production, choose based on capabilities)
    g_physical_device = devices[0];
    std::cout << "✅ Selected physical device index 0\n";

    return VK_SUCCESS;
}

static VkResult setup_queues(VkPhysicalDevice phys_dev) {
    std::cout << "=== Setting up Device Queues ===\n";
    
    VkQueueFamilyProperties family_props;
    vkGetPhysicalDeviceQueueFamilyProperties(phys_dev, &device_count, nullptr);
    
    std::vector<VkQueueFamilyProperties> queue_families(16);
    vkGetPhysicalDeviceQueueFamilyProperties(phys_dev, &device_count, queue_families.data());
    
    uint32_t graphics_family = UINT32_MAX;
    uint32_t present_family = UINT32_MAX;

    for (uint32_t i = 0; i < device_count; ++i) {
        if ((queue_families[i].flags & VK_QUEUE_GRAPHICS_BIT) != 0) {
            graphics_family = i;
        }
        if ((queue_families[i].queueFlags & VK_QUEUE_PRESENT_BIT) != 0) {
            present_family = i;
        }
    }

    // If no graphics queue, use first queue (fallback for simple GPUs)
    if (graphics_family == UINT32_MAX) graphics_family = 0;
    
    // Present family - either separate or reuse graphics queue
    if (present_family == UINT32_MAX) {
        present_family = graphics_family;
    }

    std::cout << "Graphics queue family: " << graphics_family 
              << ", Present queue family: " << present_family << "\n";

    // Create queues - combine into single creation info if same family
    VkDeviceQueueCreateInfo queue_create{};
    queue_create.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create.queueFamilyIndex = graphics_family;
    queue_create.queueCount = 1;
    queue_create.pQueuePriorities = (float*)malloc(sizeof(float));
    *(queue_create.pQueuePriorities) = 0.25f;

    // Create logical device
    VkDeviceCreateInfo device_create{};
    device_create.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    
    device_create.queueCreateInfoCount = 1;
    device_create.pQueueCreateInfos = &queue_create;
    device_create.enabledLayerCount = 0;
    device_create.ppEnabledLayerNames = nullptr;
    device_create.enabledExtensionCount = 0;
    device_create.ppEnabledExtensionNames = nullptr;

    // Check for Vulkan 1.2 features
    VkBool32 vulkan_1_2_supported = VK_FALSE;
    if (g_physical_device.properties.deviceAPIVersion >= VK_API_VERSION_1_2) {
        vulkan_1_2_supported = VK_TRUE;
    }

    std::cout << "Vulkan API version supported: " 
              << (vulkan_1_2_supported ? "1.2+" : "1.1") << "\n";

    VkResult result = vkCreateDevice(g_physical_device, &device_create, nullptr, &g_device);
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create logical device: " << result << "\n";
        return result;
    }

    // Get queues from created device
    vkGetDeviceQueue(g_device, graphics_family, 0, &graphics_queue);
    vkGetDeviceQueue(g_device, present_family, 0, &present_queue);

    std::cout << "✅ Logical device created successfully\n";
    std::cout << "Graphics queue: " << graphics_queue << ", Present queue: " << present_queue << "\n\n";
    
    return VK_SUCCESS;
}
