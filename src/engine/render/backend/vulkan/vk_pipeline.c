#include "vk_pipeline.h"
#include "vk_utils.h"
#include <stdlib.h>
#include <stddef.h>

static VkShaderModule create_shader_module_from_spv(VulkanRendererState* state, const char* path) {
    size_t code_size_bytes = 0; 
    uint32_t* code = read_file_bin_u32(path, &code_size_bytes);
    if (!code) fatal("read spv");
    
    VkShaderModuleCreateInfo smci = { .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize = code_size_bytes, .pCode = code };
    VkShaderModule mod; 
    state->res = vkCreateShaderModule(state->device, &smci, NULL, &mod);
    if (state->res != VK_SUCCESS) fatal_vk("vkCreateShaderModule", state->res);
    free(code); 
    return mod;
}

void vk_create_descriptor_layout(VulkanRendererState* state) {
    VkDescriptorSetLayoutBinding binding = { 
        .binding = 0, 
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 
        .descriptorCount = 1, 
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT 
    };
    VkDescriptorSetLayoutCreateInfo lci = { 
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, 
        .bindingCount = 1, 
        .pBindings = &binding 
    };
    state->res = vkCreateDescriptorSetLayout(state->device, &lci, NULL, &state->descriptor_layout);
    if (state->res != VK_SUCCESS) fatal_vk("vkCreateDescriptorSetLayout", state->res);
}

void vk_create_pipeline(VulkanRendererState* state, const char* vert_spv, const char* frag_spv) {
    VkShaderModule vs = create_shader_module_from_spv(state, vert_spv);
    VkShaderModule fs = create_shader_module_from_spv(state, frag_spv);
    
    VkPipelineShaderStageCreateInfo stages[2] = {
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vs, .pName = "main" },
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = fs, .pName = "main" }
    };
    
    /* vertex input binding - Unified: vec3 position + vec2 uv */
    // Stride = 20 bytes (5 floats)
    VkVertexInputBindingDescription bind = { .binding = 0, .stride = 5 * sizeof(float), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX };
    
    // Attribute 0: Position (vec3)
    // Attribute 1: UV (vec2)
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
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
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
    
    // Unified Push Constants:
    // mat4 model (64) + mat4 view_proj (64) + vec4 color (16) + vec4 uv_rect (16) + vec4 params (16) = 176 bytes
    VkPushConstantRange pcr = { .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, .offset = 0, .size = 176 };
    
    VkPipelineLayoutCreateInfo plci = { .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .setLayoutCount = 1, .pSetLayouts = &state->descriptor_layout, .pushConstantRangeCount = 1, .pPushConstantRanges = &pcr };
    
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
