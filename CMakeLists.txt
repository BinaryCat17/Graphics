cmake_minimum_required (VERSION 3.10)

project ("Graphics" LANGUAGES C)

# --- Options & Output ---

find_program(CLANG_TIDY "clang-tidy")
if(CLANG_TIDY)
    message(STATUS "Clang-tidy found: ${CLANG_TIDY}")
    set(CMAKE_C_CLANG_TIDY "${CLANG_TIDY}")
else()
    message(STATUS "Clang-tidy not found")
endif()

if (MSVC)
    add_compile_options(/utf-8 /W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
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

find_package(Python3 REQUIRED)

add_custom_target(Shaders
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/build_shaders.py"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Building Shaders..."
)

# --- CODE GENERATION ---

find_package(Python3 REQUIRED)
set(GENERATED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/generated")
set(REFLECTION_REGISTRY "${GENERATED_DIR}/reflection_registry.c")

add_custom_target(Codegen
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/codegen.py"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Running reflection codegen..."
    BYPRODUCTS ${REFLECTION_REGISTRY}
)
set_source_files_properties(${REFLECTION_REGISTRY} PROPERTIES GENERATED TRUE)

# --- FOUNDATION LAYER ---

# Thread
add_library(foundation_thread STATIC "src/foundation/thread/thread.c")
target_include_directories(foundation_thread PUBLIC "src/")
target_link_libraries(foundation_thread PUBLIC Threads::Threads)

# Logger
add_library(foundation_logger STATIC "src/foundation/logger/logger.c")
target_include_directories(foundation_logger PUBLIC "src/")
target_link_libraries(foundation_logger PUBLIC foundation_thread)

# String
add_library(foundation_string STATIC "src/foundation/string/string_id.c")
target_include_directories(foundation_string PUBLIC "src/")

# Meta (Reflection)
set(FOUNDATION_META_SOURCES
    "src/foundation/meta/reflection.c"
    ${REFLECTION_REGISTRY}
)
add_library(foundation_meta STATIC ${FOUNDATION_META_SOURCES})
target_include_directories(foundation_meta PUBLIC "src/")
target_link_libraries(foundation_meta PUBLIC foundation_logger foundation_string)
add_dependencies(foundation_meta Codegen)

# Platform
set(FOUNDATION_PLATFORM_SOURCES
    "src/foundation/platform/glfw_platform.c"
    "src/foundation/platform/fs.c")
add_library(foundation_platform STATIC ${FOUNDATION_PLATFORM_SOURCES})
target_include_directories(foundation_platform PUBLIC "src/")
target_link_libraries(foundation_platform PUBLIC glfw Vulkan::Vulkan foundation_logger foundation_string)

# Math
set(FOUNDATION_MATH_SOURCES
    "src/foundation/math/math_types.c"
    "src/foundation/math/coordinate_systems.c")
add_library(foundation_math STATIC ${FOUNDATION_MATH_SOURCES})
target_include_directories(foundation_math PUBLIC "src/")
if(UNIX)
    target_link_libraries(foundation_math PUBLIC m)
endif()

# Memory
set(FOUNDATION_MEMORY_SOURCES 
    "src/foundation/memory/arena.c"
    "src/foundation/memory/pool.c"
)
add_library(foundation_memory STATIC ${FOUNDATION_MEMORY_SOURCES})
target_include_directories(foundation_memory PUBLIC "src/")
target_link_libraries(foundation_memory PUBLIC foundation_logger)

# Image
add_library(foundation_image STATIC "src/foundation/image/image.c")
target_include_directories(foundation_image PUBLIC "src/" ${Stb_INCLUDE_DIR})
target_link_libraries(foundation_image PUBLIC foundation_logger)

# Config
set(FOUNDATION_CONFIG_SOURCES
    "src/foundation/config/simple_yaml.c"
    "src/foundation/config/config_types.c"
    "src/foundation/config/config_system.c")
add_library(foundation_config STATIC ${FOUNDATION_CONFIG_SOURCES})
target_include_directories(foundation_config PUBLIC "src/")
target_link_libraries(foundation_config PUBLIC foundation_platform foundation_logger)

# --- ENGINE LAYER ---

# Scene
add_library(engine_scene STATIC 
    "src/engine/scene/scene.c"
    "src/engine/scene/scene_asset.c"
    "src/engine/scene/internal/scene_graph.c"
    "src/engine/scene/internal/scene_loader.c"
)
target_include_directories(engine_scene PUBLIC "src/")
target_link_libraries(engine_scene PUBLIC foundation_math foundation_memory foundation_string foundation_meta foundation_config)

# Input
add_library(engine_input STATIC "src/engine/input/input.c")
target_include_directories(engine_input PUBLIC "src/")
target_link_libraries(engine_input PUBLIC foundation_platform foundation_logger)

# Text
set(ENGINE_TEXT_SOURCES
    "src/engine/text/text_renderer.c"
    "src/engine/text/font.c"
    "src/engine/text/internal/stb_impl.c"
)
add_library(engine_text STATIC ${ENGINE_TEXT_SOURCES})
target_include_directories(engine_text PUBLIC "src/" ${Stb_INCLUDE_DIR})
target_link_libraries(engine_text PUBLIC foundation_platform foundation_logger engine_scene)

# Assets
set(ENGINE_ASSETS_SOURCES
    "src/engine/assets/assets.c"
    "src/engine/assets/internal/asset_storage.c"
    "src/engine/assets/internal/asset_loader.c"
)
add_library(engine_assets STATIC ${ENGINE_ASSETS_SOURCES})
target_include_directories(engine_assets PUBLIC "src/")
target_link_libraries(engine_assets PUBLIC foundation_platform foundation_config engine_scene)

# Graphics (Renderer + Vulkan)
set(ENGINE_GRAPHICS_SOURCES
    "src/engine/graphics/internal/backend/renderer_backend.c"
    "src/engine/graphics/internal/backend/vulkan/vulkan_renderer.c"
    "src/engine/graphics/internal/backend/vulkan/vk_utils.c"
    "src/engine/graphics/internal/backend/vulkan/vk_buffer.c"
    "src/engine/graphics/internal/backend/vulkan/vk_context.c"
    "src/engine/graphics/stream.c"
    "src/engine/graphics/primitive_batcher.c"
    "src/engine/graphics/gpu_input.c"
    "src/engine/graphics/compute_graph.c"
    "src/engine/graphics/internal/backend/vulkan/vk_swapchain.c"
    "src/engine/graphics/internal/backend/vulkan/vk_pipeline.c"
    "src/engine/graphics/internal/backend/vulkan/vk_resources.c"
    "src/engine/graphics/render_system.c"
)
add_library(engine_graphics STATIC ${ENGINE_GRAPHICS_SOURCES})
target_include_directories(engine_graphics PUBLIC "src/" ${Stb_INCLUDE_DIR})
target_link_libraries(engine_graphics PUBLIC 
    foundation_platform foundation_math foundation_memory foundation_image
    Vulkan::Vulkan glfw Threads::Threads 
    foundation_config engine_assets engine_scene
)

# UI
set(ENGINE_UI_SOURCES
    src/engine/ui/ui_core.c
    src/engine/ui/internal/ui_input.c
    src/engine/ui/internal/ui_layout.c
    src/engine/ui/internal/ui_renderer.c
    src/engine/ui/internal/ui_command_system.c
    src/engine/ui/internal/ui_binding.c)
add_library(engine_ui STATIC ${ENGINE_UI_SOURCES})
target_include_directories(engine_ui PUBLIC "src/" ${Stb_INCLUDE_DIR})
target_link_libraries(engine_ui PUBLIC 
    foundation_config engine_assets foundation_meta foundation_math 
    engine_scene engine_text engine_input
)

# --- FEATURES LAYER ---

# Math Engine (Feature)
set(FEATURE_MATH_ENGINE_SOURCES
    "src/features/math_engine/math_graph.c"
    "src/features/math_engine/math_editor.c"
    "src/features/math_engine/internal/math_editor_view.c"
    "src/features/math_engine/internal/transpiler.c"
    "src/features/math_engine/internal/emitters/glsl_emitter.c"
    "src/features/math_engine/internal/emitters/c_emitter.c"
    "src/features/math_engine/math_serializer.c")
add_library(feature_math_engine STATIC ${FEATURE_MATH_ENGINE_SOURCES})
target_include_directories(feature_math_engine PUBLIC "src/")
target_link_libraries(feature_math_engine PUBLIC foundation_math foundation_memory)
target_link_libraries(feature_math_engine PRIVATE 
    engine_graphics engine_ui engine_scene engine_text
    foundation_platform foundation_config foundation_logger foundation_meta
)
if(UNIX)
    target_link_libraries(feature_math_engine PUBLIC m)
endif()

# Engine Core
set(ENGINE_CORE_SOURCES
    "src/engine/core/engine.c"
)
add_library(engine_core STATIC ${ENGINE_CORE_SOURCES})
target_include_directories(engine_core PUBLIC "src/")
target_link_libraries(engine_core PUBLIC 
    engine_graphics engine_ui engine_assets engine_text feature_math_engine 
    foundation_meta foundation_logger engine_input
)

# --- APP ---

add_executable(Graphics "src/app/main.c")
target_include_directories(Graphics PRIVATE "src/" ${Vulkan_INCLUDE_DIRS} ${Stb_INCLUDE_DIR})
target_link_libraries(Graphics PRIVATE
    engine_core
    feature_math_engine
    engine_graphics
    engine_ui
    engine_assets
    engine_scene
    engine_text
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
add_graphics_test(transform_tests tests/transform_tests.c foundation_math engine_scene)
add_graphics_test(transpiler_tests tests/transpiler_tests.c feature_math_engine)
add_graphics_test(ui_tests tests/ui_tests.c engine_ui foundation_logger feature_math_engine)
add_graphics_test(memory_tests tests/memory_tests.c foundation_memory)
add_graphics_test(string_tests tests/string_tests.c foundation_string)

# Config Tests (Manual definition to include reflection.c source)
add_executable(config_tests tests/config_tests.c src/foundation/meta/reflection.c)
target_include_directories(config_tests PRIVATE src ${Stb_INCLUDE_DIR})
target_link_libraries(config_tests PRIVATE foundation_config foundation_string foundation_logger foundation_math foundation_memory)
add_test(NAME config_tests COMMAND config_tests)