# Project Roadmap

**Current Focus:** Phase 6 - 3D & Scene Expansion
**Date:** December 20, 2025

## 🏁 Current State (v0.6 Pre-Alpha)

The project has transitioned to a stable architecture with strict encapsulation.
*   **Architecture Hardening:** Complete. The Engine uses opaque handles (RenderSystem), discrete Input Events, and proper MVVM separation for the Editor.
*   **Memory:** Standardized Arena/Pool usage across all systems.
*   **Input:** Hybrid Event/Polling system is active.

---

## 🚀 Active Phases

### Phase 6: 3D Visualization & Compute (IN PROGRESS)
**Objective:** Visualize mathematical functions and data in 3D space.
- [ ] **Procedural Geometry:** Generate meshes from math functions (e.g., $z=f(x,y)$) using Compute Shaders.
- [ ] **Arcball Camera:** Implement an orbit camera for inspecting 3D surfaces.
- [ ] **Compute Particles:** Visualizing vector fields using particle systems.

### Phase 7: Editor & Tooling Maturity
**Objective:** Improve the user experience of the Visual Programming Environment.
- [ ] **Undo/Redo System:** Implement command history for graph operations.
- [ ] **Node Library Expansion:** Add Noise (Perlin/Simplex), Trigonometry, and Logic nodes.
- [ ] **Export System:** Export generated shaders (GLSL/SPIR-V) for external use.
- [ ] **Graph Optimization:** Dead code elimination in the Transpiler.

---

## 📜 Completed Phases

### Phase 5: Architecture Hardening (Completed Dec 2025)
*   **Decoupled Logic/View:** Implemented ViewModel pattern.
*   **Encapsulation:** Hidden Engine headers and platform details.
*   **Interface Abstraction:** Opaque handles for `RenderSystem`.
*   **Input Upgrade:** Implemented Event Queue.
*   **Memory Audit:** Removed mallocs from hot paths.

---

## 🛠 Technical Debt & Backlog

### Text Rendering
- **Issue:** Current implementation creates 1 draw call per character.
- **Plan:** Implement Glyph Batching.
