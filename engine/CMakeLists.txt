cmake_minimum_required(VERSION 3.16)
project(TazECS VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Only need glm for math library, SDL2/Vulkan optional for demo
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(taz_ecs_test
    src/ECS.cpp
    src/entities.cpp
    src/main_with_ecs.cpp
)

target_include_directories(taz_ecs_test PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(taz_ecs_test m pthread)

message(STATUS "=== TazECS Build Configuration ===")
message(STATUS "Target: ECS-only demonstration (no Vulkan/SDL2)")

