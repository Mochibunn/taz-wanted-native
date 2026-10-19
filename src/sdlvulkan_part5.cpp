// ============================================================================
// TAZ: WANTED - SDL2 + VULKAN RENDER LOOP - PART 5
// Main render loop, event handling, cleanup
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
VkCommandPool g_command_pool = VK_NULL_HANDLE;
VkSurfaceKHR g_surface = VK_NULL_HANDLE;
std::vector<VkCommandBuffer> g_command_buffers;
uint32_t g_current_image_index = 0;
SDL_Window* window_ptr = nullptr;

// ============================================================================
// STEP 16: MAIN RENDER LOOP WITH EVENT HANDLING
// ============================================================================
static int run_render_loop() {
    std::cout << "=== Starting Main Render Loop ===\n";
    
    while (!g_swapchain) {
        // Create swapchain if not done yet (initialization phase)
        VkResult result = create_swapchain();
        if (result != VK_SUCCESS) {
            std::cerr << "Failed to create swapchain\n";
            return -1;
        }

        // Need to get framebuffers after swapchain images exist
        uint32_t framebuffer_count = g_swapchain_images.size();
        if (framebuffer_count > 0) {
            g_swapchain_framebuffers.resize(framebuffer_count);
            
            VkFramebufferCreateInfo fb_create{};
            fb_create.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            fb_create.renderPass = g_render_pass;
            fb_create.attachmentCount = 1;
            fb_create.width = WINDOW_WIDTH;
            fb_create.height = WINDOW_HEIGHT;
            fb_create.layers = 1;
            fb_create.pAttachments = nullptr; // Will be set dynamically

            for (uint32_t i = 0; i < framebuffer_count; ++i) {
                VkFramebuffer framebuffer;
                fb_create.attachmentCount = 1;
                fb_create.pAttachments = &g_swapchain_image_views[i];
                
                VkResult res = vkCreateFramebuffer(g_device, &fb_create, nullptr, &framebuffer);
                if (res != VK_SUCCESS) {
                    std::cerr << "Failed to create framebuffer\n";
                    continue;
                }
                g_swapchain_framebuffers[i] = framebuffer;
            }

            std::cout << "✅ Created " << g_swapchain_framebuffers.size() 
                      << " framebuffers\n";
        }
    }

    // Framebuffer allocation - dynamic creation based on image count
    uint32_t fb_count = 0;
    vkGetSwapchainImagesKHR(g_device, g_swapchain, nullptr, &fb_count, nullptr);
    
    if (fb_count > 0) {
        std::vector<VkImage> swc_images(fb_count);
        vkGetSwapchainImagesKHR(g_device, g_swapchain, nullptr, &fb_count, swc_images.data());

        // Create views for each swapchain image
        VkImageViewCreateInfo view_info{};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.layerCount = 1;

        for (uint32_t i = 0; i < fb_count; ++i) {
            VkImageView attachment_view;
            view_info.image = swc_images[i];
            
            VkResult res = vkCreateImageView(g_device, &view_info, nullptr, &attachment_view);
            if (res != VK_SUCCESS) continue;
        }

        std::cout << "✅ Framebuffers and views created\n";
    }

    // Allocate command pool
    create_command_pool();

    // Allocate command buffers
    allocate_command_buffers();

    std::cout << "✅ Initial setup complete, entering render loop\n\n";
    
    return 0;
}

// ============================================================================
// STEP 17: RENDER A SINGLE FRAME
// ============================================================================
static void render_frame() {
    std::cout << "=== Rendering Frame " << g_current_image_index << " ===\n";
    
    // Acquire next image from swapchain
    VkResult acquire_result = vkAcquireNextImageKHR(g_device, g_swapchain, 
                                                      UINT64_MAX, VK_NULL_HANDLE, VK_NULL_HANDLE, 
                                                      &g_current_image_index);
    
    if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR) {
        // Window resized - need to recreate swapchain (handled in loop)
        return;
    } else if (acquire_result == VK_SUBOPTIMAL_KHR) {
        std::cout << "Swapchain suboptimal\n";
    }

    uint32_t image_index = static_cast<uint32_t>(acquire_result);

    // Begin command buffer recording
    VkCommandBuffer cmd_buffer = g_command_buffers[0]; // Use first for demo
    begin_command_buffer(cmd_buffer);

    // Record render commands
    record_render_commands(cmd_buffer, image_index);

    // End command buffer
    end_command_buffer(cmd_buffer);

    // Submit and present
    submit_command_buffer(cmd_buffer, image_index);
    present_frame(g_swapchain, image_index);

    std::cout << "✅ Frame " << g_current_image_index << " rendered\n";
}

// ============================================================================
// STEP 18: MAIN FUNCTION - TIES IT ALL TOGETHER
// ============================================================================
int main() {
    // Set up signal handlers for clean exit (Linux)
#ifdef __linux__
    struct sigaction sa;
    sa.sa_sigaction = [](int signum) -> void {
        std::cout << "\nReceived signal " << signum << ", cleaning up...\n";
        cleanup_vulkan_resources();
        SDL_DestroyWindow(window_ptr);
        SDL_Quit();
        exit(signum);
    };
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
#endif

    // Initialize Vulkan instance
    std::cout << "=== TAZ: WANTED - SDL2 + Vulkan Render Loop ===\n\n";
    
    VkResult init_result = init_vulkan_instance();
    if (init_result != VK_SUCCESS) {
        std::cerr << "Failed to initialize Vulkan\n";
        return -1;
    }

    // Initialize SDL window and create surface
    init_sdl_window();

    // Select physical device
    select_physical_device();

    // Setup queues
    setup_queues(g_physical_device);

    // Create render pass
    create_render_pass();

    // Create swapchain (initially null, will be created in loop)
    g_swapchain = VK_NULL_HANDLE;

    // Start main render loop
    run_render_loop();

    if (!g_swapchain) {
        std::cerr << "Failed to create swapchain\n";
        cleanup_vulkan_resources();
        cleanup_sdl_resources();
        return -1;
    }

    // Enter event-driven rendering (would poll events and render frames here)
    SDL_Event event;
    uint64_t last_time = SDL_GetTicks();
    
    std::cout << "=== Event-driven Rendering Phase ===\n";
    
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            break;
        } else if (event.type == SDL_WINDOWEVENT) {
            if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                // Window resized - would recreate swapchain here
                std::cout << "Window resized to " 
                          << event.window.data1 << "x" << event.window.data2 << "\n";
            }
        } else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
            break;
        }
    }

    std::cout << "\n=== Exiting Render Loop ===\n";
    
    // Cleanup resources
    cleanup_vulkan_resources();
    cleanup_sdl_resources();

    std::cout << "✅ Taz: Wanted SDL2 + Vulkan render loop completed successfully\n";
    return 0;
}
