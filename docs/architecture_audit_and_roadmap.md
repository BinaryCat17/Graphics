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

**Key Achievement:** The Engine can now dispatch compute jobs without knowing *anything* about Vulkan types (`VkPipeline`, `VkFence`, etc.), preserving architectural purity.

## 2. Current Architecture State

| Feature | Status | Analysis |
| :--- | :--- | :--- |
| **Compute Backend** | ✅ **Ready** | `renderer_backend.h` exposes `compute_pipeline_create/dispatch`. Vulkan backend implements full resource management (Pipelines, Fences, Storage Images). |
| **Unified Scene** | ⚠️ Partial | Data structures exist for 2D/3D. Renderer focuses on 2D Instanced Quads. 3D Mesh rendering is pending. |
| **Visual Graph Editor** | ⚠️ Air-Gapped | Transpiler works (generates GLSL), but is not yet connected to the runtime compiler or backend. |
| **Data-Oriented** | ✅ Verified | Strict usage of Arenas/Pools. Compute Pipelines are managed via a fixed-size pool (`MAX_COMPUTE_PIPELINES = 32`) to avoid fragmentation. |
| **Declarative UI** | ✅ Robust | Reflection-based UI system remains a strong pillar. |

## 3. Phase 1: Compute Infrastructure (Completed)

We have implemented the following capabilities in the Vulkan Backend:
*   **Compute Pipeline Pool:** Fixed-size pool for managing shader pipelines.
*   **Automatic Resource Management:** The backend automatically manages the "Compute Target" (Storage Image), resizing it and handling layout transitions (`GENERAL` <-> `SHADER_READ`).
*   **Synchronization:** Dedicated `VkFence` and `VkCommandBuffer` for compute workloads, ensuring execution does not interfere with the graphics loop.
*   **Descriptor Management:** Automatic creation of Descriptor Sets for:
    *   **Set 0 (Compute Write):** Allows the compute shader to write to the global target image.
    *   **Set 2 (Graphics Read):** Allows the main render pass to sample the compute result.

## 4. Roadmap to v0.2 (Math Engine Prototype)

### Phase 2: Runtime Shader Compilation & Bridge (Next)
**Objective:** Connect the Graph Editor to the GPU.
1.  **Runtime Compiler:** Implement `shader_compiler.c` to invoke `glslc` (via system call) to convert Transpiler GLSL -> SPIR-V.
2.  **Graph Bridge:** Create a system that watches the `MathGraph`, triggers transpilation on change, compiles the shader, and updates the Compute Pipeline.

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