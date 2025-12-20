# Project Roadmap

**Current Focus:** Phase 7 - 3D Visualization & Compute
**Date:** December 20, 2025

## 🏁 Current State 0.7.1 (Standardized)

Structural standardization (Phase 6) is complete. The codebase enforces strict Public/Internal API boundaries across all modules. The Math Engine is fully encapsulated, and the foundation is covered by unit tests. The system is now ready for 3D procedural geometry and compute shader integration.

---

## 🚀 Active Phases

### Phase 7: 3D Visualization & Compute (IMMEDIATE PRIORITY)
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