# Core module structure

This directory hosts reusable primitives that feed higher-level UI, runtime, and rendering systems. Files are grouped by domain instead of being co-located in a single flat folder.

## Domains and responsibilities
- `math/coordinate_spaces.*` — basic vector math plus conversions between world, logical UI, and screen coordinates, including the shared `RenderContext` projection payload.
- `layout/layout_geometry.*` — lightweight layout boxes, resolution into device space, and hit-testing helpers.
- `utils/buffer_reserve.*` — small allocation helper for growing dynamic buffers used across the rendering and layout pipelines.
- `Graphics.h` — convenience umbrella header for the core module.

## Migration map
To reduce ambiguous names and single-file folders, prior core files were reorganized as follows:
- `coordinate_transform.*` → `math/coordinate_spaces.*`
- `layout.*` → `layout/layout_geometry.*`
- `render_commands.*` → `render/common/render_composition.*`
- `vertex_buffers.*` → `render/common/ui_mesh_builder.*`
- `memory_utils.*` → `utils/buffer_reserve.*`

Include paths in the rest of the codebase now reference these scoped locations. New additions should follow the same domain-oriented layout and avoid creating one-file directories when a related sibling exists.
