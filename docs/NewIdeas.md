# Future Concepts & Brainstorming (Post-v3.0)

**Status:** Experimental / Unscheduled
**Context:** Ideas that go beyond the current Roadmap (v3.0).

---

## 🔮 1. Distributed Compute
*   **Idea:** Run the Macro Graph across multiple GPUs or even multiple machines over a network.
*   **Challenge:** Latency and data synchronization of `Streams`.

## 🕸️ 2. WebGPU Export
*   **Idea:** Since the "Transpiler" already generates GLSL, we could generate WGSL.
*   **Goal:** "Publish to Web" button that exports the interactive graph as a standalone WASM/WebGPU page.

## 🧠 3. AI-Assisted Graph Gen
*   **Idea:** A "Copilot" node where you type "Flocking Boids simulation" and it generates the Micro Graph structure automatically.

## 📦 4. Package Manager
*   **Idea:** `gpm` (Graph Package Manager). Share custom generic nodes (e.g., "Advanced Noise", "Fluid Solver") via a central repo.
