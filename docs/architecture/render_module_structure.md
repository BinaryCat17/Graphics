# Render module layout

The `src/render` tree now separates reusable renderer interfaces/helpers from backend-specific implementations.

## Directory expectations
- `common/` hosts CPU-side rendering helpers that are shared by all backends:
  - `render_composition.*` defines `ViewModel`/`RenderCommand` data and sorting for draw order.
  - `ui_mesh_builder.*` builds UI quad/text vertex buffers from render commands, handling projection and clipping.
  - `renderer_backend.*` declares the backend interface, logging helpers, and registration utilities used by runtime wiring.
  - `render_context.h` exposes `RenderRuntimeContext` with the GLFW window and coordinate transformer owned by the runtime.
- `vulkan/` contains Vulkan-specific backend code (`vulkan_renderer.*`) that implements the `RendererBackend` contract and consumes the shared composition/mesh builder outputs.

## Usage guidelines
- Shared render logic should live in `common/` so additional backends can reuse it without pulling from `src/core`.
- New backend implementations should reside in their own subfolder under `src/render/` (for example, `metal/`), depend on `common/`, and keep platform-specific details contained.
- Prefer include paths that communicate scope, such as `render/common/render_composition.h` or `render/vulkan/vulkan_renderer.h`.

