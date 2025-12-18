# UI Modernization Plan

This document outlines the roadmap for transforming the current UI prototype (`src/engine/ui`) into a professional-grade interface toolkit comparable to industry standards.

## 1. Text Input (High Priority)
Currently, the UI only supports output (Labels). Input is essential for editors.
*   **Widget:** `TextField` / `TextArea`.
*   **Features:**
    *   Caret navigation (Arrow keys, Home, End).
    *   Text Selection (Shift + Arrows, Mouse Drag).
    *   Clipboard Integration (Copy/Paste via OS).
    *   Input Filtering (Regex, numeric-only).
    *   Key Repeat support (Held key events).

## 2. Event System & Focus (Interaction Model)
Moving away from immediate-mode polling to a robust event model.
*   **Focus Management:**
    *   `UI_FLAG_FOCUSABLE`.
    *   Tab navigation / Cycle focus.
    *   Keyboard events routed strictly to the focused element.
*   **Event Bubbling:**
    *   Events (Click, Scroll) start at the target leaf and bubble up to parents if unhandled.
    *   Allows containers to handle interactions ignored by children.
*   **Mouse Capture:**
    *   Robust "Drag" support where an element keeps receiving mouse events even if the cursor leaves its bounds (e.g., dragging a slider).

## 3. Layering & Z-Ordering
Support for elements that must break out of the standard container hierarchy and clipping regions.
*   **Overlay Layer:** For Tooltips, Context Menus, Dropdown lists.
*   **Modal Stack:** For blocking Popups/Alerts.
*   **Implementation:** Deferred rendering queue or separate render pass for the Overlay layer.

## 4. Visual Styling (9-Slice & Theming)
Hardcoded colors in C code must be replaced with data-driven styling.
*   **9-Slice Scaling (Nine-Patch):** Textures that scale the center but preserve corner details (essential for rounded buttons/panels).
*   **Style Sheets:**
    *   YAML-based definitions: `style: { background: "button_bg", color: "white", hover: { color: "yellow" } }`.
    *   State-based property overrides (Hover, Active, Disabled, Focused).

## 5. Visual Scrollbars
The mechanics of scrolling are implemented, but the visual feedback is missing.
*   **Scrollbar Widget:**
    *   Calculates thumb size based on `view_size / content_size`.
    *   Draggable thumb updates the container's `scroll_offset`.

## 6. Layout Improvements
*   **Grid Layout:** Rigid table structures.
*   **Docking:** Ability to re-arrange panels (Drag tab to dock Left/Right/Bottom).

---

## Immediate Next Steps (Infrastructure)
Before adding these features, the rendering backend must be hardened to support the increased object count and dynamic updates required by a complex UI.
1.  **Dynamic Buffers:** Auto-growing GPU buffers to remove the 1000-object limit.
2.  **Double Buffering:** Per-frame resources to eliminate CPU/GPU race conditions during UI updates.
