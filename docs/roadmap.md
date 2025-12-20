# Project Roadmap

**Current Focus:** Phase 6 - Architectural Hardening & 3D
**Date:** December 20, 2025

## 🏁 Current State (v0.7.1 Refactoring)

The project is undergoing a structural standardization to enforce strict public/private API boundaries before expanding into complex 3D features.

---

## 🚀 Active Phases

### Phase 6: Architectural Hardening & Cleanup (IMMEDIATE PRIORITY)
**Objective:** Enforce the "Public/Internal" separation pattern, optimize memory usage, and remove hardcoded logic.

- [ ] **Input Action Mapping:** Implement an abstraction layer to map physical keys (e.g., `KEY_Z`) to logical actions (e.g., `ACTION_UNDO`), removing hardcoded key checks from game logic.
- [ ] **Test Suite Expansion:** Expand unit tests to cover critical foundation modules (Memory, Strings, Containers) and ensuring automated execution in the build pipeline.
- [ ] **Math Engine Encapsulation:** Refactor `MathNode` to hide internal struct details behind an opaque handle API, ensuring changes to node logic do not break dependent features.
- [ ] **C Standard Downgrade Analysis:** Investigate feasibility of strict C99 compliance to maximize compiler portability (evaluating cost of losing C11 features like anonymous structs/unions).

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
*   **API Documentation:** Setup Doxygen or a similar tool to generate up-to-date API documentation from public headers.
