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
- [ ] **UI Styling Data-Drive:** Remove hardcoded visual logic (e.g., `color *= 1.1f` for inputs) from `ui_renderer.c`. Move state-based visual changes into `UiStyle` or config data.
- [ ] **Uniform Error Handling:** Refactor `engine_create` and subsystem initializers to use a standardized `goto cleanup` pattern or shared destructor helper to prevent resource leaks and reduce code duplication on failure.

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

*   **Input Mapping:** Replace hardcoded keys (e.g., `KEY_Z`) with an Action Mapping system.
