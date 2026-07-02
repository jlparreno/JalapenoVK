# JalapenoVK 🌶️

A personal Vulkan rendering engine written in C++20. Built as a learning project to explore modern Vulkan 1.4.

> **Status: work in progress.** Currently renders a single textured glTF model (viking_room) with MSAA. Internals are being iterated on, expect the architecture to keep evolving.

## Tech Stack

| | |
|---|---|
| **Graphics API** | Vulkan 1.4 via Vulkan-Hpp (`vk::raii`) |
| **Shaders** | [Slang](https://shader-slang.com/) → SPIR-V 1.4 |
| **Windowing** | GLFW |
| **Math** | GLM |
| **Model loading** | tinygltf (glTF 2.0) |
| **Texture loading** | libktx (KTX2) |
| **Build system** | CMake 3.29 + vcpkg |
| **Compiler / Platform** | MSVC C++20 on Windows |

## Features

- **Pass-based Dynamic Rendering**
- **Entity-Component Composition**
- **Typed Resource Manager**
- **glTF 2.0 Mesh Loading**
- **KTX2 Textures**
- **MSAA**
- **Framebuffer Resize Handling**


## Architecture

- **`VulkanContext`** — Instance, device, queues, command pool, and some helpers. Owns all global GPU state.
- **`Swapchain`** — image views, sync objects, and full recreation on resize.
- **`RenderPass` + `RenderPassManager`** — abstraction for ordering render passes without going full render-graph yet.
- **`GeometryPass`** — self-contained pass that owns everything specific to its rendering.
- **`Renderer`** — thin coordinator: builds `FrameData` each frame, drives the acquire/present cycle, orchestrates command buffers.
- **`ResourceManager`** — central registry for loadable engine resources (`Texture`, `Mesh`, `Shader`).

## Building

### Prerequisites

- [Vulkan SDK 1.4.335+](https://vulkan.lunarg.com/) — includes `slangc` for shader compilation.
- [vcpkg](https://vcpkg.io/) with the following packages: `glfw3`, `glm`, `tinygltf`, `ktx`, `tinyobjloader`, `stb`.
- CMake 3.29+.
- Visual Studio 2022 (or any C++20-capable compiler on Windows).

### Steps

```bat
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=<path_to_vcpkg>/scripts/buildsystems/vcpkg.cmake
cmake --build build
```

The compiled binary and shaders are placed under `build/JalapenoVK/`.

## Project Structure

```
JalapenoVK/
├── src/
│   ├── core/          # Vulkan backend (context, device, includes)
│   ├── render/        # Renderer, swapchain, per-frame types
│   │   └── passes/    # RenderPass base + manager + concrete passes
│   ├── resources/     # Resource system (meshes, textures, shaders)
│   ├── scene/         # Entity + components (ECS-style composition)
│   └── main.cpp
├── shaders/           # Slang shader sources (compiled to SPIR-V at build time)
├── assets/            # Models and textures (not versioned)
└── CMakeLists.txt
```

## License

See [LICENSE](LICENSE).
