# Architecture Guide

**Version:** 0.8.0
**Philosophy:** Data-Oriented, C11, Zero-Allocation Loop.

---

## 1. The "Layered Cake" Structure
The engine enforces a strict unidirectional dependency flow (Downwards only).

1.  **🧱 Foundation** (`src/foundation`):
    * *Role:* Zero-dependency bedrock.
    * **Memory:** `MemoryArena` (linear) & `MemoryPool` (fixed-block). No `malloc` in hot paths.
    * **Meta:** Reflection system (`codegen.py`) for UI binding and serialization.
    * **Platform:** OS abstractions (Window, Input, FS, Threading).

2.  **⚙️ Engine** (`src/engine`):
    * *Role:* Core systems for interactive applications.
    * **Core:** Main loop, specialized allocators.
    * **Scene:** Unified world representation (UI + 3D).
    * **Graphics:** Stateless Vulkan backend consuming `RenderFramePacket`.
    * **UI:** Layout engine (Flexbox-like) and Event Bubbling.
    * **Input:** Action mapping and event queue.

3.  **🧩 Features** (`src/features`):
    * *Role:* Reusable domain-specific logic.
    * **Math Engine:** Node graph editor, Shader IR generation, Transpiler.

4.  **🚀 App** (`src/app`):
    * *Role:* Entry point. Orchestrates Engine and Features.

---

## 2. Interface Standards (Public vs Internal)
We strictly separate API from Implementation to prevent spaghetti code.

* **Root (`module.h`):** Defines *what* the module does. Opaque handles (`typedef struct X X;`).
* **Internal (`internal/module_internal.h`):** Defines *how* it works. Full struct definitions.
* **Rule:** External code includes `module.h`. Implementation includes `internal/*.h`.

---

## 3. Subsystems Deep Dive

### 🎬 The Unified Scene
Everything is a `SceneObject`. We do not have separate 2D and 3D renderers.
* **Storage:** Single linear `MemoryArena` reset every frame.
* **Polymorphism:** `SceneObject` uses an anonymous `union` to store data for UI (rects) or 3D (meshes).
* **Benefit:** Unified sorting, culling, and resource management.

### 🖼️ Graphics & Render Packet
Decoupled Logic from Rendering using **Double Buffering**.
1.  **Logic Step:** Builds a `Scene` in Arena A.
2.  **Submit:** Wraps `Scene` in `RenderFramePacket`.
3.  **Render Step:** Backend draws Arena A while Logic starts building Arena B.
* **Backend:** Stateless Vulkan. Rebuilds command buffers every frame. Supports Compute Pipelines.

### ⌨️ Input System
* **Layer:** Decoupled from GLFW.
* **Events:** Queue-based (Pressed, Released, Typed).
* **Mapping:** Logical Actions ("Undo", "CamRotate") mapped to physical keys.

### 🧠 Reflection & Binding
* **Codegen:** Python script scans headers for `// REFLECT` and generates metadata C code.
* **UI Binding:** UI elements bind to C structs via string paths (e.g., `target: "transform.pos.x"`).
* **Safety:** Allows building generic inspectors without manual boilerplate.

### 🧮 Math Engine (Feature)
* **Graph:** Node-based data flow.
* **IR:** Compiles graph to custom Bytecode (ShaderIR).
* **Transpiler:** Converts IR to GLSL/SPIR-V for GPU Compute execution.

---

## 4. Memory Strategy
* **Frame Arena:** Temporary data (packet generation, string formatting). Reset every frame. O(1).
* **Scene Assets:** Persistent data (Meshes, Textures). Loaded once.
* **Pools:** Fixed-size objects (Graph Nodes) that need stable pointers but dynamic lifetime.