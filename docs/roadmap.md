# Project Roadmap

**Current Focus:** Phase 7 - 3D Visualization & Compute
**Date:** December 20, 2025

## 🏁 Current State 0.7.1 (Standardized)

Structural standardization (Phase 6) is largely complete, but critical limitations in the UI module were identified during the Phase 7 kickoff.

### Phase 4: Architectural Integrity & Custom Widgets (IMMEDIATE PRIORITY)
**Objective:** Restore architectural boundaries by implementing a "Custom Widget" protocol, eliminating the need for hardcoded Z-depths and dependency leaks between Engine and Features.
- [ ] **Clean Up:** Revert `src/engine/graphics/layer_constants.h` to only contain engine-level constants (UI_BASE, UI_OVERLAY). Remove feature-specific constants.
- [ ] **Engine API:** Implement `UI_KIND_CUSTOM` in `ui_core.h` and a renderer registry `ui_register_renderer(name, callback)`.
- [ ] **Render Loop Refactor:** Update `ui_renderer.c` to dispatch `UI_KIND_CUSTOM` elements to their registered callbacks, passing the correct Z-depth and clip rect.
- [ ] **Feature Integration:** Refactor `MathEditor` to register a "GraphWires" renderer. Move wire/port drawing logic into this callback.
- [ ] **YAML Migration:** Update `manifest.yaml` to use `type: custom` and `renderer: GraphWires` for the graph canvas.

### Phase 5: Stability & Validation (Refinement)
**Objective:** Resolve data-binding warnings and schema inconsistencies found during enhanced logging.
- [x] **Fix: White Inspector:** Resolve the persistent white background issue (still visible in screenshots).
- [x] **Fix: Missing Wires:** Debug why connections are invisible (Check Z-Order vs Canvas Background).
- [x] **Fix: Clear Button:** Fix layout overflow and remove white artifact under the button.
- [x] **Fix: UI Gradient:** Remove unwanted white gradient/highlight in the top-left sidebar header.
- [x] **Refactor: Magic Numbers:** Extract hardcoded layout constants (`150.0f`, `45.0f`) from `math_editor.c` into a `LayoutConfig` struct or defines.
- [x] **Refactor: Z-Order Calibration:** Fine-tune Z-values to ensure Wires render behind Nodes and the entire Canvas area remains below the Sidebar/Inspector.
- [x] **Fix: Global UI Clipping:** Ensure procedural elements (Wires, Ports) strictly respect the Canvas clipping rect to prevent overlapping with the Sidebar.
- [ ] **Optimization: Event-Based Evaluation:** Optimize `math_editor_update` to evaluate the graph only when dirty, not every frame.
- [ ] **Refactor: String Memory Strategy:** Replace the temporary leak fix in `reflection.c` with a proper Arena-based string allocation strategy.
- [x] **Refactor: Auto-Layout Buttons:** Remove hardcoded widths (e.g., "Clear" button) and use padding/flex-growth for localization support.
- [x] **Refactor: Z-Layer Constants:** Replace magic float values (`-9.985`, etc.) with a centralized `LayerDepth` enum or constants system to prevent sorting fragility.
- [ ] **Refactor: Input State Machine:** Replace the growing switch-statement in `math_editor_update` with a formal State Machine (Idle, DraggingNode, DraggingWire) for maintainability.
- [ ] **Fix: Sidebar Overflow:** Resolve layout overlap where the "Clear" button covers the "Time" palette item by adjusting container height or enabling scrolling.
- [ ] **Fix: Dangling Wire Artifact:** Investigate and fix the stray wire curve rendering artifact visible below the Multiply node.
- [ ] **Refactor: Inspector Style:** Style the white placeholder rectangle in the Properties panel to match the dark theme.
- [ ] **Refactor: Coordinate Spaces:** Fix the "Clip Rect Offset" hack by implementing proper Screen-to-World coordinate transformation for Graph connections.
- [ ] **Refactor: MathEditor Split:** Split `math_editor.c` into `_view` (Rendering) and `_logic` (Input/State) to reduce file complexity.
- [x] **Fix: String Ownership:** Fix `meta_set_string` to avoid `free()` on Arena-allocated strings (Critical for stability).
- [ ] **Feature: UI Z-Order:** Implement Z-indexing or "bring to front" logic for nodes in the Canvas.
- [ ] **Refactor: Wire ID Logic:** Replace bit-shifting ID generation with a safer 64-bit ID or hash-based system to avoid collisions.

### Phase 6: Structural Standardization (Refinement)
**Objective:** Address architectural limitations in the UI system to support complex editors.
- [ ] **Optimization:** Optimize Graph CPU evaluation to only recalculate dirty nodes.
- [ ] **Stability:** Implement Graph Cycle Detection to prevent stack overflow crashes on recursive evaluation.
- [ ] **Refactor:** Optimize Input System to use `StringId` instead of `strcmp` in the update loop.
- [ ] **UI Layout:** Implement Flexbox-style properties (`flex-grow`, `justify-content`) for robust responsive layouts.
- [ ] **Dynamic Node Inputs:** Remove `MATH_NODE_MAX_INPUTS` (4) limit. Implement dynamic array support for node inputs to allow variadic nodes (e.g., Sum(A, B, C, D, E)).
- [ ] **Asset Hot-Reloading:** Watch asset files for changes and reload graph/UI automatically without restarting.
- [ ] **Frame Allocator:** Introduce a per-frame temporary memory arena to avoid persistent memory leaks during hot-reloading or temporary parsing.
- [ ] **UI Text Input:** Implement proper UTF-8 support for text fields to handle Cyrillic and special characters.
- [ ] **UI Scrolling & Events:** Add inertia/smoothing to scrolling and use a dynamic event queue to prevent input loss.

### Phase 6.5: Graphics & Pipeline Maturity
**Objective:** Eliminate development shortcuts in the rendering pipeline and enforce strict architecture.
- [x] **Reflection: Generic String Setter:** Move `atoi`/`strtof` logic from UI to `meta_set_from_string` to centralize type parsing.
- [ ] **UI: Dynamic Command Registry:** Replace static `MAX_COMMANDS` array with a dynamic structure to support unlimited commands.
- [ ] **Refactor: MathEditor Separation:** Split `math_editor.c` into `math_editor_view.c` (Rendering) and `math_editor.c` (Logic/Input).
- [ ] **Foundation: String Safety:** Replace manual `strncpy`/`snprintf` with safe foundation wrappers to prevent buffer overflows.
- [ ] **Async Shader Compilation:** Replace blocking `system("glslc ...")` calls with a dedicated thread or library (shaderc) integration to prevent UI freezes.
- [ ] **Non-Blocking Screenshots:** Implement async GPU readback (PBO-style) to avoid `vkQueueWaitIdle` stalls during capture.
- [ ] **Configurable V-Sync:** Expose Swapchain Present Mode configuration (Immediate/FIFO/Mailbox) to the RenderSystem API.
- [ ] **Compute Visualization:** Move the debug Compute Result quad (currently hardcoded in `render_system_update`) to a proper UI Image Panel or Editor Node.
- [ ] **UTF-8 Text Rendering:** Implement UTF-8 decoding in the text renderer loop to support Cyrillic and other multi-byte characters.
- [ ] **Scene API Decoupling:** Introduce helper functions for creating common SceneObjects (Text, Panels) to avoid manual struct poking and magic numbers in higher layers.

The codebase enforces strict Public/Internal API boundaries across all modules. The Math Engine is fully encapsulated, and the foundation is covered by unit tests. The system is now ready for 3D procedural geometry and compute shader integration.

---

## 🚀 Active Phases

### Phase 7: 3D Visualization & Compute (IMMEDIATE PRIORITY)
**Objective:** Visualize mathematical functions and data in 3D space.
- [ ] **Procedural Geometry:** Generate meshes from math functions (e.g., $z=f(x,y)$) using Compute Shaders.
- [ ] **Arcball Camera:** Implement an orbit camera for inspecting 3D surfaces.
- [ ] **Compute Particles:** Visualizing vector fields using particle systems.

### Phase 8: Editor & Tooling Maturity
**Objective:** Improve the user experience of the Visual Programming Environment.
- [ ] **Undo/Redo System:** Implement command history for graph operations.
- [ ] **Node Library Expansion:** Add Noise (Perlin/Simplex), Trigonometry, and Logic nodes.
- [ ] **Export System:** Export generated shaders (GLSL/SPIR-V) for external use.
- [ ] **Topological Sort Evaluation:** Replace recursive CPU graph evaluation with a flat loop to prevent stack overflow on deep graphs.


---

## 🛠 Technical Debt & Backlog

*   **Shader Hot-Reloading:** Allow editing shaders at runtime without restarting.
*   **API Documentation:** Setup Doxygen or a similar tool to generate up-to-date API documentation from public headers.