# Project Roadmap

## Phase 1: Decoupling & Modularization [COMPLETED]
*Goal: Separate the Generic Engine from the Graph Editor Application.*

- [x] **Refactor Entry Point**: Moved `engine_setup_default_graph` and logic to `src/app/main.c`.
- [x] **Abstract UI Building**: `Engine` no longer manages `MathGraph` or `UiView` directly.
- [x] **Callback System**: Implemented `on_init` / `on_update` in `EngineConfig`.
- [x] **Feature Interface**: `main.c` now orchestrates the connection between `GraphEditor` and `RenderSystem`.

## Phase 2: Renderer Hardening [COMPLETED]
*Goal: Abstract Vulkan dependencies from public headers.*

- [x] **Strict Backend Interface**: `platform.h` and `renderer_backend.h` now use generic `void*` signatures.
- [x] **API Agnosticism**: Removed `<vulkan/vulkan.h>` includes from generic headers.

## Phase 3: Memory Safety [COMPLETED]
*Goal: Replace ad-hoc `malloc` with Memory Arenas.*

- [x] **Arena Implementation**: Added `MemoryArena` to Foundation layer.
- [x] **Transpiler**: Rewrote `transpiler.c` to use `MemoryArena` instead of `StringBuilder` (malloc).
- [x] **UI Loader**: Refactor `ui_loader.c` to use Arenas for parsing.
- [ ] **Global Allocator**: Introduce a frame-based linear allocator for per-frame data.

## Phase 4: Features [PENDING]
*Goal: Expand the Graph Editor capabilities.*

- [ ] **Graph Editor 2.0**: Support for complex data types (Matrices, Structures) on wires.
- [ ] **Save/Load**: Serialize the `MathGraph` to JSON/Binary.
- [ ] **Compute Visualization**: Expand the visualizer to support 3D volume rendering.

## Phase 5: Documentation & Testing [ONGOING]
- [x] **Docs Update**: Updated Architecture and API Reference.
- [ ] **Unit Tests**: Expand `tests/` to cover the Transpiler logic more rigorously.
