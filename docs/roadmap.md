# Project Roadmap

**Current Focus:** Phase 7 - 3D Visualization & Compute
**Date:** December 20, 2025

## 🏁 Current State 0.7.1 (Standardized)

Structural standardization (Phase 6) is largely complete, but critical limitations in the UI module were identified during the Phase 7 kickoff.

### Phase 6: Structural Standardization (Refinement)
**Objective:** Address architectural limitations in the UI system to support complex editors.
- [x] **UI Data Binding:** Support iteration over contiguous arrays and generic pointer arrays (currently limited to specific pointer arrays).
- [x] **UI Conditional Templates:** Support logic-based template selection (e.g., `template_selector: "type"`) to handle polymorphic collections (like heterogeneous node lists).
- [x] **UI Event Safety:** Implement typesafe wrappers for event callbacks to replace brittle `void*` casting and string-based logic.
- [x] **Declarative Node Palette:** Replace hardcoded "Add Node" logic with a data-driven palette system.
- [x] **Generic YAML Deserializer:** Implement a reflection-based YAML loader (`config_load_struct_array`) to replace manual parsing logic.
- [x] **Graph Serialization:** Move hardcoded default graph setup to a data file (YAML/JSON) to support saving/loading. Use strict DTO separation (Logic/Layout).
- [ ] **UI Layout:** Implement Flexbox-style properties (`flex-grow`, `justify-content`) for robust responsive layouts.
- [ ] **Dynamic Node Inputs:** Remove `MATH_NODE_MAX_INPUTS` (4) limit. Implement dynamic array support for node inputs to allow variadic nodes (e.g., Sum(A, B, C, D, E)).
- [ ] **Asset Hot-Reloading:** Watch asset files for changes and reload graph/UI automatically without restarting.
- [ ] **Frame Allocator:** Introduce a per-frame temporary memory arena to avoid persistent memory leaks during hot-reloading or temporary parsing.

The codebase enforces strict Public/Internal API boundaries across all modules. The Math Engine is fully encapsulated, and the foundation is covered by unit tests. The system is now ready for 3D procedural geometry and compute shader integration.

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
- [ ] **Topological Sort Evaluation:** Replace recursive CPU graph evaluation with a flat loop to prevent stack overflow on deep graphs.


---

## 🛠 Technical Debt & Backlog

*   **Shader Hot-Reloading:** Allow editing shaders at runtime without restarting.
*   **API Documentation:** Setup Doxygen or a similar tool to generate up-to-date API documentation from public headers.