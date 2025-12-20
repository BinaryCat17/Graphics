# Project Roadmap

**Current Focus:** Phase 6 - Architectural Hardening & 3D
**Date:** December 20, 2025

## 🏁 Current State (v0.7 Refactoring)

The project is undergoing a structural standardization to enforce strict public/private API boundaries before expanding into complex 3D features.

---

## 🚀 Active Phases

### Phase 6: Architectural Hardening & Cleanup (IMMEDIATE PRIORITY)
**Objective:** Enforce the "Public/Internal" separation pattern across all modules to prevent technical debt and remove legacy code.
- [ ] **Scene Encapsulation:** Apply Opaque Handle pattern to `Scene` struct (hide implementation in `internal/scene_internal.h`).
- [ ] **Shader Constants:** Extract magic numbers (e.g., UI rendering modes `3.0f`, `4.0f`) into a shared header/enum.
- [ ] **Font Memory Safety:** Replace raw `malloc` in `font.c` with the Foundation memory subsystem (`MemoryArena`).
- [ ] **Backend Cleanup:** Move screenshot IO logic (`stb_image_write`) out of `vulkan_renderer.c` into a dedicated Foundation module.

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

*None currently prioritized.*