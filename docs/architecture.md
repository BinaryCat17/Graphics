# Architecture Overview (v3.0)

**Paradigm:** Implicitly Parallel Compute Graph
**Philosophy:** "The Graph is the Source Code"
**Target:** Visual Compute Engine for High-Performance Tools

---

## 1. Core Philosophy: The Two Graphs
The engine strictly separates high-level data flow from low-level mathematical logic to avoid "spaghetti code" and maximize performance.

### Level 1: The Macro Graph (The Pipeline)
* **Role:** Data Scheduling & Memory Management.
* **Nodes:** Systems or Simulation Steps (e.g., "ParticleSim", "Culling", "RenderPass").
* **Links:** Entire Buffers/Streams of data (`Stream<Vec3>`).
* **Execution:** Managed by the CPU. Handles synchronisation barriers and resource transitions (Compute -> Graphics).

### Level 2: The Micro Graph (The Kernel)
* **Role:** Mathematical Logic.
* **Nodes:** Atomic operations (`Add`, `Mul`, `Sin`, `Dot`).
* **Links:** Temporary registers / Local variables.
* **Execution:** **Compiled** into a single Compute Shader (Kernel Fusion) and executed on the GPU.
* **Benefit:** Eliminates memory bandwidth overhead by fusing multiple operations into one kernel.

---

## 2. Hybrid UI Architecture
We use the right tool for the job, avoiding a "pure GPU" dogma where it hurts usability.

### A. The Shell (Editor UI)
* **Technology:** CPU-based, Declarative Layouts (`*.layout.yaml`).
* **Role:** Panels, Menus, Inspectors, File Browsers.
* **Rendering:** Traditional textured quads / font atlas.
* **State:** Maintained on CPU.

### B. The Viewport (Project Content)
* **Technology:** GPU-based, Graph-Driven.
* **Role:** The node graph itself, the 3D scene, huge particle systems.
* **Rendering:** Instanced rendering from GPU buffers (Zero-Copy).
* **State:** Maintained in VRAM (SSBOs).

---

## 3. The Data Flow Pipeline
The engine executes a "Compute-First" loop:

1.  **Input (Hybrid):**
    *   CPU events update Editor UI.
    *   Mouse/Keyboard state is packed into a Uniform Buffer for the GPU.
2.  **Logic (Compute Queues):**
    *   The **Macro Graph** executes Compute Shaders.
    *   Physics, Animation, Layout calculations happen here.
    *   Result: Updated SSBOs (Positions, Colors).
3.  **Synchronization:**
    *   Memory Barriers ensure Compute is finished.
4.  **Render (Graphics Queue):**
    *   **Zero-Copy:** Vertex Shaders read directly from the SSBOs written in step 2.
    *   No data is copied back to CPU for rendering.

---

## 4. Key Systems

### The Transpiler (The Brain)
Converts the **Micro Graph** (AST) into GLSL/SPIR-V.
*   Performs **Kernel Fusion** to optimize math.
*   Handles type inference.

### Storage System (The Heart)
Manages `Stream<T>` arrays.
*   Uses **Structure of Arrays (SoA)** layout (e.g., `pos_x[]`, `pos_y[]`) for coalesced GPU access.
*   Double-buffered (Ping-Pong) where necessary for simulation time-steps.
