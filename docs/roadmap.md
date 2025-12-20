# Project Roadmap

**Current Focus:** v0.5 - Architecture Hardening
**Date:** December 20, 2025

## 🏁 Current State (v0.5)

The project has completed a major architectural cleanup.
*   **Decoupling:** Logic models (MathNode) are now strictly separated from View models (MathNodeView).
*   **Encapsulation:** Engine headers are cleaned up; platform and UI implementation details are hidden.
*   **Memory:** Arena and Pool allocation is standard across all systems.

---

## 🚀 Active Phases

### Phase 5: Architecture Hardening (IN PROGRESS)
**Objective:** Enforce strict layer boundaries and improve compile times.
- [x] **Decouple Logic/View:** Remove UI data from `MathNode`. Implemented ViewModel pattern in Editor.
- [x] **Engine Encapsulation:** Hide `PlatformWindow` and split `InputState` from heavy UI headers.
- [x] **Header Hygiene:** Reduced include bloat in `engine.h`.
- [ ] **Interface Abstraction:** (Next) Define opaque handles for Engine systems to further hide implementation.
- [x] **Memory Audit:** 
    - [x] Refactor `simple_yaml.c` and `fs.c` to use scratch arenas instead of `malloc`.
    - [x] Cleanup `assets.c` path concatenation.
    - [x] Optimize `ui_renderer.c` overlay buffer (replace `realloc` with Arena).
- [ ] **Input System Upgrade:** Replace polling-based `InputState` with an Event Queue (essential for Editor shortcuts).
- [x] **Zero Warnings Policy:** Fix unused functions in `math_editor.c` and other pending warnings.

### Phase 6: 3D & Scene Expansion
**Objective:** Move beyond 2D quads and prepare the engine for 3D content.
- [ ] **Mesh Rendering:** Implement 3D mesh loading (OBJ/GLTF) and rendering in `vulkan_renderer.c`.
- [ ] **Camera System:** Implement a proper 3D camera controller (Perspective/Orthographic).
- [ ] **Transform Hierarchy:** Upgrade `SceneObject` to support parent-child transforms.

---

## 🛠 Technical Debt & Backlog

### Text Rendering
- **Issue:** Current implementation creates 1 draw call per character.
- **Plan:** Implement Glyph Batching.
