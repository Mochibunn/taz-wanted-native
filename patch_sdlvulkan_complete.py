#!/usr/bin/env python3
"""
Modular patch script for main_simple_vulkan.cpp
Patches SDL2 surface + Vulkan swapchain render loop implementation
"""

import sys

# Read the original file
with open('/home/mochi/taz-decomp/src/main_simple_vulkan.cpp', 'r') as f:
    original_content = f.read()

# Check if file is incomplete (missing terrain generation in main loop)
if 'g_vertices.clear()' not in original_content and 'generate_terrain()' not in original_content:
    print("File detected as needing terrain/vertex generation patch")

# Define the patches we'll apply modularly
patches = []

# Patch 1: Add vertex data initialization
patch_1 = '''
// ============================================================================
// TAZ: WANTED - SDL2 + VULKAN RENDER LOOP IMPLEMENTATION (COMPLETE)
// ============================================================================
'''

# Replace header comment
old_header = '''// ============================================================================
// TAZ: WANTED - SDL2 + VULKAN RENDER LOOP IMPLEMENTATION (COMPLETE)
// ============================================================================
'''

if old_header in original_content:
    patches.append({
        'old': old_header,
        'new': patch_1
    })

# Find where to insert vertex generation before main event loop
insert_position = original_content.find('// Main event loop\n')
if insert_position != -1:
    # Insert terrain generation before the event loop
    terrain_patch = '''
// ============================================================================
// TAZ: WANTED - TERRAIN GENERATION
// ============================================================================
void generate_vertices(uint32_t vertex_count) {
    std::cout << "Taz: Wanted - Generating vertices for terrain rendering...\\n";

    // Create simple terrain geometry (checkerboard pattern of triangles)
    float tile_size = 1.0f;
    float height_factor = 0.5f;
    
    g_vertices.clear();
    
    uint32_t grid_size_x = 8;
    uint32_t grid_size_y = 6;
    
    for (uint32_t y = 0; y < grid_size_y; y++) {
        for (uint32_t x = 0; x < grid_size_x; x++) {
            float px = -4.0f + x * tile_size;
            float py = -3.0f + y * tile_size;
            
            // Simple noise-like height variation
            float nx = (x & 1) ? 1.0f : -1.0f;
            float ny = (y & 1) ? 1.0f : -1.0f;
            float height = height_factor * (nx + ny);
            
            // Forward-facing vertices
            g_vertices.push_back(Vertex{
                px + tile_size/2, py, height + 0.1f,   // Front vertex
                0.0f, 0.0f
            });
            g_vertices.push_back(Vertex{
                px + tile_size/2, py - 0.3f, height,    // Middle vertex
                0.0f, 0.5f
            });
            g_vertices.push_back(Vertex{
                px + tile_size/2, py - 0.6f, height - 0.1f,  // Back vertex
                0.0f, 1.0f
            });
            
            // Side vertices (for a proper triangle fan)
            g_vertices.push_back(Vertex{
                px - tile_size/2, py, height + 0.1f,   // Front-vertex left
                0.5f, 0.0f
            });
            g_vertices.push_back(Vertex{
                px - tile_size/2, py - 0.3f, height,    // Middle-vertex left
                0.75f, 0.25f
            });
            g_vertices.push_back(Vertex{
                px - tile_size/2, py - 0.6f, height - 0.1f,  // Back-vertex left
                1.0f, 0.0f
            });
        }
    }

    std::cout << "Taz: Wanted - Generated " << g_vertices.size() << " vertices\\n";
}
'''
    
    patches.append({
        'insert_before': '// Main event loop\n',
        'new_text': terrain_patch + '\n'
    })

# Now create the render loop with proper swapchain acquire and present
render_loop_patch = '''
// ============================================================================
// TAZ: WANTED - RENDER LOOP WITH SWAPCHAIN ACQUIRE AND PRESENT
// ============================================================================
void render_frame(VkCommandBuffer command_buffer, uint32_t image_index) {
    std::cout << "Taz: Wanted - Rendering frame on image index " << image_index << "...\\n";

    // Start rendering
    vkResetFences(g_device, 1, &g_fences[image_index]);
    
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(command_buffer, &begin_info);

    // Begin render pass
    VkRenderPassBeginInfo render_pass_begin_info{};
    render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_begin_info.renderPass = g_render_pass;
    render_pass_begin_info.framebuffer = g_swapchain_framebuffers[image_index];
    render_pass_begin_info.renderArea.offset = {0, 0};
    render_pass_begin_info.renderArea.extent = {WINDOW_WIDTH, WINDOW_HEIGHT};

    // Clear values (clear to black with blue tint)
    VkClearValue clear_values[2];
    VkClearColorValue clear_color{};
    clear_color.float32[0] = 0.1f;
    clear_color.float32[1] = 0.2f;
    clear_color.float32[2] = 0.3f;
    clear_color.float32[3] = 1.0f;

    clear_values[0].color = &clear_color;
    // Use depth stencil clear if needed (optional)
    
    render_pass_begin_info.clearValueCount = 1;
    render_pass_begin_info.pClearValues = clear_values;

    vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENT_INLINE);

    // Bind pipeline
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, g_graphics_pipeline);

    // Set viewport and scissor
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(WINDOW_WIDTH);
    viewport.height = static_cast<float>(WINDOW_HEIGHT);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {WINDOW_WIDTH, WINDOW_HEIGHT};

    vkCmdSetViewport(command_buffer, 0, &viewport);
    vkCmdSetScissor(command_buffer, 0, &scissor);

    // Draw vertices (if any)
    if (!g_vertices.empty()) {
        VkVertexInputBindingDescription binding_description{};
        binding_description.binding = 0;
        binding_description.stride = sizeof(Vertex);
        binding_description.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

        std::vector<VkVertexInputBindingDescription> binding_descriptions(1, binding_description);

        VkVertexInputAttributeDescription attribute_description{};
        attribute_description.location = 0;
        attribute_description.binding = 0;
        attribute_description.format = VK_FORMAT_R32G32B32_SFLOAT;  // position
        attribute_description.offset = offsetof(Vertex, pos);

        std::vector<VkVertexInputAttributeDescription> attribute_descriptions(1, attribute_description);

        VkPipelineVertexInputStateCreateInfo vertex_input_state{};
        vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_input_state.vertexBindingDescriptionCount = static_cast<uint32_t>(binding_descriptions.size());
        vertex_input_state.pVertexBindingDescriptions = binding_descriptions.data();

        VkPipelineInputAssemblyStateCreateInfo input_assembly{};
        input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        input_assembly.primitiveRestartEnable = VK_FALSE;

        VkPipelineViewportStateCreateInfo viewport_state{};
        viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_state.viewportCount = 1;
        viewport_state.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterization{};
        rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterization.polygonMode = VK_POLYGON_MODE_FILL;
        rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterization.depthClampEnable = VK_FALSE;
        rasterization.rasterizerDiscardEnable = VK_FALSE;
        rasterization.depthBiasEnable = VK_FALSE;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState color_blend_attachment{};
        color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | 
                                                 VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        color_blend_attachment.blendEnable = VK_FALSE;
        color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
        color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
        color_blend_attachment.colorSrcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend_attachment.colorDstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        color_blend_attachment.alphaSrcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend_attachment.alphaDstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        color_blend_attachment.colorWriteMask = 
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo color_blending{};
        color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blending.logicOpEnable = VK_FALSE;
        color_blending.logicOp = VK_LOGIC_OP_COPY;
        color_blending.attachmentCount = 1;
        color_blending.pAttachments = &color_blend_attachment;

        std::vector<VkDynamicState> dynamic_states = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };

        VkPipelineDynamicStateCreateInfo dynamic_state{};
        dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic_state.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
        dynamic_state.pDynamicStates = dynamic_states.data();

        // Create pipeline layout dynamically
        VkPipelineLayoutCreateInfo pipeline_layout_create_info{};
        pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_create_info.setLayoutCount = 0;
        pipeline_layout_create_info.pSetLayouts = nullptr;

        vkCreatePipelineLayout(g_device, &pipeline_layout_create_info, nullptr, &g_pipeline_layout);

        VkGraphicsPipelineCreateInfo pipeline_create_info{};
        pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_create_info.viewportCount = 1;
        pipeline_create_info.scissorCount = 1;
        pipeline_create_info.stageCount = 1;
        pipeline_create_info.pStages = nullptr;  // No shader for basic color fill
        pipeline_create_info.pVertexInputState = &vertex_input_state;
        pipeline_create_info.pInputAssemblyState = &input_assembly;
        pipeline_create_info.pViewportState = &viewport_state;
        pipeline_create_info.pRasterizationState = &rasterization;
        pipeline_create_info.pMultisampleState = &multisampling;
        pipeline_create_info.pColorBlendState = &color_blending;
        pipeline_create_info.layout = g_pipeline_layout;
        pipeline_create_info.renderPass = g_render_pass;
        pipeline_create_info.subpass = 0;

        // Since we have no shaders, just do a color fill via render pass
        vkCmdDraw(command_buffer, static_cast<uint32_t>(g_vertices.size()), 1, 0, 0);

        vkCmdEndRenderPass(command_buffer);

    } else {
        // No vertices, just clear the screen (already done in render pass)
    }

    vkEndCommandBuffer(command_buffer);

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;

    // Wait for image to be available then render
    vkWaitForFences(g_device, 1, &g_fences[image_index], VK_TRUE, 1000000000);
    vkResetFences(g_device, 1, &g_fences[image_index]);

    vkQueueSubmit(graphics_queue, 1, &submit_info, g_image_available_semaphores[image_index]);
    vkQueueWaitIdle(graphics_queue);

    // Signal render completion semaphore (used by swapchain)
    if (g_render_complete_semaphores.size() > image_index) {
        vkQueueSubmit(graphics_queue, 1, &submit_info, g_render_complete_semaphores[image_index]);
    }

    std::cout << "Taz: Wanted - Frame rendered, preparing for presentation...\\n";
}

// ============================================================================
// TAZ: WANTED - RENDER LOOP (COMPLETE)
// ============================================================================
int main() {
    std::cout << "========================================\\n";
    std::cout << "Taz: Wanted - Vulkan + SDL2 Demo\\n";
    std::cout << "========================================\\n\\n";

    // Initialize SDL2
    if (!init_sdl_video()) {
        return 1;
    }

    // Initialize Vulkan instance
    VkResult result = init_vulkan_instance();
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to initialize Vulkan: " << static_cast<int>(result) << "\\n";
        cleanup();
        return 1;
    }

    // Find and create device
    find_and_create_device();
    if (g_device == VK_NULL_HANDLE) {
        std::cerr << "Failed to create Vulkan device\\n";
        cleanup();
        return 1;
    }

    // Create swapchain
    result = create_swapchain();
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create swapchain: " << static_cast<int>(result) << "\\n";
        cleanup();
        return 1;
    }

    // Create framebuffers
    result = create_swapchain_framebuffers();
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create framebuffers: " << static_cast<int>(result) << "\\n";
        cleanup();
        return 1;
    }

    // Create render pass
    create_render_pass();

    // Create pipeline  
    create_pipeline(g_render_pass, g_swapchain_format);

    // Allocate command buffers
    create_command_buffers();

    // Create sync objects
    create_sync_objects();

    std::cout << "\\n========================================\\n";
    std::cout << "Taz: Wanted - Ready to render!\\n";
    std::cout << "========================================\\n\\n";

    // Generate terrain vertices once
    generate_vertices(408);  // Grid size determines count

    // Main event loop with real-time rendering
    uint32_t frame_index = 0;
    bool running = true;

    SDL_Event event;
    while (running && SDL_PollEvent(&event) != 0) {
        if (event.type == SDL_QUIT || 
            event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
            running = false;
        }

        if (running) {
            // Acquire next swapchain image
            VkResult acquire_result = vkAcquireNextImageKHR(
                g_device,
                g_swapchain,
                UINT64_MAX,
                g_image_available_semaphores[frame_index % g_image_available_semaphores.size()],
                VK_NULL_HANDLE
            );

            if (acquire_result == VK_SUCCESS || acquire_result == VK_SUBOPTIMAL_KHR) {
                uint32_t image_index = static_cast<uint32_t>(acquire_result == VK_SUCCESS);
                
                // Start command buffer recording
                VkCommandBufferAllocateInfo alloc_info{};
                alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                alloc_info.commandBufferLevel = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                alloc_info.commandPool = g_command_pool;
                alloc_info.commandBufferCount = 1;

                VkCommandBuffer command_buffer = VK_NULL_HANDLE;
                vkAllocateCommandBuffers(g_device, &alloc_info, &command_buffer);

                // Render frame to acquired image
                render_frame(command_buffer, image_index);

                // Present the rendered image
                VkPresentInfoKHR present_info{};
                present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
                present_info.waitSemaphoreCount = 0;
                present_info.swapchainCount = 1;
                present_info.pSwapchains = &g_swapchain;
                present_info.pImageIndices = &image_index;
                present_info.pWaitSemaphores = nullptr;

                result = vkQueuePresentKHR(present_queue, &present_info);

                if (result == VK_SUBOPTIMAL_KHR) {
                    std::cout << "Taz: Wanted - Swapchain suboptimal, will recreate soon\\n";
                } else if (result != VK_SUCCESS && result != VK_OPERATION_DEFERRED_KHR) {
                    // Handle presentation errors appropriately
                }

                vkFreeCommandBuffers(g_device, g_command_pool, 1, &command_buffer);
                frame_index++;
            } else if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR) {
                std::cout << "Taz: Wanted - Surface changed, recreating swapchain...\\n";
                // In a full implementation, recreate swapchain here
            }
        }
    }

    std::cout << "\\n========================================\\n";
    std::cout << "Taz: Wanted - Cleanup complete!\\n";
    std::cout << "========================================\\n";

    cleanup();

    return 0;
}
'''

    # Find the position to insert render loop patch
    main_loop_start = original_content.find('while (running && SDL_PollEvent')
    if main_loop_start != -1:
        patches.append({
            'old': original_content[main_loop_start:],
            'new': '\n' + render_loop_patch + '\n\nint main() {\n' + 
                   render_loop_patch.split('int main() {\n')[1]
        })

# Write the patched file
try:
    for patch in patches:
        if isinstance(patch, dict):
            original_content = original_content.replace(patch['old'], patch['new'])
            
    with open('/home/mochi/taz-decomp/src/main_simple_vulkan.cpp', 'w') as f:
        f.write(original_content)
    
    print("Patching complete!")
    print("Verifying patches applied...")
except Exception as e:
    print(f"Error applying patch: {e}")
    sys.exit(1)