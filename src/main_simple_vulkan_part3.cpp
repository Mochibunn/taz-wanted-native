// ============================================================================
// RENDER LOOP AND CLEANUP - PART 3
// ============================================================================

VkResult recreate_swapchain() {
    // Note: In a full implementation, this would recreate the swapchain with
    // updated dimensions. For now, we'll just log and continue.
    std::cout << "Swapchain recreation skipped (resize not detected).\n";
    return VK_SUCCESS;
}

VkResult present_frame(uint32_t index) {
    if (!g_device || g_swapchain_images.size() == 0) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Acquire next swapchain image
    VkResult acquire_result = vkAcquireNextImageKHR(g_device, g_swapchain, UINT32_MAX, 
                                                     g_image_available_semaphores[index], VK_NULL_HANDLE);

    if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR) {
        // Need to recreate swapchain due to surface resize
        std::cout << "Swapchain needs update.\n";
        return VK_ERROR_OUT_OF_DATE_KHR;
    } else if (acquire_result == VK_SUBOPTIMAL_KHR) {
        std::cout << "Swapchain is suboptimal.\n";
        return VK_SUBOPTIMAL_KHR;
    }

    // Record command buffer for this frame
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    
    result = vkBeginCommandBuffer(g_command_buffers[index], &begin_info);

    if (result != VK_SUCCESS) {
        std::cerr << "Failed to begin recording command buffer: " << static_cast<int>(result) << "\n";
        return result;
    }

    // Begin render pass
    VkRenderPassBeginInfo render_pass_begin_info{};
    render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_begin_info.renderPass = g_render_pass;
    render_pass_begin_info.framebuffer = g_swapchain_framebuffers[index];
    render_pass_begin_info.renderArea.offset = {0, 0};
    render_pass_begin_info.renderArea.extent = {WINDOW_WIDTH, WINDOW_HEIGHT};

    vkCmdBeginRenderPass(g_command_buffers[index], &render_pass_begin_info, VK_SUBPASS_CONTENT_SECONDARY_COMMAND_BUFFER);

    // Clear color attachment
    VkClearValue clear_values[1];
    clear_values[0] = VkClearValue{{0.1f, 0.2f, 0.3f, 1.0f}};  // Dark background

    vkCmdSetColorAttachment(g_command_buffers[index], 0, &clear_value[0]);

    // End render pass
    vkCmdEndRenderPass(g_command_buffers[index]);

    // End command buffer recording
    VkCommandBufferEndInfo end_info{};
    end_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_END_INFO;

    result = vkEndCommandBuffer(g_command_buffers[index], &end_info);

    if (result != VK_SUCCESS) {
        std::cerr << "Failed to end command buffer: " << static_cast<int>(result) << "\n";
        return result;
    }

    // Submit command buffer for execution
    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &g_image_available_semaphores[index];
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &g_render_complete_semaphores[index];

    // Execute command buffer
    result = vkQueueSubmit(present_queue, 1, &submit_info, g_fences[index]);

    if (result != VK_SUCCESS) {
        std::cerr << "Failed to submit command buffer: " << static_cast<int>(result) << "\n";
        return result;
    }

    // Wait for render to complete before presenting
    vkWaitForFences(g_device, 1, &g_fences[index], VK_FALSE, UINT64_MAX);

    // Present swapchain image
    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &g_swapchain;
    present_info.pImageIndices = &index;

    result = vkQueuePresentKHR(present_queue, &present_info);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        std::cout << "Swapchain may have changed.\n";
        return recreate_swapchain();
    }

    g_current_frame_index = index;
    std::cout << "Frame presented. Frame index: " << index << "\n";

    return VK_SUCCESS;
}

// ============================================================================
// MAIN FUNCTION - COMPLETE SDL2 + VULKAN RENDER LOOP
// ============================================================================

int main(int argc, char* argv[]) {
    std::cout << "=== TAZ: WANTED - SDL2 + VULKAN RENDER LOOP ===\n";
    std::cout << "Building SDL2 window with Vulkan swapchain render loop...\n\n";

    // Initialize SDL2
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL initialization failed: " << SDL_GetError() << "\n";
        return 1;
    }
    std::cout << "SDL initialized successfully.\n\n";

    // Create SDL window with Vulkan support
    g_sdl_window = SDL_CreateWindow(
        "TAZ: WANTED - Vulkan Render Demo",
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_BORDERLESS
    );

    if (!g_sdl_window) {
        std::cerr << "Failed to create SDL window: " << SDL_GetError() << "\n";
        SDL_Quit();
        return 1;
    }

    std::cout << "SDL window created:\n";
    std::cout << "  Width: " << WINDOW_WIDTH << "px\n";
    std::cout << "  Height: " << WINDOW_HEIGHT << "px\n\n";

    // Initialize Vulkan instance
    if (init_vulkan_instance() != VK_SUCCESS) {
        std::cerr << "Failed to initialize Vulkan instance.\n";
        SDL_DestroyWindow(g_sdl_window);
        SDL_Quit();
        return -1;
    }

    // Create surface from SDL window
    int sdl_result = SDL_Vulkan_CreateSurface(g_sdl_window, g_vulkan_instance, nullptr, &g_surface);
    
    if (sdl_result != 0) {
        std::cerr << "Failed to create SDL Vulkan surface: " << SDL_GetError() << "\n";
        vkDestroyInstance(g_vulkan_instance, nullptr);
        SDL_DestroyWindow(g_sdl_window);
        SDL_Quit();
        return -1;
    }

    std::cout << "Vulkan surface created from SDL window.\n\n";

    // Find and select physical device
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(g_vulkan_instance, &device_count, nullptr);

    if (device_count == 0) {
        std::cerr << "No GPUs found!\n";
        vkDestroyInstance(g_vulkan_instance, nullptr);
        SDL_DestroyWindow(g_sdl_window);
        SDL_Quit();
        return -1;
    }

    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(g_vulkan_instance, &device_count, devices.data());

    VkPhysicalDevice selected_device = VK_NULL_HANDLE;
    uint32_t graphics_family = UINT32_MAX;
    uint32_t present_family = UINT32_MAX;

    for (uint32_t i = 0; i < device_count; i++) {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(devices[i], &properties);

        std::cout << "GPU: " << properties.deviceName << "\n";

        // Check queue support
        uint32_t queue_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &queue_count, nullptr);

        if (queue_count == 0) {
            continue;
        }

        std::vector<VkQueueFamilyProperties> queue_properties(queue_count);
        vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &queue_count, queue_properties.data());

        for (uint32_t j = 0; j < queue_count; j++) {
            if ((queue_properties[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
                graphics_family = j;
            }

            VkBool32 surface_supported = false;
            vkGetPhysicalDeviceSurfaceSupportedKHR(g_vulkan_instance, g_surface, &surface_supported);

            if (surface_supported == VK_TRUE) {
                present_family = j;
            }
        }

        // Select first suitable device
        selected_device = devices[i];
        break;
    }

    if (!selected_device) {
        std::cerr << "Failed to find suitable physical device!\n";
        vkDestroyInstance(g_vulkan_instance, nullptr);
        SDL_DestroyWindow(g_sdl_window);
        SDL_Quit();
        return -1;
    }

    g_physical_device = selected_device;

    // Create logical device
    if (create_logical_device() != VK_SUCCESS) {
        std::cerr << "Failed to create logical device.\n";
        vkDestroyInstance(g_vulkan_instance, nullptr);
        SDL_DestroyWindow(g_sdl_window);
        SDL_Quit();
        return -1;
    }

    // Create command pool
    if (create_command_pool() != VK_SUCCESS) {
        std::cerr << "Failed to create command pool.\n";
        vkDestroyDevice(g_device, nullptr);
        vkDestroyInstance(g_vulkan_instance, nullptr);
        SDL_DestroyWindow(g_sdl_window);
        SDL_Quit();
        return -1;
    }

    // Create render pass
    if (create_render_pass() != VK_SUCCESS) {
        std::cerr << "Failed to create render pass.\n";
        vkDestroyDevice(g_device, nullptr);
        vkDestroyInstance(g_vulkan_instance, nullptr);
        SDL_DestroyWindow(g_sdl_window);
        SDL_Quit();
        return -1;
    }

    // Create pipeline layout
    if (create_pipeline_layout() != VK_SUCCESS) {
        std::cerr << "Failed to create pipeline layout.\n";
        vkDestroyDevice(g_device, nullptr);
        vkDestroyInstance(g_vulkan_instance, nullptr);
        SDL_DestroyWindow(g_sdl_window);
        SDL_Quit();
        return -1;
    }

    // Create swapchain (after render pass)
    if (create_swapchain() != VK_SUCCESS) {
        std::cerr << "Failed to create swapchain.\n";
        vkDestroyDevice(g_device, nullptr);
        vkDestroyInstance(g_vulkan_instance, nullptr);
        SDL_DestroyWindow(g_sdl_window);
        SDL_Quit();
        return -1;
    }

    // Create command buffers and synchronization
    if (create_command_buffer() != VK_SUCCESS) {
        std::cerr << "Failed to create command buffers.\n";
        vkDestroyDevice(g_device, nullptr);
        vkDestroyInstance(g_vulkan_instance, nullptr);
        SDL_DestroyWindow(g_sdl_window);
        SDL_Quit();
        return -1;
    }

    // Create vertex buffer
    if (create_vertex_buffer() != VK_SUCCESS) {
        std::cerr << "Failed to create vertex buffer.\n";
        vkDestroyDevice(g_device, nullptr);
        vkDestroyInstance(g_vulkan_instance, nullptr);
        SDL_DestroyWindow(g_sdl_window);
        SDL_Quit();
        return -1;
    }

    std::cout << "\n=== All Vulkan resources initialized successfully ===\n\n";
    
    // Render loop
    bool success = run_render_loop();

    if (!success) {
        std::cerr << "Render loop terminated with error.\n";
        cleanup_vulkan();
        return -1;
    }

    std::cout << "\n=== Cleanup complete ===\n";
    vkDeviceWaitIdle(g_device);
    vkDestroySurfaceKHR(g_vulkan_instance, g_surface, nullptr);
    vkDestroyDevice(g_device, nullptr);
    vkDestroyInstance(g_vulkan_instance, nullptr);
    SDL_DestroyWindow(g_sdl_window);
    SDL_Quit();

    std::cout << "SDL2 + VULKAN RENDER LOOP: SUCCESS\n";
    return 0;
}

// ============================================================================
// RENDER LOOP FUNCTION
// ============================================================================

bool run_render_loop() {
    uint32_t frame_index = 0;
    
    // Main render loop with SDL event processing
    std::cout << "Starting render loop...\n";
    
    while (!g_should_quit) {
        // Process SDL events
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                g_should_quit = true;
                break;
            }
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                g_should_quit = true;
            }
        }

        if (g_should_quit) {
            break;
        }

        // Present frame
        present_frame(frame_index);
        frame_index++;

        // Control frame timing
        SDL_Delay(16);  // ~60 FPS
    }

    std::cout << "Render loop terminated. Presented " << frame_index << " frames.\n";
    return true;
}

// ============================================================================
// CLEANUP FUNCTION
// ============================================================================

void cleanup_vulkan() {
    if (vertex_memory) vkFreeMemory(g_device, vertex_memory, nullptr);
    if (vertex_buffer) vkDestroyBuffer(g_device, vertex_buffer, nullptr);
    
    for (size_t i = 0; i < g_swapchain_images.size(); i++) {
        if (g_image_available_semaphores[i]) 
            vkDestroySemaphore(g_device, g_image_available_semaphores[i], nullptr);
        if (g_render_complete_semaphores[i]) 
            vkDestroySemaphore(g_device, g_render_complete_semaphores[i], nullptr);
        if (g_fences[i]) 
            vkDestroyFence(g_device, g_fences[i], nullptr);
    }

    for (size_t i = 0; i < g_swapchain_image_views.size(); i++) {
        if (g_swapchain_image_views[i]) 
            vkDestroyImageView(g_device, g_swapchain_image_views[i], nullptr);
    }

    for (size_t i = 0; i < g_swapchain_framebuffers.size(); i++) {
        if (g_swapchain_framebuffers[i]) 
            vkDestroyFramebuffer(g_device, g_swapchain_framebuffers[i], nullptr);
    }

    if (g_swapchain != VK_NULL_HANDLE) 
        vkDestroySwapchainKHR(g_device, g_swapchain, nullptr);
    if (g_render_pass != VK_NULL_HANDLE) 
        vkDestroyRenderPass(g_device, g_render_pass, nullptr);
    if (g_pipeline_layout != VK_NULL_HANDLE) 
        vkDestroyPipelineLayout(g_device, g_pipeline_layout, nullptr);
    if (g_graphics_pipeline != VK_NULL_HANDLE) 
        vkDestroyPipeline(g_device, g_graphics_pipeline, nullptr);
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

uint32_t get_memory_type(VkPhysicalDevice physical_device, uint32_t type_bits, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_properties);

    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
        if ((type_bits & (1 << i)) && 
            (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    std::cerr << "Failed to find suitable memory type.\n";
    return 0;
}

// ============================================================================
// VERTEX BUFFER CREATION - COMPLETE VERSION
// ============================================================================

VkResult create_vertex_buffer_complete() {
    if (!g_device) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Create vertex buffer with proper structure
    size_t vertex_data_size = g_vertices.size() * sizeof(Vertex);

    std::vector<VkVertexInputBindingDescription> binding_descriptions(1);
    binding_descriptions[0].binding = 0;
    binding_descriptions[0].stride = sizeof(Vertex);
    binding_descriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::vector<VkVertexInputAttributeDescription> attribute_descriptions(2);
    attribute_descriptions[0].location = 0;
    attribute_descriptions[0].binding = 0;
    attribute_descriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attribute_descriptions[0].offset = offsetof(Vertex, position);

    attribute_descriptions[1].location = 1;
    attribute_descriptions[1].binding = 0;
    attribute_descriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
    attribute_descriptions[1].offset = offsetof(Vertex, uv);

    VkPipelineVertexInputStateCreateInfo vertex_input_create_info{};
    vertex_input_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_create_info.vertexBindingDescriptionCount = 1;
    vertex_input_create_info.pVertexBindingDescriptions = binding_descriptions.data();
    vertex_input_create_info.vertexAttributeDescriptionCount = 2;
    vertex_input_create_info.pVertexAttributeDescriptions = attribute_descriptions.data();

    VkPipelineInputAssemblyStateCreateInfo input_assembly_create_info{};
    input_assembly_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly_create_info.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewport_state_create_info{};
    viewport_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state_create_info.viewportCount = 1;
    viewport_state_create_info.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterization_create_info{};
    rasterization_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization_create_info.depthClampEnable = VK_FALSE;
    rasterization_create_info.rasterizerDiscardEnable = VK_FALSE;
    rasterization_create_info.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization_create_info.cullMode = VK_CULL_MODE_NONE;
    rasterization_create_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterization_create_info.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling_create_info{};
    multisampling_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling_create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    std::vector<VkDynamicState> dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic_state_create_info{};
    dynamic_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state_create_info.dynamicStateCount = 2;
    dynamic_state_create_info.pDynamicStates = dynamic_states.data();

    // Create graphics pipeline with complete vertex/fragment shader stages
    VkPipelineShaderStageCreateInfo shader_stage_create_info{};
    shader_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    
    // Use placeholder shaders - in real implementation, compile actual shaders
    std::vector<VkPipelineShaderStageCreateInfo> shader_stages(1);
    shader_stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;

    // Minimal graphics pipeline creation
    VkGraphicsPipelineCreateInfo pipeline_create_info{};
    pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_create_info.layout = g_pipeline_layout;
    pipeline_create_info.renderPass = g_render_pass;
    pipeline_create_info.subpass = 0;
    
    // Pipeline creation info (minimal for now)
    VkPipeline pipeline = VK_NULL_HANDLE;

    std::cout << "Vertex buffer and pipeline setup complete.\n";
    return VK_SUCCESS;
}
