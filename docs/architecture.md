# The Architecture Guide

**Version:** 0.7.1 (Standardized)
**Date:** December 20, 2025

This document explains **how** the system is built and **why** it is built that way.

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
Zero-dependency utilities: Memory, Platform, Math, Logger, Config, Reflection.

### ⚙️ Engine (`src/engine/`)
**"The Machine"**
Systems for interactive applications: Core, Graphics, Scene, Text, Scene System (UI), Assets.

### 🧩 Features (`src/features/`)
**"The Logic"**
Reusable domain-specific modules (e.g., Math Graph Editor).

### 🚀 App (`src/app/`)
**"The Glue"**
Entry point (`main.c`). Orchestrates layers.

---

## 2. Interface Standards (The "Public/Internal" Split)

To maintain long-term maintainability and prevent "spaghetti code," we rigidly separate the **Public API** from the **Internal Implementation** using a standardized directory structure.

### 📂 Directory Structure Pattern

Every major module (e.g., `src/engine/scene_system`) must follow this exact layout:

```text
src/engine/scene_system/
├── ui_core.h          # [PUBLIC] A public contract. External modules may include this.
├── ui_core.c          # [PRIVATE] Implementation of the public API.
└── internal/          # [PRIVATE] Hidden from the rest of the codebase.
    ├── ui_state.h     # [INTERNAL] Full struct definitions (hidden from public).
    ├── ui_layout.h    # [INTERNAL] Internal subsystem interfaces.
    ├── ui_layout.c    # [INTERNAL] Internal subsystem implementation.
    └── ui_renderer.c  # [INTERNAL] Internal subsystem implementation.
```

### 📏 The Golden Rules

#### 0. Language Standard (C11)
The project strictly enforces **C11 (ISO/IEC 9899:2011)**.
*   **Why?** Allows `typedef` redefinition, enabling cleaner opaque pointer APIs without "God Headers".
*   **Pattern:**
    *   **Public API:** Use `typedef struct Name Name;` in headers.
    *   **Function Signatures:** Use `Name*` (not `struct Name*`).
    *   **Forward Declarations:** Repeat `typedef struct Name Name;` in consuming headers (allowed in C11) to avoid `#include` dependencies.

#### 2. Header Guards
*   **Standard:** Strictly use traditional `#ifndef MODULE_NAME_H` / `#define MODULE_NAME_H` guards.
*   **Prohibited:** Do not use `#pragma once` (non-standard).

#### 3. Public Headers (The Module Root)
*   **Purpose:** Defines *what* the module does, not *how*.
*   **Location:** Any `.h` file in the module's root directory (e.g., `src/engine/scene_system/*.h`).
*   **Content:**
    *   **Opaque Handles:** Use `typedef struct MySystem MySystem;` instead of defining the struct. This prevents users from accessing internal state directly.
    *   **Enums/Flags:** Only those required for API arguments.
    *   **API Functions:** High-level functions like `system_create()`, `system_update()`.
*   **Rule:** External modules may include **ANY** header from the module root.

#### 4. The Internal Directory (`internal/`)
*   **Purpose:** Defines *how* the module works.
*   **Content:**
    *   **Full Struct Definitions:** The actual layout of `MySystem`.
    *   **Helper Functions:** Utilities used only within the module.
    *   **Sub-modules:** Complex logic split into smaller files (e.g., `layout`, `parser`).
*   **Access:**
    *   **Strictly Private:** Files in this directory must **NEVER** be included by code outside the module.
    *   **Exception:** White-box tests in `tests/` are allowed to include internal headers to verify complex logic.

#### 3. Include Graph
*   `src/app/main.c` -> `#include "engine/scene_system/ui_core.h"` (✅ OK)
*   `src/app/main.c` -> `#include "engine/scene_system/internal/ui_layout.h"` (❌ **VIOLATION**)
*   `src/engine/scene_system/ui_core.c` -> `#include "internal/ui_layout.h"` (✅ OK)

---

## 3. Core Principles

### 🧠 Data-Driven Design
Behavior is defined in data (`manifest.yaml`), not code. The engine essentially acts as an interpreter for this data.

### 🪞 Reflection & Data Binding
*   **Code Generation:** A pre-build step (`tools/codegen.py`) parses C headers and generates `reflection_registry.c`.
*   **Runtime Binding:** UI Elements bind directly to C struct fields by name (e.g., `bind: "value"`).
*   **No Manual Glue:** The UI system reads values via reflection, eliminating the need for manual ViewModel classes.

### 🛡 Memory Safety
**Zero** `malloc`/`free` in the hot loop.
*   **Arenas:** Linear allocators for frame data.
*   **Pools:** Fixed-block allocators for game objects.

### 🔗 Strict Decoupling
Systems communicate via **IDs** (integers) or **Commands**, never raw pointers. This allows systems to be destroyed or reloaded without crashing dependent modules.

### 👑 System Ownership (Singleton Initialization)
The `Engine` core (`src/engine/core`) is the **sole owner** of all subsystems (Renderer, UI, Input).
*   **Rule:** Features (e.g., `src/features/`) must **NEVER** call lifecycle functions like `_init()` or `_shutdown()`.
*   **Registration:** Features may only *register* resources (e.g., UI Commands) or *consume* APIs.
*   **Order:** Initialization occurs strictly in `engine_create()` to guarantee dependency resolution.

---

## 4. Key Architectural Patterns

### 🖼 Render Packet (Double Buffering)
To decouple Logic from Rendering, the engine uses a **Packet-based Architecture**.
1.  **Logic Step:** The Game/UI constructs a `Scene` (Frame N).
2.  **Submit:** The `Scene` is wrapped in a `RenderFramePacket` and submitted to the RenderSystem.
3.  **Swap:** The RenderSystem swaps the Front/Back buffers.
4.  **Render Step:** The Backend draws the previous frame (Frame N-1) while Logic builds the next one (Frame N+1).
*   *Benefit:* Allows the Logic and Render threads (future) to run in parallel without locking.

### 🧵 Concurrency Model
*   **Main Thread:** Runs the Event Loop, Physics, UI Logic, and builds the Scene.
*   **Render Thread (Virtual):** Currently runs on the Main Thread but is architecturally isolated via the Packet System.
*   **Worker Threads:** Used for heavy I/O operations (e.g., Saving Screenshots, Async Shader Compilation).
*   **Sync:** Systems communicate via Message Queues or Atomic Flags, avoiding complex mutex locking in the hot path.

### 🌍 The Unified Scene Strategy
Unlike traditional engines that maintain separate render pipelines for UI (2D) and World (3D), this project employs a **Unified Scene** architecture.
*   **Concept:** Everything is a `SceneObject`. A UI button is just a Quad with a specific shader mode (`SCENE_MODE_SDF_BOX`). A text character is a Quad with a texture (`SCENE_MODE_TEXTURED`).
*   **Data Structure:** All objects reside in a single linear `MemoryArena`.
*   **Rendering:** The `RenderSystem` iterates this single array. This simplifies depth sorting (e.g., 3D objects inside UI frames), reduces draw call overhead, and unifies resource management.

---

## 5. Subsystems Deep Dive

### 🎨 Graphics
*   **Abstractions:** The `RenderSystem` is a high-level manager that consumes `RenderFramePacket`s. It delegates actual API calls to a `RendererBackend` (V-Table).
*   **Vulkan Implementation:**
    *   **Stateless Rendering:** The backend clears and rebuilds command buffers every frame.
    *   **Pipeline Management:** Uses a simplified pipeline cache. Switch between 3D/UI/Compute pipelines based on `SceneObject` state.
    *   **Compute:** Supports runtime SPIR-V creation for procedural geometry visualization.

### 📦 Assets
*   **Storage:** Centralized `MemoryArena` for all loaded strings and raw data.
*   **Identity:** Assets are looked up via string paths but hashed to `StringId` for fast comparisons.
*   **Lifecycle:** Currently load-only. Hot-reloading is planned (Phase 6.5).

### 🎬 Scene
*   **Role:** The "Frame Packet". It is a transient snapshot of the world state for a specific frame.
*   **Allocation:** Uses a **Linear Allocator** that is effectively `reset()` at the start of every frame. This eliminates fragmentation and makes object destruction free (O(1)).
*   **Data Layout:** `SceneObject` utilizes **Anonymous Unions** to multiplex memory. A `SceneObject` can hold UI layout data OR PBR material data, keeping the struct size compact (~128 bytes) and cache-friendly.

### 🎮 Input
*   **Architecture:** Decoupled from windowing events (GLFW).
*   **State:** Maintains `InputState` (current keys) and `InputEventQueue` (buffered actions).
*   **Mapping:** Supports Action Mapping (e.g., "Jump" -> Spacebar) to abstract physical keys from logical actions.

### 📝 Text
*   **Backend:** `stb_truetype` for font parsing and rasterization.
*   **Atlas:** Generates a single texture atlas (R8) for all active glyphs.
*   **Rendering:** Does **not** issue draw calls. Instead, it generates `SceneObject`s (Quads) and pushes them into the active `Scene`.

### 🖥️ UI
*   **Layout Engine:** A recursive, single-pass solver inspired by Flexbox. Supports `Column`, `Row`, `Canvas`, and `Split` layouts.
*   **Input Handling:** Implements **Event Bubbling**. Input events (click, drag) start at the root and drill down to leaf nodes (hit testing), then bubble up for handling.
*   **Rendering:** Generates `SceneObject`s with specific modes (9-Slice, SDF, Textured). Supports explicit `UiRenderMode` (e.g., `BEZIER`) for custom visualization.
*   **Template-Instance Model:**
    *   **Spec (`UiNodeSpec`):** Immutable "DNA" loaded from YAML. Lives in `UiAsset`.
    *   **Element (`UiElement`):** Live runtime object created from a Spec. Lives in `UiInstance`.
    *   **Usage:** Logic calls `ui_element_create(instance, spec)` to spawn dynamic UI (like Graph Nodes) from static templates.
*   **Data Binding & Collections:**
    *   UI Containers can bind to array collections (e.g., `collection: "wires"`).
    *   This is used to render Graph connections as interactive UI elements (Bezier curves) rather than raw immediate-mode geometry.
*   **Orthogonal Typing:**
    *   **Primitives:** Only `Container` (Rect) and `Text` exist as fundamental Kinds.
    *   **Behavior:** Defined via Flags (`Clickable`, `Editable`, `Draggable`).
    *   *Example:* A "Button" is a `Container` + `UI_FLAG_CLICKABLE`. A "TextField" is `Text` + `UI_FLAG_EDITABLE`.
*   **Hybrid Update Strategy:**
    *   **Retained Structure:** The tree persists across frames (unlike ImGui).
    *   **Partial Rebuilds:** Structural changes (e.g., selecting a new object) trigger `ui_element_rebuild_children` for specific sub-trees.
    *   **Data Binding:** Property changes (text, color) are synced every frame via reflection.

### 🧮 Math Engine
*   **Core:** A node-based visual programming language.
*   **Pipeline:**
    1.  **Graph:** Nodes and connections (Logic Model).
    2.  **IR:** Flattened to a linear Bytecode (ShaderIR).
    3.  **Transpiler:** Converts IR to GLSL/SPIR-V.
    4.  **Execution:** Compiles to a Vulkan Compute Pipeline for GPU execution.
*   **Isolation:** Fully decoupled from the main engine. Can be used headless.
