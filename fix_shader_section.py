#!/usr/bin/env python3

with open('/home/mochi/taz-decomp/src/main_simple_vulkan.cpp', 'r') as f:
    content = f.read()

# Find the position of shader creation
marker = '// Compile shaders to modules'
pos = content.find(marker)

if pos == -1:
    print("Marker not found")
else:
    # Extract from this marker to before "// Create pipeline layout"
    start = pos
    
    end = content.find('// Create pipeline layout', pos)
    
    # Read the section we need to replace
    section = content[start:end]
    print(f"Section to replace starts at {start}, ends at {end}")
    print("First 200 chars:", repr(section[:200]))
