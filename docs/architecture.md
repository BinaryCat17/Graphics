# The Architecture Guide

**Version:** 0.3 (UI Modernization)
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
*   **UI:** The layout, widget, and command system.
*   **Assets:** Loading files from disk.

### 🧩 Features (`src/features/`)
**"The Logic"**
These are reusable modules that define specific business logic. They use the Engine to do work, but they don't *contain* engine code.
*   **Math Engine:** The node graph editor logic. It calculates values and generates code, but it asks the App to actually run that code on the GPU.

### 🚀 App (`src/app/`)
**"The Glue"**
This is the entry point (`main.c`). It connects everything together.
*   It initializes the **Engine**.
*   It registers **Commands** (e.g., `Graph.AddNode`).
*   It tells the Engine to render the Feature.

---

## 2. Deep Dive: The UI System (v0.3)

The UI system follows a **Data-Driven, Reactive, MVVM** architecture.

### The Philosophy
1.  **Manifest (`manifest.yaml`):** The entry point that registers **Templates** (The Registry) and imports the **Root Layout**.
2.  **View (YAML Layouts)::** The structure defined in files like `editor_layout.yaml`.
3.  **Model (C Structs):** The state is plain C data.
4.  **ViewModel (Bindings):** The engine uses Reflection to glue them together.

### Key Features
*   **Manifest-Based Loading:** The engine loads a single manifest that acts as a "Library" of available components. This separates component definitions from the main layout.
*   **Templates & Instances:** UI components are loaded as templates and instantiated via `type: instance`.
*   **Strict Import Rules:**
    *   Imports are ONLY allowed at the top level of a file (Manifest or Root).
    *   **NEVER** use `import` inside a `children` list. Use a Template and instantiate it instead. The parser will throw an error if this is violated.
*   **Recursive Composition:** Supported via `type: instance`.

### The Command System
We decouple the UI from the App logic using the **Command Pattern**.
*   **Registry:** The App registers callbacks (e.g., `ui_command_register("Graph.Clear", ...)`).
*   **Trigger:** The UI triggers them via strings (e.g., `on_click: "Graph.Clear"`).
*   **Benefit:** The UI parser doesn't need to know about "Graphs" or "Nodes". It just fires string events.

---

## 3. Deep Dive: The Graphics System

The graphics layer is split into two parts to keep things clean.

### The Data: Unified Scene (`scene/scene.h`)
Instead of having separate lists for UI, 2D Sprites, and 3D Models, the engine uses a **Single Linear Array** of `SceneObject`s.

*   **Everything is an Object:** A UI button, a text character, and a 3D cube are all `SceneObject` structs.
*   **Why?** This makes the `RenderSystem` incredibly simple. It iterates the array **once**, sorts by Z-index, and submits to the backend.

### The Manager: `RenderSystem` (`render_system.h`)
This is the **High-Level API**. Features and the App talk to this.
*   **You say:** "Draw this UI", "Render this scene", "Set the camera".
*   **It does:** Sorts objects, culls invisible items, and prepares packets for the backend.

### The Worker: `RendererBackend` (`renderer_backend.h`)
This is the **Low-Level Hardware Interface**. Only the `RenderSystem` talks to this.
*   **It does:** "Bind Vulkan Pipeline", "Copy Memory to GPU", "Submit Command Buffer".

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

---

## 5. Directory Structure Guide

```text
src/
├── app/                  
│   └── main.c            # The "Glue" code. Registers Commands.
│
├── features/             
│   └── math_engine/      # The Logic Layer
│
├── engine/               
│   ├── core/             # Application lifecycle.
│   ├── graphics/         
│   └── ui/               # The UI System (Parser, Layout, Input, Renderer)
│       ├── ui_command_system.c
│       └── ...
│
└── foundation/           # Independent Utilities (Memory, Math, Logs).
```

---

## 6. Known Constraints (Technical Debt)

*   **Text Rendering:** Each letter is a separate object (inefficient). Needs batching.
*   **String Hashing:** The engine still uses `strcmp` in many places (Registry, UI lookups). Should move to `StringID` (FNV-1a).
