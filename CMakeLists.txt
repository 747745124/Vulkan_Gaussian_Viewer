cmake_minimum_required(VERSION 3.10)
project(Triangle)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)

# Add base source files
set(BASE_SOURCES
    base/application.cpp
    base/camera.cpp
    base/object3d.cpp
    base/ply_parser.cpp
)

# Add Objective-C file for macOS
if(APPLE)
    set_source_files_properties(base/ObjC-interface.m PROPERTIES COMPILE_FLAGS "-x objective-c")
    list(APPEND BASE_SOURCES base/ObjC-interface.m)
endif()

# Add executable
# add_executable(vk_test vk_test.cpp)
add_executable(Triangle 
    main.cpp
    ${BASE_SOURCES}
)

# Include directories
target_include_directories(Triangle PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/external
    ${GLFW_INCLUDE_DIRS}
    ${GLFW_INCLUDE_DIR}/native
)

# Add debug definitions
target_compile_definitions(Triangle PRIVATE
    $<$<CONFIG:Debug>:_DEBUG>
)

# Link libraries
target_link_libraries(Triangle PRIVATE
    Vulkan::Vulkan
    glfw
    glm::glm
)

# Add framework dependencies for macOS
if(APPLE)
    target_link_libraries(Triangle PRIVATE
        "-framework Cocoa"
        "-framework QuartzCore"
    )
endif()

# Test: PLY loader validation (no Vulkan/GLFW deps)
add_executable(ply_loader_test
    test/ply_loader_test.cpp
    base/ply_parser.cpp
)

# CPU visualization tool
add_executable(ply_cpu_viz
    test/ply_cpu_viz.cpp
    base/ply_parser.cpp
)

target_include_directories(ply_loader_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/external
)

target_link_libraries(ply_loader_test PRIVATE
    glm::glm
)

target_include_directories(ply_cpu_viz PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/external
)

target_link_libraries(ply_cpu_viz PRIVATE
    glm::glm
)
