# Texture Loading Performance Comparison in OpenGL

A graduation project comparing different approaches to texture loading in OpenGL, focusing on single-context vs shared-context performance.

## Table of Contents

* [Project Description](#project-description)
* [Key Features](#key-features)
* [Dependencies](#dependencies)
* [Build Instructions](#build-instructions)

  * [Windows with vcpkg](#windows-with-vcpkg)
  * [Generic CMake Build](#generic-cmake-build)
* [Usage](#usage)
* [Acknowledgements](#acknowledgements)

## Project Description

This project implements a comparative study of texture loading techniques in modern OpenGL, developed as a graduation project at Karadeniz Technical University. The system evaluates two primary approaches:

1. **Single Context Approach**: Textures are decoded on worker threads but uploaded in the main OpenGL context.
2. **Shared Context Approach**: Uses a dedicated OpenGL context for texture uploads.

The application loads textures from *This Person Does Not Exist* ([https://thispersondoesnotexist.com](https://thispersondoesnotexist.com)) and renders them on 3D objects with real-time performance monitoring.

## Key Features

* **Rendering Engine**

  * Modern OpenGL 3.3+ core profile
  * Entity Component System (ECS) architecture
  * Instanced rendering support
  * Configurable camera system

* **Texture Loading**

  * Multi-threaded image downloading
  * Two distinct upload strategies
  * STB\_image for texture decoding

* **Performance Analysis**

  * Real-time FPS monitoring
  * Frame time measurement
  * Memory usage tracking (Windows)
  * CSV log generation

* **User Interface**

  * ImGui-based control panel
  * Interactive object manipulation
  * System diagnostics
  * Viewport configuration

## Dependencies

| Dependency | Minimum Version | Notes                     |
| ---------- | --------------- | ------------------------- |
| CMake      | 3.15            | Build system              |
| vcpkg      | 2023.08.09      | Package manager (Windows) |
| OpenGL     | 3.3             | Core profile              |
| GLFW       | 3.3             | Window management         |
| GLEW       | 2.1             | OpenGL extension loader   |
| GLM        | 0.9.9           | Mathematics library       |
| stb\_image | 2.27            | Image loading             |
| ImGui      | 1.89            | User interface            |

## Build Instructions

### Windows with vcpkg

1. **Install vcpkg**

   ```powershell
   git clone https://github.com/microsoft/vcpkg.git
   .\vcpkg\bootstrap-vcpkg.bat
   ```
2. **Install dependencies**

   ```powershell
   vcpkg install glfw3 glew glm stb imgui
   ```
3. **Configure with CMake**

   ```powershell
   mkdir build
   cd build
   cmake .. -DCMAKE_TOOLCHAIN_FILE=[path_to_vcpkg]\scripts\buildsystems\vcpkg.cmake
   ```
4. **Build the project**

   ```powershell
   cmake --build . --config Release
   ```

### Generic CMake Build

1. **Create a build directory**

   ```bash
   mkdir build && cd build
   ```
2. **Run CMake**

   ```bash
   cmake ..
   ```
3. **Build**

   ```bash
   cmake --build . --config Release
   ```

## Usage

### Application Workflow

1. **Launch the application**
2. **Create 3D objects**

   * Use the **Cube Creator** panel to generate objects.
   * Choose between regular or instanced rendering.
   * Configure quantity and position ranges.
3. **Load textures**

   * Select batch size (1–500 textures).
   * Choose loading approach:

     * **Single Context** (default)
     * **Shared Context**
   * Click **Download Textures**.
4. **Monitor performance**

   * Real-time graphs show FPS and frame times.
   * CSV logs are saved in `assets/log/`.

### Control Reference

| Control      | Action                 |
| ------------ | ---------------------- |
| WASD         | Camera movement        |
| Mouse + CTRL | Camera look            |
| 1 / 2        | Move up / Move down    |
| SPACE        | Cycle object selection |
| ESC          | Quit                   |


## Acknowledgements

* [This Person Does Not Exist](https://thispersondoesnotexist.com)
* stb\_image by Sean Barrett
* Dear ImGui by Omar Cornut et al.
