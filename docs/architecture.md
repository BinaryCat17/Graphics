# The Architecture Guide

**Version:** 0.2 (Math Engine Update)
**Date:** December 19, 2025

This document explains **how** the system is built and, more importantly, **why** it is built that way.

---

## 1. The "Layered Cake" Philosophy

The codebase is organized into four strict layers. Think of it like a building: you can change the furniture (App) without rebuilding the foundation, but if the foundation cracks, everything falls.

```mermaid
graph TD
    App[App (src/app)] --> Features[Features (src/features)]
    Features --> Engine[Engine (src/engine)]
    Engine --> Foundation[Foundation (src/foundation)]
```

### 🧱 Foundation (`src/foundation/`)
**"The Bedrock"**
These are the tools that have **zero dependencies** on the rest of the engine.
*   **Memory:** Custom allocators (Arenas, Pools). We rarely use `malloc` in the game loop.
*   **Platform:** Wrappers for Windows/Linux stuff (Window creation, File IO).
*   **Math:** Vectors, Matrices.
*   **Logger:** `LOG_INFO`, `LOG_ERROR`.
*   **Meta (Reflection):** A custom system allowing the engine to inspect C structs at runtime. Essential for UI Data Binding.

### ⚙️ Engine (`src/engine/`)
**"The Machine"**
This layer manages the lifecycle of the application. It knows how to draw things, play sounds, and handle input, but it doesn't know *what* game you are making.
*   **Graphics:** The renderer.
*   **UI:** The layout and widget system.
*   **Assets:** Loading files from disk.

### 🧩 Features (`src/features/`)
**"The Logic"**
These are reusable modules that define specific business logic. They use the Engine to do work, but they don't *contain* engine code.
*   **Math Engine:** The node graph editor logic. It calculates values and generates code, but it asks the App to actually run that code on the GPU.

### 🚀 App (`src/app/`)
**"The Glue"**
This is the entry point (`main.c`). It connects everything together.
*   It initializes the **Engine**.
*   It creates the **Feature** (Math Graph).
*   It tells the Engine to render the Feature.

---

## 2. Deep Dive: The Math Engine

The `math_engine` is designed to be a "Code Generator", not a "Renderer".

### The Problem
We want to run a math graph on the GPU.
*   If we generate **GLSL**, we can run on Vulkan/OpenGL.
*   If we want to run on WebGPU later, we need **WGSL**.
*   If we want to run on Metal, we need **MSL**.

### The Solution: Shader IR (Intermediate Representation)
We don't generate GLSL text directly. We generate a generic "list of instructions".

```
[Math Graph] --> [Shader IR] --> [Emitter] --> [Final Code]
```

1.  **Math Graph:** The user's nodes (Add, Sin, Mul).
2.  **Shader IR:** A simplified assembly language (`OP_ADD`, `OP_SIN`). It knows nothing about `{}` or `;`.
3.  **Emitter:** A small translator that converts IR to text.
    *   `glsl_emitter.c` -> Writes "vec3 x = ..."
    *   `wgsl_emitter.c` -> Writes "var x: vec3 = ..." (Future)

### Why this is cool
The `Math Engine` doesn't care about Vulkan or WebGPU. It just says "I need to Multiply these numbers". The **Emitter** handles the syntax.

---

## 3. Deep Dive: The Graphics System

The graphics layer is split into two parts to keep things clean.

### The Data: Unified Scene (`scene/scene.h`)
Instead of having separate lists for UI, 2D Sprites, and 3D Models, the engine uses a **Single Linear Array** of `SceneObject`s.

*   **Everything is an Object:** A UI button, a text character, and a 3D cube are all `SceneObject` structs.
*   **The Struct:**
    ```c
    typedef struct SceneObject {
        Vec3 position;
        Vec3 rotation;
        Vec3 scale;
        ScenePrimitiveType prim_type; // SCENE_PRIM_QUAD or SCENE_PRIM_MESH
        Vec4 color;
        // ...
    } SceneObject;
    ```
*   **Why?** This makes the `RenderSystem` incredibly simple. It doesn't need 10 different loops for 10 different types of things. It iterates the array **once**, sorts by Z-index (or Shader), and submits to the backend.

### The Manager: `RenderSystem` (`render_system.h`)
This is the **High-Level API**. Features and the App talk to this.
*   **You say:** "Draw this UI", "Render this scene", "Set the camera".
*   **It does:** Sorts objects, culls invisible items, and prepares packets for the backend.

### The Worker: `RendererBackend` (`renderer_backend.h`)
This is the **Low-Level Hardware Interface**. Only the `RenderSystem` talks to this.
*   **It does:** "Bind Vulkan Pipeline", "Copy Memory to GPU", "Submit Command Buffer".
*   **Why split it?** If we switch from Vulkan to DirectX 12, we only rewrite the Backend. The rest of the engine (UI, Game Logic) doesn't even notice.

> **Rule:** `src/features` should NEVER include `renderer_backend.h`. If a feature needs to do something complex (like Compute), the Engine should provide a clean abstraction for it.

### 3. UI System
*   **Mode:** **Retained Mode UI**. We build a persistent tree of `UiElement` objects (usually from YAML).
*   **Logic:** The system handles input, focus, and state. You don't rebuild the tree every frame; you only update the data it points to.
*   **Constraint:** Currently, the layout logic is "hungry" (it recalculates positions for the entire tree every frame).

---

## 4. The Build Pipeline & Tools

We automate the boring stuff using Python scripts invoked by CMake.

### Code Generation (`tools/codegen.py`)
*   **When:** Runs *before* C compilation.
*   **What:** Scans source files for structs/enums marked with `// REFLECT`.
*   **Output:** Generates `src/generated/reflection_registry.c`.
*   **Why:** Enables the UI to automatically read/write C structs (Data Binding) without manual boilerplate.

### Shader Compilation (`tools/build_shaders.py`)
*   **When:** Runs during the build.
*   **What:** Scans `assets/shaders/` for `.vert` and `.frag` files.
*   **Output:** Generates binary `.spv` files using `glslc`.
*   **Why:** The game loads pre-compiled binaries, avoiding runtime compilation overhead and dependencies.


---

## 5. Directory Structure Guide

```text
src/
├── app/                  
│   └── main.c            # The "Glue" code.
│
├── features/             
│   └── math_engine/      # The Logic Layer
│       ├── math_graph.c  # Node data structures.
│       ├── transpiler.c  # The "Compiler" (Graph -> IR).
│       ├── shader_ir.h   # The "Assembly Language" definitions.
│       └── emitters/     # The Translators
│           └── glsl_emitter.c
│
├── engine/               
│   ├── core/             # Application lifecycle.
│   ├── graphics/         
│   │   ├── render_system.c # The Manager (Logic).
│   │   └── backend/      
│   │       └── vulkan/   # The Worker (Driver calls).
│   └── ui/               
│
└── foundation/           # Independent Utilities (Memory, Math, Logs).
```

---

## 6. Known Constraints (Technical Debt)

Even good architectures have limits. Here are ours:

### 1. The "1000 Object" Limit
*   **What:** You can't draw more than 1000 items (quads/letters) at once.
*   **Why:** We use a fixed-size buffer in Vulkan to keep things simple for now.
*   **Fix:** We need to implement "Dynamic Buffer Resizing" (growing the buffer when full).

### 2. Slow Text
*   **What:** Each letter is a separate object.
*   **Why:** It was the fastest way to get text on screen for the prototype.
*   **Fix:** "Glyph Batching" (merging text into one big mesh).

### 3. Per-Frame Layout Recalculation
*   **What:** The UI engine recalculates the position and size of every element every single frame.
*   **Why:** It was simpler to implement than a complex "Dirty Flag" system (which only updates what changed).
*   **Fix:** Implement a "Dirty" system so layout only runs when a window is resized or content changes.