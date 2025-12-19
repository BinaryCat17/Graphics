#include "vk_swapchain.h"
#include "vk_utils.h"
#include "foundation/logger/logger.h"
#include <stdlib.h>

typedef struct {
    VkBool32 color_attachment;
    VkBool32 blend;
} FormatSupport;

static FormatSupport get_format_support(VkPhysicalDevice physical, VkFormat fmt) {
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(physical, fmt, &props);
    FormatSupport support = {
        .color_attachment = (props.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) != 0,
        .blend = (props.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT) != 0
    };
    return support;
}

static VkFormat choose_depth_format(VkPhysicalDevice physical)
{
    VkFormat candidates[] = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D16_UNORM,
    };

    for (size_t i = 0; i < sizeof(candidates)/sizeof(candidates[0]); ++i) {
        VkFormat format = candidates[i];
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physical, format, &props);
        if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            return format;
        }
    }

    return VK_FORMAT_UNDEFINED;
}

void vk_create_swapchain_and_views(VulkanRendererState* state, VkSwapchainKHR old_swapchain) {
    /* choose format */
    uint32_t fc = 0; 
    vkGetPhysicalDeviceSurfaceFormatsKHR(state->physical_device, state->surface, &fc, NULL); 
    if (fc == 0) LOG_FATAL("no surface formats");
    
    VkSurfaceFormatKHR* fmts = malloc(sizeof(VkSurfaceFormatKHR) * fc); 
    vkGetPhysicalDeviceSurfaceFormatsKHR(state->physical_device, state->surface, &fc, fmts);
    
    VkSurfaceFormatKHR chosen_fmt = { 0 };
    FormatSupport chosen_support = { 0 };
    VkSurfaceFormatKHR srgb_choice = { 0 };
    FormatSupport srgb_support = { 0 };
    VkBool32 have_srgb = VK_FALSE;
    VkBool32 have_srgb_blend = VK_FALSE;
    VkSurfaceFormatKHR blend_choice = { 0 };
    FormatSupport blend_support = { 0 };
    VkBool32 have_blend_only = VK_FALSE;

    for (uint32_t i = 0; i < fc; i++) {
        FormatSupport support = get_format_support(state->physical_device, fmts[i].format);
        if (!support.color_attachment) continue;

        if (chosen_support.color_attachment == VK_FALSE) {
            chosen_fmt = fmts[i];
            chosen_support = support;
        }

        if (support.blend && !have_blend_only) {
            blend_choice = fmts[i];
            blend_support = support;
            have_blend_only = VK_TRUE;
        }

        if (fmts[i].format == VK_FORMAT_B8G8R8A8_UNORM) {
            srgb_choice = fmts[i];
            srgb_support = support;
            have_srgb = VK_TRUE;
            if (support.blend) {
                have_srgb_blend = VK_TRUE;
                chosen_fmt = srgb_choice;
                chosen_support = srgb_support;
                break;
            }
        }
    }

    if (have_srgb_blend) {
        chosen_fmt = srgb_choice;
        chosen_support = srgb_support;
    }
    else if (!chosen_support.color_attachment && have_srgb) {
        chosen_fmt = srgb_choice;
        chosen_support = srgb_support;
    }
    else if (!chosen_support.color_attachment && have_blend_only) {
        chosen_fmt = blend_choice;
        chosen_support = blend_support;
    }

    if (!chosen_support.color_attachment) LOG_FATAL("no color attachment format for swapchain");

    if (chosen_fmt.format == VK_FORMAT_UNDEFINED) {
        chosen_fmt.format = VK_FORMAT_B8G8R8A8_UNORM;
    }
    
    // Check support again for the chosen (possibly defaulted) format
    chosen_support = get_format_support(state->physical_device, chosen_fmt.format);
    if (!chosen_support.color_attachment) LOG_FATAL("swapchain format lacks color attachment support");
    
    state->swapchain_supports_blend = chosen_support.blend;
    state->swapchain_format = chosen_fmt.format;
    free(fmts);

    PlatformWindowSize fb_size = platform_get_framebuffer_size(state->window);
    int w = fb_size.width;
    int h = fb_size.height;
    while (w == 0 || h == 0) {
        platform_wait_events();
        if (platform_window_should_close(state->window)) return;
        fb_size = platform_get_framebuffer_size(state->window);
        w = fb_size.width;
        h = fb_size.height;
    }

    VkSurfaceCapabilitiesKHR caps; 
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(state->physical_device, state->surface, &caps);

    uint32_t img_count = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && img_count > caps.maxImageCount) img_count = caps.maxImageCount;

    if (caps.currentExtent.width != UINT32_MAX) state->swapchain_extent = caps.currentExtent;
    else {
        uint32_t clamped_w = (uint32_t)w; uint32_t clamped_h = (uint32_t)h;
        if (clamped_w < caps.minImageExtent.width) clamped_w = caps.minImageExtent.width;
        if (clamped_w > caps.maxImageExtent.width) clamped_w = caps.maxImageExtent.width;
        if (clamped_h < caps.minImageExtent.height) clamped_h = caps.minImageExtent.height;
        if (clamped_h > caps.maxImageExtent.height) clamped_h = caps.maxImageExtent.height;
        state->swapchain_extent.width = clamped_w; 
        state->swapchain_extent.height = clamped_h;
    }

    state->transformer.viewport_size = (Vec2){(float)state->swapchain_extent.width, (float)state->swapchain_extent.height};

    VkCompositeAlphaFlagBitsKHR comp_alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    VkCompositeAlphaFlagBitsKHR pref_alphas[] = {
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR
    };
    for (size_t i = 0; i < sizeof(pref_alphas) / sizeof(pref_alphas[0]); i++) {
        if (caps.supportedCompositeAlpha & pref_alphas[i]) { comp_alpha = pref_alphas[i]; break; }
    }

    VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (!(caps.supportedUsageFlags & usage)) LOG_FATAL("swapchain color usage unsupported");

    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
    LOG_INFO("Selected Present Mode: %d (FIFO=%d, MAILBOX=%d, IMMEDIATE=%d)", 
           present_mode, VK_PRESENT_MODE_FIFO_KHR, VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR);

    VkSwapchainCreateInfoKHR sci = { 
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR, 
        .surface = state->surface, 
        .minImageCount = img_count, 
        .imageFormat = state->swapchain_format, 
        .imageColorSpace = chosen_fmt.colorSpace, 
        .imageExtent = state->swapchain_extent, 
        .imageArrayLayers = 1, 
        .imageUsage = usage, 
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE, 
        .preTransform = caps.currentTransform, 
        .compositeAlpha = comp_alpha, 
        .presentMode = present_mode, 
        .clipped = VK_TRUE, 
        .oldSwapchain = old_swapchain 
    };
    
    state->res = vkCreateSwapchainKHR(state->device, &sci, NULL, &state->swapchain);
    LOG_INFO("vkCreateSwapchainKHR: Swapchain created.");
    
    if (state->res != VK_SUCCESS) fatal_vk("vkCreateSwapchainKHR", state->res);
    
    vkGetSwapchainImagesKHR(state->device, state->swapchain, &state->swapchain_img_count, NULL);
    state->swapchain_imgs = malloc(sizeof(VkImage) * state->swapchain_img_count);
    vkGetSwapchainImagesKHR(state->device, state->swapchain, &state->swapchain_img_count, state->swapchain_imgs);
    
    state->swapchain_imgviews = malloc(sizeof(VkImageView) * state->swapchain_img_count);
    for (uint32_t i = 0; i < state->swapchain_img_count; i++) {
        VkImageViewCreateInfo ivci = { 
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, 
            .image = state->swapchain_imgs[i], 
            .viewType = VK_IMAGE_VIEW_TYPE_2D, 
            .format = state->swapchain_format, 
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0,1,0,1 } 
        };
        state->res = vkCreateImageView(state->device, &ivci, NULL, &state->swapchain_imgviews[i]);
        if (state->res != VK_SUCCESS) fatal_vk("vkCreateImageView", state->res);
    }
}

void vk_destroy_depth_resources(VulkanRendererState* state) {
    if (state->depth_image_view) {
        vkDestroyImageView(state->device, state->depth_image_view, NULL);
        state->depth_image_view = VK_NULL_HANDLE;
    }
    if (state->depth_image) {
        vkDestroyImage(state->device, state->depth_image, NULL);
        state->depth_image = VK_NULL_HANDLE;
    }
    if (state->depth_memory) {
        vkFreeMemory(state->device, state->depth_memory, NULL);
        state->depth_memory = VK_NULL_HANDLE;
    }
}

void vk_create_depth_resources(VulkanRendererState* state) {
    vk_destroy_depth_resources(state);

    state->depth_format = choose_depth_format(state->physical_device);
    if (state->depth_format == VK_FORMAT_UNDEFINED) {
        LOG_FATAL("No supported depth format found");
    }

    VkImageCreateInfo image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = state->depth_format,
        .extent = { state->swapchain_extent.width, state->swapchain_extent.height, 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    state->res = vkCreateImage(state->device, &image_info, NULL, &state->depth_image);
    if (state->res != VK_SUCCESS) fatal_vk("vkCreateImage (depth)", state->res);

    VkMemoryRequirements mem_req;
    vkGetImageMemoryRequirements(state->device, state->depth_image, &mem_req);
    VkMemoryAllocateInfo alloc_info = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = mem_req.size };
    VkPhysicalDeviceMemoryProperties mem_props; vkGetPhysicalDeviceMemoryProperties(state->physical_device, &mem_props);
    uint32_t mem_type = UINT32_MAX;
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
        if ((mem_req.memoryTypeBits & (1u << i)) && (mem_props.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            mem_type = i; break; }
    }
    if (mem_type == UINT32_MAX) LOG_FATAL("No suitable memory type for depth buffer");
    alloc_info.memoryTypeIndex = mem_type;
    state->res = vkAllocateMemory(state->device, &alloc_info, NULL, &state->depth_memory);
    if (state->res != VK_SUCCESS) fatal_vk("vkAllocateMemory (depth)", state->res);
    vkBindImageMemory(state->device, state->depth_image, state->depth_memory, 0);

    VkImageViewCreateInfo view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = state->depth_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = state->depth_format,
        .subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 }
    };

    state->res = vkCreateImageView(state->device, &view_info, NULL, &state->depth_image_view);
    if (state->res != VK_SUCCESS) fatal_vk("vkCreateImageView (depth)", state->res);
}

void vk_create_render_pass(VulkanRendererState* state) {
    if (state->depth_format == VK_FORMAT_UNDEFINED) {
        state->depth_format = choose_depth_format(state->physical_device);
        if (state->depth_format == VK_FORMAT_UNDEFINED) LOG_FATAL("No supported depth format found");
    }

    VkAttachmentDescription attachments[2] = {
        { .format = state->swapchain_format, .samples = VK_SAMPLE_COUNT_1_BIT, .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, .storeOp = VK_ATTACHMENT_STORE_OP_STORE, .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE, .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE, .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED, .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR },
        { .format = state->depth_format, .samples = VK_SAMPLE_COUNT_1_BIT, .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE, .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE, .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE, .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED, .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL }
    };
    VkAttachmentReference color_ref = { .attachment = 0, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkAttachmentReference depth_ref = { .attachment = 1, .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
    VkSubpassDescription sub = { .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS, .colorAttachmentCount = 1, .pColorAttachments = &color_ref, .pDepthStencilAttachment = &depth_ref };
    VkRenderPassCreateInfo rpci = { .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, .attachmentCount = 2, .pAttachments = attachments, .subpassCount = 1, .pSubpasses = &sub };
    state->res = vkCreateRenderPass(state->device, &rpci, NULL, &state->render_pass);
    if (state->res != VK_SUCCESS) fatal_vk("vkCreateRenderPass", state->res);
}

void vk_create_cmds_and_sync(VulkanRendererState* state) {
    if (state->sem_img_avail) { vkDestroySemaphore(state->device, state->sem_img_avail, NULL); state->sem_img_avail = VK_NULL_HANDLE; }
    if (state->sem_render_done) { vkDestroySemaphore(state->device, state->sem_render_done, NULL); state->sem_render_done = VK_NULL_HANDLE; }
    
    VkCommandPoolCreateInfo cpci = { .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .queueFamilyIndex = state->graphics_family, .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT };
    state->res = vkCreateCommandPool(state->device, &cpci, NULL, &state->cmdpool);
    if (state->res != VK_SUCCESS) fatal_vk("vkCreateCommandPool", state->res);
    
    state->cmdbuffers = malloc(sizeof(VkCommandBuffer) * state->swapchain_img_count);
    VkCommandBufferAllocateInfo cbai = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = state->cmdpool, .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = state->swapchain_img_count };
    state->res = vkAllocateCommandBuffers(state->device, &cbai, state->cmdbuffers);
    if (state->res != VK_SUCCESS) fatal_vk("vkAllocateCommandBuffers", state->res);

    state->framebuffers = malloc(sizeof(VkFramebuffer) * state->swapchain_img_count);
    for (uint32_t i = 0; i < state->swapchain_img_count; i++) {
        VkImageView attachments[2] = { state->swapchain_imgviews[i], state->depth_image_view };
        VkFramebufferCreateInfo fci = { .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, .renderPass = state->render_pass, .attachmentCount = 2, .pAttachments = attachments, .width = state->swapchain_extent.width, .height = state->swapchain_extent.height, .layers = 1 };
        state->res = vkCreateFramebuffer(state->device, &fci, NULL, &state->framebuffers[i]);
        if (state->res != VK_SUCCESS) fatal_vk("vkCreateFramebuffer", state->res);
    }
    VkSemaphoreCreateInfo sci = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO }; 
    vkCreateSemaphore(state->device, &sci, NULL, &state->sem_img_avail); 
    vkCreateSemaphore(state->device, &sci, NULL, &state->sem_render_done);
    
    state->fences = malloc(sizeof(VkFence) * state->swapchain_img_count);
    for (uint32_t i = 0; i < state->swapchain_img_count; i++) {
        VkFenceCreateInfo fci = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT };
        vkCreateFence(state->device, &fci, NULL, &state->fences[i]);
    }

    free(state->image_frame_owner);
    state->image_frame_owner = calloc(state->swapchain_img_count, sizeof(int));
    if (state->image_frame_owner) {
        for (uint32_t i = 0; i < state->swapchain_img_count; ++i) {
            state->image_frame_owner[i] = -1;
        }
    }
    state->current_frame_cursor = 0;
}

void vk_cleanup_swapchain(VulkanRendererState* state, bool keep_swapchain_handle) {
    if (state->cmdbuffers) {
        vkFreeCommandBuffers(state->device, state->cmdpool, state->swapchain_img_count, state->cmdbuffers);
        free(state->cmdbuffers);
        state->cmdbuffers = NULL;
    }
    if (state->cmdpool) {
        vkDestroyCommandPool(state->device, state->cmdpool, NULL);
        state->cmdpool = VK_NULL_HANDLE;
    }
    if (state->framebuffers) {
        for (uint32_t i = 0; i < state->swapchain_img_count; i++) vkDestroyFramebuffer(state->device, state->framebuffers[i], NULL);
        free(state->framebuffers);
        state->framebuffers = NULL;
    }
    if (state->fences) {
        for (uint32_t i = 0; i < state->swapchain_img_count; i++) vkDestroyFence(state->device, state->fences[i], NULL);
        free(state->fences);
        state->fences = NULL;
    }
    for (size_t i = 0; i < 2; ++i) {
        state->frame_resources[i].stage = FRAME_AVAILABLE;
        state->frame_resources[i].inflight_fence = VK_NULL_HANDLE;
    }
    free(state->image_frame_owner);
    state->image_frame_owner = NULL;
    if (state->swapchain_imgviews) {
        for (uint32_t i = 0; i < state->swapchain_img_count; i++) vkDestroyImageView(state->device, state->swapchain_imgviews[i], NULL);
        free(state->swapchain_imgviews);
        state->swapchain_imgviews = NULL;
    }
    if (state->swapchain_imgs) {
        free(state->swapchain_imgs);
        state->swapchain_imgs = NULL;
    }
    vk_destroy_depth_resources(state);
    
    if (!keep_swapchain_handle && state->swapchain) {
        vkDestroySwapchainKHR(state->device, state->swapchain, NULL);
        LOG_INFO("vkDestroySwapchainKHR: Swapchain destroyed.");
        state->swapchain = VK_NULL_HANDLE;
    }
    if (state->pipeline) {
        vkDestroyPipeline(state->device, state->pipeline, NULL);
        state->pipeline = VK_NULL_HANDLE;
    }
    if (state->pipeline_layout) {
        vkDestroyPipelineLayout(state->device, state->pipeline_layout, NULL);
        state->pipeline_layout = VK_NULL_HANDLE;
    }
    if (state->render_pass) {
        vkDestroyRenderPass(state->device, state->render_pass, NULL);
        state->render_pass = VK_NULL_HANDLE;
    }
    state->swapchain_img_count = 0;
}
