# Core module structure

This directory hosts reusable primitives that feed higher-level UI, runtime, and rendering systems. Files are grouped by domain instead of being co-located in a single flat folder.

## Domains and responsibilities
- `math/coordinate_spaces.*` — basic vector math plus conversions between world, logical UI, and screen coordinates, including the shared `RenderContext` projection payload.
- `layout/layout_geometry.*` — lightweight layout boxes, resolution into device space, and hit-testing helpers.
- `rendering/render_composition.*` — immutable view models and the sorted render-command list that bridges UI/layout outputs to GPU-friendly primitives.
- `rendering/ui_mesh_builder.*` — conversion of render commands into UI vertex buffers for quads and text, including clipping and projection logic.
- `utils/buffer_reserve.*` — small allocation helper for growing dynamic buffers used across the rendering pipeline.
- `Graphics.h` — convenience umbrella header for the core module.

## Migration map
To reduce ambiguous names and single-file folders, prior core files were reorganized as follows:
- `coordinate_transform.*` → `math/coordinate_spaces.*`
- `layout.*` → `layout/layout_geometry.*`
- `render_commands.*` → `rendering/render_composition.*`
- `vertex_buffers.*` → `rendering/ui_mesh_builder.*`
- `memory_utils.*` → `utils/buffer_reserve.*`

Include paths in the rest of the codebase now reference these scoped locations. New additions should follow the same domain-oriented layout and avoid creating one-file directories when a related sibling exists.
