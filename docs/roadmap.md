# Project Roadmap

**Current Focus:** v0.2 - Math Engine Prototype
**Date:** December 19, 2025

## ✅ Completed Milestones

### Phase 1: Compute & Scalability
- [x] **Vulkan Compute:** Implemented `compute_pipeline_create`, `dispatch`, and storage image management in the backend.
- [x] **Synchronization:** Added dedicated compute fences and command buffers.
- [x] **Resource Management:** Automatic descriptor set handling for writing (Set 0) and reading (Set 2).
- [x] **Scalability (Dynamic Buffers):** Resolved the hardcoded 1k object limit. Instance buffers and descriptor pools now handle dynamic loads.

### Phase 2: Shader Pipeline & Architecture
- [x] **Shader IR:** Decoupled `math_engine` from specific shader languages (GLSL) using an Intermediate Representation.
- [x] **Build System:** Implemented `tools/build_shaders.py` for offline shader compilation (No runtime `glslc` dependency).
- [x] **API Agnosticism:** Introduced `ShaderTarget` enum to support future backends (WebGPU/Metal).

---

## 🚀 Active Phases

### Phase 3: The "Unified" Render Loop (Visualizer)
- [x] **Render System Integration:** Update `render_system.c` to trigger compute dispatch before the main render pass.
- [x] **Visualization:** Render a full-screen quad (or UI Image) displaying the result texture (Descriptor Set 2).
- [x] **Auto-Update:** Automatically re-transpile and re-run compute when graph nodes change.

### Phase 4: Modern Data-Driven UI (New Architecture)
**Objective:** Create a beautiful, reactive, and lightweight UI system without the bloat of web frameworks or the mess of Immediate Mode.
- [ ] **Step 1: Visual Polish (SDF Rendering):** Implement Signed Distance Fields (SDF) in shaders for rounded corners, soft shadows, and borders.
- [ ] **Step 2: Optimized Binding (Fast Reactivity):** Replace per-frame string lookups with cached pointer bindings (`void* direct_ptr`).
- [ ] **Step 3: Declarative Animations:** Implement state transition logic (e.g., `hover -> scale: 1.2`) driven purely by data/YAML.
- [ ] **Step 4: Advanced Layout (Docking):** Implement "Split Containers" logic to support dynamic docking without a monolithic manager.

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
