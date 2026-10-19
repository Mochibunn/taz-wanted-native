#!/usr/bin/env python3

with open('/home/mochi/taz-decomp/src/main_simple_vulkan.cpp', 'r') as f:
    lines = f.readlines()

# Find the line number where we need to insert shader code
insert_marker = '// Compile shaders to modules'
insert_line = None
for i, line in enumerate(lines):
    if insert_marker in line:
        insert_line = i
        break

if insert_line is None:
    print("Marker not found")
    exit(1)

# Find the end marker
end_marker = '// Create pipeline layout'
end_line = None
for i in range(insert_line, len(lines)):
    if end_marker in lines[i]:
        end_line = i
        break

print(f"Insert at line {insert_line}, end at line {end_line}")

# Replace the problematic section with fixed code
shader_vertex_code = '''    // Compile shaders to modules
    std::string vertex_shader_source = R"(
            #version 450
            layout (location = 0) in vec3 inPos { x, y, z };
            layout (location = 1) in vec2 inUv { x, y };
            
            out vec2 frag_uv;
            
            void main() {
                gl_Position = vec4(inPos, 1.0);
                frag_uv = inUv;
            }
        )";

    std::string fragment_shader_source = R"(
            #version 450
            layout (location = 0) out vec4 fColor;
            
            in vec2 frag_uv;
            
            void main() {
                float height = fract(frag_uv.x * 10.0);
                vec3 color = vec3(height, 0.2, 0.8);
                fColor = vec4(color, 1.0);
            }
        )";

    VkShaderModuleCreateInfo vertex_shader_ci{};
    vertex_shader_ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vertex_shader_ci.codeSize = vertex_shader_source.size();
    vertex_shader_ci.pCode = reinterpret_cast<const uint32_t*>(vertex_shader_source.c_str());

    VkShaderModuleCreateInfo fragment_shader_ci{};
    fragment_shader_ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    fragment_shader_ci.codeSize = fragment_shader_source.size();
    fragment_shader_ci.pCode = reinterpret_cast<const uint32_t*>(fragment_shader_source.c_str());

    if (vkCreateShaderModule(g_device, &vertex_shader_ci, nullptr, &g_vertex_shader_module) != VK_SUCCESS) {
        std::cerr << "Failed to create vertex shader module\\n";
        return -1;
    }

    if (vkCreateShaderModule(g_device, &fragment_shader_ci, nullptr, &g_fragment_shader_module) != VK_SUCCESS) {
        std::cerr << "Failed to create fragment shader module\\n";
        return -1;
    }

    std::cout << "✅ Shaders compiled\\n";

'''

# Replace the lines
new_lines = lines[:insert_line] + [shader_vertex_code + '\n'] + lines[end_line:]

# Write back
with open('/home/mochi/taz-decomp/src/main_simple_vulkan.cpp', 'w') as f:
    f.writelines(new_lines)

print("Fixed shader section")
