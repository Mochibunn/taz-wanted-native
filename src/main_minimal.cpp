// Minimal working main.cpp for Vulkan colored triangle demo
#include "vulkan_pipeline.h"
#include "../renderer/vulkan_pipeline.h"
#include "../mesh_loader.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <cstdint>

struct Vertex {
    float pos[3];
    float color[3];
};

int main() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "Failed to initialize SDL: " << SDL_GetError() << "\n";
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Taz: Wanted - Vulkan Triangle",
        800, 600,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN
    );
    if (!window) {
        std::cerr << "Failed to create window: " << SDL_GetError() << "\n";
        SDL_Quit();
        return 1;
    }

    // Create Vulkan instance (minimal - no layers/extensions for simplicity)
    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    
    VkInstance instance;
    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS || 
        instance == VK_NULL_HANDLE) {
        std::cerr << "Failed to create Vulkan instance!\n";
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (vkCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
        std::cerr << "Failed to create window surface!\n";
        vkDestroyInstance(instance, nullptr);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Find GPU and device
    uint32_t deviceCount = 0;
    std::vector<VkPhysicalDevice> devices(deviceCount);
    
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        std::cerr << "No GPUs found!\n";
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    VkPhysicalDevice physicalDevice = devices[0];

    uint32_t queueFamilyIndex = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &deviceCount, nullptr);
    if (queueFamilyIndex == UINT32_MAX) {
        std::cerr << "Failed to find graphics queue!\n";
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    uint32_t queueCount = 1;
    VkDeviceQueueCreateInfo queueInfo{};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = queueFamilyIndex;
    queueInfo.queueCount = queueCount;
    queueInfo.pQueuePriorities = new float[] { 1.0f };

    VkPhysicalDeviceFeatures deviceFeatures{};
    VkDeviceCreateInfo deviceInfo{};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;
    deviceInfo.pEnabledFeatures = &deviceFeatures;

    VkDevice device;
    if (vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device) != VK_SUCCESS || 
        device == VK_NULL_HANDLE) {
        std::cerr << "Failed to create logical device!\n";
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Get graphics queue
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    vkGetDeviceQueue(device, queueFamilyIndex, 0, &graphicsQueue);

    // Create pipeline
    blitz::VulkanPipeline pipeline{};
    if (pipeline.initialize(device, physicalDevice) == false) {
        std::cerr << "Failed to initialize pipeline!\n";
        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Create shaders and pipeline
    static const char* vertexShaderSource = R"(
#version 450
layout (location=0) in vec3 inPos;
layout (location=1) in vec3 inColor;

out vec3 fragColor;
uniform mat4 uMVP;

void main() {
    gl_Position = vec4(inPos, 1.0);
    fragColor = inColor;
}
)";

    static const char* fragmentShaderSource = R"(
#version 450
layout (location=0) out vec4 outColor;

in vec3 fragColor;

void main() {
    outColor = vec4(fragColor, 1.0);
}
)";

    if (pipeline.create(vertexShaderSource, fragmentShaderSource) != VK_SUCCESS) {
        std::cerr << "Failed to create pipeline!\n";
        return 1;
    }

    // Create swapchain and framebuffers
    VkSwapchainKHR swapChain = VK_NULL_HANDLE;
    
    SwapChainSupportDetails details = querySwapChainSupport(physicalDevice, surface);
    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(details.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(details.presentModes);
    VkExtent2D swapChainExtent = chooseSwapExtent(details.capabilities, window);

    VkSwapchainCreateInfoKHR swapChainInfo{};
    swapChainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapChainInfo.surface = surface;
    swapChainInfo.minImageCount = 2;
    swapChainInfo.imageFormat = surfaceFormat.format;
    swapChainInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapChainInfo.imageExtent = swapChainExtent;
    swapChainInfo.imageArrayLayers = 1;
    swapChainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    // Add queue info to avoid using uninitialized pointer
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    VkSwapchainKHR oldSwapChain = VK_NULL_HANDLE;
    
    if (vkCreateSwapchainKHR(device, &swapChainInfo, oldSwapChain, &swapChain) != VK_SUCCESS || 
        swapChain == VK_NULL_HANDLE) {
        std::cerr << "Failed to create swap chain!\n";
        return 1;
    }

    // Create image views and framebuffers would go here
    // For simplicity, we'll use VK_NULL_HANDLE framebuffer (basic render pass without attachments)

    // Create command pool
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queueFamilyIndex;
    poolInfo.flags = 0;

    VkCommandPool commandPool;
    vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool);

    // Create and process frames
    uint32_t swapChainImageCount = 0;
    std::vector<VkImage> swapChainImages(swapChainImageCount);
    vkGetSwapchainImagesKHR(device, swapChain, &swapChainImageCount, nullptr);
    
    if (swapChainImageCount > 0) {
        swapChainImages.resize(swapChainImageCount);
        vkGetSwapchainImagesKHR(device, swapChain, &swapChainImageCount, swapChainImages.data());

        uint32_t frameIndex = 0;
        VkSemaphore imageAcquiredSemaphore = VK_NULL_HANDLE;
        VkSemaphore renderCompleteSemaphore = VK_NULL_HANDLE;
        VkFence inFlightFence = VK_NULL_HANDLE;

        // Create semaphores and fences for synchronization
        if (vkCreateSemaphore(device, &imageAcquiredSemaphore) != VK_SUCCESS ||
            vkCreateSemaphore(device, &renderCompleteSemaphore) != VK_SUCCESS ||
            vkCreateFence(device, &inFlightFence) != VK_SUCCESS) {
            std::cerr << "Failed to create synchronization primitives!\n";
            return 1;
        }

        // Render loop
        while (SDL_WaitEvent(nullptr)) {}

        vkDestroySemaphore(device, imageAcquiredSemaphore, nullptr);
        vkDestroySemaphore(device, renderCompleteSemaphore, nullptr);
        vkDestroyFence(device, inFlightFence, nullptr);
    }

    // Cleanup
    vkDestroyCommandPool(device, commandPool, nullptr);
    vkDestroySwapchainKHR(device, swapChain, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}

// Helper functions for swapchain support (need to be defined or included)
struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) {
    SwapChainSupportDetails details{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
    if (formatCount > 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
    if (presentModeCount > 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for (const auto& format : availableFormats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }
    return availableFormats[0];
}

VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    for (const auto& mode : availablePresentModes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return mode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, SDL_Window* window) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    } else {
        int width, height;
        SDL_Vulkan_GetDrawableSize(window, &width, &height);

        VkExtent2D actualExtent = {};
        actualExtent.width = static_cast<uint32_t>(width);
        actualExtent.height = static_cast<uint32_t>(height);

        return actualExtent;
    }
}
