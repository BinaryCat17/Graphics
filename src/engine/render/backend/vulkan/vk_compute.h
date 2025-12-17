#ifndef VK_COMPUTE_H
#define VK_COMPUTE_H

#include "vk_types.h"

// Compiles GLSL source string to SPIR-V using system 'glslc'.
// Returns a malloc-ed buffer that must be freed by caller.
// 'out_size' receives the size in bytes.
uint32_t* vk_compile_glsl_runtime(const char* glsl_source, size_t* out_size);

// Creates a compute pipeline from SPIR-V bytecode.
// Returns VK_NULL_HANDLE on failure.
VkPipeline vk_create_compute_pipeline(VulkanRendererState* state, const uint32_t* spv_code, size_t spv_size, VkPipelineLayout layout);

// One-shot compute dispatch helper.
// 1. Creates a buffer for output.
// 2. Creates a descriptor set binding that buffer.
// 3. Dispatches the pipeline.
// 4. Reads back result.
// NOTE: This is a synchronous, slow, blocking function for testing/prototyping.
float vk_run_compute_graph_oneshot(VulkanRendererState* state, const char* glsl_source);

#endif // VK_COMPUTE_H
