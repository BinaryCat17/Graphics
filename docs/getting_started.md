# Getting Started

## Prerequisites

### Linux (Ubuntu/Debian)
1.  **Build Tools:** `sudo apt install build-essential cmake ninja-build curl zip unzip tar`
2.  **Vulkan SDK:** Ensure you have the Vulkan SDK installed (or let `vcpkg` handle headers, but drivers are needed).
3.  **Vcpkg:** The project uses `vcpkg` for dependency management.
    ```bash
    git clone https://github.com/microsoft/vcpkg.git
    ./vcpkg/bootstrap-vcpkg.sh
    export VCPKG_ROOT=$(pwd)/vcpkg
    ```

## Building the Project

The project uses CMakePresets for easy configuration.

1.  **Clone the repository:**
    ```bash
    git clone <repo_url>
    cd Graphics
    ```

2.  **Configure:**
    This step downloads dependencies (GLFW, etc.) via vcpkg and generates build files.
    ```bash
    cmake --preset x64-debug-linux
    ```

3.  **Build:**
    ```bash
    cmake --build out/build/x64-debug-linux
    ```

## Running the Application

The executable requires assets and specific command-line arguments to locate them.

```bash
timeout 5s out/build/x64-debug-linux/bin/Graphics \
    --log-level DEBUG \
    --assets assets \
    --ui assets/ui/editor.yaml
```

*Note: The `timeout 5s` is useful for automated testing/CI. Remove it for interactive sessions.*

## Controls

- **Left Click + Drag:** Move Nodes.
- **Scroll:** Pan the view (currently logs offsets).
- **'C' Key:** Toggle "Compute Mode". This transpiles the current graph to GLSL, runs it on the GPU, and displays the result.

## Troubleshooting

- **Logs:** Check `logs/graphics.log` (created at runtime) for TRACE-level details.
- **Vulkan Errors:** Ensure your GPU drivers support Vulkan 1.3.
