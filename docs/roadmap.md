# Project Roadmap

**Current Focus:** v0.4 - UI Stability & Pre-3D
**Date:** December 19, 2025

## 🏁 Current State (v0.4)

The project has successfully completed a major UI modernization phase.
*   **Architecture:** Stable, Memory-Safe, Data-Driven MVVM.
*   **UI Engine:** Feature-complete for 2D tools (Canvas, Flex, Splitters, Inputs).
*   **Optimization:** Zero per-frame allocations in the renderer.

We are now shifting focus from **Tooling/UI** to **Core Graphics/3D**.

---

## 🚀 Active Phases

### Phase 5: 3D & Scene Expansion
**Objective:** Move beyond 2D quads and prepare the engine for 3D content.
- [ ] **Mesh Rendering:** Implement 3D mesh loading (OBJ/GLTF) and rendering in `vulkan_renderer.c`.
- [ ] **Camera System:** Implement a proper 3D camera controller (Perspective/Orthographic) with input handling.
- [ ] **Transform Hierarchy:** Upgrade `SceneObject` to support parent-child transforms (currently flat).

---

## 🛠 Technical Debt & Backlog

### Text Rendering
- **Issue:** Current implementation creates 1 draw call per character (via `SceneObject`).
- **Plan:** Implement Glyph Batching (single vertex buffer for text strings).

### Input System
- **Issue:** Input state is currently polled. 
- **Plan:** Move to an Event-Based input queue for more reliable handling of fast keypresses.