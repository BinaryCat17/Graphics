cmake_minimum_required (VERSION 3.8)

project ("Graphics")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_CXX_COMPILER "")
set(CMAKE_CXX_FOUND FALSE)

find_package(Stb REQUIRED)
find_package(cglm CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(Vulkan REQUIRED)

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

# Добавьте источник в исполняемый файл этого проекта.
add_executable (Graphics
        "app/main.c"
        "src/Graphics.c" "src/Graphics.h"
)

add_dependencies(Graphics Shaders)

set_source_files_properties(src/main.c PROPERTIES LANGUAGE C)
target_include_directories(${PROJECT_NAME} PRIVATE
    ${Vulkan_INCLUDE_DIRS}
)

target_link_libraries(Graphics PRIVATE glfw)
target_link_libraries(Graphics PRIVATE cglm::cglm)
target_link_libraries(Graphics PRIVATE Vulkan::Vulkan)
target_include_directories(Graphics PRIVATE ${Stb_INCLUDE_DIR})
