# Technical Design & Internals

**Context:** Implementation details for the v1.0 Architecture.

---

## 1. Memory Layout

### Frame Arena Strategy
We use a Double-Buffered Arena system for the render thread safety.
*   **Arena A (Logic):** Used by the main thread to build the `RenderFramePacket`.
*   **Arena B (Render):** Read by the background render thread (if multithreaded) or used for the previous frame's cleanup.
*   **Transient Vertex Buffer:** A large ring-buffer in the Arena used for generating dynamic geometry (Text, UI batches) on the fly.

### Reflection & Binding
Instead of string lookups (`strcmp`), the `codegen.py` tool generates a **Byte Offset Table**.
*   **Runtime:** Bindings use `base_ptr + offset` for O(1) access.
*   **Safety:** The codegen verifies types at build time.

---

## 2. Rendering Pipeline Implementation

### The `RenderFramePacket`
The packet is not a simple list of objects. It is a struct of **Buckets**:

```c
struct RenderFramePacket {
    CameraData camera;
    
    // Bucket 1: Compute Jobs
    ComputeCmd* jobs; 

    // Bucket 2: 3D World (Sorted Front-to-Back)
    DrawCmd3D* opaque_queue;
    DrawCmd3D* transparent_queue; // Sorted Back-to-Front

    // Bucket 3: UI (Sorted by hierarchy/z-index)
    // Uses Transient Vertex Buffer handles
    DrawCmdUI* ui_batches;
};
```

### Text Rendering (Batching)
Text is not drawn as individual objects.
*   **Write:** text_renderer writes raw vertices (pos, uv, color) into the Frame Arena's transient buffer.
*   **Batch:** Consecutive characters share a single DrawCmdUI.
*   **Draw:** The backend binds the Font Atlas once and issues one draw call for thousands of glyphs.

### Async Shader Compilation
User modifies a Math Node.
*   **Main Thread:** Marks graph dirty, issues a CompileJob.
*   **Worker Thread:** Invokes shader compiler (via library, not system()).
*   **Main Thread:** Checks job status. If ready, uploads SPIR-V to GPU and replaces the pipeline.

## 3. Math Engine Internals
IR (Intermediate Representation)
The graph is not transpiled directly to GLSL string concatenation.
*   **Graph:** Nodes and Links.
*   **Linear IR:** Flattened bytecode (Stack-based machine).
*   **Backend Emit:** IR is converted to GLSL/SPIR-V for GPU or C-code for CPU execution.

## 4. Input System
*   **Action Mapping:** Uses hashed string IDs (StringId) for fast lookup.
*   **Event Queue:** Fixed-size ring buffer. No allocations during event processing.

