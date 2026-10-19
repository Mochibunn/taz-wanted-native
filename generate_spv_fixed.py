#!/usr/bin/env python3
"""Generate valid SPIR-V bytecode for vertex and fragment shaders."""
import subprocess
import sys

def compile_shader(source, target_name):
    """Compile GLSL source to SPIR-V binary."""
    spv_file = f"/tmp/{target_name}.spv"
    
    # Use glslangValidator to compile GLSL to SPIR-V
    cmd = [
        "glslangValidator", "-V", "-C",
        "-D", "GL_GOOGLE_IMPLEMENTATION",
        "-x", "hlsl",  # HLSL syntax works well with vulkan
        target_name,
        "-o", spv_file
    ]
    
    print(f"Compiling {target_name} to SPIR-V...")
    result = subprocess.run(cmd, capture_output=True, text=False)
    
    if result.returncode == 0:
        print(f"✅ Successfully compiled {target_name}")
        return spv_file
    else:
        print(f"❌ Failed to compile {target_name}:")
        print(result.stderr.decode('utf-8', errors='ignore'))
        return None

def main():
    """Main entry point."""
    # GLSL sources
    vert_source = r"""#version 450
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec2 a_texCoord;
layout(location = 0) out vec3 fragColor;

uniform LayoutLocation {
    mat4 ubo_projectionMatrix;
    mat4 ubo_viewMatrix;
    mat4 ubo_modelMatrix;
} ubo_transform;

void main() {
    vec4 transformedPosition = vec4(a_position, 1.0);
    transformedPosition = ubo_transform.ubo_modelMatrix * transformedPosition;
    transformedPosition = ubo_transform.ubo_viewMatrix * transformedPosition;
    transformedPosition = ubo_transform.ubo_projectionMatrix * transformedPosition;
    gl_Position = transformedPosition;
    float red = sin(a_position.x * 3.14159) * 0.5 + 0.5;
    float green = cos(a_position.y * 3.14159) * 0.5 + 0.5;
    float blue = sin(a_position.z * 3.14159) * 0.5 + 0.5;
    fragColor = vec3(red, green, blue);
}
"""

    frag_source = r"""#version 450
layout(location = 0) in vec3 fragColor;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(fragColor, 1.0);
}
"""

    # Write source files
    with open("/tmp/shaders.vert", "w") as f:
        f.write(vert_source)
    
    with open("/tmp/shaders.frag", "w") as f:
        f.write(frag_source)
    
    # Compile shaders
    vert_spv = compile_shader("/tmp/shaders.vert", "/tmp/shaders.vert")
    frag_spv = compile_shader("/tmp/shaders.frag", "/tmp/shaders.frag")
    
    # Write SPIR-V bytecode to files for embedding in C++
    if vert_spv and frag_spv:
        with open(f"/home/mochi/taz-decomp/src/vert.spv", "wb") as f:
            with open(vert_spv, "rb") as sf:
                f.write(sf.read())
        
        with open(f"/home/mochi/taz-decomp/src/frag.spv", "wb") as f:
            with open(frag_spv, "rb") as sf:
                f.write(sf.read())
        
        print("✅ Successfully wrote SPIR-V bytecode to:")
        print(f"  - /home/mochi/taz-decomp/src/vert.spv ({len(open('/tmp/shaders.vert').read().split(chr(0))[-1])} bytes)")
        print(f"  - /home/mochi/taz-decomp/src/frag.spv ({len(open('/tmp/shaders.frag').read().split(chr(0))[-1])} bytes)")

if __name__ == "__main__":
    main()
