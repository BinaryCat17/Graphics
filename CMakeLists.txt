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

# --- Shaders ---
# (Keeping shader compilation logic as is)
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
    set(SHADER_OUTPUTS ${SHADER_OUTPUTS} ${OUTPUT} PARENT_SCOPE)
endfunction()

add_shader_compile("shader.vert")
add_shader_compile("shader.frag")

add_custom_target(Shaders DEPENDS ${SHADER_OUTPUTS})

# --- CODE GENERATION ---

find_package(Python3 REQUIRED)
set(GENERATED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/generated")
set(REFLECTION_REGISTRY "${GENERATED_DIR}/reflection_registry.c")

add_custom_command(
    OUTPUT ${REFLECTION_REGISTRY}
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/codegen.py" "${CMAKE_CURRENT_SOURCE_DIR}/src" "${REFLECTION_REGISTRY}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tools/codegen.py"
    COMMENT "Generating reflection metadata..."
)

# --- FOUNDATION LAYER ---

# Meta (Reflection)
set(FOUNDATION_META_SOURCES
    "src/foundation/meta/reflection.c"
    ${REFLECTION_REGISTRY}
)
add_library(foundation_meta STATIC ${FOUNDATION_META_SOURCES})
target_include_directories(foundation_meta PUBLIC "src/")

# Platform
set(FOUNDATION_PLATFORM_SOURCES
    "src/foundation/platform/glfw_platform.c"
    "src/foundation/platform/fs.c")
add_library(foundation_platform STATIC ${FOUNDATION_PLATFORM_SOURCES})
target_include_directories(foundation_platform PUBLIC "src/")
target_link_libraries(foundation_platform PUBLIC glfw Vulkan::Vulkan)

# Math
set(FOUNDATION_MATH_SOURCES
    "src/foundation/math/coordinate_systems.c"
    "src/foundation/math/layout_geometry.c")
add_library(foundation_math STATIC ${FOUNDATION_MATH_SOURCES})
target_include_directories(foundation_math PUBLIC "src/")
if(UNIX)
    target_link_libraries(foundation_math PUBLIC m)
endif()

# Memory
set(FOUNDATION_MEMORY_SOURCES "src/foundation/memory/buffer.c")
add_library(foundation_memory STATIC ${FOUNDATION_MEMORY_SOURCES})
target_include_directories(foundation_memory PUBLIC "src/")

# Config
set(FOUNDATION_CONFIG_SOURCES
    "src/foundation/config/simple_yaml.c"
    "src/foundation/config/config_document.c"
    "src/foundation/config/config_io.c")
add_library(foundation_config STATIC ${FOUNDATION_CONFIG_SOURCES})
target_include_directories(foundation_config PUBLIC "src/")
target_link_libraries(foundation_config PUBLIC foundation_platform)

# --- ENGINE LAYER ---

# Render (Common + Vulkan + Graph)
# Note: merging backend/common, backend/vulkan, render_graph into one 'engine_render' lib for simplicity.
# Or keeping them split but under engine_render umbrella.
set(ENGINE_RENDER_SOURCES
    "src/engine/render/backend/common/render_composition.c"
    "src/engine/render/backend/common/ui_mesh_builder.c"
    "src/engine/render/backend/common/renderer_backend.c"
    "src/engine/render/backend/common/stb_impl.c"
    "src/engine/render/backend/vulkan/vulkan_renderer.c"
    "src/engine/render/backend/vulkan/vk_utils.c"
    "src/engine/render/backend/vulkan/vk_context.c"
    "src/engine/render/backend/vulkan/vk_swapchain.c"
    "src/engine/render/backend/vulkan/vk_pipeline.c"
    "src/engine/render/backend/vulkan/vk_resources.c"
    "src/engine/render/backend/vulkan/vk_ui_render.c"
    "src/engine/render/backend/vulkan/vk_render_graph.c"
    "src/engine/render/render_graph/render_graph.c"
    # Runtime
    "src/engine/render/render_thread.c"
    "src/engine/render/render_system.c"
)
add_library(engine_render STATIC ${ENGINE_RENDER_SOURCES})
target_include_directories(engine_render PUBLIC "src/" ${Stb_INCLUDE_DIR})
target_link_libraries(engine_render PUBLIC foundation_platform foundation_math foundation_memory Vulkan::Vulkan glfw Threads::Threads foundation_config engine_ui engine_assets)
# Note: circular dependency UI <-> Render might exist in legacy code.

# Assets
set(ENGINE_ASSETS_SOURCES "src/engine/assets/assets_service.c")
add_library(engine_assets STATIC ${ENGINE_ASSETS_SOURCES})
target_include_directories(engine_assets PUBLIC "src/")
target_link_libraries(engine_assets PUBLIC foundation_platform foundation_config)

# UI
set(ENGINE_UI_SOURCES
    "src/engine/ui/ui_renderer.c"
    "src/engine/ui/ui_loader.c"
    "src/engine/ui/ui_def.c")
add_library(engine_ui STATIC ${ENGINE_UI_SOURCES})
target_include_directories(engine_ui PUBLIC "src/" ${Stb_INCLUDE_DIR})
target_link_libraries(engine_ui PUBLIC foundation_config engine_assets foundation_meta)

# --- DOMAIN LAYER ---

# CAD Model
set(DOMAIN_CAD_SOURCES
    "src/domains/cad_model/cad_scene.c"
    "src/domains/cad_model/cad_scene_yaml.c"
    "src/domains/cad_model/scene_service.c")
add_library(domain_cad STATIC ${DOMAIN_CAD_SOURCES})
target_include_directories(domain_cad PUBLIC "src/")
target_link_libraries(domain_cad PUBLIC foundation_config engine_assets engine_ui)

# Math Model
set(DOMAIN_MATH_SOURCES
    "src/domains/math_model/math_scene.c"
    "src/domains/math_model/math_mesh_builder.c"
    "src/domains/math_model/math_graph.c")
add_library(domain_math STATIC ${DOMAIN_MATH_SOURCES})
target_include_directories(domain_math PUBLIC "src/")
target_link_libraries(domain_math PUBLIC foundation_math)
if(UNIX)
    target_link_libraries(domain_math PUBLIC m)
endif()

# --- APP ---

# We need to create a dummy main.c first or use the existing one, but existing one is full of service junk.
# I will overwrite the existing main.c in next step.

add_executable(Graphics "src/app/main.c")
target_include_directories(Graphics PRIVATE "src/" ${Vulkan_INCLUDE_DIRS} ${Stb_INCLUDE_DIR})
target_link_libraries(Graphics PRIVATE
    domain_cad
    domain_math
    engine_render
    engine_ui
    engine_assets
    foundation_config
    foundation_platform
    foundation_meta
    foundation_math
    foundation_memory
    glfw
    Vulkan::Vulkan
    Threads::Threads)
add_dependencies(Graphics Shaders)

# --- Tests ---

function(add_graphics_test TEST_NAME SOURCE_FILE)
    add_executable(${TEST_NAME} ${SOURCE_FILE})
    target_include_directories(${TEST_NAME} PRIVATE src ${Stb_INCLUDE_DIR})
    target_link_libraries(${TEST_NAME} PRIVATE ${ARGN} foundation_math foundation_memory) # Link common base
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endfunction()

# Update test paths and libs
add_graphics_test(transform_tests tests/transform_tests.c foundation_math engine_render)
add_graphics_test(render_composition_tests tests/render_composition_tests.c foundation_math engine_render)
add_graphics_test(ui_mesh_builder_tests tests/ui_mesh_builder_tests.c foundation_math engine_render)
add_graphics_test(text_clip_integration_tests tests/text_clip_integration_tests.c foundation_math engine_render)
add_graphics_test(scene_yaml_tests tests/scene_yaml_tests.c domain_cad foundation_config)
add_graphics_test(math_scene_tests tests/math_scene_tests.c domain_math)
add_graphics_test(math_mesh_tests tests/math_mesh_tests.c domain_math)
add_graphics_test(render_graph_tests tests/render_graph_tests.c engine_render)