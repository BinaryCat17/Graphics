# Project Roadmap

**Current Focus:** Phase 6 - Architectural Hardening & 3D
**Date:** December 20, 2025

## 🏁 Current State (v0.7.1 Refactoring)

The project is undergoing a structural standardization to enforce strict public/private API boundaries before expanding into complex 3D features.

---

## 🚀 Active Phases

### Phase 6: Architectural Hardening & Cleanup (IMMEDIATE PRIORITY)
**Objective:** Enforce the "Public/Internal" separation pattern, optimize memory usage, and remove hardcoded logic.

- [x] **Scene Memory Optimization:** Replace `realloc` in `Scene` with `MemoryArena` (Frame Allocator) to eliminate heap fragmentation and allocation spikes during rendering.
- [x] **Asset Decoupling:** Refactor `Assets` module to load file content into memory buffers. Update `RenderSystem` to accept data pointers (bytes) instead of file paths, decoupling rendering from the OS file system.
- [x] **Font System Decoupling:** Update `font_init` to accept a memory buffer instead of a file path, ensuring all I/O is centralized in the `Assets` module.
- [x] **Geometry Deduplication:** Centralize primitive generation (e.g., Unit Quad) in a shared helper to eliminate duplicate vertex data definitions in `Assets` and `VulkanBackend`.
- [x] **UI Styling Data-Drive:** Remove hardcoded visual logic (e.g., `color *= 1.1f` for inputs) from `ui_renderer.c`. Move state-based visual changes into `UiStyle` or config data.
- [x] **Uniform Error Handling:** Refactor `engine_create` and subsystem initializers to use a standardized `goto cleanup` pattern or shared destructor helper to prevent resource leaks and reduce code duplication on failure.
- [ ] **Strict Compiler Compliance:** Enable `-Werror` (treat warnings as errors) in CMake and resolve all existing warnings to ensure code hygiene.
- [ ] **Static Analysis Integration:** Integrate `cppcheck` or `clang-tidy` into the CMake pipeline to automatically detect bugs and memory issues.
- [ ] **Debug String Database:** Implement a debug-only global hash map in `string_id` to store original strings, allowing reverse lookup (Hash -> String) for easier debugging.
- [ ] **Const Correctness Audit:** Review public APIs to enforce `const` correctness for input pointers, improving safety and compiler optimization potential.
- [ ] **Header Dependency Cleanup:** Refactor headers to reduce inclusion pollution, ensuring each header includes what it uses (IWYU) and uses forward declarations where possible.
- [x] **Unified Frame Memory:** Implement a central `FrameArena` in `Engine` that is reset daily. Refactor `UiRenderer` and other systems to use this arena instead of managing their own scratch memory.
- [ ] **Input Action Mapping:** Implement an abstraction layer to map physical keys (e.g., `KEY_Z`) to logical actions (e.g., `ACTION_UNDO`), removing hardcoded key checks from game logic.

### Phase 7: 3D Visualization & Compute
**Objective:** Visualize mathematical functions and data in 3D space.
- [ ] **Procedural Geometry:** Generate meshes from math functions (e.g., $z=f(x,y)$) using Compute Shaders.
- [ ] **Arcball Camera:** Implement an orbit camera for inspecting 3D surfaces.
- [ ] **Compute Particles:** Visualizing vector fields using particle systems.

### Phase 8: Editor & Tooling Maturity
**Objective:** Improve the user experience of the Visual Programming Environment.
- [ ] **Undo/Redo System:** Implement command history for graph operations.
- [ ] **Node Library Expansion:** Add Noise (Perlin/Simplex), Trigonometry, and Logic nodes.
- [ ] **Export System:** Export generated shaders (GLSL/SPIR-V) for external use.

---

## 🛠 Technical Debt & Backlog

*   **Shader Hot-Reloading:** Allow editing shaders at runtime without restarting.
