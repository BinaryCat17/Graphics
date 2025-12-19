# Architectural Audit and Roadmap: Math Engine Foundation

**Date:** December 19, 2025
**Status:** Phase 1 Complete (Compute Backend Infrastructure Ready)
**Operating System:** Linux

## 1. Executive Summary

The "Graphics" project is transitioning from a 2D rendering framework to a high-performance **Math Engine**. We have successfully implemented the **Compute Subsystem** in the Vulkan backend, enabling the execution of arbitrary compute shaders. This bridges the critical gap between the Math Graph (CPU/Transpiler) and the GPU.

The architecture strictly follows a **layered approach**:
1.  **Platform Layer:** OS abstraction (Window, Input, Files).
2.  **Renderer Backend (API Agnostic):** Defines the contract (`renderer_backend.h`) for Graphics and Compute.
3.  **Vulkan Backend (Implementation):** Implements the contract, managing device resources, pipelines, and synchronization.
4.  **Engine/Feature Layer:** High-level logic (Graph Editor, UI) that uses the Renderer Backend.

**Key Achievement:** The Engine uses a **Shader IR** to decouple logic from syntax. Compute jobs are dispatched via a clean `ShaderTarget` API.

## 2. Current Architecture State

| Feature | Status | Analysis |
| :--- | :--- | :--- |
| **Compute Backend** | ✅ **Ready** | `renderer_backend.h` exposes `compute_pipeline_create/dispatch`. |
| **Math Engine** | ✅ **Decoupled** | Uses IR + Emitters. Supports GLSL target. Ready for WebGPU. |
| **Asset Pipeline** | ✅ **Robust** | `tools/build_shaders.py` handles offline compilation. No runtime `glslc` dependency. |
| **Unified Scene** | ⚠️ Partial | Renderer focuses on 2D Instanced Quads. |
| **Visual Graph Editor** | ⚠️ Partial | UI exists, but "Run" button is manual (Key 'C'). |

## 3. Phase 1: Compute Infrastructure (Completed)
...
## 4. Roadmap to v0.2 (Math Engine Prototype)

### Phase 2: Shader Pipeline (Completed)
*   [x] **Shader IR:** Decoupled `transpiler.c` from GLSL strings.
*   [x] **Build Script:** Implemented `tools/build_shaders.py`.
*   [x] **API:** `math_graph_transpile(..., TARGET_GLSL)`.

### Phase 3: The "Unified" Render Loop
**Objective:** Visualize the Math.
1.  **Dispatch:** Update `render_system.c` to dispatch the active Compute Pipeline before the main render pass.
2.  **Visualize:** Render a full-screen quad (or UI panel) displaying the result texture (Descriptor Set 2).

### Phase 4: 3D Support
**Objective:** True Unified Scene.
1.  **Mesh Rendering:** Add support for 3D meshes in `vulkan_renderer.c`.
2.  **Camera:** Implement a proper 3D camera controller.

## 5. Directory Structure Recommendations

To maintain strict layering, new "Math Engine" features should be organized as follows:

```
src/
├── features/
│   └── math_engine/          <-- NEW: The core logic
│       ├── graph_model.c     (Node graph data structures)
│       ├── transpiler.c      (GLSL generation)
│       ├── shader_compiler.c (GLSL -> SPIR-V wrapper)
│       └── engine_bridge.c   (Orchestrates Graph -> Compiler -> RendererBackend)
├── engine/
│   ├── graphics/
│   │   ├── backend/          (Low-level API)
│   │   └── ...
│   └── ...
```

**Rule:** `src/features/math_engine` must NOT include `<vulkan/vulkan.h>`. It must only communicate via `renderer_backend.h`.