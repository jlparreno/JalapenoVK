# JalapenoVK 🌶️

A personal Vulkan rendering engine written in C++20, built as a learning project.

## Tech Stack

| | |
|---|---|
| **Graphics API** | Vulkan 1.4 via Vulkan-Hpp (`vk::raii`) |
| **Shaders** | [Slang](https://shader-slang.com/) → SPIR-V 1.4 |
| **Windowing** | GLFW |
| **Math** | GLM |
| **Model loading** | tinyobjloader, tinygltf |
| **Texture loading** | stb_image, KTX |
| **Build system** | CMake 3.29 + vcpkg |
| **Compiler** | MSVC (C++20) |

## Features

- ...
- ...
- ...

## Building

### Prerequisites

- [Vulkan SDK 1.4.335+](https://vulkan.lunarg.com/)
- [vcpkg](https://vcpkg.io/) with the following packages:
  - `glfw3`, `glm`, `tinyobjloader`, `tinygltf`, `ktx`, `stb`
- CMake 3.29+
- Visual Studio 2022 (or any C++20-capable compiler)

### Steps

```bat
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=<path_to_vcpkg>/scripts/buildsystems/vcpkg.cmake
cmake --build build
```

The compiled binary and shaders are placed under `build/JalapenoVK/`.

## Project Structure

```
JalapenoVK/
├── src/            # C++ source and headers
├── shaders/        # Slang shader sources (compiled to SPIR-V at build time)
├── assets/         # Models and textures (not versioned)
└── CMakeLists.txt
```
