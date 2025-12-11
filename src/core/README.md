# Core module structure

This directory hosts reusable primitives that feed higher-level UI, runtime, and rendering systems. Files are grouped by domain instead of being co-located in a single flat folder.

## Domains and responsibilities
- `coordinate_systems/*` — shared vector/matrix math with 2D/3D transforms, coordinate hierarchy helpers (local/world/logical/screen), projection math, and the reusable `RenderContext` payload.
- `memory/buffer.*` — shared buffer growth helpers (`ensure_capacity` and `MEM_BUFFER_DECLARE`) that standardize allocation checks and doubling strategy.
- `Graphics.h` — convenience umbrella header for the core module.

## Migration map
To reduce ambiguous names and single-file folders, prior core files were reorganized as follows:
- `coordinate_transform.*` → `coordinate_systems/coordinate_systems.*`
- `layout.*` → `coordinate_systems/layout_geometry.*`
- `render_commands.*` → `render/common/render_composition.*`
- `vertex_buffers.*` → `render/common/ui_mesh_builder.*`
- `memory_utils.*` → `memory/buffer.*`

Include paths in the rest of the codebase now reference these scoped locations. New additions should follow the same domain-oriented layout and avoid creating one-file directories when a related sibling exists.
