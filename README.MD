# Texture Loading Performance Comparison in OpenGL

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

A graduation project comparing different approaches to texture loading in OpenGL, focusing on single-context vs shared-context performance.

## Table of Contents
- [Project Description](#project-description)
- [Key Features](#key-features)
- [Dependencies](#dependencies)
- [Build Instructions](#build-instructions)
  - [Windows with vcpkg](#windows-with-vcpkg)
  - [Generic CMake Build](#generic-cmake-build)
- [Usage](#usage)
- [Performance Metrics](#performance-metrics)
- [Architecture](#architecture)
- [Project Structure](#project-structure)
- [License](#license)
- [Acknowledgements](#acknowledgements)

## Project Description

This project implements a comparative study of texture loading techniques in modern OpenGL, developed as a graduation project (Bitirme Çalışması) at Karadeniz Technical University. The system evaluates two primary approaches:

1. **Single Context Approach**: Textures are decoded on worker threads but uploaded in the main OpenGL context
2. **Shared Context Approach**: Uses a dedicated OpenGL context for texture uploads

The application loads textures from "This Person Does Not Exist" (https://thispersondoesnotexist.com) and renders them on 3D objects with real-time performance monitoring.

## Key Features

- **Rendering Engine**:
  - Modern OpenGL 3.3+ core profile
  - Entity Component System (ECS) architecture
  - Instanced rendering support
  - Configurable camera system

- **Texture Loading**:
  - Multi-threaded image downloading
  - Two distinct upload strategies
  - STB_image for texture decoding

- **Performance Analysis**:
  - Real-time FPS monitoring
  - Frame time measurement
  - Memory usage tracking (Windows)
  - CSV log generation

- **User Interface**:
  - ImGui-based control panel
  - Interactive object manipulation
  - System diagnostics
  - Viewport configuration

## Dependencies

| Dependency  | Minimum Version | Notes |
|-------------|----------------|-------|
| CMake       | 3.15           | Build system |
| vcpkg       | 2023.08.09     | Package manager (Windows) |
| OpenGL      | 3.3            | Core profile |
| GLFW        | 3.3            | Window management |
| GLEW        | 2.1            | OpenGL extension loader |
| GLM         | 0.9.9          | Mathematics library |
| stb_image   | 2.27           | Image loading |
| ImGui       | 1.89           | User interface |

## Build Instructions

### Windows with vcpkg

1. **Install vcpkg**
   ```powershell
   git clone https://github.com/microsoft/vcpkg.git
   .\vcpkg\bootstrap-vcpkg.bat