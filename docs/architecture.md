# Architecture Overview

**Philosophy:** Data-Oriented Design (DOD) | C11 | Zero-Allocation Loop
**Target:** High-Performance Interactive Tools & Visualization

---

## 1. Core Philosophy
The engine ignores traditional OOP hierarchies. Instead, it focuses on **Data Transformations**.
*   **Data > Objects:** We process homogenous arrays of data, not individual objects.
*   **Frame Transient:** Most memory used in a frame is valid *only* for that frame. We use linear allocators (Arenas) that reset continuously.
*   **Separation of Concerns:** The Logic Layer (Scene Graph) knows nothing about the GPU. The Render Layer (Backend) knows nothing about game logic.

## 2. Global Data Flow (The Pipeline)
The engine executes a strict unidirectional pipeline every frame:

```text
[INPUT] -> [LOGIC UPDATE] -> [EXTRACTION] -> [BACKEND RENDER]
```

### Phase A: Logic Update (CPU)
*   **Input Processing:** Raw OS events are converted into logical Actions.
*   **Graph Evaluation:** The Math Engine processes the node graph to update values.
*   **Scene Hierarchy:** The Logical Scene (Tree) is updated. Local and World transforms are recalculated here.
*   **Output:** A dirty Scene Tree and updated application state.

### Phase B: Extraction (The Bridge)
This is the synchronization point.
The engine traverses the Logical Scene and "extracts" visual data into a RenderPacket.
*   **Culling:** Invisible objects are discarded.
*   **Sorting & Binning:** Objects are sorted by material/depth and placed into specific "Buckets" (e.g., UI, Opaque 3D, Transparent).
*   **Output:** An immutable RenderFramePacket stored in temporary memory.

### Phase C: Backend Render (GPU)
The Backend consumes the RenderFramePacket.
It is stateless: it builds command buffers from scratch every frame based only on the packet.
It executes distinct Render Passes (Compute -> Shadow -> World -> UI).

## 3. System Architecture
🧱 Foundation Layer
Zero-dependency utilities.
*   **Memory:** Arena (Linear) and Pool (Chunked) allocators.
*   **Meta:** Reflection system for serialization and UI binding.
*   **Platform:** OS abstraction (Window, Files, Threads).

⚙️ Engine Layer
*   **Scene System:** A unified logical hierarchy. To the user, a UI Button and a 3D Cube are just Nodes.
*   **UI System:** Layout engine and event bubbling. It operates on the Scene Tree.
*   **Render System:** Manages the Extraction phase and hands data to the Backend.

🧩 Feature Layer
Math Engine: Domain-specific logic. It compiles node graphs into Bytecode or Shaders. It runs asynchronously to avoid stalling the UI.

## 4. Key Concepts
The "Unified" Logical Scene
To the developer, there is only one world. You can attach a UI Label to a 3D Cube. The hierarchy handles coordinate transformations automatically. The separation into 2D/3D only happens strictly during the Extraction phase.

Asynchronous Compute
Heavy operations (Shader compilation, Geometry generation) are offloaded to a Job System. The main loop never blocks waiting for these. Placeholders are rendered until results are ready.

## 5. Interface Standards (Public vs Internal)
We strictly separate API from Implementation to prevent spaghetti code.

Root (module.h): Defines what the module does. Opaque handles (typedef struct X X;).
Internal (internal/module_internal.h): Defines how it works. Full struct definitions.
Rule: External code includes module.h. Implementation includes internal/*.h.