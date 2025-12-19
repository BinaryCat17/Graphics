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
- [ ] **Refactor `main.c`:** Extract `MathEditor` logic into `src/features/math_engine/math_editor.c`.
    *   Move `AppState`, `MathGraph`, and UI generation logic out of `main.c`.
    *   Prepare for future "Collection Binding" by isolating imperative UI construction.
- [ ] **Strict Type Safety:** Enforce type checking in Data Bindings.
- [ ] **String IDs:** Replace runtime `strcmp` with **Hash IDs**.
- [ ] **Layer System (Z-Order & Clipping):** Implement `layer: top/overlay` to handle Z-depth naturally and **disable parent clipping** for popups/dropdowns so they can "break out" of containers.
- [ ] **Conditional Visibility:** Add `bind_visible` / `bind_if` logic to declarative show/hide branches based on boolean flags.
- [ ] **Refactor `main.c`:** Move command callbacks to `EditorLayer`.
- [ ] **Input System Cleanup:** Refactor `ui_input.c`.
- [x] **Unused Code Removal:** Clean up structs.

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
