# Project Roadmap: Visual Compute Engine (v3.0)

**Vision:** "The Graph is the Source Code"
**Architecture:** Data-Oriented | GPU-Driven | Kernel Fusion

---

## 🧠 Phase 1: The Brain (Transpiler & Kernel Fusion)
*Objective: Teach the engine to understand and optimize mathematical graphs before execution.*

The most critical phase. Without this, the graph behaves like a slow interpreter.

- [x] **Graph AST (Micro Graph):**
    - Define data structures for kernel nodes (`Add`, `Mul`, `Sin`, `Sample`).
    - Implement AST construction from raw node data.
- [x] **Transpiler V2 (GLSL Emitter):**
    - Implement **Kernel Fusion**: traverse AST and generate a single `void main()` function body.
    - Support GLSL Compute Shader code generation.
- [x] **Graph Inputs & Uniforms:**
    - Implement system to define graph parameters (Time, Mouse, Resolution).
    - Dynamic generation of `layout(push_constant)` or Uniform Blocks.
- [x] **Texture Sampling:**
    - Support `sampler2D` data type in AST and Transpiler.
    - Implement `SampleTexture` node and `texture()` GLSL generation.
- [x] **CPU Fallback (C Emitter):**
    - (Optional) Generate C code for debugging and non-GPU logic.

## 💾 Phase 2: The Heart (Data & Compute Foundation)
*Objective: Build the infrastructure for data storage and kernel execution.*

Here we implement SoA and GPU memory management.

- [x] **Storage Infrastructure (SoA):**
    - Implement wrappers over Vulkan SSBO (Shader Storage Buffer Objects).
    - Manage "Streams" (`Stream<T>`) residing in VRAM.
- [x] **Compute Graph Executor (Macro Graph):**
    - [x] Implement `vkCmdDispatch` execution system.
    - [x] Automatic Memory Barrier insertion between compute stages.
    - [x] Ping-Pong buffering for simulations (read `State_A`, write `State_B`).

## 🎨 Phase 3: The Eyes (Zero-Copy Rendering)
*Objective: Render data directly from memory prepared in Phase 2.*

Abandon the classic `Scene::Update` -> `RenderPacket` flow.

- [x] **Render Nodes:**
    - Create `DrawInstanced` graph node accepting position/color buffers as inputs.
    - Implement SSBO binding as Vertex Attributes (Zero-Copy).
- [x] **Unified Pipeline:**
    - Synchronization: Compute Queue -> Graphics Queue.
    - Integrate with existing `vulkan_renderer.c` (using it as a "draw command executor").

## 🖱️ Phase 4: The Hands (Interaction & Editor)
*Objective: Make the engine an interactive tool.*

Hybrid approach to Input and UI.

- [x] **GPU Picking (Raycasting):**
    - [x] Implement Compute Kernel for mouse ray intersection (Sphere/AABB).
    - [x] Parallel Reduction to find the closest object ID.
- [x] **Graph Editor Rendering:**
    - [x] Render thousands of nodes via Instancing (node positions in GPU buffers).
    - [x] Spline links generated in Geometry/Compute shaders.
- [x] **Hybrid Input System:**
    - **CPU:** Standard event processing for Editor UI panels (Inspector, Menus).
    - **GPU:** Uniform `InputState` structure for Graph Nodes.

## 💾 Phase 5: Persistence & Ecosystem
*Objective: Save/Load systems and tooling.*

- [ ] **File Format Split:**
    - **Editor UI:** Keep and refine existing `*.layout.yaml` for static panels/menus (Declarative UI).
    - **Logic/Scene:** Design `*.gdl` (GDL) to serialize the Graph structure (Nodes, Links, Properties).
- [ ] **Hot Reloading:** Recompile the graph on-the-fly without restarting the app.
- [ ] **Visual Debugger:** GPU Buffer Readback to inspect values.
