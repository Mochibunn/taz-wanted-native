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

// Helper: Find memory type suitable for vertex/index buffers
static uint32_t findMemoryType(VkPhysicalDevice device, uint32_t typeIndex, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties{};
    vkGetPhysicalDeviceMemoryProperties(device, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((i == typeIndex) && 
            ((memProperties.memoryTypes[i].propertyFlags & typeFilter) == properties) &&
            (memProperties.memoryTypes[i].heapIndex == memProperties.memoryTypes[0].heapIndex)) {
            return i;
        }
    }
    return 0; // fallback - should use first heap
}

// Vertex structure matching the mesh loader's packed format
struct Vertex {
    float position[3];  // x, y, z
    float texCoord[2];   // u, v
};

// Mesh data container
struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
    bool valid = false;
};

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
        std::cerr << "❌ Failed to initialize SDL: " << SDL_GetError() << "\n";
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Taz: Wanted - Native Linux Port",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        1280, 720,
        SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );

    if (!window) {
        std::cerr << "❌ Failed to create SDL window: " << SDL_GetError() << "\n";
        SDL_Quit();
        return 1;
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Taz Wanted Native";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    uint32_t extensionCount = 0;
    std::vector<const char*> extensions(extensionCount);
    if (SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, nullptr) == VK_SUCCESS && extensionCount > 0) {
        extensions.resize(extensionCount);
        SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, extensions.data());
    }

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkInstance instance;
    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        std::cerr << "❌ Failed to create Vulkan instance!\n";
        SDL_Quit();
        return 1;
    }

    VkSurfaceKHR surface;
    if (!SDL_Vulkan_CreateSurface(window, instance, &surface)) {
        std::cerr << "❌ Failed to create surface!\n";
        vkDestroyInstance(instance, nullptr);
        SDL_Quit();
        return 1;
    }

    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        std::cerr << "❌ No GPUs available!\n";
        vkDestroyInstance(instance, nullptr);
        SDL_Quit();
        return 1;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    uint32_t graphicsFamily = UINT32_MAX;

    for (const auto& device : devices) {
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        uint32_t i = 0;
        for (const auto& queueFamily : queueFamilies) {
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, static_cast<uint32_t>(i), surface, &presentSupport);

            if ((queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) && presentSupport) {
                physicalDevice = device;
                graphicsFamily = i;
                break;
            }
            i++;
        }
        if (physicalDevice != VK_NULL_HANDLE) break;
    }

    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
    std::cout << "🚀 Selected active GPU: " << deviceProperties.deviceName << "\n";

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = graphicsFamily;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

    VkDevice device;
    if (vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device) != VK_SUCCESS) {
        std::cerr << "❌ Failed to create logical device!\n";
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
        SDL_Quit();
        return 1;
    }

    VkQueue graphicsQueue;
    vkGetDeviceQueue(device, graphicsFamily, 0, &graphicsQueue);

    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice, surface);
    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities, window);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR swapChainCreateInfo{};
    swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapChainCreateInfo.surface = surface;
    swapChainCreateInfo.minImageCount = imageCount;
    swapChainCreateInfo.imageFormat = surfaceFormat.format;
    swapChainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapChainCreateInfo.imageExtent = extent;
    swapChainCreateInfo.imageArrayLayers = 1;
    swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapChainCreateInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapChainCreateInfo.presentMode = presentMode;
    swapChainCreateInfo.clipped = VK_TRUE;

    VkSwapchainKHR swapChain;
    if (vkCreateSwapchainKHR(device, &swapChainCreateInfo, nullptr, &swapChain) != VK_SUCCESS) {
        std::cerr << "❌ Failed to create swapchain!\n";
        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
        SDL_Quit();
        return 1;
    }

    uint32_t imageCountResult = 0;
    vkGetSwapchainImagesKHR(device, swapChain, &imageCountResult, nullptr);
    std::vector<VkImage> swapChainImages(imageCountResult);
    vkGetSwapchainImagesKHR(device, swapChain, &imageCountResult, swapChainImages.data());

    std::vector<VkImageView> swapChainImageViews(swapChainImages.size());
    for (size_t i = 0; i < swapChainImages.size(); i++) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = swapChainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = surfaceFormat.format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &viewInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) {
            std::cerr << "❌ Failed to create image view!\n";
            vkDestroyDevice(device);
            vkDestroySurfaceKHR(instance, surface, nullptr);
            vkDestroyInstance(instance);
            SDL_Quit();
            return 1;
        }
    }

    // Render Pass
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = surfaceFormat.format;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;

    VkRenderPass renderPass;
    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        std::cerr << "❌ Failed to create render pass!\n";
        return 1;
    }

    // Framebuffers
    std::vector<VkFramebuffer> swapChainFramebuffers(swapChainImageViews.size());
    for (size_t i = 0; i < swapChainImageViews.size(); i++) {
        VkImageView attachments[] = { swapChainImageViews[i] };
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = extent.width;
        framebufferInfo.height = extent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
            std::cerr << "❌ Failed to create framebuffer!\n";
            return 1;
        }
    }

    // Command Pool & Buffer
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = graphicsFamily;

    VkCommandPool commandPool;
    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        std::cerr << "❌ Failed to create command pool!\n";
        return 1;
    }

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    if (vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
        std::cerr << "❌ Failed to allocate command buffer!\n";
        return 1;
    }

    // Synchronization Semaphores & Fences
    VkSemaphore imageAvailableSemaphore{};
    VkSemaphore renderFinishedSemaphore{};
    VkFence inFlightFence{};

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphore) != VK_SUCCESS ||
        vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphore) != VK_SUCCESS ||
        vkCreateFence(device, &fenceInfo, nullptr, &inFlightFence) != VK_SUCCESS) {
        std::cerr << "❌ Failed to create synchronization objects!\n";
        return 1;
    }

    // Load mesh geometry from .pc file using mesh_loader.cpp
    std::string meshPath = "assets/meshes/terrain.pc";
    
    struct MeshData {
        std::vector<Vertex> vertices;
        std::vector<uint16_t> indices;
        bool valid = false;
    };

    // Load mesh from disk using the mesh loader
    LoadedMesh loadedMesh;
    if (!blitz::loadFromDisk(loadedMesh, meshPath)) {
        std::cerr << "❌ Failed to load mesh from " << meshPath << "!";
        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
        SDL_Quit();
        return 1;
    }
    
    std::cout << "📐 Loaded mesh: " << loadedMesh.vertices.size() 
              << " vertices, " << loadedMesh.indices.size() 
              << " indices" << std::endl;

    // Create vertex buffer from mesh vertices
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
    VkDeviceSize vertexBufferSize = loadedMesh.vertices.size() * sizeof(Vertex);

    if (vertexBufferSize > 0) {
        VkBufferCreateInfo vertexBufInfo{};
        vertexBufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        vertexBufInfo.size = vertexBufferSize;
        vertexBufInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        vertexBufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(device, &vertexBufInfo, nullptr, &vertexBuffer) != VK_SUCCESS) {
            std::cerr << "❌ Failed to create vertex buffer!\n";
            return 1;
        }

        VkMemoryRequirements vertexMemReqs{};
        vkGetBufferMemoryRequirements(device, vertexBuffer, &vertexMemReqs);

        VkMemoryAllocateInfo vertexAllocInfo{};
        vertexAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        vertexAllocInfo.allocationSize = vertexMemReqs.size;
        vertexAllocInfo.memoryTypeIndex = findMemoryType(device, 
            0, vertexMemReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        if (vkAllocateMemory(device, &vertexAllocInfo, nullptr, &vertexBufferMemory) != VK_SUCCESS) {
            std::cerr << "❌ Failed to allocate vertex buffer memory!\n";
            return 1;
        }

        void* vertexData = nullptr;
        if (vkMapMemory(device, vertexBufferMemory, 0, vertexBufferSize, 0, &vertexData) != VK_SUCCESS) {
            std::cerr << "❌ Failed to map vertex buffer!\n";
            return 1;
        }

        memcpy(vertexData, loadedMesh.vertices.data(), vertexBufferSize);
        vkUnmapMemory(device, vertexBufferMemory);
    }

    // Create index buffer from mesh indices
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;
    VkDeviceSize indexBufferSize = loadedMesh.indices.size() * sizeof(uint16_t);

    if (indexBufferSize > 0) {
        VkBufferCreateInfo indexBufInfo{};
        indexBufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        indexBufInfo.size = indexBufferSize;
        indexBufInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        indexBufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(device, &indexBufInfo, nullptr, &indexBuffer) != VK_SUCCESS) {
            std::cerr << "❌ Failed to create index buffer!\n";
            return 1;
        }

        VkMemoryRequirements indexMemReqs{};
        vkGetBufferMemoryRequirements(device, indexBuffer, &indexMemReqs);

        VkMemoryAllocateInfo indexAllocInfo{};
        indexAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        indexAllocInfo.allocationSize = indexMemReqs.size;
        indexAllocInfo.memoryTypeIndex = findMemoryType(device, 
            0, indexMemReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        if (vkAllocateMemory(device, &indexAllocInfo, nullptr, &indexBufferMemory) != VK_SUCCESS) {
            std::cerr << "❌ Failed to allocate index buffer memory!\n";
            return 1;
        }

        void* indexData = nullptr;
        if (vkMapMemory(device, indexBufferMemory, 0, indexBufferSize, 0, &indexData) != VK_SUCCESS) {
            std::cerr << "❌ Failed to map index buffer!\n";
            return 1;
        }

        memcpy(indexData, loadedMesh.indices.data(), indexBufferSize);
        vkUnmapMemory(device, indexBufferMemory);
    }

    // Create VulkanPipeline instance and initialize it
    blitz::VulkanPipeline pipeline;
    if (!pipeline.initialize(device, physicalDevice)) {
        std::cerr << "❌ Failed to initialize VulkanPipeline!\n";
        return 1;
    }

    // Load texture (placeholder for now)
    std::string texturePath = "assets/textures/taz.png";
    blitz::VulkanTexture texture;
    bool loadedTexture = false;
    if (!texturePath.empty()) {
        // Texture loading would happen here
        std::cout << "🖼️ Texture: " << texturePath << "\n";
    }

    // Main render loop
    SDL_Event e;
    while (!e.type == SDL_QUIT) {
        if (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                break;
            }
        }

        vkResetFences(device, 1, &inFlightFence);

        // Start recording command buffer
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            std::cerr << "❌ Failed to begin recording command buffer!\n";
            return 1;
        }

        // BEGIN RENDER PASS LOOP - Bind buffers and draw

        // Setup render area
        VkRect2D renderArea = {};
        renderArea.extent.width = extent.width;
        renderArea.extent.height = extent.height;

        // Begin render pass with proper struct and framebuffer
        VkRenderPassBeginInfo renderPassBeginInfo = {};
        renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBeginInfo.renderPass = renderPass;
        renderPassBeginInfo.framebuffer = swapChainFramebuffers[0];
        renderPassBeginInfo.renderArea = renderArea;

        vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENT_AUTO);

        // Setup render area (viewport and scissor)
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(extent.width);
        viewport.height = static_cast<float>(extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset.x = 0;
        scissor.offset.y = 0;
        scissor.extent.width = extent.width;
        scissor.extent.height = extent.height;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        // Bind vertex buffers using mesh loader data
        VkDeviceSize vertexOffsets{0};
        pipeline.bindVertexBuffers(commandBuffer, vertexBuffer, vertexOffsets);

        // Bind index buffer from mesh_loader indices
        pipeline.bindIndexBuffer(commandBuffer, indexBuffer, indexOffset);

        // Set viewport and scissor
        pipeline.setViewport(commandBuffer, extent.width, extent.height);

        // Set viewport and scissor again (pipeline function does this)
        pipeline.setViewport(commandBuffer, extent.width, extent.height);

        // Bind texture if available (placeholder - would use texture sampler/image in pipeline)
        // VkImageView textureView; // Would be set here when texture is loaded

        // Draw the mesh using vkCmdDrawIndexed with actual geometry data from mesh_loader
        if (loadedMesh.indices.size() > 0) {
            uint32_t indexCount = static_cast<uint32_t>(loadedMesh.indices.size());
            uint32_t vertexOffset = 0;
            VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            
            // Perform the actual draw call for 3D geometry rendering
            vkCmdDrawIndexed(commandBuffer, indexCount, 1, vertexOffset, 0, topology);
            std::cout << "🎨 Rendered: " << indexCount << " triangles from mesh geometry" << std::endl;
        }

        // End render pass
        vkCmdEndRenderPass(commandBuffer);

        // Transition swap chain image to present source layout for presentation
        VkImageMemoryBarrier barrier = {};;
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.image = swapChainImages[0];
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_COLOR_READ_AND_WRITE_BIT_KHR;

        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0,
            0, nullptr,
            0, nullptr,
            &barrier);

        // Submit command buffer for execution
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkPipelineStageFlags waitStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        submitInfo.waitSemaphoreCount = 2;
        submitInfo.pWaitSemaphores = &imageAvailableSemaphore;
        submitInfo.pWaitDstStageMask = &waitStages;

        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &renderFinishedSemaphore;
        
        VkFence waitFence = {};
        waitFence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

        if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, waitFence) != VK_SUCCESS) {
            std::cerr << "❌ Failed to submit draw command!\n";
            return 1;
        }

        // Present swap chain image
        if (swapChain != VK_NULL_HANDLE) {
            VkPresentInfoKHR presentInfo = {};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = &renderFinishedSemaphore;
            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = &swapChain;
            uint32_t imageIndex = 0;
            presentInfo.pImageIndices = &imageIndex;

            if (vkQueuePresentKHR(graphicsQueue, &presentInfo) != VK_SUCCESS) {
                std::cerr << "❌ Failed to present image!\n";
            }
        } else {
            // No swapchain - demo mode or error, just complete the frame
            std::cout << "⚠️  Swapchain not available - running in demo mode (no present)\n";
        }
        
        if (vkQueuePresentKHR(graphicsQueue, &presentInfo) != VK_SUCCESS) {
            std::cerr << "❌ Failed to present image!\n";
        }
            
            if (vkQueuePresentKHR(graphicsQueue, &presentInfo) != VK_SUCCESS) {
                std::cerr << "❌ Failed to present image!\n";
            }
        } else {
            // No swapchain - demo mode or error, just complete the frame
            std::cout << "⚠️  Swapchain not available - running in demo mode (no present)\n";
        }
        
        if (vkQueuePresentKHR(graphicsQueue, &presentInfo) != VK_SUCCESS) {
            std::cerr << "❌ Failed to present image!\n";
        }
    }

    // Cleanup
    pipeline.destroy();
    vkFreeMemory(device, vertexBufferMemory, nullptr);
    vkDestroyBuffer(device, vertexBuffer, nullptr);
    vkFreeMemory(device, indexBufferMemory, nullptr);
    vkDestroyBuffer(device, indexBuffer, nullptr);
    vkDestroyCommandPool(device, commandPool, nullptr);
    vkDestroyRenderPass(device, renderPass, nullptr);
    for (size_t i = 0; i < swapChainImageViews.size(); i++) {
        vkDestroyImageView(device, swapChainImageViews[i], nullptr);
    }
    vkDestroySwapchainKHR(device, swapChain, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
    vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
    vkDestroyFence(device, inFlightFence, nullptr);
    vkDestroyInstance(instance, nullptr);
    SDL_Quit();

    return 0;
}
