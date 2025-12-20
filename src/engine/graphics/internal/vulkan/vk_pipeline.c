#include "vk_pipeline.h"
#include "vk_utils.h"
#include "foundation/logger/logger.h"
#include <stdlib.h>
#include <stddef.h>

static VkShaderModule create_shader_module_from_spv(VulkanRendererState* state, const char* path) {
    size_t size;
    uint32_t* code = read_file_bin_u32(path, &size);
    if (!code) {
        LOG_FATAL("Failed to read SPV file: %s", path);
        return VK_NULL_HANDLE;
    }
    
    VkShaderModuleCreateInfo ci = { 
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = size,
        .pCode = code
    };
    
    VkShaderModule mod; 
    state->res = vkCreateShaderModule(state->device, &ci, NULL, &mod);
    if (state->res != VK_SUCCESS) {
        LOG_FATAL("vkCreateShaderModule failed: %d", state->res);
    }
    
    free(code); 
    return mod;
}

void vk_create_descriptor_layout(VulkanRendererState* state) {
    // Set 0: Texture Sampler
    VkDescriptorSetLayoutBinding binding0 = { 
        .binding = 0, 
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 
        .descriptorCount = 1, 
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT 
    };
    VkDescriptorSetLayoutCreateInfo lci0 = { 
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, 
        .bindingCount = 1, 
        .pBindings = &binding0 
    };
    state->res = vkCreateDescriptorSetLayout(state->device, &lci0, NULL, &state->descriptor_layout);
    if (state->res != VK_SUCCESS) fatal_vk("vkCreateDescriptorSetLayout (Set 0)", state->res);
    
    // Set 1: Instance Buffer (SSBO)
    VkDescriptorSetLayoutBinding binding1 = { 
        .binding = 0, 
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 
        .descriptorCount = 1, 
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
    };
    VkDescriptorSetLayoutCreateInfo lci1 = { 
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, 
        .bindingCount = 1, 
        .pBindings = &binding1 
    };
    state->res = vkCreateDescriptorSetLayout(state->device, &lci1, NULL, &state->instance_layout);
    if (state->res != VK_SUCCESS) fatal_vk("vkCreateDescriptorSetLayout (Set 1)", state->res);

    // Compute Layout: Set 0 = Storage Image (Write)
    VkDescriptorSetLayoutBinding bindingC = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
    };
    VkDescriptorSetLayoutCreateInfo lciC = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &bindingC
    };
    state->res = vkCreateDescriptorSetLayout(state->device, &lciC, NULL, &state->compute_write_layout);
    if (state->res != VK_SUCCESS) fatal_vk("vkCreateDescriptorSetLayout (Compute)", state->res);
}

VkResult vk_create_compute_pipeline_shader(VulkanRendererState* state, const uint32_t* code, size_t size, int layout_idx, VkPipeline* out_pipeline, VkPipelineLayout* out_layout) {
    (void)layout_idx; // Unused for now
    // 1. Create Shader Module
    VkShaderModuleCreateInfo ci = { 
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = size,
        .pCode = code
    };
    VkShaderModule mod;
    VkResult res = vkCreateShaderModule(state->device, &ci, NULL, &mod);
    if (res != VK_SUCCESS) return res;

    // 2. Create Layout
    // For now, layout_idx is ignored and we always use the default:
    // Set 0: Compute Write (Storage Image)
    // Push Constants: 128 bytes
    
    VkPushConstantRange pcr = { 
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT, 
        .offset = 0, 
        .size = 128 
    };
    
    VkPipelineLayoutCreateInfo plci = { 
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, 
        .setLayoutCount = 1, 
        .pSetLayouts = &state->compute_write_layout, 
        .pushConstantRangeCount = 1, 
        .pPushConstantRanges = &pcr 
    };
    
    res = vkCreatePipelineLayout(state->device, &plci, NULL, out_layout);
    if (res != VK_SUCCESS) {
        vkDestroyShaderModule(state->device, mod, NULL);
        return res;
    }

    // 3. Create Pipeline
    VkPipelineShaderStageCreateInfo stage = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = mod,
        .pName = "main"
    };

    VkComputePipelineCreateInfo cpci = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = stage,
        .layout = *out_layout
    };
    
    res = vkCreateComputePipelines(state->device, VK_NULL_HANDLE, 1, &cpci, NULL, out_pipeline);
    
    // Module can be destroyed after pipeline creation
    vkDestroyShaderModule(state->device, mod, NULL);
    
    if (res != VK_SUCCESS) {
        vkDestroyPipelineLayout(state->device, *out_layout, NULL);
    }
    
    return res;
}

void vk_create_pipeline(VulkanRendererState* state, const char* vert_spv, const char* frag_spv) {
    VkShaderModule vs = create_shader_module_from_spv(state, vert_spv);
    VkShaderModule fs = create_shader_module_from_spv(state, frag_spv);
    
    // ... (stages, vertex input) ...
    VkPipelineShaderStageCreateInfo stages[2] = {
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vs, .pName = "main" },
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = fs, .pName = "main" }
    };
    
    // Stride = 20 bytes (5 floats)
    VkVertexInputBindingDescription bind = { .binding = 0, .stride = 5 * sizeof(float), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX };
    
    VkVertexInputAttributeDescription attr[2] = {
        {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = 0 },
        {.location = 1, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = 3 * sizeof(float) }
    };
    
    VkPipelineVertexInputStateCreateInfo vxi = { 
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, 
        .vertexBindingDescriptionCount = 1, 
        .pVertexBindingDescriptions = &bind, 
        .vertexAttributeDescriptionCount = 2, 
        .pVertexAttributeDescriptions = attr 
    };

    VkPipelineInputAssemblyStateCreateInfo ia = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST };
    
    float viewport_w = (state->swapchain_extent.width == 0) ? 1.0f : (float)state->swapchain_extent.width;
    float viewport_h = (state->swapchain_extent.height == 0) ? 1.0f : (float)state->swapchain_extent.height;
    
    VkViewport vp = { .x = 0, .y = 0, .width = viewport_w, .height = viewport_h, .minDepth = 0.0f, .maxDepth = 1.0f };
    VkRect2D sc = { .offset = {0,0}, .extent = state->swapchain_extent };
    VkPipelineViewportStateCreateInfo vpci = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .viewportCount = 1, .pViewports = &vp, .scissorCount = 1, .pScissors = &sc };
    
    VkPipelineRasterizationStateCreateInfo rs = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, .polygonMode = VK_POLYGON_MODE_FILL, .cullMode = VK_CULL_MODE_NONE, .frontFace = VK_FRONT_FACE_CLOCKWISE, .lineWidth = 1.0f };
    VkPipelineMultisampleStateCreateInfo ms = { .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT };
    
    VkPipelineDepthStencilStateCreateInfo ds = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_FALSE,
        .depthWriteEnable = VK_FALSE,
        .depthCompareOp = VK_COMPARE_OP_ALWAYS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE
    };
    
    VkPipelineColorBlendAttachmentState cbatt = { 
        .blendEnable = state->swapchain_supports_blend, 
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA, 
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, 
        .colorBlendOp = VK_BLEND_OP_ADD, 
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE, 
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, 
        .alphaBlendOp = VK_BLEND_OP_ADD, 
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT 
    };
    VkPipelineColorBlendStateCreateInfo cb = { .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, .attachmentCount = 1, .pAttachments = &cbatt };
    
    // Unified Push Constants: view_proj only (64 bytes)
    VkPushConstantRange pcr = { .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, .offset = 0, .size = 64 };
    
    // Layouts: Set 0 (Tex), Set 1 (Inst), Set 2 (User Tex)
    VkDescriptorSetLayout layouts[] = { state->descriptor_layout, state->instance_layout, state->descriptor_layout };
    
    VkPipelineLayoutCreateInfo plci = { .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .setLayoutCount = 3, .pSetLayouts = layouts, .pushConstantRangeCount = 1, .pPushConstantRanges = &pcr };
    
    state->res = vkCreatePipelineLayout(state->device, &plci, NULL, &state->pipeline_layout);
    if (state->res != VK_SUCCESS) fatal_vk("vkCreatePipelineLayout", state->res);
    
    VkGraphicsPipelineCreateInfo gpci = { 
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, 
        .stageCount = 2, 
        .pStages = stages, 
        .pVertexInputState = &vxi, 
        .pInputAssemblyState = &ia, 
        .pViewportState = &vpci, 
        .pRasterizationState = &rs, 
        .pMultisampleState = &ms, 
        .pDepthStencilState = &ds, 
        .pColorBlendState = &cb, 
        .layout = state->pipeline_layout, 
        .renderPass = state->render_pass, 
        .subpass = 0 
    };
    
    state->res = vkCreateGraphicsPipelines(state->device, VK_NULL_HANDLE, 1, &gpci, NULL, &state->pipeline);
    if (state->res != VK_SUCCESS) fatal_vk("vkCreateGraphicsPipelines", state->res);
    
    vkDestroyShaderModule(state->device, vs, NULL); 
    vkDestroyShaderModule(state->device, fs, NULL);
}