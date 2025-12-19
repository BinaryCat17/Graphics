# Project Roadmap

**Current Focus:** v0.2 - Math Engine Prototype
**Date:** December 19, 2025

## ✅ Completed Milestones

### Phase 1: Compute Infrastructure
- [x] **Vulkan Compute:** Implemented `compute_pipeline_create`, `dispatch`, and storage image management in the backend.
- [x] **Synchronization:** Added dedicated compute fences and command buffers.
- [x] **Resource Management:** Automatic descriptor set handling for writing (Set 0) and reading (Set 2).

### Phase 2: Shader Pipeline & Architecture
- [x] **Shader IR:** Decoupled `math_engine` from specific shader languages (GLSL) using an Intermediate Representation.
- [x] **Build System:** Implemented `tools/build_shaders.py` for offline shader compilation (No runtime `glslc` dependency).
- [x] **API Agnosticism:** Introduced `ShaderTarget` enum to support future backends (WebGPU/Metal).

---

## 🚀 Active Phases

### Phase 3: The "Unified" Render Loop (Visualizer)
**Objective:** Connect the Compute results to the Screen.
- [ ] **Render System Integration:** Update `render_system.c` to trigger compute dispatch before the main render pass.
- [ ] **Visualization:** Render a full-screen quad (or UI Image) displaying the result texture (Descriptor Set 2).
- [ ] **Auto-Update:** Automatically re-transpile and re-run compute when graph nodes change.

### Phase 4: 3D & Scene Expansion
**Objective:** Move beyond 2D quads.
- [ ] **Mesh Rendering:** Implement 3D mesh loading and rendering in `vulkan_renderer.c`.
- [ ] **Camera System:** Implement a proper 3D camera controller (Perspective/Orthographic).

---

## 🛠 Technical Debt & backlog

### Scalability (The "1k Limit")
- **Issue:** The renderer has a hardcoded instance limit (`1000` quads).
- **Plan:** Implement dynamic buffer resizing or large fixed-page allocators.

### Text Rendering
- **Issue:** Current implementation creates 1 draw call per character (via `SceneObject`).
- **Plan:** Implement Glyph Batching (single vertex buffer for text strings).

### UI System
- **Issue:** UI layout and interaction are tightly coupled in `ui_core.c`.
- **Plan:** Further refine the Event System and separate Layout from Rendering logic.
