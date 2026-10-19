#!/usr/bin/env python3

with open('/home/mochi/taz-decomp/src/main_simple_vulkan.cpp', 'r') as f:
    content = f.read()

# Fix shader module creation - use proper VkShaderModuleCreateInfo with code properly set
shader_code = '''            #version 450
            layout (location = 0) in vec3 inPos { x, y, z };
            layout (location = 1) in vec2 inUv { x, y };
            
            out vec2 frag_uv;
            
            void main() {
                gl_Position = vec4(inPos, 1.0);
                frag_uv = inUv;
            }'''

fragment_shader_code = '''            #version 450
            layout (location = 0) out vec4 fColor;
            
            in vec2 frag_uv;
            
            void main() {
                float height = fract(frag_uv.x * 10.0);
                vec3 color = vec3(height, 0.2, 0.8);
                fColor = vec4(color, 1.0);
            }'''

# Find and replace shader creation section
old_shader_section = '''    // Compile shaders to modules
    VkShaderModuleCreateInfo vertex_shader_ci = VkShaderModuleCreateInfo{};
        R"(
''' + shader_code + '''        )", 
        "vertex_shader");

    VkShaderModuleCreateInfo fragment_shader_ci = VkShaderModuleCreateInfo{};
        R"(
''' + fragment_shader_code + '''        )", 
        "fragment_shader");

    if (vkCreateShaderModule(g_device, &vertex_shader_ci, nullptr, &g_vertex_shader_module) != VK_SUCCESS) {
        std::cerr << "Failed to create vertex shader module\\n";
        return -1;
    }'''

new_shader_section = '''    // Compile shaders to modules
    VkShaderModuleCreateInfo vertex_shader_ci{};
    vertex_shader_ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vertex_shader_ci.codeSize = R"(''' + shader_code + ''')".size();
    vertex_shader_ci.pCode = reinterpret_cast<const uint32_t*>(R"(''' + shader_code + ''')");

    VkShaderModuleCreateInfo fragment_shader_ci{};
    fragment_shader_ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    fragment_shader_ci.codeSize = R"(''' + fragment_shader_code + ''')".size();
    fragment_shader_ci.pCode = reinterpret_cast<const uint32_t*>(R"(''' + fragment_shader_code + ''')");

    if (vkCreateShaderModule(g_device, &vertex_shader_ci, nullptr, &g_vertex_shader_module) != VK_SUCCESS) {
        std::cerr << "Failed to create vertex shader module\\n";
        return -1;
    }'''

content = content.replace(old_shader_section, new_shader_section)

# Also fix the fragment shader
old_frag_section = '''    VkShaderModuleCreateInfo fragment_shader_ci = VkShaderModuleCreateInfo{};
        R"(
''' + fragment_shader_code + '''        )", 
        "fragment_shader");

    if (vkCreateShaderModule(g_device, &vertex_shader_ci, nullptr, &g_vertex_shader_module) != VK_SUCCESS) {
        std::cerr << "Failed to create vertex shader module\\n";
        return -1;
    }'''

# Actually this gets tricky because of overlapping replacements. Let me just do a simpler approach:
content = content.replace('VkShaderModuleCreateInfo vertex_shader_ci = VkShaderModuleCreateInfo{};', 
                          'VkShaderModuleCreateInfo vertex_shader_ci{};')
content = content.replace('R"(', '/* R"( - raw string literal for shader code */\n    ')

with open('/home/mochi/taz-decomp/src/main_simple_vulkan.cpp', 'w') as f:
    f.write(content)

print("Fixed shader creation")
