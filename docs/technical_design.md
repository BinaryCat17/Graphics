# Technical Design & Internals (v3.0)

**Context:** Implementation details for the Visual Compute Engine.

---

## 1. The Transpiler (Micro-Graph Compiler)

The engine does not interpret nodes at runtime. It compiles them.

### Data Structures (AST)
The Micro Graph is represented as an Abstract Syntax Tree before compilation.

```c
typedef enum { NODE_ADD, NODE_MUL, NODE_SIN, ... } NodeType;

typedef struct {
    NodeType type;
    NodeId inputs[MAX_INPUTS];
    // ...
} AstNode;
```

### Compilation Stages
1.  **Topological Sort:** Determine evaluation order.
2.  **Type Inference:** Deduce output types based on inputs (e.g., `Vec3 + Float = Vec3`).
3.  **Kernel Fusion (GLSL Generation):**
    *   Iterate through sorted nodes.
    *   Generate GLSL variables for intermediate results.
    *   *Example:* `vec3 temp_0 = var_A + var_B; vec3 result = temp_0 * var_C;`
4.  **Backend Emit:** Wraps the logic in a Compute Shader boilerplate (`layout(local_size_x = 64) in; void main() { ... }`).

---

## 2. Memory & Data Layout

### SoA (Structure of Arrays)
We avoid Array of Structs (`struct Particle { pos, vel }`) in VRAM.
Instead, we use independent buffers:
*   `Buffer<float> PosX`
*   `Buffer<float> PosY`
*   `Buffer<float> PosZ`

**Why?**
1.  **Alignment:** No padding issues between C and GLSL.
2.  **Flexibility:** A kernel needing only `PosX` doesn't load `VelY` into cache.
3.  **SIMD:** Fits GPU architecture perfectly.

### Stream<T> Abstraction
A C-side handle representing a GPU buffer.
*   `stream_create(type, count)`: Allocates SSBO.
*   `stream_bind_compute(stream, binding_slot)`: Descriptor set update.
*   `stream_bind_vertex(stream, attribute_slot)`: Binds as Vertex Buffer (if supported) or SSBO (for manual fetching).

---

## 3. Zero-Copy Rendering
The traditional `RenderPacket` is replaced/augmented by direct Buffer binding.

**Vertex Shader Approach:**
Instead of `layout(location=0) in vec3 inPos;`, we use:

```glsl
layout(std430, binding = 0) readonly buffer PosX { float data[]; } posX;
layout(std430, binding = 1) readonly buffer PosY { float data[]; } posY;
// ...
void main() {
    uint id = gl_VertexIndex;
    vec3 pos = vec3(posX.data[id], posY.data[id], ...);
    gl_Position = view_proj * vec4(pos, 1.0);
}
```

This allows the **Compute Shader** to be the *sole producer* of geometry data without CPU intervention.

---

## 4. File Formats

### `*.layout.yaml` (Editor UI)
Declarative layout for the application shell. Parsed by `ui_layout.c`.
*   Static structure (Panels, Splits).
*   CPU-driven.

### `*.graph.yaml` / `*.gdl` (Logic & Scene)
Serialization of the Nodes and Links.
*   Defines the Macro and Micro graphs.
*   Contains initial values for properties.
*   Loaded into the Graph System -> Transpiled -> Executed.