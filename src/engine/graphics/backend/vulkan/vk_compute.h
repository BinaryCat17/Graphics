#ifndef VK_COMPUTE_H
#define VK_COMPUTE_H

#include "vk_types.h"
#include <vulkan/vulkan.h>

// Compiles GLSL to SPIR-V at runtime using 'glslc' (must be in PATH)
// Returns malloc'd buffer (caller frees)
uint32_t* vk_compile_glsl_runtime(const char* glsl_source, size_t* out_size);

// Creates a Compute Pipeline from SPIR-V code
VkPipeline vk_create_compute_pipeline(VulkanRendererState* state, const uint32_t* spv_code, size_t spv_size, VkPipelineLayout layout);

// Runs a compute shader on a 1D float buffer (Buffer Mode)
// Returns the result of the last calculation (heuristic)
float vk_run_compute_graph_oneshot(VulkanRendererState* state, const char* glsl_source);

// Runs a compute shader on a 2D image (Image Mode)
// Output is stored in state->compute_target_image
void vk_run_compute_graph_image(VulkanRendererState* state, const char* glsl_source, int width, int height);

#endif // VK_COMPUTE_H