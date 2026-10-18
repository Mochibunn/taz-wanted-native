cmake_minimum_required(VERSION 3.16)
project(TazWantedNative VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add assets directory to include path for texture loading
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/assets/textures)

# Find required packages on your system
find_package(SDL2 REQUIRED)
find_package(Vulkan REQUIRED)

# Use our local burrow's GLM headers directly
set(GLM_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/include")

if(NOT EXISTS "${GLM_INCLUDE_DIRS}/glm/glm.hpp")
    message(FATAL_ERROR "GLM headers not found at ${GLM_INCLUDE_DIRS}/glm/glm.hpp. Please clone glm into include/")
endif()

include_directories(${GLM_INCLUDE_DIRS})

# Add active source files (omitting legacy/conflicting stubs)
set(SOURCES
    src/main_simple_vulkan.cpp
)

# Add our native executable target
add_executable(taz_wanted
    ${SOURCES}
)

# Link SDL2, Vulkan, and GLM libraries
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
    ${GLM_INCLUDE_DIRS}
    ${Vulkan_INCLUDE_DIRS}
)

# Add compile definitions with MVP support
target_compile_definitions(taz_wanted PRIVATE
    _DEBUG
    VK_NO_EXPOSE_EXTENSION_FUNCTION_MACROS
    TEXTURE_PATH="taz.png"
    HAVE_MVP_UNIFORMS  # Flag to indicate MVP uniforms are available
    HAVE_SCENE_GRAPH   # Flag for scene graph support
)

# Compile options
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
target_compile_options(taz_wanted PRIVATE 
    -O2 
    $<$<CONFIG:Debug>:-g>
)

# Create transforms library target
add_library(${PROJECT_NAME}_transforms STATIC
    src/transforms.cpp
)

target_include_directories(${PROJECT_NAME}_transforms PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${GLM_INCLUDE_DIRS}
    ${Vulkan_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME}_transforms PUBLIC 
    Vulkan::Vulkan
)

# Export compile commands for IntelliSense and IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

message(STATUS "=== TazWantedNative Build Configuration ===")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "GLM Include Dir: ${GLM_INCLUDE_DIRS}")
message(STATUS "Vulkan Include Dir: ${Vulkan_INCLUDE_DIRS}")
message(STATUS "SDL2 include dirs: ${SDL2_INCLUDE_DIRS}")
