# UI configuration

The UI is loaded directly from the parsed `ConfigNode` tree produced by `load_config_document` or `parse_config_text`. The loader no longer serializes YAML into JSON; it navigates the in-memory map and sequence nodes instead.

## Access helpers
- `config_node_get_map` / `config_node_get_sequence` / `config_node_get_scalar` return typed children or `NULL` when the node is absent or of another type.
- Scalars preserve their detected type (`string`, `number`, `bool`, or `null`) and are parsed in place when reading UI models, styles, and widgets.

## YAML example
```yaml
styles:
  primary:
    padding: 6
    textColor: [0.9, 0.9, 1.0, 1.0]

layout:
  type: column
  children:
    - type: label
      text: "Hello"
      style: primary
```

With this structure, `ui_config_load_styles` reads the `styles.primary` map directly and `ui_config_load_layout` walks the `layout.children` sequence—no intermediate JSON text is produced.
