#include "vk_compute.h"
#include "foundation/logger/logger.h"
#include "foundation/platform/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- Runtime Compilation ---

uint32_t* vk_compile_glsl_runtime(const char* glsl_source, size_t* out_size) {
    if (!glsl_source) return NULL;
    
    // 1. Write Source to Temp File
    // Using platform temp dir would be better, but local is easier for now.
    const char* src_filename = "temp_graph.comp";
    const char* spv_filename = "temp_graph.spv";
    
    FILE* f = fopen(src_filename, "w");
    if (!f) {
        LOG_ERROR("Failed to write temp shader file");
        return NULL;
    }
    fprintf(f, "%s", glsl_source);
    fclose(f);
    
    // 2. Invoke glslc
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "glslc %s -o %s", src_filename, spv_filename);
    int ret = system(cmd);
    
    if (ret != 0) {
        LOG_ERROR("Shader compilation failed (glslc returned %d)", ret);
        return NULL;
    }
    
    // 3. Read SPV
    FILE* bf = fopen(spv_filename, "rb");
    if (!bf) {
        LOG_ERROR("Failed to read compiled SPV");
        return NULL;
    }
    
    fseek(bf, 0, SEEK_END);
    long sz = ftell(bf);
    fseek(bf, 0, SEEK_SET);
    
    if (sz <= 0) {
        fclose(bf);
        return NULL;
    }
    
    uint32_t* buffer = malloc(sz);
    fread(buffer, 1, sz, bf);
    fclose(bf);
    
    if (out_size) *out_size = sz;
    
    // 4. Cleanup
    remove(src_filename);
    remove(spv_filename);
    
    return buffer;
}

// --- Pipeline Creation ---

VkPipeline vk_create_compute_pipeline(VulkanRendererState* state, const uint32_t* spv_code, size_t spv_size, VkPipelineLayout layout) {
    VkShaderModuleCreateInfo ci = { 
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = spv_size,
        .pCode = spv_code
    };
    
    VkShaderModule mod;
    if (vkCreateShaderModule(state->device, &ci, NULL, &mod) != VK_SUCCESS) {
        LOG_ERROR("Failed to create compute shader module");
        return VK_NULL_HANDLE;
    }
    
    VkPipelineShaderStageCreateInfo stage = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = mod,
        .pName = "main"
    };
    
    VkComputePipelineCreateInfo cpci = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = stage,
        .layout = layout
    };
    
    VkPipeline pipeline;
    if (vkCreateComputePipelines(state->device, VK_NULL_HANDLE, 1, &cpci, NULL, &pipeline) != VK_SUCCESS) {
        LOG_ERROR("Failed to create compute pipeline");
        vkDestroyShaderModule(state->device, mod, NULL);
        return VK_NULL_HANDLE;
    }
    
    vkDestroyShaderModule(state->device, mod, NULL);
    return pipeline;
}

// --- One-Shot Execution ---

float vk_run_compute_graph_oneshot(VulkanRendererState* state, const char* glsl_source) {
    if (!state || !glsl_source) return 0.0f;
    
    // 1. Compile
    size_t spv_size = 0;
    uint32_t* spv_code = vk_compile_glsl_runtime(glsl_source, &spv_size);
    if (!spv_code) return 0.0f;
    
    // 2. Create Layout (Set 0 Binding 0: Storage Buffer)
    VkDescriptorSetLayoutBinding binding = { 
        .binding = 0, 
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 
        .descriptorCount = 1, 
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT 
    };
    VkDescriptorSetLayoutCreateInfo lci = { 
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, 
        .bindingCount = 1, 
        .pBindings = &binding 
    };
    
    VkDescriptorSetLayout ds_layout;
    vkCreateDescriptorSetLayout(state->device, &lci, NULL, &ds_layout);
    
    VkPipelineLayoutCreateInfo plci = { 
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, 
        .setLayoutCount = 1, 
        .pSetLayouts = &ds_layout 
    };
    VkPipelineLayout pipe_layout;
    vkCreatePipelineLayout(state->device, &plci, NULL, &pipe_layout);
    
    // 3. Create Pipeline
    VkPipeline pipeline = vk_create_compute_pipeline(state, spv_code, spv_size, pipe_layout);
    free(spv_code);
    
    if (pipeline == VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(state->device, pipe_layout, NULL);
        vkDestroyDescriptorSetLayout(state->device, ds_layout, NULL);
        return 0.0f;
    }
    
    // 4. Create Output Buffer (4 bytes float)
    VkBuffer buffer;
    VkDeviceMemory memory;
    
    // Using simple buffer creation (assuming host visible for readback)
    VkBufferCreateInfo bci = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(float),
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    vkCreateBuffer(state->device, &bci, NULL, &buffer);
    
    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(state->device, buffer, &mem_reqs);
    
    VkMemoryAllocateInfo alloc = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_reqs.size,
        .memoryTypeIndex = 0 // Needs valid host visible type
    };
    
    // Find memory type
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(state->physical_device, &mem_props);
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
        if ((mem_reqs.memoryTypeBits & (1 << i)) && 
            (mem_props.memoryTypes[i].propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))) {
            alloc.memoryTypeIndex = i;
            break;
        }
    }
    
    vkAllocateMemory(state->device, &alloc, NULL, &memory);
    vkBindBufferMemory(state->device, buffer, memory, 0);
    
    // 5. Descriptor Set
    VkDescriptorPoolSize pool_size = { .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1 };
    VkDescriptorPoolCreateInfo pool_ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1,
        .poolSizeCount = 1,
        .pPoolSizes = &pool_size
    };
    VkDescriptorPool pool;
    vkCreateDescriptorPool(state->device, &pool_ci, NULL, &pool);
    
    VkDescriptorSetAllocateInfo alloc_set = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &ds_layout
    };
    VkDescriptorSet set;
    vkAllocateDescriptorSets(state->device, &alloc_set, &set);
    
    VkDescriptorBufferInfo dbi = { .buffer = buffer, .offset = 0, .range = VK_WHOLE_SIZE };
    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = set,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pBufferInfo = &dbi
    };
    vkUpdateDescriptorSets(state->device, 1, &write, 0, NULL);
    
    // 6. Record & Submit
    VkCommandBufferAllocateInfo cbai = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = state->cmdpool, // Using graphics pool is fine for compute usually
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(state->device, &cbai, &cmd);
    
    VkCommandBufferBeginInfo begin = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
    vkBeginCommandBuffer(cmd, &begin);
    
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipe_layout, 0, 1, &set, 0, NULL);
    vkCmdDispatch(cmd, 1, 1, 1);
    
    vkEndCommandBuffer(cmd);
    
    VkSubmitInfo submit = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd
    };
    
    // We need a fence
    VkFence fence;
    VkFenceCreateInfo fence_ci = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    vkCreateFence(state->device, &fence_ci, NULL, &fence);
    
    vkQueueSubmit(state->queue, 1, &submit, fence);
    vkWaitForFences(state->device, 1, &fence, VK_TRUE, 1000000000); // 1 sec timeout
    
    // 7. Read Result
    float result = 0.0f;
    void* mapped;
    vkMapMemory(state->device, memory, 0, sizeof(float), 0, &mapped);
    memcpy(&result, mapped, sizeof(float));
    vkUnmapMemory(state->device, memory);
    
    // 8. Cleanup (Ouch, lots of manual cleanup)
    vkDestroyFence(state->device, fence, NULL);
    vkFreeCommandBuffers(state->device, state->cmdpool, 1, &cmd);
    vkDestroyDescriptorPool(state->device, pool, NULL);
    vkDestroyBuffer(state->device, buffer, NULL);
    vkFreeMemory(state->device, memory, NULL);
    vkDestroyPipeline(state->device, pipeline, NULL);
    vkDestroyPipelineLayout(state->device, pipe_layout, NULL);
    vkDestroyDescriptorSetLayout(state->device, ds_layout, NULL);
    
    return result;
}
