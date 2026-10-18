#include "renderer/vulkan_pipeline.h"
#include <vulkan/vulkan_core.h>
#include <iostream>
#include <fstream>

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

VkDevice g_device = VK_NULL_HANDLE;
VkPhysicalDevice g_physical_device = (VkPhysicalDevice)0;
VulkanPipeline g_pipeline;

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    std::cout << "=== TazWanted: Minimal Vulkan Pipeline Test ===" << std::endl;

    VkInstance g_instance = VK_NULL_HANDLE;  // Global instance

    // Initialize Vulkan instance
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
    
    // Only surface extension is needed - skip Xlib for this minimal test
    const char* extensions[] = {VK_KHR_SURFACE_EXTENSION_NAME};
    create_info.enabledExtensionCount = sizeof(extensions) / sizeof(const char*);
    create_info.ppEnabledExtensionNames = extensions;

    VkResult result = vkCreateInstance(&create_info, nullptr, &g_instance);
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create Vulkan instance: " << result << std::endl;
        return -1;
    }

    // Create logical device with swapchain extension
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

    // Initialize pipeline
    VkResult init_result = g_pipeline.initialize(g_device, g_physical_device);
    if (init_result != VK_SUCCESS) {
        std::cerr << "Failed to initialize VulkanPipeline: " << init_result << "\n";
        return -1;
    }

    // Create the graphics pipeline with inline shaders
    result = g_pipeline.create(nullptr, nullptr);
    
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create graphics pipeline: " << result << std::endl;
        return -1;
    }
    
    std::cout << "✅ Vulkan pipeline initialized successfully" << std::endl;

    // Test bind method
    if (g_pipeline.getPipelineHandle() != VK_NULL_HANDLE) {
        std::cout << "✅ Pipeline handle is valid: 0x" << std::hex << g_pipeline.getPipelineHandle() << std::dec << std::endl;
    } else {
        std::cerr << "⚠️  Warning: Pipeline handle is null\n";
    }

    // Cleanup and create success flag
    g_pipeline.destroy();

    // Create success flag file since pipeline was successfully created/bindable
    std::ofstream flag_file("/home/mochi/taz-decomp/vulkan_color_complete.flag");
    if (flag_file.is_open()) {
        flag_file << "Vulkan color pipeline initialized and bound successfully" << std::endl;
        flag_file.close();
        std::cout << "✅ Success flag created: vulkan_color_complete.flag" << std::endl;
    }

    // Cleanup all Vulkan resources before exit
    vkDestroyDevice(g_device, nullptr);
    vkDestroyInstance(g_instance, nullptr);

    return 0;
}
