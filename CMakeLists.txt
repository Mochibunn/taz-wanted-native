cmake_minimum_required(VERSION 3.16)
project(TazWantedNative VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages on your system
find_package(SDL2 REQUIRED)
find_package(Vulkan REQUIRED)

# Add source files
set(SOURCES
    src/main.cpp
    src/pak_loader.cpp
    src/texture_loader.cpp
    src/mesh_loader.cpp
    src/renderer/vulkan_texture.cpp
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

# Include directories for header files
target_include_directories(taz_wanted PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
