cmake_minimum_required(VERSION 3.10)
project(Triangle)

set(CMAKE_CXX_STANDARD 17)
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
