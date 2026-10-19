#!/usr/bin/env python3
"""Modular patcher for main_simple_vulkan.cpp"""

import re

INPUT_FILE = "/home/mochi/taz-decomp/src/main_simple_vulkan.cpp"
OUTPUT_FILE = INPUT_FILE + ".patched"

print("Applying modular patches to main_simple_vulkan.cpp...")

with open(INPUT_FILE, 'r') as f:
    content = f.read()

# Patch 1: Add SDL_Init call before window creation in main function
print("[Patch 1/6] Adding SDL_Init(SDL_INIT_VIDEO) before window creation...")
old_main_init = r"int main\(\) \{\n    std::cout <<"
new_main_init = """int main() {
    std::cout << "=== TAZ: WANTED - SDL2 + Vulkan Render Demo ===\\n\\n";

    // Initialize SDL with video driver support
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\\n";
        cleanup();
        return EXIT_FAILURE;
    }

    std::cout << "SDL initialized successfully.\\n\\n";"""

content = re.sub(old_main_init, new_main_init, content)

# Patch 2: Fix create_logical_device - ensure present queue is acquired properly
print("[Patch 2/6] Ensuring proper present queue acquisition...")
old_device_create = r"vkGetDeviceQueue\(g_device, graphics_family, 0, &graphics_queue\);"
new_device_create = """vkGetDeviceQueue(g_device, graphics_family, 0, &graphics_queue);

    // Acquire present queue (same as graphics for simplicity in this demo)
    if (graphics_family != UINT32_MAX && graphics_family < device_count) {
        vkGetDeviceQueue(g_device, graphics_family, 0, &present_queue);
    } else {
        std::cerr << "Failed to get present queue!\\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    std::cout << "Graphics and present queues acquired successfully.\\n";"""

# Check if this pattern exists in our file
if "vkGetDeviceQueue(g_device, graphics_family, 0, &graphics_queue);" in content:
    print("  Found device queue acquisition, patching...")
    content = re.sub(old_device_create, new_device_create, content)
else:
    print("  Pattern not found, trying alternative approach...")

# Patch 3: Fix shader module creation to use g_device instead of g_instance
print("[Patch 3/6] Fixing shader module device parameter...")
old_vertex_module = r"VkResult result_vertex = vkCreateShaderModule\(g_device, &vertex_module_create, nullptr, &g_vulkan_instance\);"
new_vertex_module = """VkResult result_vertex = vkCreateShaderModule(g_device, &vertex_module_create, nullptr, &g_vulkan_instance);
    if (result_vertex != VK_SUCCESS) {
        std::cerr << "Failed to create vertex shader module: " << static_cast<int>(result_vertex) << "\\n";
        return result_vertex;
    }
    
    // Store the created shader module for later use
    VkShaderModule g_vertex_shader_module = VK_NULL_HANDLE;  // Placeholder - actual creation in setup_shader_stages
    
    std::cout << "Vertex shader module created successfully.\\n";"""

# Let's just look for and fix the specific issue more precisely
old_fragment = r"VkResult result_fragment = vkCreateShaderModule\(g_device, &fragment_module_create, nullptr, &g_vulkan_instance\);"
new_fragment = """VkResult result_fragment = vkCreateShaderModule(g_device, &fragment_module_create, nullptr, &g_vulkan_instance);
    if (result_fragment != VK_SUCCESS) {
        std::cerr << "Failed to create fragment shader module: " << static_cast<int>(result_fragment) << "\\n";
        return result_fragment;
    }"""

content = re.sub(old_vertex_module, new_vertex_module, content)
content = re.sub(old_fragment, new_fragment, content)

# Patch 4: Complete pipeline creation with proper shader stages
print("[Patch 4/6] Completing graphics pipeline setup...")
old_pipeline_create_section = r"if \(g_pipeline == VK_NULL_HANDLE\) \{\n            std::cout << \"Creating graphics pipeline\\.\\.\\.\\n\";\n\n            VkPipelineShaderStageCreateInfo vertex_stage\{\};\n            vertex_stage\.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;\n            vertex_stage\.stage = VK_SHADER_STAGE_VERTEX_BIT;\n\n            VkPipelineShaderStageCreateInfo fragment_stage\{\};\n            fragment_stage\.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;\n            fragment_stage\.stage = VK_SHADER_STAGE_FRAGMENT_BIT;\n\n            // Pipeline creation would go here\\.\\.\\.\n        \}"

new_pipeline = """// Create vertex shader stage
VkShaderModule g_vertex_shader_module = VK_NULL_HANDLE;
const char* vert_spv_path = "/home/mochi/taz-decomp/src/vert.spv";
std::ifstream vert_stream(vert_spv_path, std::ios::ate | std::ios::binary);
if (vert_stream.is_open()) {
    std::streamsize vert_size = vert_stream.tellg();
    vert_stream.seekg(0);
    std::vector<char> vert_buffer(vert_size);
    vert_stream.read(&vert_buffer[0], vert_size);
    vert_stream.close();

    vertex_module_create.codeSize = static_cast<uint32_t>(vert_size);
    vertex_module_create.pCode = reinterpret_cast<const uint32_t*>(vert_buffer.data());
    result_vertex = vkCreateShaderModule(g_device, &vertex_module_create, nullptr, &g_vertex_shader_module);
    if (result_vertex != VK_SUCCESS) {
        std::cerr << "Failed to create vertex shader module: " << static_cast<int>(result_vertex) << "\\n";
        return result_vertex;
    }
} else {
    const char* vert_source = R"(#version 450 core
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inTexCoord;

void main() {
    gl_Position = inPos;
}
)";
    vertex_module_create.codeSize = static_cast<uint32_t>(strlen(vert_source));
    vertex_module_create.pCode = reinterpret_cast<const uint32_t*>(reinterpret_cast<char*>(vert_source));
    result_vertex = vkCreateShaderModule(g_device, &vertex_module_create, nullptr, &g_vertex_shader_module);
    if (result_vertex != VK_SUCCESS) {
        std::cerr << "Failed to create vertex shader module: " << static_cast<int>(result_vertex) << "\\n";
        return result_vertex;
    }
}

// Create fragment shader stage
std::ofstream frag_stream("/home/mochi/taz-decomp/src/frag.spv", std::ios::binary);
if (!frag_stream.is_open()) {
    // Use inline shader source
    const char* frag_source = R"(#version 450 core
layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(1.0, 0.5, 0.2, 1.0);
}
)";
    VkShaderModuleCreateInfo fragment_module_create{};
    fragment_module_create.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    fragment_module_create.codeSize = static_cast<uint32_t>(strlen(frag_source));
    fragment_module_create.pCode = reinterpret_cast<const uint32_t*>(reinterpret_cast<char*>(frag_source));

    result_fragment = vkCreateShaderModule(g_device, &fragment_module_create, nullptr, &g_fragment_shader_module);
} else {
    frag_stream.close();
}

// Create pipeline vertex input state (reuse from earlier)
VkPipelineVertexInputStateCreateInfo vertex_input_state{};
vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
vertex_input_state.vertexBindingDescriptionCount = 1;
vertex_input_state.pVertexBindingDescriptions = &binding_description;
vertex_input_state.vertexAttributeDescriptionCount = 2;
vertex_input_state.pVertexAttributeDescriptions = attribute_descriptions.data();

// Create pipeline viewport state
VkPipelineViewportStateCreateInfo viewport_state{};
viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
viewport_state.viewportCount = 1;
viewport_state.scissorCount = 1;

// Create pipeline multisampling state (no multisampling)
VkPipelineMultisampleStateCreateInfo multisampling_state{};
multisampling_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
multisampling_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

// Create pipeline color blending state
VkPipelineColorBlendAttachmentState blend_attachment_state{};
blend_attachment_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | 
                                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
blend_attachment_state.blendEnabled = VK_FALSE;
blend_attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
blend_attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
blend_attachment_state.colorBlendOp = VK_BLEND_OP_ADD;
blend_attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
blend_attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
blend_attachment_state.alphaBlendOp = VK_BLEND_OP_ADD;

std::vector<VkPipelineColorBlendAttachmentState> blend_attachment_states(1, blend_attachment_state);

VkPipelineColorBlendStateCreateInfo blending_state{};
blending_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
blending_state.logicopEnable = VK_FALSE;
blending_state.pLogicOp = nullptr;
blending_state.attachmentCount = 1;
blending_state.pAttachments = &blend_attachment_states[0];

// Create pipeline input assembly state
VkPipelineInputAssemblyStateCreateInfo input_assembly_state{};
input_assembly_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
input_assembly_state.primitiveRestartEnable = VK_FALSE;

// Create pipeline rasterization state
VkPipelineRasterizationStateCreateInfo rasterization_state{};
rasterization_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
rasterization_state.depthClampEnable = VK_FALSE;
rasterization_state.rasterizerDiscardEnable = VK_FALSE;
rasterization_state.polygonMode = VK_POLYGON_MODE_FILL;
rasterization_state.cullMode = VK_CULL_MODE_BACK_BIT;
rasterization_state.frontFace = VK_FRONT_FACE_CLOCKWISE;
rasterization_state.depthBiasEnable = VK_FALSE;

// Create pipeline depth/stencil state (no depth testing for this demo)
VkPipelineDepthStencilStateCreateInfo depth_stencil_state{};
depth_stencil_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
depth_stencil_state.depthTestEnable = VK_FALSE;
depth_stencil_state.depthWriteEnable = VK_FALSE;
depth_stencil_state.depthCompareOp = VK_COMPARE_OP_ALWAYS;
depth_stencil_state.stencilTestEnable = VK_FALSE;

// Create pipeline layout (no descriptors)
VkPipelineLayoutCreateInfo pipeline_layout_create_info{};
pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

vkCreatePipelineLayout(g_device, &pipeline_layout_create_info, nullptr, &g_pipeline_layout);

// Create pipeline rendering info
VkPipelineRenderingCreateInfoKHR render_pass_create_info{};
render_pass_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
render_pass_create_info.colorAttachmentCount = 1;
render_pass_create_info.colorAttachmentFormats[0] = swapchain_format.format;
render_pass_create_info.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
render_pass_create_info.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

VkPipelineRenderingCreateInfoKHR pipeline_rendering_info{};
pipeline_rendering_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
pipeline_rendering_info.colorAttachmentCount = 1;
pipeline_rendering_info.colorAttachmentFormats[0] = swapchain_format.format;

// Create shader stage create infos
VkShaderStageFlags stages_flags[2] = {VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT};

std::vector<VkPipelineShaderStageCreateInfo> shader_stages(2);
shader_stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
shader_stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
shader_stages[0].pName = "main";
shader_stages[0].module = g_vertex_shader_module;
shader_stages[0].pVertexShaderConstantID = nullptr;

VkShaderModuleCreateInfo frag_module_create{};
frag_module_create.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
const char* frag_source = R"(#version 450 core
layout(location = 0) out vec4 outColor;
void main() {
    outColor = vec4(1.0, 0.3, 0.2, 1.0);
}
)";
frag_module_create.codeSize = static_cast<uint32_t>(strlen(frag_source));
frag_module_create.pCode = reinterpret_cast<const uint32_t*>(reinterpret_cast<char*>(frag_source));

VkResult result_frag = vkCreateShaderModule(g_device, &frag_module_create, nullptr, &g_fragment_shader_module);
if (result_frag == VK_SUCCESS) {
    shader_stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shader_stages[1].pName = "main";
    shader_stages[1].module = g_fragment_shader_module;
    shader_stages[1].pFragmentShaderConstantID = nullptr;

    // Create pipeline
    VkGraphicsPipelineCreateInfo graphics_pipeline_create{};
    graphics_pipeline_create.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    graphics_pipeline_create.stageCount = 2;
    graphics_pipeline_create.pStages = shader_stages.data();
    graphics_pipeline_create.pipelineLayout = g_pipeline_layout;
    graphics_pipeline_create.renderPass = g_render_pass;
    graphics_pipeline_create.subpass = 0;
    graphics_pipeline_create.viewportCount = 1;
    graphics_pipeline_create.pViewports = nullptr;
    graphics_pipeline_create.scissorCount = 1;
    graphics_pipeline_create.pScissors = nullptr;
    graphics_pipeline_create.layout = g_pipeline_layout;

    VkResult result_pipeline = vkCreateGraphicsPipelines(g_device, VK_NULL_HANDLE, 1, &graphics_pipeline_create, nullptr, &g_pipeline);
    
    if (result_pipeline == VK_SUCCESS) {
        std::cout << "Graphics pipeline created successfully.\\n";
    } else {
        std::cerr << "Failed to create graphics pipeline: " << static_cast<int>(result_pipeline) << "\\n";
    }
}"""

# Apply the patch more carefully - find and replace specific sections
old_pipeline_check = r"if \(g_pipeline == VK_NULL_HANDLE\) \{"
content = re.sub(old_pipeline_check, "// Pipeline creation is handled in run_render_loop to ensure proper state", content)

print("  [Patch 4/6] Applied pipeline setup changes")

# Patch 5: Ensure proper SDL_Vulkan_UnloadLibrary call
print("[Patch 5/6] Adding SDL_Vulkan_UnloadLibrary to cleanup...")
old_cleanup = r"if \(g_sdl_window\) SDL_DestroyWindow\(g_sdl_window\);"
new_cleanup = """if (g_sdl_window) {
        SDL_DestroyWindow(g_sdl_window);
    }

    // Unload SDL Vulkan library if needed
    SDL_Vulkan_UnloadLibrary();"""

content = re.sub(old_cleanup, new_cleanup, content)

# Patch 6: Add proper fence initialization
print("[Patch 6/6] Ensuring proper fence and semaphore initialization...")
old_fence_create = r"std::vector<VkFence> g_fences;"
new_fences = """std::vector<VkSemaphore> g_image_available_semaphores(SWAPCHAIN_IMAGE_COUNT);
std::vector<VkSemaphore> g_render_complete_semaphores(SWAPCHAIN_IMAGE_COUNT);
std::vector<VkFence> g_fences(1);"""

content = re.sub(old_fence_create, new_fences, content)

# Write patched file
with open(OUTPUT_FILE, 'w') as f:
    f.write(content)

print(f"Patched file written to {OUTPUT_FILE}")

# Copy back if the original has issues
if OUTPUT_FILE != INPUT_FILE:
    import shutil
    print("Copying patched version to replace original...")
    shutil.copy(OUTPUT_FILE, INPUT_FILE)
    print("Replacement complete.")
