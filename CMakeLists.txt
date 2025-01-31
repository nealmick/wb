cmake_minimum_required(VERSION 3.15)
project(SimpleBrowser)

# Silence OpenGL deprecation warnings on macOS
add_definitions(-DGL_SILENCE_DEPRECATION)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Explicit GLEW paths (update version if different)
set(GLEW_INCLUDE_DIRS "/usr/local/Cellar/glew/2.2.0_1/include")
set(GLEW_LIBRARY_DIRS "/usr/local/Cellar/glew/2.2.0_1/lib")
set(GLEW_LIBRARIES "/usr/local/Cellar/glew/2.2.0_1/lib/libGLEW.dylib")

find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(CURL REQUIRED)

# ImGui sources
file(GLOB IMGUI_SOURCES
    "imgui/*.cpp"
    "imgui/backends/imgui_impl_glfw.cpp"
    "imgui/backends/imgui_impl_opengl3.cpp"
)

add_executable(SimpleBrowser
    main.cpp
    browser.cpp
    browser.h
    ${IMGUI_SOURCES}
)

target_include_directories(SimpleBrowser PRIVATE
    imgui
    imgui/backends
    ${CURL_INCLUDE_DIRS}
    ${GLEW_INCLUDE_DIRS}  # Add GLEW includes
)

target_link_libraries(SimpleBrowser PRIVATE
    ${OPENGL_LIBRARIES}
    glfw
    ${CURL_LIBRARIES}
    ${GLEW_LIBRARIES}  # Add GLEW linking
    "-framework OpenGL"
    "-framework Foundation"
)

if(APPLE)
    target_link_libraries(SimpleBrowser PRIVATE
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
    )
endif()