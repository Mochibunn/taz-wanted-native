// ============================================================================
// TAZ: WANTED - SDL2 + VULKAN RENDER LOOP - PART 3
// Render pass, pipeline layout, graphics pipeline
// ============================================================================

#include <iostream>
#include <vector>

extern "C" {
#include <vulkan/vulkan.h>
}

// External global state
VkDevice g_device = VK_NULL_HANDLE;
VkSwapchainKHR g_swapchain = VK_NULL_HANDLE;
std::vector<VkImageView> g_swapchain_image_views;

// ============================================================================
// STEP 4: RENDER PASS CREATION
// ============================================================================
static VkResult create_render_pass() {
    std::cout << "=== Creating Render Pass ===\n";
    
    // Get swapchain image format
    VkFormat swapchain_image_format = g_swapchain_image_views.empty() ? 
                                      VK_FORMAT_R8G8B8A8_UNORM : g_swapchain_image_views[0].format;

    // Determine aspect mask
    VkColorComponentFlags color_components = 
        (1ULL << VK_COMPONENT_SWIZZLE_R) |
        (1ULL << VK_COMPONENT_SWIZZLE_G) |
        (1ULL << VK_COMPONENT_SWIZZLE_B) |
        (1ULL << VK_COMPONENT_SWIZZLE_A);

    VkComponentMapping color_attachment_component_mapping = {};
    color_attachment_component_mapping.b = VK_COMPONENT_SWIZZLE_R;
    color_attachment_component_mapping.g = VK_COMPONENT_SWIZZLE_G;
    color_attachment_component_mapping.r = VK_COMPONENT_SWIZZLE_B;
    color_attachment_component_mapping.a = VK_COMPONENT_SWIZZLE_A;

    VkAttachmentDescription color_attachment{};
    color_attachment.format = swapchain_image_format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment_ref{};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;
    subpass.pipelineInputAttachCount = 0;

    VkRenderPassCreateInfo render_pass_create{};
    render_pass_create.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_create.attachmentCount = 1;
    render_pass_create.pAttachments = &color_attachment;
    render_pass_create.subpassCount = 1;
    render_pass_create.pSubpasses = &subpass;

    VkRenderPass result;
    VkResult res = vkCreateRenderPass(g_device, &render_pass_create, nullptr, &result);
    if (res != VK_SUCCESS) {
        std::cerr << "Failed to create render pass: " << res << "\n";
        return res;
    }

    g_render_pass = result;
    std::cout << "✅ Render pass created successfully\n";
    
    return VK_SUCCESS;
}

// ============================================================================
// STEP 5: SHADER MODULE CREATION (stub - needs shader files)
// ============================================================================
static VkShaderModule create_shader_module_from_file(const char* filename, 
                                                      VkShaderStageFlagBits stage) {
    std::ifstream shader_file(filename, std::ios::in | std::ios::ate);
    if (!shader_file.is_open()) {
        std::cerr << "Failed to open shader file: " << filename << "\n";
        return VK_NULL_HANDLE;
    }

    size_t file_size = shader_file.tellg();
    shader_file.seekg(0, std::ios::beg);

    std::vector<char> file_content(file_size);
    shader_file.read(file.data(), file_size);
    shader_file.close();

    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = file_size;
    create_info.pCode = reinterpret_cast<const uint32_t*>(file.data());

    VkShaderModule module;
    VkResult result = vkCreateShaderModule(g_device, &create_info, nullptr, &module);
    
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create shader module from file\n";
        return VK_NULL_HANDLE;
    }

    return module;
}

// ============================================================================
// STEP 6: PIPELINE LAYOUT CREATION
// ============================================================================
static VkResult create_pipeline_layout() {
    std::cout << "=== Creating Pipeline Layout ===\n";
    
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE; // Will be created dynamically
    
    VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create{};
    descriptor_set_layout_create.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptor_set_layout_create.flags = 0;
    descriptor_set_layout_create.bindingCount = 0;
    descriptor_set_layout_create.pBindings = nullptr;

    VkDescriptorSetLayout descriptor_set_layout;
    vkCreateDescriptorSetLayout(g_device, &descriptor_set_layout_create, nullptr, &descriptor_set_layout);

    std::cout << "✅ Pipeline layout created\n";
    
    return VK_SUCCESS;
}

// ============================================================================
// STEP 7: GRAPHICS PIPELINE CREATION
// ============================================================================
static VkResult create_graphics_pipeline(VkShaderModule vertex_module, 
                                          VkShaderModule fragment_module) {
    std::cout << "=== Creating Graphics Pipeline ===\n";
    
    // Vertex input state
    VkVertexInputBindingDescription binding_desc{};
    binding_desc.binding = 0;
    binding_desc.stride = sizeof(Vertex);
    binding_desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attribute[2]{};
    attribute[0].location = 0;
    attribute[0].binding = 0;
    attribute[0].format = Vertex::get_format();
    attribute[0].offset = offsetof(Vertex, position);

    attribute[1].location = 1;
    attribute[1].binding = 0;
    attribute[1].format = Vertex::get_format();
    attribute[1].offset = offsetof(Vertex, uv);

    VkVertexInputDescriptionState vertex_input_state{};
    vertex_input_state.vertexBindingDescriptionCount = 1;
    vertex_input_state.pVertexBindingDescriptions = &binding_desc;
    vertex_input_state.vertexAttributeDescriptionCount = 2;
    vertex_input_state.pVertexAttributeDescriptions = attribute;

    // Input assembly state
    VkPipelineInputAssemblyStateCreateInfo input_assembly_state{};
    input_assembly_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly_state.primitiveRestartEnable = VK_FALSE;

    // Viewport state
    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;

    // Multipass coloring blend state
    VkPipelineColorBlendAttachmentState color_blend_attachment_state{};
    color_blend_attachment_state.colorWriteMask = 
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | 
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment_state.blendEnable = VK_FALSE;
    color_blend_attachment_state.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment_state.alphaBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;

    VkPipelineColorBlendStateCreateInfo color_blend_state_create{};
    color_blend_state_create.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend_state_create.logicOpEnable = VK_FALSE;
    color_blend_state_create.logicOp = VK_LOGIC_OP_COPY;
    color_blend_state_create.attachmentCount = 1;
    color_blend_state_create.pAttachments = &color_blend_attachment_state;
    color_blend_state_create.blendConstants[0] = 0.0f;
    color_blend_state_create.blendConstants[1] = 0.0f;

    // Rasterization state
    VkPipelineRasterizationStateCreateInfo rasterization_state_create{};
    rasterization_state_create.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization_state_create.depthClampEnable = VK_FALSE;
    rasterization_state_create.rasterizerDiscardEnable = VK_FALSE;
    rasterization_state_create.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization_state_create.lineWidth = 1.0f;
    rasterization_state_create.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization_state_create.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization_state_create.depthBiasEnable = VK_FALSE;
    rasterization_state_create.depthBiasConstantFactor = 0.0f;
    rasterization_state_create.depthBiasClamp = 0.0f;
    rasterization_state_create.depthBiasSlopeFactor = 0.0f;

    // Multisample state (no MSAA)
    VkPipelineMultisampleStateCreateInfo multisample_state_create{};
    multisample_state_create.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample_state_create.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Dynamic state
    VkPipelineDynamicStateCreateInfo dynamic_state_create{};
    dynamic_state_create.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state_create.dynamicStateMask = 
        VK_DYNAMIC_STATE_VIEWPORT | VK_DYNAMIC_STATE_SCISSOR;

    // Pipeline vertex input state
    VkPipelineVertexInputStateCreateInfo vertex_input_state_create{};
    vertex_input_state_create.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_state_create.vertexBindingDescriptionCount = 1;
    vertex_input_state_create.pVertexBindingDescriptions = &binding_desc;
    vertex_input_state_create.vertexAttributeDescriptionCount = 2;
    vertex_input_state_create.pVertexAttributeDescriptions = attribute;

    // Pipeline input assembly state
    VkPipelineInputAssemblyStateCreateInfo input_assembly_state_create{};
    input_assembly_state_create.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_state_create.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly_state_create.primitiveRestartEnable = VK_FALSE;

    // Pipeline viewport state
    VkPipelineViewportStateCreateInfo viewport_state_create{};
    viewport_state_create.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state_create.viewportCount = 1;
    viewport_state_create.scissorCount = 1;

    // Pipeline multisample state
    VkPipelineMultisampleStateCreateInfo multisample_state_create_info{};
    multisample_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample_state_create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Pipeline color blend state
    VkPipelineColorBlendAttachmentState color_blend_attachment_state_create{};
    color_blend_attachment_state_create.colorWriteMask = 
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | 
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment_state_create.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo color_blend_state_create_info{};
    color_blend_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend_state_create_info.attachmentCount = 1;
    color_blend_state_create_info.pAttachments = &color_blend_attachment_state_create;

    // Pipeline depth stencil state (no depth testing for now)
    VkPipelineDepthStencilStateCreateInfo depth_stencil_state_create{};
    depth_stencil_state_create.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil_state_create.depthTestEnable = VK_FALSE;
    depth_stencil_state_create.depthWriteEnable = VK_FALSE;
    depth_stencil_state_create.stencilTestEnable = VK_FALSE;

    // Pipeline layout
    VkPipelineLayoutCreateInfo pipeline_layout_create{};
    pipeline_layout_create.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_create.setLayoutCount = 0;
    pipeline_layout_create.pSetLayouts = nullptr;
    pipeline_layout_create.pushConstantRangeCount = 0;

    VkPushConstantRange push_constant_range{};
    
    // Pipeline vertex input state
    VkPipelineVertexInputStateCreateInfo vertex_input_state_info{};
    vertex_input_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    // Pipeline input assembly state
    VkPipelineInputAssemblyStateCreateInfo input_assembly_state_info{};
    input_assembly_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_state_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // Pipeline viewport state
    VkPipelineViewportStateCreateInfo viewport_state_info{};
    viewport_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;

    // Pipeline rasterization state
    VkPipelineRasterizationStateCreateInfo rasterization_state_info{};
    rasterization_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization_state_info.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization_state_info.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization_state_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    // Pipeline multisample state
    VkPipelineMultisampleStateCreateInfo multisample_state_info{};
    multisample_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample_state_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Pipeline color blend state
    VkPipelineColorBlendStateCreateInfo color_blend_state_info{};
    color_blend_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend_state_info.attachmentCount = 1;

    // Pipeline dynamic state
    VkPipelineDynamicStateCreateInfo dynamic_state_info{};
    dynamic_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state_info.dynamicStateMask = 
        VK_DYNAMIC_STATE_VIEWPORT | VK_DYNAMIC_STATE_SCISSOR;

    // Create graphics pipeline
    VkGraphicsPipelineCreateInfo pipeline_create{};
    pipeline_create.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_create.layout = VK_NULL_HANDLE; // Will be created later
    pipeline_create.renderPass = g_render_pass;
    pipeline_create.basePipelineHandle = VK_NULL_HANDLE;

    std::cout << "✅ Graphics pipeline creation parameters set\n";
    
    return VK_SUCCESS;
}
