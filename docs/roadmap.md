# Project Roadmap

**Current Focus:** Phase 7 - 3D Visualization & Compute
**Date:** December 20, 2025

## 🏁 Current State 0.7.1 (Standardized)

Structural standardization (Phase 6) is largely complete, but critical limitations in the UI module were identified during the Phase 7 kickoff.

### Phase 3: Compositional Scene Architecture (Architectural Pivot)
**Objective:** Transition from a UI-centric generic node to a specialized component-based Scene Graph. This is critical for the "Math Engine" goal to support complex physical simulations and 3D hierarchies without polluting the UI logic.

**3.0. YAML Migration (IMMEDIATE NEXT STEP) - DONE ✅**
*   [x] **Migrate YAML Assets:** The C code now expects nested components (`layout`, `style`), but YAML files are still flat.
    *   Action: Update all files in `assets/ui/**/*.yaml`.
    *   Example: Move `width: 100` -> `layout: { width: 100 }`, `color: ...` -> `style: { color: ... }`.
    *   Target files: `templates/node.yaml`, `layouts/editor_layout.yaml`, etc.

**3.1. Data Structuring (Refactoring `UiNodeSpec`) - DONE ✅**
*   [x] **Component Decomposition:** Break the flat `UiNodeSpec` struct into semantic sub-structs:
    *   `SceneTransformSpec` (pos, rot, scale)
    *   `UiLayoutSpec` (width, padding, flex props)
    *   `UiStyleSpec` (colors, borders, effects)
*   [x] **Codebase Migration:** Update `ui_parser.c`, `ui_layout.c`, and `ui_renderer.c` to access data via these sub-structs.
*   [x] **Reflection Update:** `tools/codegen.py` and `ui_parser.c` now handle nested `META_TYPE_STRUCT` recursively.

**3.2. The Unified Scene Graph (Runtime) - DONE ✅**
*   [x] **Transform System:** Introduce `Mat4 local_matrix` and `Mat4 world_matrix` to the runtime node (formerly `UiElement`).
*   [x] **Scene Node Rename:** (Gradual) Alias `UiElement` to `SceneNode`. *Refactoring completed: struct renamed to SceneNode globally.*
*   [x] **Matrix Propagation:** Implement a pass to update World Matrices (`Parent * Local`) before layout/rendering.
*   [x] **Layout Integration:** Modify the UI Layout solver to output results into the `SceneTransformSpec` or `local_matrix`, bridging the gap between Flexbox (2D) and Scene (3D). *(Implemented in update loop)*

**3.3. 3D Capability Expansion & Renaming**
*   [x] **Refactor: Symbol Renaming:** Align terminology with the new architecture.
    *   `UiAsset` -> `SceneAsset` (Container for SceneSpecs)
    *   `UiInstance` -> `SceneTree` (Runtime container for SceneNodes)
    *   `ui_element_*` -> `scene_node_*`
    *   `ui_renderer_build_scene` -> `scene_builder_build`
*   [x] **Refactor: Module Renaming:** Move `src/engine/ui` -> `src/engine/scene_system`.
*   [x] **Mesh Component:** Add `SceneMeshSpec` (mesh asset id, material params) to the Node Spec.
*   [x] **Renderer Adaptation:** Update `ui_renderer_build_scene` (rename to `scene_builder_build`) to check for `MeshSpec`. If present, emit 3D `SceneObject`s instead of UI Quads.
*   [x] **Binding V2 (Deep Cleanup):** Refactor the Data Binding system to support dot-notation/paths (e.g., `target: "transform.position.x"`).
    *   **Objective:** Remove legacy hardcoded binding fields (`bind_x`, `bind_y`, `bind_w`, `bind_h`, `bind_text`, `bind_visible`) from the root `SceneNodeSpec`.
    *   **Implementation:** The binding system should resolve targets recursively within components (`spec->layout.width`) rather than expecting them at the root.

**3.4. The Great Refactoring: Scene vs. UI Separation (New) - DONE ✅**
*   [x] **Analysis:** Determine exact ownership of `SceneNode` data. Split "Graph Structure" from "UI Behavior".
*   [x] **Refactor: Scene Core:** Move `SceneNode`, `Transform`, `SceneTree`, and Hierarchy logic into `src/engine/scene`. Make `src/engine/scene` the "Owner of the World".
*   [x] **Refactor: UI Resurrection:** Re-create `src/engine/ui` as a System that operates *on* `SceneNode`s. It handles Layout, Styling, and Input Bubbling.
*   [x] **Cleanup:** Dissolve `src/engine/scene_system`, distributing its contents between `scene` (core) and `ui` (system).

### Phase 3.5: The Great Merge (Scene & UI Unification)
**Objective:** Finalize the decoupling of the Scene System from the UI Module, moving core functionality (Parsing, Bindings) into the Scene Engine.
- [x] **Refactor: Move Parser:** Migrate `ui_parser.c` from `src/engine/ui` to `src/engine/scene/loader`. The Scene Core must be able to load itself without UI dependencies.
- [x] **Refactor: Unify Bindings:** Rename `UiBinding` (runtime) to `SceneBinding` and move it to `src/engine/scene`. Allow any SceneNode property to be bound, not just UI fields.
- [x] **Refactor: Flag Consolidation:** Review `UiFlags` vs `SceneNodeFlags`. Ensure `SceneNode` has a unified flag system where UI-specific flags use reserved bits or a dedicated subsystem mask.
- [ ] **Cleanup:** Remove legacy `UiAsset` stubs if they are fully replaced by `SceneAsset`.

### Phase 3.6: Architecture Hardening (Cleanup)
**Objective:** Solidify the foundation after the "Great Merge" of UI and Scene systems. Remove "construction debris" and ensure the engine is truly 3D-ready, not just a UI engine wrapper.
- [x] **Refactor: Loader Decoupling:** Remove UI-specific hardcoding from `scene_loader.c`.
    *   [x] Remove legacy bindings support (`bind_x`, `bind_y`) in favor of the generic `bindings` array.
    *   [ ] Remove default UI values (layout/style) from the parser; let the `UiSystem` apply defaults.
- [x] **Optimization: Asset Caching:** Implement a global `SceneAssetCache` (Path -> Asset*) to prevent re-parsing the same YAML templates (e.g., ports, nodes) multiple times.
- [ ] **Refactor: Data-Driven Node Kinds:** Replace hardcoded string checks (`strcmp("text")`) in the parser with a reflection-based or hash-based lookup for `SceneNodeKind`.
- [ ] **Feature: 3D Input (Raycasting):** Replace 2D `point_in_rect` hit-testing with Ray-AABB/Plane intersection.
    *   Support clicking on nodes rotated via `world_matrix`.
    *   Ensure events bubble correctly up the 3D hierarchy.
- [ ] **Refactor: Flag Segregation:** Split `SceneNodeFlags` into generic Scene flags and logic-specific flags to avoid "God Enums".
    *   `SceneFlags`: Core state (`HIDDEN`, `DIRTY`).
    *   `InteractionFlags`: Shared inputs (`CLICKABLE`, `FOCUSABLE`).
    *   `TypeFlags`: Context-dependent flags based on Node Kind (e.g., `SCROLLABLE` for Containers, `CAST_SHADOWS` for Meshes).
- [ ] **Refactor: MathEditor Separation:** Strict separation of Logic vs View.
    *   `MathGraph`: Pure logic, knows nothing about Scene/UI.
    *   `MathGraphView`: Listens to Graph events and manages `SceneNode`s.

**4. 3D Interaction & Logic (New)**
*   [ ] **Camera Component:** Implement `SceneCameraSpec` (FOV, type: Ortho/Perspective) to define viewports dynamically within the Scene Graph.
*   [ ] **Input Raycasting:** Extend `ui_input.c` to support Ray-AABB/Ray-Sphere intersection for clicking 3D objects (replacing pure 2D `point_in_rect` for 3D nodes).
*   [ ] **Layout/Transform Arbitration:** Define explicit rules: does Layout write to Transform? Does Transform override Layout? Implement flags (e.g., `SCENE_FLAG_IGNORE_LAYOUT`) to mix UI and 3D freely.

### Phase 4.5: Code Hygiene & Refactoring (Follow-up)
**Objective:** Address technical debt identified during the Viewport implementation to ensure the editor is maintainable and scalable.
- [x] **Refactor: Explicit UI Render Modes:** Replace implicit rendering logic (Text/Texture/SDF guessing) with explicit `UiRenderMode` enum (`Box`, `Text`, `Image`, `Bezier`) in `UiNodeSpec`.
- [x] **Refactor: Split MathEditor:** Decompose `math_editor.c` into `math_editor.c` (Lifecycle/Commands) and `internal/math_editor_view.c` (Rendering/UI Sync).
- [ ] **Refactor: Extract Input Logic:** Move event processing and state machine (Idle, Dragging) into `internal/math_editor_input.c` to clean up the main update loop.
- [ ] **Refactor: Data-Driven Style:** Extract hardcoded layout constants (node width, colors, spacing) into a `MathEditorConfig` struct loaded from YAML.
- [ ] **Refactor: Input State Machine:** Replace ad-hoc boolean flags with a formal FSM (`IDLE`, `DRAGGING_NODE`, `DRAGGING_WIRE`) for robust input handling.
- [ ] **Refactor: Coordinate System:** Implement `GraphViewTransform` to abstract Screen-to-World conversions and prepare for Zoom/Pan, removing manual offset math.
- [ ] **Refactor: Iterative Graph Evaluation:** Replace recursive CPU graph evaluation with a flat topological sort loop.
- [ ] **Test: Math Graph Suite:** Implement unit tests for graph logic (connection, evaluation, cycle detection).

### Phase 5: Stability & Validation (Refinement)
**Objective:** Resolve data-binding warnings and schema inconsistencies found during enhanced logging.
- [ ] **Optimization: Event-Based Evaluation:** Optimize `math_editor_update` to evaluate the graph only when dirty.
- [ ] **Refactor: String Memory Strategy:** Replace temporary leak fixes with Arena-based allocation.
- [ ] **Fix: Sidebar Overflow:** Resolve layout overlap where the "Clear" button covers palette items.
- [ ] **Fix: Dangling Wire Artifact:** Investigate stray wire curve rendering artifacts.
- [ ] **Refactor: Inspector Style:** Style the white placeholder rectangle in Properties.
- [ ] **Feature: UI Z-Order:** Implement Z-indexing logic for nodes.
- [ ] **Refactor: Wire ID Logic:** Replace bit-shifting ID generation with a safer hash-based system.

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
- [ ] **Refactor: MathEditor Separation:** Split `math_editor.c` into `math_editor_view.c` (Rendering) and `math_editor.c` (Logic/Input).
- [ ] **Foundation: String Safety:** Replace manual `strncpy`/`snprintf` with safe foundation wrappers to prevent buffer overflows.
- [ ] **Async Shader Compilation:** Replace blocking `system("glslc ...")` calls with a dedicated thread or library (shaderc) integration to prevent UI freezes.
- [ ] **Non-Blocking Screenshots:** Implement async GPU readback (PBO-style) to avoid `vkQueueWaitIdle` stalls during capture.
- [ ] **Configurable V-Sync:** Expose Swapchain Present Mode configuration (Immediate/FIFO/Mailbox) to the RenderSystem API.
- [ ] **Compute Visualization:** Move the debug Compute Result quad (currently hardcoded in `render_system_update`) to a proper UI Image Panel or Editor Node.
- [ ] **UTF-8 Text Rendering:** Implement UTF-8 decoding in the text renderer loop to support Cyrillic and other multi-byte characters.

The codebase enforces strict Public/Internal API boundaries across all modules. The Math Engine is fully encapsulated, and the foundation is covered by unit tests. The system is now ready for 3D procedural geometry and compute shader integration.

---

## 🚀 Active Phases

### Phase 7: 3D Visualization & Compute (DEPENDS ON PHASE 3)
**Objective:** Visualize mathematical functions and data in 3D space.
- [ ] **Procedural Geometry:** Generate meshes from math functions (e.g., $z=f(x,y)$) using Compute Shaders.
- [ ] **Arcball Camera:** Implement an orbit camera for inspecting 3D surfaces.
- [ ] **Compute Particles:** Visualizing vector fields using particle systems.

### Phase 8: Editor & Tooling Maturity
**Objective:** Improve the user experience of the Visual Programming Environment.
- [ ] **Undo/Redo System:** Implement command history for graph operations.
- [ ] **Node Library Expansion:** Add Noise (Perlin/Simplex), Trigonometry, and Logic nodes.
- [ ] **Export System:** Export generated shaders (GLSL/SPIR-V) for external use.


---

## 🛠 Technical Debt & Backlog

*   **Shader Hot-Reloading:** Allow editing shaders at runtime without restarting.
*   **API Documentation:** Setup Doxygen or a similar tool to generate up-to-date API documentation from public headers.