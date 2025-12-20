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

### Phase 6: 3D & Scene Expansion (IN PROGRESS)
**Objective:** Move beyond 2D quads and prepare the engine for 3D content.
- [ ] **Mesh Rendering:** Implement 3D mesh loading (OBJ/GLTF) and rendering in `vulkan_renderer.c`.
- [ ] **Camera System:** Implement a proper 3D camera controller (Perspective/Orthographic).
- [ ] **Transform Hierarchy:** Upgrade `SceneObject` to support parent-child transforms.

### Phase 7: Advanced Rendering
**Objective:** Visual fidelity improvements.
- [ ] **Lighting:** Basic Phong/PBR implementation.
- [ ] **Shadow Mapping:** Directional light shadows.
- [ ] **Post-Processing:** Bloom, Tone Mapping.

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
