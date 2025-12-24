import os
import subprocess
import sys
import platform
import shutil

def find_glslc():
    # 1. Check VULKAN_SDK environment variable
    sdk_path = os.environ.get("VULKAN_SDK")
    exe_name = "glslc.exe" if platform.system() == "Windows" else "glslc"

    if sdk_path:
        # Check standard layout
        candidate = os.path.join(sdk_path, "bin", exe_name)
        if os.path.exists(candidate):
            return candidate
        
        # Check Windows layout (sometimes Bin is capitalized)
        candidate = os.path.join(sdk_path, "Bin", exe_name)
        if os.path.exists(candidate):
            return candidate

    # 2. Check PATH
    if shutil.which("glslc"):
        return "glslc"

    return None

def main():
    root_dir = os.getcwd()
    # Ensure we are in the project root
    if not os.path.exists(os.path.join(root_dir, "assets")):
        # Try to find it relative to the script if run from tools/
        script_dir = os.path.dirname(os.path.abspath(__file__))
        potential_root = os.path.dirname(script_dir)
        if os.path.exists(os.path.join(potential_root, "assets")):
            root_dir = potential_root
            os.chdir(root_dir)
    
    shader_dir = os.path.join(root_dir, "assets", "shaders")
    if not os.path.exists(shader_dir):
        print(f"Warning: Shader directory not found at {shader_dir}")
        return

    glslc = find_glslc()
    if not glslc:
        print("Error: 'glslc' not found. Please install the Vulkan SDK and ensure VULKAN_SDK is set or glslc is in PATH.")
        sys.exit(1)

    print(f"Using shader compiler: {glslc}")

    extensions = {".vert", ".frag", ".comp"}
    count = 0

    for root, dirs, files in os.walk(shader_dir):
        for file in files:
            base, ext = os.path.splitext(file)
            if ext in extensions:
                input_path = os.path.join(root, file)
                output_path = input_path + ".spv"
                
                # Always compile
                print(f"[Shader] Compiling {file} -> {file}.spv")
                cmd = [glslc, input_path, "-o", output_path]
                res = subprocess.run(cmd)
                if res.returncode != 0:
                    print(f"Error compiling {file}")
                    sys.exit(1)
                count += 1
                
    if count == 0:
        print("Shaders are up to date.")
    else:
        print(f"Compiled {count} shaders.")

if __name__ == "__main__":
    main()
