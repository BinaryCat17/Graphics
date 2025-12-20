# The Architecture Guide

**Version:** 0.5 (Refined)
**Date:** December 20, 2025

This document explains **how** the system is built and, more importantly, **why** it is built that way.

---

## 1. The "Layered Cake" Philosophy

The codebase is organized into four strict layers. Dependencies flow **downwards only**.

```mermaid
graph TD
    App[App (src/app)] --> Features[Features (src/features)]
    Features --> Engine[Engine (src/engine)]
    Engine --> Foundation[Foundation (src/foundation)]
```

### 🧱 Foundation (`src/foundation/`)
**"The Bedrock"**
These are the tools that have **zero dependencies** on the rest of the engine.
*   **Memory:** Custom allocators (`Arena`, `Pool`). We strictly manage memory lifetimes.
*   **Platform:** OS Abstraction (`fs`, `platform`). Wraps Window creation, File IO, and Time.
*   **Math:** Linear algebra (`vec2`, `mat4`, `rect`).
*   **Logger:** Centralized logging (`LOG_INFO`, `LOG_ERROR`).
*   **Config:** Simple YAML parser (`simple_yaml`) for loading configuration files.
*   **Meta (Reflection):** Runtime introspection system for C structs. Essential for UI Data Binding.

### ⚙️ Engine (`src/engine/`)
**"The Machine"**
This layer manages the systems required to run an interactive application. It is agnostic to the specific game or tool being built.
*   **Core:** Lifecycle management (`Engine` struct), Main Loop.
*   **Graphics:** 
    *   `RenderSystem`: High-level command sorter and batcher.
    *   `RendererBackend`: Low-level Vulkan/Graphics API wrapper.
    *   `Scene`: Unified scene representation.
*   **UI:** The layout, widget, and command system.
*   **Assets:** Asset management and loading.

### 🧩 Features (`src/features/`)
**"The Logic"**
These are reusable, domain-specific modules. They use the Engine to function but contain their own business logic.
*   **Math Engine:** A visual programming node graph.
    *   **Graph:** The data structure of nodes and connections.
    *   **Transpiler:** Converts the graph into an intermediate representation (IR).
    *   **Emitters:** Converts IR into target code (e.g., GLSL).

### 🚀 App (`src/app/`)
**"The Glue"**
The entry point (`main.c`). It orchestrates the layers.
*   It initializes the **Engine** with a config.
*   It instantiates **Features**.
*   It connects **UI Events** to **Feature Logic**.

---

## 2. Core Principles

### 🧠 Data-Driven Design
*   **Definition:** Behavior is defined in data, not code.
*   **Example:** The UI is built from `manifest.yaml`. The Engine purely interprets this data.

### ⚡ Reactivity (MVVM / ViewModel)
*   **Definition:** The View updates automatically when the Model changes, but the Model never knows about the View.
*   **Implementation:** 
    *   **Model:** Pure logic data (e.g., `MathNode` with IDs and values).
    *   **ViewModel:** Editor-specific data (e.g., `MathNodeView` with `x, y` coordinates and cached labels).
    *   **Mechanism:** The Engine's Reflection system binds UI fields to the ViewModel. The Editor synchronizes Logic -> ViewModel once per frame.

### 🛡 Memory Safety
*   **Rule:** **Zero** `malloc`/`free` in the hot loop.
*   **Mechanism:**
    *   **Arenas:** Linear allocators for frame-scope or permanent data (e.g., `MathGraph` nodes).
    *   **Pools:** O(1) alloc/free for dynamic objects with stable IDs (e.g., `UiElement`).
    *   **Linked Lists:** Used for hierarchies (UI) to avoid dynamic array reallocations.

### 🔗 Strict Decoupling
*   **Rule:** Systems communicate via opaque IDs or Commands, not pointers.
*   **Architecture Hardening (v0.5):** The `Engine` header is now isolated from UI implementation details. `InputState` is moved to a dedicated core header to prevent dependency leaks.

---

## 3. Detailed Subsystems

### The Graphics System
Split into Data, Manager, and Worker.
1.  **Unified Scene (`scene.h`):** A flat array of `SceneObject`s. Everything (Text, UI, 3D) is an object.
2.  **RenderSystem (`render_system.h`):** Sorts objects (Z-index), culls, and builds render packets.
    *   **Opaque Handle:** The `RenderSystem` struct is hidden. The App interacts only via the API (`render_system_draw`, `render_system_create`), ensuring strict encapsulation of the backend.
3.  **RendererBackend (`renderer_backend.h`):** Executes Vulkan commands.

### The Input System
A hybrid Polling/Event-Driven architecture.
1.  **InputState (`input_types.h`):** Continuous state (Mouse coordinates, keys currently held down). Best for smooth movement (e.g., camera control, drag-and-drop).
2.  **InputEventQueue (`input_types.h`):** Discrete events (Key Pressed, Char Typed, Scroll, Click). Best for "trigger" actions (e.g., toggle visualizer, text entry) to ensure no inputs are lost between frames.
3.  **Flow:** Platform Callbacks -> Engine Queue -> UI System / App Logic.

### The Math Engine (Feature)
A complete transpiler pipeline embedded in the application.
1.  **Graph Model:** Nodes and connections stored in Memory Pools.
2.  **IR (Intermediate Representation):** The graph is flattened into a linear instruction set (SSA-like).
3.  **Codegen:** The IR is traversed to emit GLSL code, which is then compiled by the backend.

### The UI System
1.  **Parser:** Reads YAML and creates a simplified internal tree.
2.  **Structure:** `UiElement`s form a **Doubly Linked List** (`first_child`, `next_sibling`), allocated from a fixed-size `MemoryPool`. This eliminates `malloc`/`free` and dynamic arrays.
3.  **Layout:** Calculates absolute positions (AABB) for all widgets.
4.  **Renderer:** Converts widgets into `SceneObject`s for the `RenderSystem`.

---

## 4. Build & Tooling

*   **CMake:** Main build system.
*   **Python Scripts:**
    *   `tools/codegen.py`: Generates reflection metadata.
    *   `tools/build_shaders.py`: Compiles GLSL to SPIR-V.

---

## 5. Directory Structure

```text
src/
├── app/                  # Application Entry Point
├── features/             # Business Logic Modules
│   └── math_engine/      # Example: Node Graph Editor
│       ├── emitters/     # Code generation backends
│       └── ...
├── engine/               # Core Systems
│   ├── core/             # Engine Lifecycle
│   ├── assets/           # Asset Loading
│   ├── graphics/         # Rendering Pipeline
│   └── ui/               # UI System
└── foundation/           # Low-level Utilities
    ├── config/           # YAML Parser
    ├── logger/           # Logging
    ├── memory/           # Arenas/Pools
    └── ...
```