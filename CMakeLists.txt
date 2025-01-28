cmake_minimum_required(VERSION 3.20)

# Project details
project(Lockaway VERSION 1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set output directories for organized builds
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Add source files for the project
set(SOURCES
    src/main.cpp                              # Main entry point of the application
    libs/imgui/imgui.cpp                      # Core Dear ImGui files
    libs/imgui/imgui_draw.cpp
    libs/imgui/imgui_demo.cpp
    libs/imgui/imgui_tables.cpp
    libs/imgui/imgui_widgets.cpp
    libs/imgui/backends/imgui_impl_glfw.cpp   # Backend for GLFW
    libs/imgui/backends/imgui_impl_opengl3.cpp # Backend for OpenGL
)

# Include directories for dependencies
include_directories(
    ${CMAKE_SOURCE_DIR}/libs/imgui              # Dear ImGui main headers
    ${CMAKE_SOURCE_DIR}/libs/imgui/backends     # Dear ImGui backend headers
    ${CMAKE_SOURCE_DIR}/libs/glfw/include       # GLFW headers
)

# Specify the location of precompiled GLFW library for MinGW
set(GLFW_LIB_DIR ${CMAKE_SOURCE_DIR}/libs/glfw/lib-mingw-w64)
link_directories(${GLFW_LIB_DIR})

# Add executable and suppress console window for GUI applications on Windows
add_executable(${PROJECT_NAME} WIN32 ${SOURCES}) 
# The WIN32 flag prevents the creation of a console window when running the application on Windows.

# Link libraries required for the project
target_link_libraries(${PROJECT_NAME}
    glfw3         # Precompiled GLFW library
    opengl32      # OpenGL library (core graphics API)
    kernel32      # Windows kernel library for basic system functions
    user32        # Windows user library for GUI functions
    gdi32         # GDI for OpenGL context
    winspool      # Windows printing API (optional for some dependencies)
    shell32       # Windows Shell API
    ole32         # OLE (Object Linking and Embedding)
    oleaut32      # OLE Automation API
    uuid          # UUID library for unique identifiers
    comdlg32      # Common dialog library
    advapi32      # Advanced Windows API
)

# Print user-friendly messages during configuration
message(STATUS "CMake project configuration for Lockaway:")
message(STATUS "  - Project Name: ${PROJECT_NAME}")
message(STATUS "  - C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  - Output Directory: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "  - GLFW Library Directory: ${GLFW_LIB_DIR}")
