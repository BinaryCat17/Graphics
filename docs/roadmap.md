# Project Roadmap

**Current Focus:** v0.3 - UI Modernization & Strictness
**Date:** December 19, 2025

## ✅ Recent Achievements

*   **UI Architecture (v0.3):**
    *   **Data-Driven:** Fully declarative YAML UI with Templates and Instantiation.
    *   **Reactivity:** Optimized Reflection Binding (cached pointers).
    *   **Layouts:** Implemented Flex, Canvas, and **Split Containers** (H/V).
    *   **Animations:** Declarative state transitions (`hover_color`, `animation_speed`).
    *   **Decoupling:** Implemented **Command System** (`Graph.AddNode`) to separate UI from Logic.
    *   **Strictness:** Added parser validation for layout rules.

---

## 🚀 Active Phases

### Phase 4.5: Code Cleanup & Strict Architecture
**Objective:** Harden the codebase after the rapid UI prototyping phase.
- [ ] **Strict Type Safety:** Enforce type checking in Data Bindings (e.g., error if binding a `float` field to a text input).
- [ ] **String IDs:** Replace runtime `strcmp` with **Hash IDs (StringID)** in the Registry, Assets, and UI System for O(1) performance.
- [ ] **Refactor `main.c`:** Move command callbacks and application logic into a dedicated `EditorLayer` module.
- [ ] **Input System Cleanup:** Refactor `ui_input.c` to fully rely on the Command System and remove ad-hoc event handling logic.
- [ ] **Unused Code Removal:** Scan for and remove unused struct fields (`UiFlags`, old layout params) and legacy functions.

### Phase 5: 3D & Scene Expansion
**Objective:** Move beyond 2D quads.
- [ ] **Mesh Rendering:** Implement 3D mesh loading and rendering in `vulkan_renderer.c`.
- [ ] **Camera System:** Implement a proper 3D camera controller (Perspective/Orthographic).

---

## 🛠 Technical Debt & backlog

### Core Optimization
- **String Hashing:** See Phase 4.5.

### Text Rendering

- **Issue:** Current implementation creates 1 draw call per character (via `SceneObject`).

- **Plan:** Implement Glyph Batching (single vertex buffer for text strings).
