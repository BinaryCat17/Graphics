# Core module layout and naming guide

The `src/core` tree now groups primitives by the problem they solve rather than by implementation detail. Keep related headers and implementations together to avoid deep one-file directories.

## Directory expectations
- `math/` holds foundational geometry types (`Vec2`) and coordinate transforms (`CoordinateTransformer`, `RenderContext`). Add math helpers here when they are reusable across systems.
- `layout/` collects geometry used for UI layout resolution and hit-testing. New layout-oriented helpers should live beside `layout_geometry.*` instead of introducing standalone folders.
- `utils/` is reserved for general-purpose helpers that are not rendering-specific (e.g., buffer growth utilities).

## Audit snapshot (current files and roles)
- Math/coordinates: `src/core/math/coordinate_spaces.*` defines `Vec2`, coordinate transforms, and the shared `RenderContext` used by rendering and hit-testing.
- Layout: `src/core/layout/layout_geometry.*` resolves logical boxes to device rectangles and performs hit tests.
- CPU render composition: `src/render/common/render_composition.*` contains `ViewModel`/`RenderCommand` definitions and sorting logic for draw order.
- CPU-to-GPU mesh generation: `src/render/common/ui_mesh_builder.*` builds UI quad/text vertex buffers, applying projection and clipping.
- Memory utilities: `src/core/utils/buffer_reserve.*` provides reusable buffer growth helpers.
- Runtime context plumbing: `src/render/common/render_context.h` stores GLFW window and `CoordinateTransformer` state; `src/runtime/runtime.*` wires GLFW callbacks, computes DPI/UI scale, and feeds the transformer to rendering services.
- GPU backend shell: `src/render/common/renderer_backend.*` defines the backend interface; `src/render/vulkan/vulkan_renderer.*` implements Vulkan-specific command submission.

## Naming rules
- Prefer names that describe the domain: `render_composition.*` for view model sorting, `ui_mesh_builder.*` for vertex generation, etc.
- Keep interfaces and implementations paired: avoid a directory that exists only for one file when a sibling can house it.
- Use scoped include paths (`core/<domain>/...` for primitives, `render/<area>/...` for renderer pieces) so call sites communicate intent and avoid ambiguity with similarly named files outside `core`.

## Adding new code
- Place math or coordinate helpers under `math/` and reuse existing types where possible.
- Add layout primitives to `layout/` and keep device-space conversions close to hit-testing logic.
- When extending rendering, add new composition steps to `src/render/common/` and keep GPU backend-specific code in `src/render/` (under backend-specific folders like `vulkan/`).
- Shared utilities that allocate or manage memory belong in `utils/`; prefer extending `buffer_reserve.*` over introducing a new single-function file unless the responsibility differs.
