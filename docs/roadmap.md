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
- [ ] **Step 1: Visual Polish (SDF Rendering):** Implement Signed Distance Fields (SDF) in shaders for rounded corners, soft shadows, and borders.
- [ ] **Step 2: Optimized Binding (Fast Reactivity):** Use the **Reflection System** to pre-resolve data pointers (`void* direct_ptr`) at initialization, eliminating per-frame string lookups. Explore **Codegen** for "Dirty Proxy" setters.
- [ ] **Step 3: YAML Prefabs & Templates:** Implement a powerful templating system in the parser to reuse UI components (e.g., `type: Button`).
- [ ] **Step 4: Declarative Animations:** Implement state transition logic (e.g., `hover -> scale: 1.2`) driven purely by data/YAML.
- [ ] **Step 5: Advanced Layout (Docking):** Implement "Split Containers" logic to support dynamic docking without a monolithic manager.

### Phase 5: 3D & Scene Expansion
**Objective:** Move beyond 2D quads.
- [ ] **Mesh Rendering:** Implement 3D mesh loading and rendering in `vulkan_renderer.c`.
- [ ] **Camera System:** Implement a proper 3D camera controller (Perspective/Orthographic).

---

## 🛠 Technical Debt & backlog

### Text Rendering
- **Issue:** Current implementation creates 1 draw call per character (via `SceneObject`).
- **Plan:** Implement Glyph Batching (single vertex buffer for text strings).

### UI Layout Optimization
- **Status:** Partially Resolved (Dirty Flags implemented).
- **Next:** Implement "Cached Bindings" (Phase 4, Step 2) to eliminate reflection overhead during updates.
