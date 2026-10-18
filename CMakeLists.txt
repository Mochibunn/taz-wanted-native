cmake_minimum_required(VERSION 3.16)
project(TazWantedNative VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add assets directory to include path for texture loading
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/assets/textures)

# Find required packages on your system
find_package(SDL2 REQUIRED)
find_package(Vulkan REQUIRED)

# Add source files - include renderer Vulkan pipeline
set(SOURCES
    src/main.cpp
    src/pak_loader.cpp
    src/texture_loader.cpp
    src/mesh_loader.cpp
    src/renderer/vulkan_texture.cpp
    src/renderer/vulkan_pipeline.cpp
)

# Add our native executable target
add_executable(taz_wanted
    ${SOURCES}
)

# Link SDL2 and Vulkan libraries
target_link_libraries(taz_wanted PRIVATE 
    SDL2::SDL2
    Vulkan::Vulkan
)

# Set default texture asset path
target_compile_definitions(taz_wanted PRIVATE 
    ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
    TEXTURE_PATH="taz.png"
)

# Include directories for header files
target_include_directories(taz_wanted PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer
    ${Vulkan_INCLUDE_DIRS}
)

# Add compile definitions
target_compile_definitions(taz_wanted PRIVATE
    _DEBUG
    VK_NO_EXPOSE_EXTENSION_FUNCTION_MACROS
    TEXTURE_PATH="taz.png"
)

# Create a target for vertex/index buffer objects (placeholder for proper creation)
add_library(${PROJECT_NAME}_vertex_buffers STATIC
    src/renderer/vulkan_pipeline.cpp
)

target_include_directories(${PROJECT_NAME}_vertex_buffers PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer
    ${Vulkan_INCLUDE_DIRS}
)

# Add vertex buffer and index buffer creation functions
add_library(${PROJECT_NAME}_buffer_utils STATIC
    src/mesh_loader.cpp
)

target_include_directories(${PROJECT_NAME}_buffer_utils PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${Vulkan_INCLUDE_DIRS}
)
