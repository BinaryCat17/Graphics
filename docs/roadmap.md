# Development Roadmap

This roadmap focuses on transforming the engine from a functional prototype into a scalable professional system.

## Q1: Core Scalability (The "10k" Goal)
*Objective: Render 10,000 objects at 60 FPS.*

- [ ] **Text Rendering Pipeline 2.0**
    - [ ] Implement `FontAtlas` baking (or use signed distance fields).
    - [ ] Create a `TextBatcher` to merge strings into single vertex buffers.
    - [ ] Deprecate "1 Char = 1 Object".
- [ ] **Renderer Backend Overhaul**
    - [ ] Remove hardcoded `instance_capacity` (1000).
    - [ ] Implement dynamic buffer resizing (staging -> device local).
    - [ ] Add `DrawIndexedIndirect` for efficient batch rendering.

## Q2: Performance & Optimization
*Objective: Eliminate per-frame heap allocations.*

- [ ] **Memory System**
    - [ ] Implement `FrameArena` (linear allocator reset every frame).
    - [ ] Refactor UI `resolve_text_binding` to use `FrameArena` instead of `malloc`.
- [ ] **UI Caching**
    - [ ] Add `hash` checks to UI nodes.
    - [ ] Skip layout/text formatting if data hash hasn't changed.

## Q3: Feature Completeness
*Objective: A fully usable Graph Editor.*

- [ ] **Serialization:** Save/Load graphs to JSON.
- [ ] **Complex Types:** Support `mat4` and `struct` on graph wires.
- [ ] **Compute Visualization:** 3D texture slicing/volume rendering.

## Completed Milestones
- [x] **Architecture Decoupling:** Engine separated from App logic.
- [x] **Vulkan Abstraction:** Backend hidden behind `RendererBackend` interface.
- [x] **Arena Allocator:** Basic arena implementation added to Foundation.