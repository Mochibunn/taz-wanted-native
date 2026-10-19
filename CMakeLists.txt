cmake_minimum_required(VERSION 3.16)
project(TazWantedNative VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 QUIET sdl2)
pkg_check_modules(Vulkan QUIET vulkan)

# If pkg-config didn't find libraries, fail gracefully
if(NOT SDL2_FOUND OR NOT Vulkan_FOUND)
    message(FATAL_ERROR "Failed to find SDL2 or Vulkan libraries")
endif()

set(GLM_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/include")

if(NOT EXISTS "${GLM_INCLUDE_DIRS}/glm/glm.hpp")
    message(FATAL_ERROR "GLM headers not found")
endif()

# Find the actual library files
find_library(SDL2_LIB NAMES SDL2 REQUIRED PATHS ${SDL2_LIBRARY_PATH} /usr/lib /usr/local/lib)
find_library(VULKAN_LIB NAMES vulkan REQUIRED PATHS ${Vulkan_LIBRARY_PATH} /usr/lib /usr/local/lib)

# Direct compile of main source  
add_executable(taz_wanted 
    src/main_complete_sdlvulkan.cpp
)

target_include_directories(taz_wanted PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    /home/mochi/vulkan_headers
    ${GLM_INCLUDE_DIRS}
)

target_link_libraries(taz_wanted PRIVATE
    ${SDL2_LIB}
    ${VULKAN_LIB}
    m
    pthread
)

set_target_properties(taz_wanted PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    LINKER_LANGUAGE CXX
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

message(STATUS "=== TazWantedNative Build Configuration ===")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "SDL2 version: ${SDL2_VERSION}")
message(STATUS "Vulkan version: ${Vulkan_VERSION}")
message(STATUS "SDL2_LIB: ${SDL2_LIB}")
message(STATUS "VULKAN_LIB: ${VULKAN_LIB}")
