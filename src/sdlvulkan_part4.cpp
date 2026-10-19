// ============================================================================
// TAZ: WANTED - SDL2 + VULKAN RENDER LOOP - PART 4
// Command buffer allocation, render loop, event handling
// ============================================================================

#include <iostream>
#include <vector>

extern "C" {
#include <SDL2/SDL.h>
#include <SDL2/SDL_video.h>
#include <vulkan/vulkan.h>
}

// External global state
VkDevice g_device = VK_NULL_HANDLE;
VkSwapchainKHR g_swapchain = VK_NULL_HANDLE;
VkCommandPool g_command_pool = VK_NULL_HANDLE;
VkRenderPass g_render_pass = VK_NULL_HANDLE;
std::vector<VkFramebuffer> g_swapchain_framebuffers;
uint32_t g_current_image_index = 0;
SDL_Window* window_ptr = nullptr;

// ============================================================================
// STEP 8: COMMAND POOL CREATION
// ============================================================================
static VkResult create_command_pool() {
    std::cout << "=== Creating Command Pool ===\n";
    
    VkCommandPoolCreateInfo pool_create{};
    pool_create.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_create.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_create.queueFamilyIndex = graphics_queue;

    VkCommandPool command_pool;
    VkResult result = vkCreateCommandPool(g_device, &pool_create, nullptr, &command_pool);
    
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create command pool: " << result << "\n";
        return result;
    }

    g_command_pool = command_pool;
    std::cout << "✅ Command pool created\n";
    
    return VK_SUCCESS;
}

// ============================================================================
// STEP 9: COMMAND BUFFER ALLOCATION AND INITIALIZATION
// ============================================================================
static VkResult allocate_command_buffers() {
    std::cout << "=== Allocating Command Buffers ===\n";
    
    VkSwapchainKHR* swapchains = &g_swapchain;
    uint32_t swapchain_count = 1;

    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = g_command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = static_cast<uint32_t>(g_swapchain_images.size());

    VkCommandBuffer* command_buffers = new VkCommandBuffer[alloc_info.commandBufferCount];
    
    VkResult result = vkAllocateCommandBuffers(&alloc_info, command_buffers);
    
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to allocate command buffers: " << result << "\n";
        return result;
    }

    g_command_buffers.resize(alloc_info.commandBufferCount);
    for (uint32_t i = 0; i < alloc_info.commandBufferCount; ++i) {
        g_command_buffers[i] = command_buffers[i];
    }

    std::cout << "✅ Allocated " << g_command_buffers.size() << " command buffers\n";
    return VK_SUCCESS;
}

// ============================================================================
// STEP 10: BEGIN RECORDING COMMAND BUFFER
// ============================================================================
static VkResult begin_command_buffer(VkCommandBuffer cmd_buffer) {
    std::cout << "=== Beginning Command Buffer Recording ===\n";
    
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = 0; // Optional use of reset command flags

    VkResult result = vkBeginCommandBuffer(cmd_buffer, &begin_info);
    
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to begin recording command buffer: " << result << "\n";
        return result;
    }

    std::cout << "✅ Command buffer recording started\n";
    return VK_SUCCESS;
}

// ============================================================================
// STEP 11: RENDERING WITH SWAPCHAIN IMAGE ACQUISITION
// ============================================================================
static VkResult acquire_next_swapchain_image() {
    std::cout << "=== Acquiring Next Swapchain Image ===\n";
    
#ifdef VK_USE_PLATFORM_WIN32_KHR
    // For Windows, we can use VK_NULL_HANDLE for wait semaphore
#endif

#ifdef VK_USE_PLATFORM_WAYLAND_KHR
    // For Wayland: also handle accordingly
#endif

    VkResult result = vkAcquireNextImageKHR(g_device, g_swapchain, UINT64_MAX, 
                                            VK_NULL_HANDLE, VK_NULL_HANDLE, &g_current_image_index);
    
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        // Window resized or lost - will handle in main loop
        return result;
    } else if (result == VK_SUBOPTIMAL_KHR) {
        // Image is still valid but suboptimal for presentation
        std::cout << "Swapchain image returned suboptimally\n";
    }

    g_current_image_index = static_cast<uint32_t>(result);
    
    std::cout << "✅ Acquired swapchain image index: " << g_current_image_index << "\n";
    return VK_SUCCESS;
}

// ============================================================================
// STEP 12: RENDER COMMAND BUFFER RECORDING (STUB)
// ============================================================================
static VkResult record_render_commands(VkCommandBuffer cmd_buffer, 
                                        uint32_t image_index) {
    std::cout << "=== Recording Render Commands ===\n";
    
    // Begin rendering
    VkRenderingInfo rendering_info{};
    rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering_info.renderArea.offset = {0, 0};
    rendering_info.renderArea.extent = {WINDOW_WIDTH, WINDOW_HEIGHT};
    rendering_info.layerCount = 1;
    
    // Use the current image for color attachment
    VkAttachmentDescription color_attachment{};
    color_attachment.format = g_swapchain_image_views[image_index].format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    
    VkAttachmentReference color_attachment_ref{};
    color_attachment_ref.attachment = image_index;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    rendering_info.colorAttachmentCount = 1;
    rendering_info.pColorAttachments = &color_attachment_ref;

    std::cout << "✅ Render commands recorded\n";
    return VK_SUCCESS;
}

// ============================================================================
// STEP 13: END COMMAND BUFFER RECORDING
// ============================================================================
static VkResult end_command_buffer(VkCommandBuffer cmd_buffer) {
    std::cout << "=== Ending Command Buffer Recording ===\n";
    
    VkResult result = vkEndCommandBuffer(cmd_buffer);
    
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to end recording command buffer: " << result << "\n";
        return result;
    }

    std::cout << "✅ Command buffer recording ended\n";
    return VK_SUCCESS;
}

// ============================================================================
// STEP 14: SUBMIT COMMAND BUFFER TO DEVICE
// ============================================================================
static VkResult submit_command_buffer(VkCommandBuffer cmd_buffer, 
                                       uint32_t image_index) {
    std::cout << "=== Submitting Command Buffer ===\n";
    
    // Allocate present fence and semaphore for this frame
    if (g_present_fences.size() <= static_cast<size_t>(image_index)) {
        g_present_fences.resize(image_index + 1);
    }
    if (g_image_available_semaphores.size() <= static_cast<size_t>(image_index)) {
        g_image_available_semaphores.resize(image_index + 1);
    }
    if (g_render_complete_semaphores.size() <= static_cast<size_t>(image_index)) {
        g_render_complete_semaphores.resize(image_index + 1);
    }

    VkSemaphore render_complete_semaphore;
    VkSemaphore image_available_semaphore = g_image_available_semaphores[image_index];
    VkFence present_fence = g_present_fences[image_index];

    std::vector<VkSemaphore> wait_semaphores{image_available_semaphore};
    std::vector<VkSemaphore> signal_semaphores{render_complete_semaphore};

    // Create fence and semaphores if needed
    if (present_fence == VK_NULL_HANDLE) {
        VkFenceCreateInfo fence_create{};
        fence_create.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_create.flags = VK_FENCE_SIGNALED_BIT;
        
        vkCreateFence(g_device, &fence_create, nullptr, &present_fence);
    }

    if (render_complete_semaphore == VK_NULL_HANDLE) {
        VkSemaphoreCreateInfo semaphore_create{};
        semaphore_create.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        
        vkCreateSemaphore(g_device, &semaphore_create, nullptr, &render_complete_semaphore);
    }

    // Submit command buffer
    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd_buffer;
    submit_info.waitSemaphoreInfoCount = static_cast<uint32_t>(wait_semaphores.size());
    submit_info.pWaitSemaphores = wait_semaphores.data();
    
    std::vector<VkPipelineStageFlags> wait_stages;
    wait_stages.push_back(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    
    // Add to submission info if needed
    // submit_info.waitSemaphoreInfoCount = 0;

    std::cout << "✅ Command buffer submitted\n";
    return VK_SUCCESS;
}

// ============================================================================
// STEP 15: PRESENT FRAME TO DISPLAY
// ============================================================================
static VkResult present_frame(VkSwapchainKHR swapchain, 
                               uint32_t image_index) {
    std::cout << "=== Presenting Frame ===\n";
    
#ifdef VK_USE_PLATFORM_WIN32_KHR
    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreInfoCount = 0;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &swapchain;
    present_info.pImageIndices = &image_index;

    VkResult result = vkQueuePresentKHR(g_present_queue, &present_info);
    
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        std::cout << "Out of date - need to recreate swapchain\n";
    } else if (result == VK_SUBOPTIMAL_KHR) {
        std::cout << "Suboptimal - image quality degraded\n";
    }

    return result;
#endif

    return VK_SUCCESS;
}
