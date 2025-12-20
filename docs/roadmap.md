# Project Roadmap

**Current Focus:** Phase 6 - Architectural Hardening & 3D
**Date:** December 20, 2025

## 🏁 Current State (v0.7 Refactoring)

The project is undergoing a structural standardization to enforce strict public/private API boundaries before expanding into complex 3D features.

---

## 🚀 Active Phases

### Phase 6: Architectural Hardening (IMMEDIATE PRIORITY)
**Objective:** Enforce the "Public/Internal" separation pattern across all modules to prevent technical debt.
- [x] **UI System:** Refactor to `src/engine/ui/internal/`.
    - Move `ui_layout.*`, `ui_renderer.*`, `ui_parser.*`, `ui_command_system.*`.
    - Update includes.
- [x] **Math Engine:** Refactor to `src/features/math_engine/internal/`.
    - Move `transpiler.*`, `emitters/`, `shader_ir.h`.
- [x] **Graphics Engine:** Refactor to `src/engine/graphics/internal/`.
    - Move backend implementations and internal headers.
- [x] **Scene & Text:** Extract from Graphics to `src/engine/scene` and `src/engine/text`.
- [x] **Input System:** Extract from `engine.c` to `src/engine/input`.
- [x] **Feature Decoupling:** Remove Vulkan dependency from `feature_math_engine`.

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