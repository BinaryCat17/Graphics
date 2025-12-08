cmake_minimum_required (VERSION 3.8)

project ("Graphics")

enable_testing()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

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

    set(APP_SOURCES
            "app/main.c"
            "app/assets.c" "app/assets.h"
            "app/scroll.c" "app/scroll.h"
            "app/vulkan_renderer.c" "app/vulkan_renderer.h"
            "app/ui_json.c" "app/ui_json.h"
            "src/Graphics.c" "src/Graphics.h"
    )
else()
    set(APP_SOURCES "app/main_stub.c")
endif()

# Добавьте источник в исполняемый файл этого проекта.
add_executable (Graphics ${APP_SOURCES})
target_include_directories(Graphics PRIVATE "src/")

if (NOT VULKAN_STUB)
    add_dependencies(Graphics Shaders)
    set_source_files_properties(app/main.c PROPERTIES LANGUAGE C)
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${Vulkan_INCLUDE_DIRS}
    )
endif()

target_link_libraries(Graphics PRIVATE glfw)
target_link_libraries(Graphics PRIVATE Vulkan::Vulkan)
target_include_directories(Graphics PRIVATE ${Stb_INCLUDE_DIR})

add_executable(layout_tests
        app/layout_tests.c
        app/ui_json.c app/ui_json.h
        app/scroll.c app/scroll.h)
target_link_libraries(layout_tests PRIVATE glfw)
target_include_directories(layout_tests PRIVATE src)
add_test(NAME layout_tests COMMAND layout_tests)

add_executable(transform_tests
        app/transform_tests.c
        src/Graphics.c src/Graphics.h)
target_link_libraries(transform_tests PRIVATE glfw)
target_include_directories(transform_tests PRIVATE src)
add_test(NAME transform_tests COMMAND transform_tests)
