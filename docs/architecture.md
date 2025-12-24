# Architecture Overview (v4.0)

**Paradigm:** Data-Driven Modular Engine
**Philosophy:** "Strict Hierarchy, Loose Coupling"
**Target:** High-Performance Visual Tools

---

## 1. The Core Principles

### A. Strict Layering (The Flow of Dependency)
The engine is built in strict layers. A layer may only depend on layers **below** it. Circular dependencies are forbidden.

1.  **Application (App):** The entry point. Configures the engine and registers features.
2.  **Features (Plugins):** Isolated logic modules (e.g., `MathEngine`, `SceneEditor`). They implement specific behavior but do not own system resources.
3.  **Engine (Systems):** Major subsystems (`RenderSystem`, `InputSystem`, `AssetSystem`). They manage lifecycle and resources but contain *no business logic*.
4.  **Foundation (Base):** Zero-dependency utilities (`Memory`, `Math`, `Logger`, `Platform`).

### B. The Facade Pattern (Public vs. Internal)
Every module follows the **Iceberg Principle** to prevent architectural entropy:
*   **Public Interface (`module.h`):** Contains only opaque handles (`typedef struct System System;`) and API function prototypes. Minimal includes.
*   **Implementation (`src/internal/...`):** Contains the real structs, Vulkan headers, and logic. Hidden from the rest of the engine.
*   **Facade (`src/module.c`):** The root `.c` file acts as a thin wrapper that validates inputs and delegates work to the internal implementation.

### C. Data-Driven Pipeline
The engine loop is not hardcoded in C. It is defined declaratively in a **Pipeline Definition File (`.gdl`)**.
*   The engine reads the graph at startup.
*   It schedules systems and passes based on this graph.
*   *Benefit:* We can rearrange the rendering order or add compute passes without recompiling the C code.

---

## 2. System Architecture

### The "Dumb" Renderer
In v3.0, the Renderer knew about UI Nodes and 3D Objects. In v4.0, the Renderer is agnostic.
*   **Producers:** UI System, Scene System, Math Editor. They generate generic `RenderBatch` packets.
*   **Consumer:** Render System. It takes a list of `RenderBatch` and executes them. It does not know *what* it is drawing, only *how* to draw it (Mesh, Shader, Uniforms).

### The Macro Graph (Global Pipeline)
Manages the frame execution flow.
*   **Nodes:** `Pass` (e.g., "ShadowMapPass", "UIPass", "PhysicsStep").
*   **Edges:** Resource dependencies (Buffers/Textures).
*   **Execution:** Orchestrated by the Core, executed sequentially or in parallel (future).

### The Micro Graph (Compute Kernel)
Manages low-level math logic (The Math Engine Feature).
*   **Nodes:** Math operations (`Add`, `Sin`).
*   **Output:** JIT-compiled Compute Shaders (SPIR-V).
*   **Execution:** GPU-side numerical processing.

---

## 3. Data Flow Architecture

1.  **Input Phase:**
    *   Platform collects events.
    *   InputSystem normalizes them.
    *   Features react to events and update their internal state models.

2.  **Simulation Phase:**
    *   Features execute logic (physics steps, node graph evaluation, animation).
    *   State is updated. Dirty data is marked for upload.

3.  **Extraction Phase (The Great Decoupling):**
    *   *Critical Step:* Features extract visual data into linear **Command Lists** (`RenderBatch`).
    *   This happens on the CPU. The data is converted to GPU-friendly formats (SoA) here.
    *   This ensures the "Simulation" state is safe to modify while "Rendering" processes the snapshot.

4.  **Execution Phase:**
    *   RenderSystem takes the Command Lists.
    *   Executes the pipeline defined in `.gdl`.
    *   Submits work to GPU queues.
    