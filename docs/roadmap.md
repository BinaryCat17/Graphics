# Project Roadmap: Evolution to v1.0

**Current Status:** Architectural Refactoring (Transition to v0.9)
**Goal:** High-Performance Visualization Engine

---

## 🚨 Phase 1: The Great Split (Critical Architecture Fix)
*Objective: Separate UI and 3D rendering to fix sorting and performance issues.*

- [ ] **Render Packet Refactor:**
    - Create specialized structs: `UiBatchCmd` (for 2D) and `MeshDrawCmd` (for 3D).
    - Rewrite `RenderFramePacket` to contain separate buckets (lists) for UI and World.
- [ ] **Extraction Layer:**
    - Implement `scene_extract()` to walk the Logical Graph and populate these buckets.
- [ ] **Transient Geometry System:**
    - Implement a `DynamicVertexBuffer` in the Frame Arena.
    - Rewrite `text_renderer.c` to write vertices directly to this buffer instead of creating Objects.
- [ ] **Multi-Pass Renderer:**
    - Modify `vulkan_renderer.c` to execute passes sequentially:
        1. `WorldPass` (Perspective, Depth ON).
        2. `UiPass` (Ortho, Depth OFF).

## ⚡ Phase 2: Asynchronous Core
*Objective: Make the editor responsive; eliminate UI freezes.*

- [ ] **Job System:**
    - Implement a simple thread pool (`foundation/thread/job_system.c`).
- [ ] **Async Compiler:**
    - Move `glslc` calls to a background job.
    - Implement "Placeholder" rendering (show a spinner node while compiling).
- [ ] **Intermediate Representation (IR):**
    - Refactor `transpiler.c` to produce linear bytecode instead of direct string concatenation.

## 🎥 Phase 3: 3D Visualization Support
*Objective: Make the 3D Viewport actually useful.*

- [ ] **Camera Controller:**
    - Implement Arcball (Orbit) camera logic, distinct from UI scrolling.
- [ ] **3D Gizmos:**
    - Implement Translation/Rotation/Scale handles using the `WorldPass`.
- [ ] **Raycasting:**
    - Implement Ray-AABB intersection for 3D object selection.

## 🛠 Phase 4: Polish & Stability
*Objective: Production-ready features.*

- [ ] **Reflection V2:**
    - Update `codegen.py` to generate byte offsets for structs.
    - Replace `strcmp` in `ui_binding.c` with direct pointer arithmetic.
- [ ] **Undo/Redo:**
    - Implement a Command History stack for Graph operations.
- [ ] **Binary Assets:**
    - Create a build step to pack YAML/Shaders into binary blobs.