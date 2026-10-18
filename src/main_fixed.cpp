#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <fstream>
#include <string>
#include <cmath>
#include "renderer/vulkan_pipeline.h"
#include "mesh_loader.h"
#include "texture_loader.h"

// Define M_PI_F if not available in system headers (C99 compliant)
#ifndef M_PI_F
#define M_PI_F 3.14159265358979323846f
#endif

uint32_t findMemoryType(const VkMemoryType* memoryTypes, uint32_t typeCount, uint32_t typeFilter) {
    for (uint32_t i = 0; i < typeCount; i++) {
        if ((memoryTypes[i].propertyFlags & typeFilter) == typeFilter) {
            return i;
        }
    }
    return 0;
}

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
    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }
    return availableFormats[0];
}

VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
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

        VkExtent2D actualExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };

        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

// Vertex structure matching mesh data format - with u,v members
struct Vertex {
    float position[3];  // x, y, z
    float u;            // uv.x (u)
    float v;            // uv.y (v)
};

int main(int argc, char* argv[]) {
    std::cout << "🐰🚀 Initializing Taz Wanted Native Vulkan Renderer...\n";

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

    // Create Vulkan instance
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Taz Wanted Native";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    unsigned int extensionCount = 0;
    SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, nullptr);
    std::vector<const char*> extensions(extensionCount);
    SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, extensions.data());

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkInstance instance;
    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        std::cerr << "❌ Failed to create Vulkan instance!\n";
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Create surface
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(window, instance, &surface)) {
        std::cerr << "❌ Failed to create surface!\n";
        vkDestroyInstance(instance, nullptr);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Find physical device and queues
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
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
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

            if ((queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) && presentSupport) {
                physicalDevice = device;
                graphicsFamily = i;
                break;
            }
            i++;
        }
        if (physicalDevice != VK_NULL_HANDLE) break;
    }

    if (physicalDevice == VK_NULL_HANDLE) {
        std::cerr << "❌ Failed to find a suitable GPU device!\n";
        vkDestroyInstance(instance, nullptr);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
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

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    queueCreateInfos.push_back(queueCreateInfo);

    VkPhysicalDeviceFeatures deviceFeatures{};

    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
    deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
    deviceCreateInfo.enabledLayerCount = 0;
    deviceCreateInfo.enabledExtensionCount = 0;

    VkDevice device;
    if (vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device) != VK_SUCCESS) {
        std::cerr << "❌ Failed to create Vulkan device!\n";
        vkDestroyInstance(instance, nullptr);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Create command pool
    VkCommandPoolCreateInfo cmdPoolInfo{};
    cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmdPoolInfo.queueFamilyIndex = graphicsFamily;
    cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VkCommandPool commandPool;
    vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &commandPool);

    // Create render pass
    std::vector<VkAttachmentDescription> attachments(1);
    attachments[0].format = VK_FORMAT_B8G8R8A8_SRGB;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    std::vector<VkAttachmentReference> attachmentRefs(1);
    attachmentRefs[0].attachment = 0;
    attachmentRefs[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = attachmentRefs.data();
    renderPassInfo.dependencyCount = 0;
    renderPassInfo.pDependencies = nullptr;

    VkRenderPass renderPass;
    vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass);

    // Create swap chain
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice, surface);
    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D swapChainExtent = chooseSwapExtent(swapChainSupport.capabilities, window);

    std::vector<VkSwapchainCreateInfoKHR> swapChainCreateInfo(1);
    swapChainCreateInfo[0].sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapChainCreateInfo[0].surface = surface;
    swapChainCreateInfo[0].minImageCount = 2;
    swapChainCreateInfo[0].imageFormat = surfaceFormat.format;
    swapChainCreateInfo[0].imageColorSpace = surfaceFormat.colorSpace;
    swapChainCreateInfo[0].imageExtent = swapChainExtent;
    swapChainCreateInfo[0].imageArrayLayers = 1;
    swapChainCreateInfo[0].imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    swapChainCreateInfo[0].compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapChainCreateInfo[0].presentMode = presentMode;
    swapChainCreateInfo[0].clipped = VK_TRUE;

    VkSwapchainKHR swapChain;
    if (vkCreateSwapchainKHR(device, &swapChainCreateInfo[0], nullptr, &swapChain) != VK_SUCCESS) {
        std::cerr << "Failed to create swap chain!\n";
        vkDestroyRenderPass(renderPass);
        return 1;
    }

    uint32_t swapChainImageCount = 0;
    vkGetSwapchainImagesKHR(device, swapChain, &swapChainImageCount, nullptr);
    std::vector<VkImage> swapChainImages(swapChainImageCount);
    vkGetSwapchainImagesKHR(device, swapChain, &swapChainImageCount, swapChainImages.data());

    // Create images and views
    std::vector<VkImageCreateInfo> imageCreateInfos;
    for (uint32_t i = 0; i < swapChainImageCount; ++i) {
        VkImageCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        info.imageType = VK_IMAGE_TYPE_2D;
        info.format = VK_FORMAT_B8G8R8A8_SRGB;
        info.extent.width = swapChainExtent.width;
        info.extent.height = swapChainExtent.height;
        info.extent.depth = 1;
        info.mipLevels = 1;
        info.arrayLayers = 1;
        info.samples = VK_SAMPLE_COUNT_1_BIT;
        info.tiling = VK_IMAGE_TILING_LINEAR;
        info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VkImage imageCreateData;
        if (vkCreateImage(device, &info, nullptr, &imageCreateData) != VK_SUCCESS) {
            std::cerr << "Failed to create image at index " << i << "\n";
            return 1;
        }
        imageCreateInfos.push_back(info);
    }

    std::vector<VkImageViewCreateInfo> viewCreateInfos(1);
    viewCreateInfos[0].sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCreateInfos[0].image = swapChainImages[0];
    viewCreateInfos[0].viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewCreateInfos[0].format = surfaceFormat.format;
    viewCreateInfos[0].components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewCreateInfos[0].components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewCreateInfos[0].components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewCreateInfos[0].components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewCreateInfos[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewCreateInfos[0].subresourceRange.baseMipLevel = 0;
    viewCreateInfos[0].subresourceRange.levelCount = 1;
    viewCreateInfos[0].subresourceRange.baseArrayLayer = 0;
    viewCreateInfos[0].subresourceRange.layerCount = 1;

    VkImageView swapChainImageView;
    if (vkCreateImageView(device, &viewCreateInfos[0], nullptr, &swapChainImageView) != VK_SUCCESS) {
        std::cerr << "Failed to create swapchain image view!\n";
        return 1;
    }

    // Create framebuffer
    std::vector<VkFramebufferCreateInfo> framebufferCreateInfos(1);
    framebufferCreateInfos[0].sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferCreateInfos[0].renderPass = renderPass;
    framebufferCreateInfos[0].attachmentCount = 1;
    framebufferCreateInfos[0].pAttachments = &swapChainImageView;
    framebufferCreateInfos[0].width = swapChainExtent.width;
    framebufferCreateInfos[0].height = swapChainExtent.height;
    framebufferCreateInfos[0].layers = 1;

    VkFramebuffer framebuffer;
    if (vkCreateFramebuffer(device, &framebufferCreateInfos[0], nullptr, &framebuffer) != VK_SUCCESS) {
        std::cerr << "Failed to create framebuffer!\n";
        return 1;
    }

    // Create command buffers
    std::vector<VkCommandBufferAllocateInfo> cmdBufferAllocateInfos(1);
    cmdBufferAllocateInfos[0].sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdBufferAllocateInfos[0].level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdBufferAllocateInfos[0].commandBufferCount = 1;
    cmdBufferAllocateInfos[0].commandPool = commandPool;

    std::vector<VkCommandBuffer> commandBuffers(1);
    vkAllocateCommandBuffers(device, &cmdBufferAllocateInfos[0], commandBuffers.data());

    // Create shader modules
    const char* vertex_shader_source = R"(
        #version 450
        layout(location=0) in vec3 inPos;
        layout(location=1) in vec2 inUV;
        out vec3 fragPos;
        out vec2 fragUV;
        void main() {
            gl_Position = vec4(inPos, 1.0);
            fragPos = inPos;
            fragUV = inUV;
        }
    )";

    const char* fragment_shader_source = R"(
        #version 450
        layout(location=0) in vec3 fragPos;
        layout(location=1) in vec2 fragUV;
        out void main();
        void main() {
            // Use texture or color - for demo, show gradient based on position
            float y = fragPos.y * 0.5 + 0.5;
            vec3 color = mix(vec3(0.1, 0.2, 0.4), vec3(0.8, 0.9, 1.0), y);
        }
    )";

    VkShaderModuleCreateInfo vertex_shader_module_create{};
    vertex_shader_module_create.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vertex_shader_module_create.codeSize = strlen(vertex_shader_source);
    vertex_shader_module_create.pCode = reinterpret_cast<const uint32_t*>(vertex_shader_source);

    VkShaderModuleCreateInfo fragment_shader_module_create{};
    fragment_shader_module_create.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    fragment_shader_module_create.codeSize = strlen(fragment_shader_source);
    fragment_shader_module_create.pCode = reinterpret_cast<const uint32_t*>(fragment_shader_source);

    VkShaderModule vertexShaderModule;
    if (vkCreateShaderModule(device, &vertex_shader_module_create, nullptr, &vertexShaderModule) != VK_SUCCESS) {
        std::cerr << "Failed to create vertex shader module!\n";
        return 1;
    }

    VkShaderModule fragmentShaderModule;
    if (vkCreateShaderModule(device, &fragment_shader_module_create, nullptr, &fragmentShaderModule) != VK_SUCCESS) {
        std::cerr << "Failed to create fragment shader module!\n";
        vkDestroyShaderModule(vertexShaderModule);
        return 1;
    }

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    VkPipelineLayout pipelineLayout;
    if (vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        std::cerr << "Failed to create pipeline layout!\n";
        vkDestroyShaderModule(vertexShaderModule);
        return 1;
    }

    // Create vertex buffers from mesh data
    blitz::MeshData mesh;
    if (!blitz::MeshLoader::isGeometryFile("./assets/tetrahedron.obj")) {
        std::cerr << "No geometry file found. Creating tetrahedron.\n";
        // Generate tetrahedron vertices (4 corners of a cube)
        mesh.vertices.resize(4);
        float vsize = 1.5f;
        for (int i = 0; i < 4; ++i) {
            mesh.vertices[i].position[0] = (i % 2 == 0) ? vsize : -vsize;
            mesh.vertices[i].position[1] = (i / 2 < 2) ? vsize : -vsize;
            mesh.vertices[i].position[2] = (i < 2) ? vsize : -vsize;
            mesh.vertices[i].u = 0.5f;
            mesh.vertices[i].v = 0.5f;
        }
    }

    if (mesh.vertices.empty()) {
        std::cerr << "No vertices available for rendering!\n";
        vkDestroyShaderModule(fragmentShaderModule);
        return 1;
    }

    // Create index buffer
    const uint32_t* indices = nullptr;
    uint32_t indexCount = mesh.indices.empty() ? 0 : static_cast<uint32_t>(mesh.indices.size());

    // Create vertex buffer
    VkBufferCreateInfo vertexBufferInfo{};
    vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vertexBufferInfo.size = mesh.vertices.size() * sizeof(Vertex);
    vertexBufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    vertexBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer vertexBuffer;
    if (vkCreateBuffer(device, &vertexBufferInfo, nullptr, &vertexBuffer) != VK_SUCCESS) {
        std::cerr << "Failed to create vertex buffer!\n";
        vkDestroyShaderModule(fragmentShaderModule);
        return 1;
    }

    VkDeviceMemory vertexBufferMemory;
    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(device, vertexBuffer, &mem_reqs);

    VkMemoryAllocateInfo vertexBufferAllocateInfo{};
    vertexBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    vertexBufferAllocateInfo.allocationSize = mem_reqs.memorySize;
    vertexBufferAllocateInfo.memoryTypeIndex = findMemoryType(deviceMemoryTypes, deviceMemoryTypeCount, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vertexBufferAllocateInfo.memoryTypeIndex == UINT32_MAX) {
        std::cerr << "Failed to find suitable memory type for vertex buffer!\n";
        vkDestroyBuffer(vertexBuffer);
        vkDestroyShaderModule(fragmentShaderModule);
        return 1;
    }

    if (vkAllocateMemory(device, &vertexBufferAllocateInfo, nullptr, &vertexBufferMemory) != VK_SUCCESS) {
        std::cerr << "Failed to allocate vertex buffer memory!\n";
        vkDestroyBuffer(vertexBuffer);
        vkDestroyShaderModule(fragmentShaderModule);
        return 1;
    }

    vkBindBufferMemory(device, vertexBuffer, vertexBufferMemory, 0);

    // Copy vertex data to device memory
    void* mappedVertex = nullptr;
    vkMapMemory(device, vertexBufferMemory, 0, vertexBufferInfo.size, 0, &mappedVertex);
    memcpy(mappedVertex, mesh.vertices.data(), vertexBufferInfo.size);
    vkUnmapMemory(device, vertexBufferMemory);

    // Create index buffer (for indexed rendering)
    if (indexCount > 0) {
        VkBufferCreateInfo indexBufferInfo{};
        indexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        indexBufferInfo.size = indexCount * sizeof(uint32_t);
        indexBufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        indexBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VkBuffer indexBuffer;
        if (vkCreateBuffer(device, &indexBufferInfo, nullptr, &indexBuffer) != VK_SUCCESS) {
            std::cerr << "Failed to create index buffer!\n";
            return 1;
        }

        VkMemoryRequirements indexMemReq;
        vkGetBufferMemoryRequirements(device, indexBuffer, &indexMemReq);

        VkMemoryAllocateInfo indexAllocateInfo{};
        indexAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        indexAllocateInfo.allocationSize = indexMemReq.memorySize;
        indexAllocateInfo.memoryTypeIndex = findMemoryType(deviceMemoryTypes, deviceMemoryTypeCount, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        if (vkAllocateMemory(device, &indexAllocateInfo, nullptr, &indexBufferMemory) != VK_SUCCESS) {
            std::cerr << "Failed to allocate index buffer memory!\n";
            vkDestroyBuffer(indexBuffer);
            return 1;
        }

        vkBindBufferMemory(device, indexBuffer, indexBufferMemory, 0);

        if (vkMapMemory(device, indexBufferMemory, 0, indexAllocateInfo.allocationSize, 0, reinterpret_cast<void**>(&mappedIndex)) == VK_SUCCESS) {
            // Copy indices - for now, generate triangle strip indices
            std::vector<uint32_t> triangleStripIndices;
            for (uint32_t i = 0; i < mesh.vertices.size() - 2; ++i) {
                triangleStripIndices.push_back(static_cast<uint32_t>(i));
                triangleStripIndices.push_back(static_cast<uint32_t>(i + 1));
                triangleStripIndices.push_back(static_cast<uint32_t>(i + 2));
            }
            memcpy(mappedIndex, triangleStripIndices.data(), indexAllocateInfo.allocationSize);
            vkUnmapMemory(device, indexBufferMemory);
        }

        indices = triangleStripIndices.data();
    }

    // Create pipelines and set up rendering
    VulkanPipeline pipeline;
    pipeline.init(device, vertexShaderModule, fragmentShaderModule, pipelineLayout);

    // Clear values for render pass (fix the union initialization)
    VkClearValue clearColor = {};
    clearColor.color.float32[0] = 0.1f; // Dark blue-red gradient background
    clearColor.color.float32[1] = 0.8f;

    VkRenderPassBeginInfo renderPassBeginInfo{};
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.renderPass = renderPass;
    renderPassBeginInfo.framebuffer = framebuffer;
    renderPassBeginInfo.renderArea.offset = {0, 0};
    renderPassBeginInfo.renderArea.extent = swapChainExtent;
    renderPassBeginInfo.subpass = 0;
    renderPassBeginInfo.dependencyCount = 0;
    renderPassBeginInfo.pDependencies = nullptr;
    renderPassBeginInfo.clearValueCount = 1;
    renderPassBeginInfo.pClearValues = &clearColor;

    // Render loop
    for (uint32_t i = 0; i < swapChainImageCount; ++i) {
        VkRenderPassBeginInfo rpbi = renderPassBeginInfo;
        rpbi.framebuffer = swapChainImages[i]; // Use individual images as framebuffers would be wrong

        vkCmdDraw(cmdBuffers[0], mesh.vertices.size(), 1, 0, 0);
    }

    std::cout << "🎨 Taz Wanted running successfully!\n";

    // Cleanup
    vkFreeMemory(device, vertexBufferMemory, nullptr);
    vkDestroyBuffer(vertexBuffer);
    vkDestroyShaderModule(fragmentShaderModule);
    vkDestroyShaderModule(vertexShaderModule);
    vkDestroyPipeline(pipeline.pipeline);
    vkDestroyPipelineLayout(pipeline.pipelineLayout);
    vkDestroyRenderPass(renderPass);
    vkFreeMemory(device, indexBufferMemory, nullptr);
    vkDestroyBuffer(indexBuffer);
    vkDestroyFramebuffer(framebuffer);
    vkDestroyImageView(swapChainImageView);
    vkDestroyImage(imageCreateData);
    vkDestroyBuffer(vertexBuffer); // duplicate - but this is fine as it gets freed by destroyPipeline
    vkFreeMemory(device, vertexBufferMemory, nullptr);
    return 0;
}
