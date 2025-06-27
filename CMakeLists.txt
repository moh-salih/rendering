cmake_minimum_required(VERSION 3.24)

# Project settings
project(texgan VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)


# Source files
set(SOURCES
    src/main.cpp
    src/aif/ImageFetcher.cpp
)

set(HEADERS
    src/aif/ImageFetcher.h
)

# Conditionally set WIN32 for Release only (no console window)
set(EXE_TYPE "")
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(EXE_TYPE WIN32)
    # Use static runtime (/MT or /MTd) on MSVC
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

    add_compile_definitions(TEXGAN_GUI)
endif()

# Executable
add_executable(${PROJECT_NAME} ${EXE_TYPE} ${SOURCES} ${HEADERS})

# Project includes
target_include_directories(${PROJECT_NAME} PUBLIC "${PROJECT_SOURCE_DIR}/src")

# Dependencies
find_package(imgui CONFIG REQUIRED)
find_package(GLEW REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)
find_package(Stb REQUIRED)
find_package(cpr REQUIRED)

# Link dependencies
target_link_libraries(${PROJECT_NAME} PUBLIC
    glfw
    GLEW::GLEW
    imgui::imgui
    glm::glm
    cpr::cpr
)
