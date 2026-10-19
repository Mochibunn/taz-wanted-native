VkResult create_graphics_pipeline() {
    if (!g_device || !g_render_pass) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Vertex shader compilation (simplified - using inline SPIR-V bytecode would be better)
    VkShaderModuleCreateInfo vertex_shader_module_create_info{};
    vertex_shader_module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vertex_shader_module_create_info.codeSize = 0;  // For now, we'll skip actual shader compilation

    std::vector<VkPipelineShaderStageCreateInfo> shader_stages;
    
    // Create minimal vertex and fragment stages
    VkPipelineShaderStageCreateInfo fragment_shader_stage{};
    fragment_shader_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragment_shader_stage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;

    std::vector<VkPipelineColorBlendAttachmentState> color_blend_attachment_states(1);
    color_blend_attachment_states[0] = VkPipelineColorBlendAttachmentState{
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | 
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        .blendEnable = VK_FALSE,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
    };

    VkPipelineDepthStencilStateCreateInfo depth_stencil_state{};
    depth_stencil_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil_state.depthTestEnable = VK_FALSE;
    depth_stencil_state.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo color_blend_state_create_info{};
    color_blend_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend_state_create_info.attachmentCount = 1;
    color_blend_state_create_info.pAttachments = &color_blend_attachment_states[0];

    VkPipelineRasterizationStateCreateInfo rasterization_state_create_info{};
    rasterization_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization_state_create_info.depthClampEnable = VK_FALSE;
    rasterization_state_create_info.rasterizerDiscardEnable = VK_FALSE;
    rasterization_state_create_info.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization_state_create_info.cullMode = VK_CULL_MODE_NONE;
    rasterization_state_create_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterization_state_create_info.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling_state_create_info{};
    multisampling_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling_state_create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Vertex input and assembly - use minimal descriptions
    VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info{};
    vertex_input_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo input_assembly_state_create_info{};
    input_assembly_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_state_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly_state_create_info.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportsStateCreateInfo viewport_state_create_info{};
    viewport_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORTS_STATE_CREATE_INFO;

    VkPipelineDynamicStateCreateInfo dynamic_state_create_info{};
    dynamic_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    std::vector<VkDynamicState> dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    dynamic_state_create_info.dynamicStateCount = 2;
    dynamic_state_create_info.pDynamicStates = dynamic_states.data();

    // Create graphics pipeline
    VkGraphicsPipelineCreateInfo graphics_pipeline_create_info{};
    graphics_pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    graphics_pipeline_create_info.layout = g_pipeline_layout;
    graphics_pipeline_create_info.renderPass = g_render_pass;
    graphics_pipeline_create_info.subpass = 0;

    // Set viewport and scissor - will be dynamic
    graphics_pipeline_create_info.viewports[0].width = WINDOW_WIDTH;
    graphics_pipeline_create_info.viewports[0].height = WINDOW_HEIGHT;
    graphics_pipeline_create_info.viewports[0] = (VkViewport){.x = 0, .y = 0};

    VkRect2D scissor_region;
    scissor_region.extent.width = WINDOW_WIDTH;
    scissor_region.extent.height = WINDOW_HEIGHT;
    scissor_region.offset = {0, 0};

    return VK_SUCCESS;
}

// ============================================================================
// COMMAND BUFFER CREATION AND RECORDING
// ============================================================================

VkResult create_command_buffer() {
    if (!g_command_pool) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    std::vector<VkCommandBufferAllocateInfo> command_buffers_allocation_info(g_swapchain_images.size());
    
    for (uint32_t i = 0; i < g_swapchain_images.size(); i++) {
        VkCommandBufferAllocateInfo allocate_info{};
        allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate_info.commandPool = g_command_pool;
        allocate_info.commandBufferCount = 1;

        result = vkAllocateCommandBuffers(g_device, &allocate_info, &g_command_buffers[i]);

        if (result != VK_SUCCESS) {
            std::cerr << "Failed to allocate command buffer: " << static_cast<int>(result) << "\n";
            return result;
        }
    }

    // Create synchronization resources
    for (uint32_t i = 0; i < g_swapchain_images.size(); i++) {
        VkSemaphoreCreateInfo semaphore_create_info{};
        semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        
        vkCreateSemaphore(g_device, &semaphore_create_info, nullptr, &g_image_available_semaphores[i]);
        vkCreateSemaphore(g_device, &semaphore_create_info, nullptr, &g_render_complete_semaphores[i]);

        VkFenceCreateInfo fence_create_info{};
        fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        
        vkCreateFence(g_device, &fence_create_info, nullptr, &g_fences[i]);
    }

    std::cout << "Command buffers and synchronization created.\n";
    return VK_SUCCESS;
}

// ============================================================================
// SWAPCHAIN CREATION
// ============================================================================

VkResult create_swapchain() {
    if (!g_vulkan_instance || !g_physical_device) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Get swapchain surface format and present mode
    uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceSupportedFormats(g_vulkan_instance, g_surface, &format_count, nullptr);

    std::vector<VkSurfaceFormatKHR> supported_formats(format_count);
    vkGetPhysicalDeviceSurfaceSupportedFormats(g_vulkan_instance, g_surface, &format_count, supported_formats.data());

    // Choose the first available format (usually BGRA8 or BGR8)
    if (supported_formats[0].format == VK_FORMAT_B8G8R8A8_UNORM) {
        g_swapchain_format.format = VK_FORMAT_B8G8R8A8_UNORM;
    } else if (supported_formats[0].format == VK_FORMAT_B8G8R8_UNORM) {
        g_swapchain_format.format = VK_FORMAT_B8G8R8_UNORM;
    } else {
        g_swapchain_format.format = supported_formats[0].format;
    }

    g_swapchain_format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

    uint32_t present_mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModes(g_vulkan_instance, g_surface, &present_mode_count, nullptr);

    std::vector<VkPresentModeKHR> supported_present_modes(present_mode_count);
    vkGetPhysicalDeviceSurfacePresentModes(g_vulkan_instance, g_surface, &present_mode_count, supported_present_modes.data());

    // Choose the first present mode (usually VK_PRESENT_MODE_FIFO_KHR)
    g_swapchain_present_mode = supported_present_modes[0];

    uint32_t transform_count = 0;
    vkGetPhysicalDeviceSurfaceTransformPropertiesKHR(g_vulkan_instance, g_surface, &transform_count, nullptr);

    std::vector<VkSurfaceTransformFlagBitsKHR> supported_transforms(transform_count);
    vkGetPhysicalDeviceSurfaceTransformPropertiesKHR(g_vulkan_instance, g_surface, &transform_count, supported_transforms.data());

    g_preferred_transform = supported_transforms[0];

    // Get swapchain capabilities
    VkSurfaceCapabilitiesKHR surface_capabilities{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_vulkan_instance, g_surface, &surface_capabilities);

    // Create swapchain creation info
    VkSwapchainCreateInfoKHR swapchain_create_info{};
    swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_create_info.surface = g_surface;
    swapchain_create_info.minImageCount = SWAPCHAIN_IMAGE_COUNT;
    swapchain_create_info.imageFormat = g_swapchain_format.format;
    swapchain_create_info.imageColorSpace = g_swapchain_format.colorSpace;
    swapchain_create_info.imageExtent = surface_capabilities.currentExtent;
    swapchain_create_info.imageArrayLayers = 1;
    swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_create_info.queueFamilyIndexCount = 0;
    swapchain_create_info.pQueueFamilyIndices = nullptr;
    swapchain_create_info.preTransform = g_preferred_transform;
    swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_create_info.presentMode = g_swapchain_present_mode;
    swapchain_create_info.clipped = VK_TRUE;
    swapchain_create_info.oldSwapchain = VK_NULL_HANDLE;

    result = vkCreateSwapchainKHR(g_device, &swapchain_create_info, nullptr, &g_swapchain);

    if (result != VK_SUCCESS || g_swapchain == VK_NULL_HANDLE) {
        std::cerr << "Failed to create swapchain: " << static_cast<int>(result) << "\n";
        return result;
    }

    // Get swapchain images info
    uint32_t image_count = 0;
    vkGetSwapchainImagesKHR(g_device, g_swapchain, &image_count, nullptr);

    std::vector<VkImage> swapchain_images(image_count);
    vkGetSwapchainImagesKHR(g_device, g_swapchain, &image_count, swapchain_images.data());

    g_swapchain_images.assign(swapchain_images.begin(), swapchain_images.end());

    // Create image views and framebuffers
    g_swapchain_image_views.resize(image_count);
    g_swapchain_framebuffers.resize(image_count);

    for (uint32_t i = 0; i < image_count; i++) {
        VkImageViewCreateInfo image_view_create_info{};
        image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        image_view_create_info.image = g_swapchain_images[i];
        image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        image_view_create_info.format = g_swapchain_format.format;
        image_view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_view_create_info.subresourceRange.baseMipLevel = 0;
        image_view_create_info.subresourceRange.levelCount = 1;
        image_view_create_info.subresourceRange.baseArrayLayer = 0;
        image_view_create_info.subresourceRange.layerCount = 1;

        result = vkCreateImageView(g_device, &image_view_create_info, nullptr, &g_swapchain_image_views[i]);

        if (result != VK_SUCCESS) {
            std::cerr << "Failed to create swapchain image view: " << static_cast<int>(result) << "\n";
            return result;
        }

        // Create framebuffer for each swapchain image
        VkFramebufferCreateInfo framebuffer_create_info{};
        framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_create_info.renderPass = g_render_pass;
        framebuffer_create_info.attachmentCount = 1;
        framebuffer_create_info.pAttachments = &g_swapchain_image_views[i];
        framebuffer_create_info.width = surface_capabilities.currentExtent.width;
        framebuffer_create_info.height = surface_capabilities.currentExtent.height;
        framebuffer_create_info.layers = 1;

        result = vkCreateFramebuffer(g_device, &framebuffer_create_info, nullptr, &g_swapchain_framebuffers[i]);

        if (result != VK_SUCCESS) {
            std::cerr << "Failed to create swapchain framebuffer: " << static_cast<int>(result) << "\n";
            return result;
        }
    }

    std::cout << "Swapchain created with " << image_count << " images.\n";
    return VK_SUCCESS;
}

// ============================================================================
// VERTEX BUFFER CREATION
// ============================================================================

VkResult create_vertex_buffer() {
    if (!g_device) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    size_t vertex_data_size = g_vertices.size() * sizeof(Vertex);

    // Create vertex buffer
    VkBufferCreateInfo buffer_create_info{};
    buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_create_info.size = vertex_data_size;
    buffer_create_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    result = vkCreateBuffer(g_device, &buffer_create_info, nullptr, &vertex_buffer);

    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create vertex buffer: " << static_cast<int>(result) << "\n";
        return result;
    }

    // Allocate vertex memory
    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(g_device, vertex_buffer, &memory_requirements);

    VkMemoryAllocateInfo memory_allocate_info{};
    memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory_allocate_info.allocationSize = memory_requirements.size;
    memory_allocate_info.memoryTypeIndex = get_memory_type(g_device, memory_requirements.memoryTypeBits, 
                                                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    result = vkAllocateMemory(g_device, &memory_allocate_info, nullptr, &vertex_memory);

    if (result != VK_SUCCESS) {
        std::cerr << "Failed to allocate vertex memory: " << static_cast<int>(result) << "\n";
        return result;
    }

    // Bind buffer to memory
    vkBindBufferMemory(g_device, vertex_buffer, vertex_memory, 0);

    // Upload vertex data
    void* data;
    vkMapMemory(g_device, vertex_memory, 0, vertex_data_size, 0, &data);
    
    // Copy vertices to GPU memory (would need vertex buffer structure here)
    memcpy(data, g_vertices.data(), vertex_data_size);

    vkUnmapMemory(g_device, vertex_memory);

    std::cout << "Vertex buffer created and uploaded.\n";
    return VK_SUCCESS;
}

// ============================================================================
// COMMAND BUFFER RECORDING FUNCTION
// ============================================================================

VkResult record_command_buffer(uint32_t index) {
    if (!g_command_pool || g_swapchain_images.size() == 0) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkCommandBuffer command_buffer = g_command_buffers[index];
    
    // Start recording command buffer
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    result = vkBeginCommandBuffer(command_buffer, &begin_info);

    if (result != VK_SUCCESS) {
        std::cerr << "Failed to begin recording command buffer: " << static_cast<int>(result) << "\n";
        return result;
    }

    // Render pass begin
    VkRenderPassBeginInfo render_pass_begin_info{};
    render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_begin_info.renderPass = g_render_pass;
    render_pass_begin_info.framebuffer = g_swapchain_framebuffers[index];
    render_pass_begin_info.renderArea.offset = {0, 0};
    render_pass_begin_info.renderArea.extent = {WINDOW_WIDTH, WINDOW_HEIGHT};

    // Record begin render pass
    vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENT_SECONDARY_COMMAND_BUFFER);

    // Clear color attachment
    VkClearValue clear_values[1];
    clear_values[0] = VkClearValue{{0.0f, 0.0f, 0.0f, 1.0f}};  // Dark blue background
    
    vkCmdSetColorAttachment(command_buffer, 0, &clear_value[0]);

    // End render pass
    vkCmdEndRenderPass(command_buffer);

    // Render end
    vkCmdEndCommandBuffer(command_buffer);

    std::cout << "Command buffer recorded for frame " << index << ".\n";
    return VK_SUCCESS;
}

// ============================================================================
// FRAME PRESENTATION FUNCTION
// ============================================================================

VkResult present_frame(uint32_t index) {
    if (!g_device || g_swapchain_images.size() == 0) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Acquire next swapchain image
    VkResult acquire_result = vkAcquireNextImageKHR(g_device, g_swapchain, UINT32_MAX, 
                                                     g_image_available_semaphores[index], VK_NULL_HANDLE, &index);

    if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR) {
        // Need to recreate swapchain due to surface resize
        std::cout << "Swapchain needs update - resurface resized.\n";
        recreate_swapchain();
    } else if (acquire_result == VK_SUBOPTIMAL_KHR) {
        // Swapchain may have changed, but still valid
        std::cout << "Swapchain is suboptimal - will present anyway.\n";
    }

    return acquire_result;
}
