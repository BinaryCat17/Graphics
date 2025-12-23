# Project Roadmap

**Current Focus:** Phase 7 - 3D Visualization & Compute
**Status:** Architecture Refactoring Complete (v0.7.1)

---

## 📅 Active Development (Phase 7: 3D Visualization)
*Objective: Transition from 2D UI Graphs to procedural 3D Geometry.*

### 🛠 Core Tech
- [ ] **SceneObjectProvider API:** Create an interface to inject 3D objects into the UI scene without hardcoding (for the Viewport).
- [ ] **Compute Grid:** Implement `MATH_NODE_SURFACE_GRID` producing a heightmap texture via Compute Shader.
- [ ] **Async Shader Compilation:** Move `glslc` calls to a worker thread to prevent UI freezes.

### 🎥 3D Interaction (Crucial)
- [ ] **Arcball Camera:** Implement an orbital camera for the 3D viewport (Rotate/Pan/Zoom).
- [ ] **Raycasting System:**
    * Implement `SceneRaycaster` to replace simple 2D `point_in_rect`.
    * Support Ray-AABB/Sphere intersections for selecting 3D nodes.
    * Coordinate space conversion (Screen -> World -> Local).
- [ ] **3D Gizmos:** Render transformation handles (Arrows/Rings) for manipulating 3D objects.

---

## 🔮 Backlog & Future Improvements

### 🏗 Architecture & Stability (Phase 6.5)
- [ ] **Input System Refactor:** Use `StringId` hashes instead of string comparisons for Actions.
- [ ] **Reflection Hardening:** Improve `codegen.py` robustness against complex C syntax.
- [ ] **Renderer Modularization:** Split `vulkan_renderer.c` into `vk_texture`, `vk_compute`, `vk_pipeline`.
- [ ] **UTF-8 Support:** Full Unicode support in Text Renderer and Input.

### 🖥 UI & Editor Experience
- [ ] **Focus Management:** Implement a `FocusStack` for modals and overlapping windows.
- [ ] **Typed Events:** Allow UI events to carry complex payloads (not just signals).
- [ ] **Undo/Redo:** Command history for Graph operations.
- [ ] **Node Library:** Expand with Trigonometry, Noise (Perlin), and Vector Math.

### 🚀 Advanced Graphics (v1.0+)
- [ ] **Retained Mode 3D:** Optimization for static geometry (don't upload vertices every frame).
- [ ] **PBR Materials:** Implement basic Physically Based Rendering (Roughness/Metalness).
- [ ] **Shadow Mapping:** Basic directional shadows for the 3D viewport.