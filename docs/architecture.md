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
Systems for interactive applications: Core, Graphics, Scene, Text, UI, Assets.

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

Every major module (e.g., `src/engine/ui`) must follow this exact layout:

```text
src/engine/ui/
├── ui_core.h          # [PUBLIC] The CONTRACT. The ONLY file external modules may include.
├── ui_core.c          # [PRIVATE] Implementation of the public API.
└── internal/          # [PRIVATE] Hidden from the rest of the codebase.
    ├── ui_state.h     # [INTERNAL] Full struct definitions (hidden from public).
    ├── ui_layout.h    # [INTERNAL] Internal subsystem interfaces.
    ├── ui_layout.c    # [INTERNAL] Internal subsystem implementation.
    └── ui_renderer.c  # [INTERNAL] Internal subsystem implementation.
```

### 📏 The Golden Rules

#### 1. The Public Header (`module.h`)
*   **Purpose:** Defines *what* the module does, not *how*.
*   **Content:**
    *   **Opaque Handles:** Use `typedef struct MySystem MySystem;` instead of defining the struct. This prevents users from accessing internal state directly.
    *   **Enums/Flags:** Only those required for API arguments.
    *   **API Functions:** High-level functions like `system_create()`, `system_update()`.
*   **Prohibited:**
    *   Including *any* file from the `internal/` directory.
    *   Exposing implementation details (e.g., internal helper structs).

#### 2. The Internal Directory (`internal/`)
*   **Purpose:** Defines *how* the module works.
*   **Content:**
    *   **Full Struct Definitions:** The actual layout of `MySystem`.
    *   **Helper Functions:** Utilities used only within the module.
    *   **Sub-modules:** Complex logic split into smaller files (e.g., `layout`, `parser`).
*   **Access:**
    *   **Strictly Private:** Files in this directory must **NEVER** be included by code outside the module.
    *   **Exception:** White-box tests in `tests/` are allowed to include internal headers to verify complex logic.

#### 3. Include Graph
*   `src/app/main.c` -> `#include "engine/ui/ui_core.h"` (✅ OK)
*   `src/app/main.c` -> `#include "engine/ui/internal/ui_layout.h"` (❌ **VIOLATION**)
*   `src/engine/ui/ui_core.c` -> `#include "internal/ui_layout.h"` (✅ OK)

---

## 3. Core Principles

### 🧠 Data-Driven Design
Behavior is defined in data (`manifest.yaml`), not code. The engine essentially acts as an interpreter for this data.

### ⚡ Reactivity (MVVM)
*   **Model:** Pure logic data.
*   **ViewModel:** Data prepared for display.
*   **View:** The UI representation.
*   **Binding:** The View updates automatically via Reflection when the ViewModel changes.

### 🛡 Memory Safety
**Zero** `malloc`/`free` in the hot loop.
*   **Arenas:** Linear allocators for frame data.
*   **Pools:** Fixed-block allocators for game objects.

### 🔗 Strict Decoupling
Systems communicate via **IDs** (integers) or **Commands**, never raw pointers. This allows systems to be destroyed or reloaded without crashing dependent modules.

---

## 4. Subsystems Overview

### Graphics
*   **Public:** `RenderSystem` (Opaque handle).
*   **Internal:** `RendererBackend`, `VulkanContext`.
*   **Goal:** The App should never know Vulkan exists. Only renders what is in the Scene.

### Scene
*   **Public:** `Scene`, `SceneObject`, `SceneCamera`.
*   **Goal:** Logic representation of the world. Decoupled from rendering implementation.

### Input
*   **Public:** `InputSystem` (Opaque handle).
*   **Internal:** `InputState`, `InputEventQueue`.
*   **Goal:** Event handling and input state management. Decoupled from platform callbacks.

### Text
*   **Public:** `Font`, `TextRenderer`.
*   **Internal:** `stb_truetype` implementation.
*   **Goal:** Font loading, atlas generation, and text measurement.

### UI
*   **Public:** `UiInstance`, `UiCore`.
*   **Internal:** `UiLayout` (Algo), `UiRenderer` (Draw commands), `UiParser` (YAML).

### Math Engine
*   **Public:** `MathEditor`, `MathGraph`.
*   **Internal:** `Transpiler`, `GlslEmitter`, `ShaderIR`.
