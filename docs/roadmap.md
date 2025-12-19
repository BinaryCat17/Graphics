# Project Roadmap

**Current Focus:** v0.2.1 - Data-Driven UI Modernization
**Date:** December 19, 2025

## ✅ Recent Achievements

*   **Compute:** Implemented Vulkan Compute pipeline, Synchronization, and Visualizer.
*   **Scalability:** Resolved the "1k Object Limit" via dynamic buffers.
*   **Architecture:** Implemented Shader IR (Intermediate Representation) and decoupling.
*   **UI Core:** Implemented Event System and Dirty Flags for layout optimization.

---

## 🚀 Active Phases

### Phase 4: Modern Data-Driven UI (New Architecture)
**Objective:** Create a beautiful, reactive, and lightweight UI system without the bloat of web frameworks or the mess of Immediate Mode.
- [x] **Step 1: Visual Polish (SDF Rendering):** Implemented Signed Distance Fields (SDF) in shaders for rounded corners, soft shadows, and borders.
- [x] **Step 2: Optimized Binding (Fast Reactivity):** Used the **Reflection System** to pre-resolve data pointers (`MetaField*`) at initialization, eliminating per-frame string lookups.
- [x] **Step 3: YAML Prefabs & Templates:** Implemented `import` directive for modular UI files and C-side template instantiation. Added support for **Hex Colors** (`#RRGGBB`).
- [ ] **Step 3.5: Advanced Composition (Strict Imports):** Implement `type: instance` to allow YAML-in-YAML instantiation using registered templates. Refactor parser to enforce top-level imports and **ban** inline `import` in children arrays.
- [ ] **Step 4: Declarative Animations:** Implement state transition logic (e.g., `hover -> scale: 1.2`) driven purely by data/YAML.
- [ ] **Step 5: Advanced Layout (Docking):** Implement "Split Containers" logic to support dynamic docking without a monolithic manager.
- [ ] **Step 6: Command System:** Implement a "Command Pattern" registry to decouple UI events from C logic (e.g., `on_click: "Graph.AddNode"`).
- [ ] **Step 7: Strict Layout Validation:** Add a validation pass to ensure UI specs adhere to layout rules (e.g., error if `x/y` is used inside a Flex container).

### Phase 5: 3D & Scene Expansion
**Objective:** Move beyond 2D quads.
- [ ] **Mesh Rendering:** Implement 3D mesh loading and rendering in `vulkan_renderer.c`.
- [ ] **Camera System:** Implement a proper 3D camera controller (Perspective/Orthographic).

---

## 🛠 Technical Debt & backlog

### Core Optimization
- **String Hashing:** Replace string comparisons with `StringID` (FNV-1a hash) across the engine (UI, Assets, Registry) for O(1) lookups and reduced memory usage.

### Text Rendering
- **Issue:** Current implementation creates 1 draw call per character (via `SceneObject`).
- **Plan:** Implement Glyph Batching (single vertex buffer for text strings).

### UI Layout Optimization
- **Status:** Resolved (Cached Bindings & YAML Templates implemented).

### Legacy UI Cleanup
- **Inspector Refactor:** Convert `ui_rebuild_inspector` in `main.c` to use YAML Templates (e.g., `inspector_row.yaml`).
- **Dead Code:** Remove `ui_create_spec` and manual styling constants from `main.c`.
- **Parser Cleanup:** Remove support for inline `import` in `children` arrays once Step 3.5 is complete.
