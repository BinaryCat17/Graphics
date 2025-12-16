# Roadmap v2: Unified Visual Space
**Goal:** A professional-grade Math Engine with unified 2D/3D visualization and data-driven rendering.

## Core Philosophy
1.  **Unified Scene:** No strict separation between "UI" and "World". Everything is a renderable object in a scene, viewed through different cameras (Perspective for 3D, Orthographic for UI/2D).
2.  **Data-Driven:** Visualization is defined by binding Data Buffers (Tensors) to Visual Templates (Instanced Meshes).
3.  **Declarative:** The scene structure and bindings are defined in YAML/Data, minimizing imperative code.

---

## Phase 1: The Unified Render Core
*Objective: Enable instanced rendering of data arrays and unify the render pipeline.*

1.  **Architecture Cleanup:**
    *   Remove legacy CPU-bound mesh generation (`math_mesh_builder`).
    *   Refactor `RenderSystem` to accept a generic `RenderScene` containing both UI elements and 3D objects.
2.  **GPU Data Abstraction:**
    *   Implement `GpuBuffer` management (Vulkan Buffers) for transferring math data to GPU.
    *   Implement `InstancedRenderer` in the backend to draw millions of primitives (e.g., points, arrows) efficiently.
3.  **Unified Camera:**
    *   Implement a Camera system that supports seamless transitions between 2D (Ortho) and 3D (Persp).

## Phase 2: Data-Driven Visualizers
*Objective: Create the "Visualizer" node type in the declarative system.*

1.  **YAML Schema Extension:**
    *   Add `visualizer` type to the loader.
    *   Support `template` (mesh/shader) and `data_source` (binding) properties.
2.  **Math Tensor Support:**
    *   Upgrade `MathGraph` to support `Tensor` (Array) data types.
    *   Implement compute shaders (or optimized CPU code initially) to populate these tensors.

## Phase 3: The Tools (Node Editor & Plots)
*Objective: Build tools using the new Unified System.*

1.  **Node Editor:**
    *   Implement as a 2D Scene Overlay.
    *   Nodes are rectangular meshes. Links are procedural lines (Line Strip instances).
2.  **Plotting:**
    *   2D Line Plots (Instanced line segments).
    *   3D Vector Fields (Instanced arrows).
    *   3D Surfaces (Grid mesh displaced by Vertex Shader).

---

## Migration Steps
1.  [x] **Cleanup:** Remove `math_mesh_builder`.
2.  [x] **Core:** Define `UnifiedScene` and `RenderObject` structures.
3.  [ ] **Backend:** Add Instancing support to Vulkan backend.
4.  [ ] **UI/Scene:** Implement the `visualizer` loader.