# System Architecture

**Version:** 0.2 (Math Engine Update)
**Date:** December 19, 2025

## 1. Architectural Philosophy

The project follows a **Data-Oriented, Layered Architecture**. Dependencies strictly flow downwards.

```mermaid
graph TD
    App[Application (src/app)] --> Features[Features (src/features)]
    Features --> Engine[Engine (src/engine)]
    Engine --> Foundation[Foundation (src/foundation)]
```

*   **Foundation:** Platform abstraction, memory, math, logging. No dependencies on Engine/Features.
*   **Engine:** Core runtime (Renderer, UI, Assets, Input).
*   **Features:** Business logic modules (Math Engine). Independent of specific Engine implementations where possible.
*   **App:** The entry point wiring everything together.

---

## 2. The Math Engine (Feature)

The `math_engine` is a standalone module responsible for defining, evaluating, and compiling mathematical graphs. It is designed to be **backend-agnostic**.

### Pipeline: Graph to Shader
Instead of generating GLSL directly, the engine uses an Intermediate Representation (IR).

```
[MathGraph] --(Traversal)--> [Shader IR] --(Emitter)--> [Target Code (GLSL/WGSL)]
```

1.  **MathGraph:** High-level node graph (Nodes, Inputs, Connections).
2.  **Shader IR (`shader_ir.h`):** A linear list of abstract instructions (`IR_OP_ADD`, `IR_OP_SIN`, `IR_OP_TEXTURE_SAMPLE`). This layer knows *nothing* about syntax.
3.  **Emitters (`emitters/*.c`):** Convert IR into backend-specific source code.
    *   `glsl_emitter.c`: Generates GLSL for Vulkan/OpenGL.
    *   *(Future)* `wgsl_emitter.c`: Will generate WGSL for WebGPU.

### API Usage
```c
// Generate code for the specific backend
char* code = math_graph_transpile(&graph, MODE_IMAGE_2D, SHADER_TARGET_GLSL_VULKAN);
```

---

## 3. Build System & Asset Pipeline

The engine does **not** compile GLSL source text at runtime. This avoids shipping a heavy compiler with the game.

### Shader Pipeline (`tools/build_shaders.py`)
1.  **Dev Time:** Developers write `.vert` and `.frag` files in `assets/shaders/`.
2.  **Build Time:** CMake invokes `tools/build_shaders.py`.
    *   Scans `assets/shaders/`.
    *   Compiles changed files using `glslc` (Vulkan SDK).
    *   Outputs binary `.spv` files next to the source.
3.  **Runtime:** The Engine loads `.spv` files directly.

---

## 4. File Structure & Recommendations

### Directory Layout

```text
src/
├── app/                  # Application Entry Point
│   └── main.c            # Configures Engine and Math Engine
│
├── features/             # Reusable Business Logic
│   └── math_engine/      # <--- NEW ARCHITECTURE
│       ├── math_graph.c  # Graph Data Structure
│       ├── transpiler.c  # Orchestrator (Graph -> IR -> Emitter)
│       ├── shader_ir.h   # Intermediate Representation Defs
│       └── emitters/     # Backend-specific Generators
│           └── glsl_emitter.c
│
├── engine/               # Runtime Systems
│   ├── core/             # Main Loop
│   ├── assets/           # Resource Loading (SPV, Fonts)
│   ├── ui/               # UI System
│   └── graphics/         # Rendering
│       ├── render_system.c # High-level dispatch
│       └── backend/      # Hardware Abstraction
│           └── vulkan/   # Vulkan Implementation
│
└── foundation/           # Low-level Utilities
    ├── memory/           # Arenas, Pools
    ├── math/             # Vectors, Matrices
    └── platform/         # OS Abstraction (Window, File IO)
```

### Recommendations for Extensions

1.  **Adding a New Shader Target (e.g., WebGPU):**
    *   Create `src/features/math_engine/emitters/wgsl_emitter.c`.
    *   Add `SHADER_TARGET_WGSL_WEBGPU` to `ShaderTarget` enum.
    *   Update `math_graph_transpile` switch case.
    *   *No changes needed to Graph logic or IR.*

2.  **Adding a New Node Type:**
    *   Add type to `MathNodeType` (`math_graph.h`).
    *   Add opcode to `IrOpCode` (`shader_ir.h`).
    *   Update `transpiler.c` to emit the new opcode.
    *   Update `glsl_emitter.c` to handle the new opcode.

3.  **Cross-Platform Builds:**
    *   Use `tools/build_shaders.py` as the single source of truth for shader compilation logic. Extend it to call `spirv-cross` for non-Vulkan targets.
