cmake_minimum_required (VERSION 3.10)

if (MSVC)
    add_compile_options(/utf-8)
endif()

project ("Graphics")

enable_testing()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

set(CMAKE_CXX_COMPILER "")
set(CMAKE_CXX_FOUND FALSE)

find_package(Stb REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(Vulkan REQUIRED)

# Platform Layer
set(PLATFORM_SOURCES "src/platform/glfw_platform.c" "src/platform/fs.c")
add_library(platform_layer STATIC ${PLATFORM_SOURCES})
target_include_directories(platform_layer PUBLIC "src/")
target_link_libraries(platform_layer PUBLIC glfw Vulkan::Vulkan)

# Core library
set(CORE_SOURCES
        "src/coordinate_systems/coordinate_systems.c"
        "src/coordinate_systems/layout_geometry.c"
        "src/memory/buffer.c")
add_library(core STATIC ${CORE_SOURCES})
target_include_directories(core PUBLIC "src/")

# Render Common
set(RENDER_COMMON_SOURCES
        "src/render/common/render_composition.c"
        "src/render/common/ui_mesh_builder.c"
        "src/render/common/renderer_backend.c"
        "src/render/common/stb_impl.c")
add_library(render_common STATIC ${RENDER_COMMON_SOURCES})
target_include_directories(render_common PUBLIC "src/" "services/" ${Stb_INCLUDE_DIR})
target_link_libraries(render_common PUBLIC core)
if (NOT MSVC)
    target_link_libraries(render_common PUBLIC m)
endif()

# Shader Compilation
find_program(GLSLC_EXECUTABLE glslc HINTS ENV VULKAN_SDK PATH_SUFFIXES Bin)
if (NOT GLSLC_EXECUTABLE)
    message(FATAL_ERROR "glslc compiler not found. Install Vulkan SDK or ensure glslc is on PATH.")
endif()
set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders)
set(SHADER_OUTPUTS
        ${SHADER_DIR}/shader.vert.spv
        ${SHADER_DIR}/shader.frag.spv
)
add_custom_target(Shaders
        DEPENDS ${SHADER_OUTPUTS}
)
add_custom_command(
        OUTPUT ${SHADER_DIR}/shader.vert.spv
        COMMAND ${GLSLC_EXECUTABLE} ${SHADER_DIR}/shader.vert -o ${SHADER_DIR}/shader.vert.spv
        DEPENDS ${SHADER_DIR}/shader.vert
        COMMENT "Compiling vertex shader"
        VERBATIM
)
add_custom_command(
        OUTPUT ${SHADER_DIR}/shader.frag.spv
        COMMAND ${GLSLC_EXECUTABLE} ${SHADER_DIR}/shader.frag -o ${SHADER_DIR}/shader.frag.spv
        DEPENDS ${SHADER_DIR}/shader.frag
        COMMENT "Compiling fragment shader"
        VERBATIM
)

# Vulkan Renderer
set(RENDER_VULKAN_SOURCES "src/render/vulkan/vulkan_renderer.c")
add_library(render_vulkan STATIC ${RENDER_VULKAN_SOURCES})
target_include_directories(render_vulkan PUBLIC "src/" "services/" ${Stb_INCLUDE_DIR})
target_link_libraries(render_vulkan PUBLIC render_common platform_layer Vulkan::Vulkan glfw Threads::Threads)
if (NOT MSVC)
    target_link_libraries(render_vulkan PUBLIC m)
endif()

# Config library
set(CONFIG_SOURCES
    "src/config/simple_yaml.c"
    "src/config/module_yaml_loader.c"
    "src/config/config_document.c"
    "src/config/config_io.c")
add_library(config STATIC ${CONFIG_SOURCES})
target_include_directories(config PUBLIC "src/" "services/")
target_link_libraries(config PUBLIC platform_layer)

# State Manager library
set(STATE_MANAGER_SOURCES "src/state/state_manager.c")
add_library(state_manager STATIC ${STATE_MANAGER_SOURCES})
target_include_directories(state_manager PUBLIC "src/")

# Assets library
set(ASSETS_SOURCES "src/assets/assets.c")
add_library(assets STATIC ${ASSETS_SOURCES})
target_include_directories(assets PUBLIC "src/")
target_link_libraries(assets PUBLIC platform_layer)

# Service Manager library
set(SERVICE_MANAGER_SOURCES "src/services/service_manager.c")
add_library(service_manager_lib STATIC ${SERVICE_MANAGER_SOURCES})
target_include_directories(service_manager_lib PUBLIC "src/" "services/")
target_link_libraries(service_manager_lib PUBLIC Threads::Threads)

# Scene Service
set(SERVICE_SCENE_SOURCES
    "services/scene/cad_scene.c"
    "services/scene/cad_scene_yaml.c"
    "services/scene/scene_service.c")
add_library(service_scene STATIC ${SERVICE_SCENE_SOURCES})
target_include_directories(service_scene PUBLIC "src/" "services/")
target_link_libraries(service_scene PUBLIC config platform_layer assets)

# UI Service
set(SERVICE_UI_SOURCES
    "services/ui/scroll.c"
    "services/ui/model_style.c"
    "services/ui/ui_node.c"
    "services/ui/layout_tree.c"
    "services/ui/widget_list.c"
    "services/ui/scene_ui.c"
    "services/ui/ui_context.c"
    "services/ui/compositor.c"
    "services/ui/ui_service.c")
add_library(service_ui STATIC ${SERVICE_UI_SOURCES})
target_include_directories(service_ui PUBLIC "src/" "services/" ${Stb_INCLUDE_DIR})
target_link_libraries(service_ui PUBLIC config service_scene platform_layer)

# Render Runtime Service
set(SERVICE_RENDER_RUNTIME_SOURCES
    "services/render_runtime/runtime.c"
    "services/render_runtime/render_runtime_service.c")
add_library(service_render_runtime STATIC ${SERVICE_RENDER_RUNTIME_SOURCES})
target_include_directories(service_render_runtime PUBLIC "src/" "services/")
target_link_libraries(service_render_runtime PUBLIC render_vulkan)

# Render Service
set(SERVICE_RENDER_SOURCES "services/render_service/render_service.c")
add_library(service_render STATIC ${SERVICE_RENDER_SOURCES})
target_include_directories(service_render PUBLIC "src/" "services/")

# App Services
set(APP_SERVICES_SOURCES "src/app/app_services.c")
add_library(app_services STATIC ${APP_SERVICES_SOURCES})
target_include_directories(app_services PUBLIC "src/" "services/")
target_link_libraries(app_services PUBLIC service_manager_lib service_scene service_ui service_render_runtime service_render)

# Main Executable
add_executable(Graphics "app/main.c")
target_include_directories(Graphics PRIVATE "src/" "services/" ${Vulkan_INCLUDE_DIRS} ${Stb_INCLUDE_DIR})
target_link_libraries(Graphics PRIVATE
    app_services
    assets
    config
    core
    platform_layer
    render_common
    render_vulkan
    service_manager_lib
    service_scene
    service_ui
    service_render_runtime
    service_render
    state_manager
    glfw
    Vulkan::Vulkan
    Threads::Threads)
if (NOT MSVC)
    target_link_libraries(Graphics PRIVATE m)
endif()
add_dependencies(Graphics Shaders)


# --- Tests ---
# Link tests against libraries instead of source files

add_executable(layout_tests tests/layout_tests.c)
target_link_libraries(layout_tests PRIVATE service_ui config platform_layer render_common glfw Threads::Threads)
if (NOT MSVC)
    target_link_libraries(layout_tests PRIVATE m)
endif()
target_include_directories(layout_tests PRIVATE src services ${Stb_INCLUDE_DIR})
add_test(NAME layout_tests COMMAND layout_tests)

add_executable(ui_compositor_tests tests/ui_compositor_tests.c)
target_link_libraries(ui_compositor_tests PRIVATE service_ui config platform_layer render_common glfw Threads::Threads)
if (NOT MSVC)
    target_link_libraries(ui_compositor_tests PRIVATE m)
endif()
target_include_directories(ui_compositor_tests PRIVATE src services ${Stb_INCLUDE_DIR})
add_test(NAME ui_compositor_tests COMMAND ui_compositor_tests)

add_executable(scene_ui_tests tests/scene_ui_tests.c)
target_link_libraries(scene_ui_tests PRIVATE service_ui service_scene config platform_layer render_common glfw Threads::Threads)
if (NOT MSVC)
    target_link_libraries(scene_ui_tests PRIVATE m)
endif()
target_include_directories(scene_ui_tests PRIVATE src services ${Stb_INCLUDE_DIR})
add_test(NAME scene_ui_tests COMMAND scene_ui_tests)

add_executable(transform_tests tests/transform_tests.c)
target_link_libraries(transform_tests PRIVATE core render_common glfw Threads::Threads)
if (NOT MSVC)
    target_link_libraries(transform_tests PRIVATE m)
endif()
target_include_directories(transform_tests PRIVATE src services)
add_test(NAME transform_tests COMMAND transform_tests)

add_executable(render_composition_tests tests/render_composition_tests.c)
target_link_libraries(render_composition_tests PRIVATE core render_common glfw Threads::Threads)
if (NOT MSVC)
    target_link_libraries(render_composition_tests PRIVATE m)
endif()
target_include_directories(render_composition_tests PRIVATE src services)
add_test(NAME render_composition_tests COMMAND render_composition_tests)

add_executable(ui_mesh_builder_tests tests/ui_mesh_builder_tests.c)
target_link_libraries(ui_mesh_builder_tests PRIVATE core render_common glfw Threads::Threads)
if (NOT MSVC)
    target_link_libraries(ui_mesh_builder_tests PRIVATE m)
endif()
target_include_directories(ui_mesh_builder_tests PRIVATE src services)
add_test(NAME ui_mesh_builder_tests COMMAND ui_mesh_builder_tests)

add_executable(text_clip_integration_tests tests/text_clip_integration_tests.c)
target_link_libraries(text_clip_integration_tests PRIVATE core render_common glfw Threads::Threads)
if (NOT MSVC)
    target_link_libraries(text_clip_integration_tests PRIVATE m)
endif()
target_include_directories(text_clip_integration_tests PRIVATE src services)
add_test(NAME text_clip_integration_tests COMMAND text_clip_integration_tests)

add_executable(scene_yaml_tests tests/scene_yaml_tests.c)
target_link_libraries(scene_yaml_tests PRIVATE service_scene config Threads::Threads)
target_include_directories(scene_yaml_tests PRIVATE src services)
add_test(NAME scene_yaml_tests COMMAND scene_yaml_tests)
