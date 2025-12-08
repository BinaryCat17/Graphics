# Input and Coordinate Conventions

The UI uses a single coordinate convention: the origin is at the top-left corner and the **Y axis increases downward**. All logical layout, hit-testing, and rendering calculations assume this orientation.

## Spaces
- **World**: authoring or simulation units before UI scaling.
- **Logical UI**: UI layout units after applying the current `ui_scale`.
- **Screen**: device pixels after applying DPI scaling.

Use `CoordinateTransformer` (from `Graphics.h`) to move between spaces:
- `world ↔ logical`: apply or remove UI scale.
- `logical ↔ screen`: apply or remove DPI scaling.
- Combined helpers cover `world ↔ screen` and `screen ↔ world`.

All input paths should convert cursor positions from screen space into logical space via the transformer before hit-testing, and all rendering paths should emit vertices through the same transformer to avoid mismatches between input and output.
