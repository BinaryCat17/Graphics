# Project Roadmap

## Phase 1: Decoupling & Modularization (Current Priority)
The immediate goal is to separate the *Generic Engine* from the *Graph Editor Application*.

- [ ] **Refactor Entry Point**: Move `engine_setup_default_graph` and specific input handling from `engine.c` to `src/app/main.c`.
- [ ] **Abstract UI Building**: Remove `ui_build_scene` call from `render_system.c`. Introduce a `SceneDelegate` or callback interface so the App can decide what to render.
- [ ] **Feature Interface**: Create a `Feature` interface (init, update, render_packet_contribute) to allow loading the Graph Editor without hardcoding it in the engine core.

## Phase 2: Renderer Hardening
- [ ] **Strict Backend Interface**: Remove all Vulkan-specific types (`VkInstance`, etc.) from `platform.h` and `renderer_backend.h`. Use opaque pointers or generic handles.
- [ ] **Multi-Backend Prep**: Ensure the API is ready for a potential OpenGL or DX12 backend (proof of abstraction).
- [ ] **Command Buffer Management**: Optimize `vk_swapchain` and command pool usage for high-throughput rendering.

## Phase 3: Memory Safety & Performance
- [ ] **Global Allocator**: Replace ad-hoc `malloc` in `transpiler.c` and `ui_loader.c` with the Foundation's `MemoryArena`.
- [ ] **Hot-Reloading**: Implement hot-reloading for UI YAML files (already partially supported by file system watcher stubs).
- [ ] **Job System**: Introduce a fiber-based job system for multi-threaded scene traversal.

## Phase 4: Features
- [ ] **Graph Editor 2.0**: Support for complex data types (Matrices, Structures) on wires.
- [ ] **Save/Load**: Serialize the `MathGraph` to JSON/Binary.
- [ ] **Compute Visualization**: Expand the visualizer to support 3D volume rendering (Raymarching) from the compute graph output.

## Phase 5: Documentation & Testing
- [ ] **Unit Tests**: Expand `tests/` to cover the Transpiler logic more rigorously.
- [ ] **API Docs**: Generate Doxygen-style HTML from headers.