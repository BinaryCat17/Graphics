cmake_minimum_required (VERSION 3.10)

project ("Graphics" LANGUAGES C)

# --- Options & Output ---

if (MSVC)
    add_compile_options(/utf-8 /W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

enable_testing()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# --- Dependencies ---

find_package(Threads REQUIRED)
find_package(Stb REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(Vulkan REQUIRED)

# --- Code Generation ---

find_package(Python3 REQUIRED)
set(GENERATED_DIR ${CMAKE_CURRENT_SOURCE_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_DIR})

add_custom_command(
    OUTPUT ${GENERATED_DIR}/services_registry.h ${GENERATED_DIR}/services_registry.c
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_services.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_services.py
    COMMENT "Generating service registry code"
)

add_custom_target(GenerateServices DEPENDS ${GENERATED_DIR}/services_registry.h ${GENERATED_DIR}/services_registry.c)

# --- Common Helpers ---

# Interface library for system dependencies (like math lib)
add_library(system_libs INTERFACE)
target_include_directories(system_libs INTERFACE "${GENERATED_DIR}")
if(NOT MSVC)
    target_link_libraries(system_libs INTERFACE m)
endif()

# --- Library Targets ---

# Platform Layer
set(PLATFORM_SOURCES "src/core/platform/glfw_platform.c" "src/core/platform/fs.c")
add_library(platform_layer STATIC ${PLATFORM_SOURCES})
target_include_directories(platform_layer PUBLIC "src/")
target_link_libraries(platform_layer PUBLIC glfw Vulkan::Vulkan system_libs)

# Core library (Math + Memory)
set(CORE_SOURCES
        "src/core/math/coordinate_systems.c"
        "src/core/math/layout_geometry.c"
        "src/core/memory/buffer.c")
add_library(core STATIC ${CORE_SOURCES})
target_include_directories(core PUBLIC "src/")
target_link_libraries(core PUBLIC system_libs)

# Render Backend Common
set(RENDER_COMMON_SOURCES
        "src/services/render/render_graph/render_graph.c"
        "src/services/render/backend/common/render_composition.c"
        "src/services/render/backend/common/ui_mesh_builder.c"
        "src/services/render/backend/common/renderer_backend.c"
        "src/services/render/backend/common/stb_impl.c")
add_library(render_common STATIC ${RENDER_COMMON_SOURCES})
target_include_directories(render_common PUBLIC "src/" ${Stb_INCLUDE_DIR})
target_link_libraries(render_common PUBLIC core system_libs)

# --- Shaders ---

find_program(GLSLC_EXECUTABLE glslc HINTS ENV VULKAN_SDK PATH_SUFFIXES Bin)
if (NOT GLSLC_EXECUTABLE)
    message(FATAL_ERROR "glslc compiler not found. Install Vulkan SDK or ensure glslc is on PATH.")
endif()

set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders)
set(SHADER_OUTPUTS "")

function(add_shader_compile FILE_NAME)
    set(INPUT "${SHADER_DIR}/${FILE_NAME}")
    set(OUTPUT "${SHADER_DIR}/${FILE_NAME}.spv")
    add_custom_command(
        OUTPUT ${OUTPUT}
        COMMAND ${GLSLC_EXECUTABLE} ${INPUT} -o ${OUTPUT}
        DEPENDS ${INPUT}
        COMMENT "Compiling shader ${FILE_NAME}"
        VERBATIM
    )
    # Append to the list in parent scope so the target sees it
    set(SHADER_OUTPUTS ${SHADER_OUTPUTS} ${OUTPUT} PARENT_SCOPE)
endfunction()

add_shader_compile("shader.vert")
add_shader_compile("shader.frag")

add_custom_target(Shaders DEPENDS ${SHADER_OUTPUTS})

# --- More Libraries ---

# Vulkan Renderer Backend
set(RENDER_VULKAN_SOURCES
    "src/services/render/backend/vulkan/vk_render_graph.c"
    "src/services/render/backend/vulkan/vulkan_renderer.c"
    "src/services/render/backend/vulkan/vk_utils.c"
    "src/services/render/backend/vulkan/vk_context.c"
    "src/services/render/backend/vulkan/vk_swapchain.c"
    "src/services/render/backend/vulkan/vk_pipeline.c"
    "src/services/render/backend/vulkan/vk_resources.c"
    "src/services/render/backend/vulkan/vk_ui_render.c")
add_library(render_vulkan STATIC ${RENDER_VULKAN_SOURCES})
target_include_directories(render_vulkan PUBLIC "src/" ${Stb_INCLUDE_DIR})
target_link_libraries(render_vulkan PUBLIC render_common platform_layer Vulkan::Vulkan glfw Threads::Threads system_libs)

# Config library
set(CONFIG_SOURCES
    "src/core/config/simple_yaml.c"
    "src/core/config/module_yaml_loader.c"
    "src/core/config/config_document.c"
    "src/core/config/config_io.c")
add_library(config STATIC ${CONFIG_SOURCES})
target_include_directories(config PUBLIC "src/")
target_link_libraries(config PUBLIC platform_layer system_libs)

# State Manager library
set(STATE_MANAGER_SOURCES "src/core/state/state_manager.c")
add_library(state_manager STATIC ${STATE_MANAGER_SOURCES})
target_include_directories(state_manager PUBLIC "src/")

# Assets Service Library
set(ASSETS_SOURCES "src/services/assets/assets_service.c")
add_library(assets STATIC ${ASSETS_SOURCES})
target_include_directories(assets PUBLIC "src/")
target_link_libraries(assets PUBLIC platform_layer)

# Service Manager library
set(SERVICE_MANAGER_SOURCES "src/core/service_manager/service_manager.c")
add_library(service_manager_lib STATIC ${SERVICE_MANAGER_SOURCES})
target_include_directories(service_manager_lib PUBLIC "src/")
target_link_libraries(service_manager_lib PUBLIC Threads::Threads)

# Scene Service
set(SERVICE_SCENE_SOURCES
    "src/services/scene/cad_scene.c"
    "src/services/scene/cad_scene_yaml.c"
    "src/services/scene/math_scene.c"
    "src/services/scene/math_mesh_builder.c"
    "src/services/scene/scene_service.c")
add_library(service_scene STATIC ${SERVICE_SCENE_SOURCES})
target_include_directories(service_scene PUBLIC "src/")
target_link_libraries(service_scene PUBLIC config platform_layer assets system_libs)
add_dependencies(service_scene GenerateServices)

# UI Service
set(SERVICE_UI_SOURCES
    "src/services/ui/scroll.c"
    "src/services/ui/model_style.c"
    "src/services/ui/ui_node.c"
    "src/services/ui/layout_tree.c"
    "src/services/ui/widget_list.c"
    "src/services/ui/scene_ui.c"
    "src/services/ui/ui_context.c"
    "src/services/ui/compositor.c"
    "src/services/ui/ui_service.c")
add_library(service_ui STATIC ${SERVICE_UI_SOURCES})
target_include_directories(service_ui PUBLIC "src/" ${Stb_INCLUDE_DIR})
target_link_libraries(service_ui PUBLIC config service_scene platform_layer system_libs)
add_dependencies(service_ui GenerateServices)

# Render Runtime Service
set(SERVICE_RENDER_RUNTIME_SOURCES
    "src/services/render/runtime/runtime.c"
    "src/services/render/runtime/render_runtime_service.c")
add_library(service_render_runtime STATIC ${SERVICE_RENDER_RUNTIME_SOURCES})
target_include_directories(service_render_runtime PUBLIC "src/" "${GENERATED_DIR}")
target_link_libraries(service_render_runtime PUBLIC render_vulkan)
add_dependencies(service_render_runtime GenerateServices)

# Render Service (Descriptor)
set(SERVICE_RENDER_SOURCES "src/services/render/render_service.c")
add_library(service_render STATIC ${SERVICE_RENDER_SOURCES})
target_include_directories(service_render PUBLIC "src/" "${GENERATED_DIR}")
add_dependencies(service_render GenerateServices)

# App Services
set(APP_SERVICES_SOURCES "src/app/app_services.c" "${GENERATED_DIR}/services_registry.c")
add_library(app_services STATIC ${APP_SERVICES_SOURCES})
target_include_directories(app_services PUBLIC "src/" "${GENERATED_DIR}")
add_dependencies(app_services GenerateServices)
target_link_libraries(app_services PUBLIC service_manager_lib service_scene service_ui service_render_runtime service_render)

# --- Main Executable ---

add_executable(Graphics "src/app/main.c")
target_include_directories(Graphics PRIVATE "src/" ${Vulkan_INCLUDE_DIRS} ${Stb_INCLUDE_DIR})
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
    Threads::Threads
    system_libs)
add_dependencies(Graphics Shaders)

# --- Tests ---

function(add_graphics_test TEST_NAME SOURCE_FILE)
    add_executable(${TEST_NAME} ${SOURCE_FILE})
    target_include_directories(${TEST_NAME} PRIVATE src ${Stb_INCLUDE_DIR})
    # Link passed libraries (${ARGN}) plus common deps
    target_link_libraries(${TEST_NAME} PRIVATE ${ARGN} system_libs glfw Threads::Threads)
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endfunction()

add_graphics_test(layout_tests tests/layout_tests.c service_ui config platform_layer render_common)
add_graphics_test(ui_compositor_tests tests/ui_compositor_tests.c service_ui config platform_layer render_common)
add_graphics_test(scene_ui_tests tests/scene_ui_tests.c service_ui service_scene config platform_layer render_common)
add_graphics_test(transform_tests tests/transform_tests.c core render_common)
add_graphics_test(render_composition_tests tests/render_composition_tests.c core render_common)
add_graphics_test(ui_mesh_builder_tests tests/ui_mesh_builder_tests.c core render_common)
add_graphics_test(text_clip_integration_tests tests/text_clip_integration_tests.c core render_common)
add_graphics_test(scene_yaml_tests tests/scene_yaml_tests.c service_scene config)
add_graphics_test(math_scene_tests tests/math_scene_tests.c service_scene)
add_graphics_test(math_mesh_tests tests/math_mesh_tests.c service_scene)
add_graphics_test(render_graph_tests tests/render_graph_tests.c render_common)
