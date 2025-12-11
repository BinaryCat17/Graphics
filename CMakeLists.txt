cmake_minimum_required (VERSION 3.8)

project ("Graphics")

enable_testing()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

set(CMAKE_CXX_COMPILER "")
set(CMAKE_CXX_FOUND FALSE)

find_package(Stb QUIET)
if (NOT Stb_FOUND)
    message(STATUS "Stb package not found; using bundled stub header")
    set(Stb_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb")
endif()
find_package(glfw3 CONFIG QUIET)
if (NOT glfw3_FOUND)
    message(STATUS "glfw3 package not found; using bundled stub library")
    add_library(glfw STATIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glfw_stub/glfw_stub.c)
    target_include_directories(glfw PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glfw_stub)
endif()
find_package(Vulkan QUIET)
if (NOT Vulkan_FOUND)
    message(STATUS "Vulkan package not found; building stub executable")
    add_library(VulkanStub INTERFACE)
    add_library(Vulkan::Vulkan ALIAS VulkanStub)
    set(VULKAN_STUB ON)
endif()

set(PLATFORM_SOURCES "src/platform/glfw_platform.c")
add_library(platform_layer STATIC ${PLATFORM_SOURCES})
target_include_directories(platform_layer PUBLIC "src/")
target_link_libraries(platform_layer PUBLIC glfw Vulkan::Vulkan)

set(CORE_SOURCES
        "src/coordinate_systems/coordinate_systems.c"
        "src/coordinate_systems/layout_geometry.c"
        "src/memory/buffer.c")

set(RENDER_COMMON_SOURCES
        "src/render/common/render_composition.c"
        "src/render/common/ui_mesh_builder.c"
        "src/render/common/renderer_backend.c")

add_library(render_common STATIC ${RENDER_COMMON_SOURCES})
target_include_directories(render_common PUBLIC "src/" "services/" ${Stb_INCLUDE_DIR})
if (NOT MSVC)
    target_link_libraries(render_common PUBLIC m)
endif()
if (NOT VULKAN_STUB)
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

    set(RENDER_VULKAN_SOURCES
            "src/render/vulkan/vulkan_renderer.c"
            "src/render/vulkan/vulkan_renderer.h")

    add_library(render_vulkan STATIC ${RENDER_VULKAN_SOURCES})
    target_include_directories(render_vulkan PUBLIC "src/" "services/" ${Stb_INCLUDE_DIR})
    target_link_libraries(render_vulkan PUBLIC render_common platform_layer Vulkan::Vulkan glfw Threads::Threads)
    if (NOT MSVC)
        target_link_libraries(render_vulkan PUBLIC m)
    endif()

    set(APP_SOURCES
            "app/main.c"
            "src/assets/assets.c" "src/assets/assets.h"
            "services/ui/scroll.c" "services/ui/scroll.h" "services/ui/ui_config.c" "services/ui/ui_config.h" "services/ui/scene_ui.c" "services/ui/scene_ui.h" "services/ui/ui_context.h"
            ${CORE_SOURCES}
            "src/cad/cad_scene.c" "src/cad/cad_scene.h"
            "services/scene/cad_scene_yaml.c" "services/scene/cad_scene_yaml.h"
            "src/state/state_manager.c" "src/state/state_manager.h"
            "src/config/simple_yaml.c" "src/config/simple_yaml.h" "src/config/module_yaml_loader.c" "src/config/module_yaml_loader.h" "src/config/config_document.c" "src/config/config_document.h" "src/config/config_io.c" "src/config/config_io.h"
            "services/render_runtime/runtime.c" "services/render_runtime/runtime.h"
            "src/app/app_services.h" "src/app/app_services.c"
            "services/scene/scene_service.c" "services/scene/scene_service.h"
            "services/ui/ui_service.c" "services/ui/ui_service.h" "src/services/service_events.h"
            "services/render_runtime/render_runtime_service.c" "services/render_runtime/render_runtime_service.h"
            "services/render_service/render_service.c" "services/render_service/render_service.h"
            "src/services/service_manager.c" "src/services/service_manager.h" "src/services/service.h"
    )
else()
    set(APP_SOURCES
            "app/main_stub.c"
            "src/cad/cad_scene.c" "src/cad/cad_scene.h"
            "services/scene/cad_scene_yaml.c" "services/scene/cad_scene_yaml.h"
            "src/state/state_manager.c" "src/state/state_manager.h"
            "src/config/simple_yaml.c" "src/config/simple_yaml.h" "src/config/module_yaml_loader.c" "src/config/module_yaml_loader.h" "src/config/config_document.c" "src/config/config_document.h" "src/config/config_io.c" "src/config/config_io.h"
            "services/ui/scene_ui.c" "services/ui/scene_ui.h" "services/ui/ui_config.c" "services/ui/ui_config.h" "services/ui/ui_context.h"
    )
endif()

# Добавьте источник в исполняемый файл этого проекта.
add_executable (Graphics ${APP_SOURCES})
target_include_directories(Graphics PRIVATE "src/" "services/")
target_link_libraries(Graphics PRIVATE render_common platform_layer)

if (NOT VULKAN_STUB)
    add_dependencies(Graphics Shaders)
    set_source_files_properties(app/main.c PROPERTIES LANGUAGE C)
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${Vulkan_INCLUDE_DIRS}
    )
    target_link_libraries(Graphics PRIVATE render_vulkan)
endif()

if (NOT Stb_FOUND)
    target_include_directories(Graphics PRIVATE "third_party/stb")
endif()

target_link_libraries(Graphics PRIVATE glfw)
target_link_libraries(Graphics PRIVATE Vulkan::Vulkan)
target_link_libraries(Graphics PRIVATE Threads::Threads)
if (NOT MSVC)
    target_link_libraries(Graphics PRIVATE m)
endif()
target_include_directories(Graphics PRIVATE ${Stb_INCLUDE_DIR})

add_executable(layout_tests
        tests/layout_tests.c
        services/ui/ui_config.c services/ui/ui_config.h
        services/ui/scene_ui.c services/ui/scene_ui.h
        services/ui/scroll.c services/ui/scroll.h
        src/config/config_document.c src/config/config_document.h src/config/config_io.c src/config/config_io.h
        src/config/simple_yaml.c src/config/simple_yaml.h)
target_link_libraries(layout_tests PRIVATE glfw Threads::Threads)
if (NOT MSVC)
    target_link_libraries(layout_tests PRIVATE m)
endif()
target_include_directories(layout_tests PRIVATE src services ${Stb_INCLUDE_DIR})
add_test(NAME layout_tests COMMAND layout_tests)

add_executable(scene_ui_tests
        tests/scene_ui_tests.c
        services/ui/ui_config.c services/ui/ui_config.h services/ui/scene_ui.c services/ui/scene_ui.h
        services/ui/scroll.c services/ui/scroll.h
        src/cad/cad_scene.c src/cad/cad_scene.h
        src/config/config_document.c src/config/config_document.h src/config/config_io.c src/config/config_io.h
        src/config/simple_yaml.c src/config/simple_yaml.h)
target_link_libraries(scene_ui_tests PRIVATE glfw Threads::Threads)
if (NOT MSVC)
    target_link_libraries(scene_ui_tests PRIVATE m)
endif()
target_include_directories(scene_ui_tests PRIVATE src services ${Stb_INCLUDE_DIR})
add_test(NAME scene_ui_tests COMMAND scene_ui_tests)

add_executable(transform_tests
        tests/transform_tests.c
        ${CORE_SOURCES})
target_link_libraries(transform_tests PRIVATE glfw Threads::Threads render_common)
if (NOT MSVC)
    target_link_libraries(transform_tests PRIVATE m)
endif()
target_include_directories(transform_tests PRIVATE src services)
add_test(NAME transform_tests COMMAND transform_tests)

add_executable(render_composition_tests
        tests/render_composition_tests.c
        ${CORE_SOURCES})
target_link_libraries(render_composition_tests PRIVATE glfw Threads::Threads render_common)
if (NOT MSVC)
    target_link_libraries(render_composition_tests PRIVATE m)
endif()
target_include_directories(render_composition_tests PRIVATE src services)
add_test(NAME render_composition_tests COMMAND render_composition_tests)

add_executable(ui_mesh_builder_tests
        tests/ui_mesh_builder_tests.c
        ${CORE_SOURCES})
target_link_libraries(ui_mesh_builder_tests PRIVATE glfw Threads::Threads render_common)
if (NOT MSVC)
    target_link_libraries(ui_mesh_builder_tests PRIVATE m)
endif()
target_include_directories(ui_mesh_builder_tests PRIVATE src services)
add_test(NAME ui_mesh_builder_tests COMMAND ui_mesh_builder_tests)

add_executable(scene_yaml_tests
        tests/scene_yaml_tests.c
        services/scene/cad_scene_yaml.c services/scene/cad_scene_yaml.h
        src/cad/cad_scene.c src/cad/cad_scene.h)
target_link_libraries(scene_yaml_tests PRIVATE Threads::Threads)
target_include_directories(scene_yaml_tests PRIVATE src services)
add_test(NAME scene_yaml_tests COMMAND scene_yaml_tests)
