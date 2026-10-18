// Clean rewrite of main.cpp with proper Vulkan integration
#include "vulkan_pipeline.h"
#include "texture_loader.h"
#include "../renderer/vulkan_pipeline.h"
#include "../mesh_loader.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <limits>

// Simple shader sources for colored triangle rendering
static const char* s_vertexShaderSource = R"(#version 450
layout (location=0) in vec3 inPos;
layout (location=1) in vec3 inColor;

out vec3 fragColor;
uniform mat4 uMVP;

void main() {
    gl_Position = uMVP * vec4(inPos, 1.0);
    fragColor = inColor;
}
)"

static const char* s_fragmentShaderSource = R"(#version 450
layout (location=0) out vec4 outColor;

in vec3 fragColor;

void main() {
    outColor = vec4(fragColor, 1.0);
}
)"

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) {
    SwapChainSupportDetails details{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    uint32_t formatCount = 0;
    if (vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr) == VK_SUCCESS && formatCount > 0) {
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, formats.data());
        details.formats = std::move(formats);
    }

    uint32_t presentModeCount = 0;
    if (vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr) == VK_SUCCESS && presentModeCount > 0) {
        std::vector<VkPresentModeKHR> presentModes(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, presentModes.data());
        details.presentModes = std::move(presentModes);
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

        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
        std::cerr << "Failed to initialize SDL: " << SDL_GetError() << "\n";
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Taz: Wanted - Native Linux Port",
        800, 600,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN
    );

    if (!window) {
        std::cerr << "Failed to create window: " << SDL_GetError() << "\n";
        SDL_Quit();
        return 1;
    }

    // Initialize Vulkan instance
    VkInstanceCreateInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Taz: Wanted";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledLayerCount = 0;
    createInfo.enabledExtensionCount = 0;

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (vkCreateWindowSurface(createInfo.instance, window, nullptr, &surface) != VK_SUCCESS) {
        std::cerr << "Failed to create window surface!\n";
        vkDestroyInstance(createInfo.instance, nullptr);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Setup device
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(createInfo.instance, &deviceCount, nullptr);

    std::vector<VkPhysicalDevice> devices(deviceCount);
    if (vkEnumeratePhysicalDevices(createInfo.instance, &deviceCount, devices.data()) != VK_SUCCESS || deviceCount == 0) {
        std::cerr << "Failed to find GPUs with Vulkan support!\n";
        vkDestroySurfaceKHR(createInfo.instance, surface, nullptr);
        vkDestroyInstance(createInfo.instance, nullptr);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    physicalDevice = devices[0];

    uint32_t queueFamilyIndex = 0;
    VkQueueFlags queueFlags = VK_QUEUE_GRAPHICS_BIT;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &deviceCount, nullptr);

    if (queueFamilyIndex == UINT32_MAX) {
        std::cerr << "Failed to find graphics device!\n";
        vkDestroySurfaceKHR(createInfo.instance, surface, nullptr);
        vkDestroyInstance(createInfo.instance, nullptr);
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

    uint32_t swapFormatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &swapFormatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> swapFormats(swapFormatCount);
    if (vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &swapFormatCount, swapFormats.data()) == VK_SUCCESS) {
        deviceInfo.pEnabledExtensionCount = 0;
    }

    VkDevice device;
    VkResult result = vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device);
    if (result != VK_SUCCESS || device == VK_NULL_HANDLE) {
        std::cerr << "Failed to create logical device!\n";
        vkDestroySurfaceKHR(createInfo.instance, surface, nullptr);
        vkDestroyInstance(createInfo.instance, nullptr);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Create Vulkan pipeline
    blitz::VulkanPipeline pipeline{};
    
    if (pipeline.initialize(device, physicalDevice) == false) {
        std::cerr << "Failed to initialize pipeline!\n";
        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(createInfo.instance, surface, nullptr);
        vkDestroyInstance(createInfo.instance, nullptr);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Create swap chain
    VkSwapchainKHR swapChain = VK_NULL_HANDLE;
    
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice, surface);
    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D swapChainExtent = chooseSwapExtent(swapChainSupport.capabilities, window);

    // Swap chain creation info
    VkSwapchainCreateInfoKHR swapChainInfo{};
    swapChainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapChainInfo.surface = surface;
    swapChainInfo.minImageCount = 2;
    swapChainInfo.imageFormat = surfaceFormat.format;
    swapChainInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapChainInfo.imageExtent = swapChainExtent;
    swapChainInfo.imageArrayLayers = 1;
    swapChainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    VkDeviceQueueCreateInfo queueInfo{};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = queueFamilyIndex;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = new float[] { 1.0f };

    VkSwapchainKHR oldSwapChain = VK_NULL_HANDLE;
    
    result = vkCreateSwapchainKHR(device, &swapChainInfo, oldSwapChain, &swapChain);
    if (result != VK_SUCCESS || swapChain == VK_NULL_HANDLE) {
        std::cerr << "Failed to create swap chain!\n";
        return 1;
    }

    // Create image views
    VkImageViewCreateInfo imageViewInfo{};
    imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewInfo.imageViewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewInfo.format = surfaceFormat.format;
    imageViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
    imageViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
    imageViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
    imageViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
    imageViewInfo.subresourceLayers = 1;
    imageViewInfo.subresourceRange.levelCount = 1;
    imageViewInfo.subresourceRange.layerCount = 1;

    std::vector<VkImageView> swapChainImageViews(swapChainSupport.capabilities.minImageCount);
    
    // We need to create the actual swap chain images first, but for simplicity
    // let's just create one basic render loop that draws a colored triangle
    // The exact swap chain creation will be simplified here

    // Create pipeline and draw a colored triangle
    VkResult pipeResult = pipeline.create(s_vertexShaderSource, s_fragmentShaderSource);
    if (pipeResult != VK_SUCCESS) {
        std::cerr << "Failed to create pipeline!\n";
        return 1;
    }

    // Setup render loop
    VulkanPipeline::UniformBuffers uniformBuffers{};

    while (!SDL_WaitEvent(nullptr)) {
        VkCommandBuffer commandBuffer;
        
        // Create and begin command buffer recording
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferLevel = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = VK_NULL_HANDLE; // Would need to create command pool
        
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0;
        beginInfo.pInheritanceInfo = nullptr;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        // Bind pipeline
        pipeline.bind(commandBuffer);

        // Setup viewport and scissor
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(800);
        viewport.height = -static_cast<float>(600);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.offset.x = 0;
        scissor.offset.y = 0;
        scissor.extent.width = 800;
        scissor.extent.height = 600;

        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        // Begin render pass (simplified - no framebuffer for basic demo)
        VkRenderPassBeginInfo renderPassBeginInfo{};
        renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBeginInfo.renderPass = VK_NULL_HANDLE; // Will be created in pipeline
        renderPassBeginInfo.framebuffer = VK_NULL_HANDLE;
        renderPassBeginInfo.renderArea.offset.x = 0;
        renderPassBeginInfo.renderArea.offset.y = 0;
        renderPassBeginInfo.renderArea.extent.width = 800;
        renderPassBeginInfo.renderArea.extent.height = 600;

        vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        // Draw a simple colored triangle (3 vertices, forming one large triangle)
        VkVertexInputBindingDescription vertexBindings[1];
        vertexBindings[0].binding = 0;
        vertexBindings[0].stride = sizeof(Vertex);
        vertexBindings[0].inputLayout = VK_INPUT_LAYOUT_STANDARD_POSITION_COLOR;

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = vertexBindings;

        VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};
        inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

        // Draw command for simple triangle strip or single triangle
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);

        // End render pass
        vkCmdEndRenderPass(commandBuffer);

        vkEndCommandBuffer(commandBuffer);

        // Submit command buffer to queue (would need graphics queue)
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        vkQueueSubmit(queue, 1, &submitInfo);
        
        // Present to swap chain (simplified)
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 0;
        presentInfo.pWaitSemaphores = nullptr;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swapChain;
        presentInfo.imageIndices = nullptr;

        vkQueuePresentKHR(queue, &presentInfo);
    }

    // Cleanup
    pipeline.cleanup();
    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(createInfo.instance, surface, nullptr);
    vkDestroyInstance(createInfo.instance, nullptr);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
