#define _POSIX_C_SOURCE 200809L
#include "vulkan_renderer.h"

#include "vulkan/vulkan.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "Graphics.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

/* ---------- Vulkan helpers & global state ---------- */
/* ---------- Vulkan helpers & global state ---------- */
static GLFWwindow* g_window = NULL;
static WidgetArray g_widgets = {0};
static VkInstance instance = VK_NULL_HANDLE;
static VkPhysicalDevice physical = VK_NULL_HANDLE;
static VkDevice device = VK_NULL_HANDLE;
static uint32_t graphics_family = (uint32_t)-1;
static VkQueue queue = VK_NULL_HANDLE;
static VkSurfaceKHR surface = VK_NULL_HANDLE;
static VkSwapchainKHR swapchain = VK_NULL_HANDLE;
static const char* g_vert_spv = NULL;
static const char* g_frag_spv = NULL;
static const char* g_font_path = NULL;
static uint32_t swapchain_img_count = 0;
static VkImage* swapchain_imgs = NULL;
static VkImageView* swapchain_imgviews = NULL;
static VkFormat swapchain_format;
static VkExtent2D swapchain_extent;
static VkBool32 swapchain_supports_blend = VK_FALSE;
static VkRenderPass render_pass = VK_NULL_HANDLE;
static VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
static VkPipeline pipeline = VK_NULL_HANDLE;
static VkCommandPool cmdpool = VK_NULL_HANDLE;
static VkCommandBuffer* cmdbuffers = NULL;
static VkFramebuffer* framebuffers = NULL;
static VkResult res = VK_SUCCESS;

static VkSemaphore sem_img_avail = VK_NULL_HANDLE;
static VkSemaphore sem_render_done = VK_NULL_HANDLE;
static VkFence* fences = NULL;

typedef struct { float viewport[2]; } ViewConstants;

/* Vertex format for GUI: pos.xy, uv.xy, use_tex, color.rgba */
typedef struct { float px, py; float u, v; float use_tex; float r, g, b, a; } Vtx;
static Vtx* vtx_buf = NULL;
static size_t vtx_count = 0;
static size_t vtx_capacity = 0;

/* GPU-side buffers (vertex) simple staging omitted: we'll create host-visible vertex buffer */
static VkBuffer vertex_buffer = VK_NULL_HANDLE;
static VkDeviceMemory vertex_memory = VK_NULL_HANDLE;
static VkDeviceSize vertex_capacity = 0;

/* Texture atlas for font */
static VkImage font_image = VK_NULL_HANDLE;
static VkDeviceMemory font_image_mem = VK_NULL_HANDLE;
static VkImageView font_image_view = VK_NULL_HANDLE;
static VkSampler font_sampler = VK_NULL_HANDLE;
static VkDescriptorSetLayout descriptor_layout = VK_NULL_HANDLE;
static VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
static VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
static CoordinateTransformer g_transformer = {0};

/* Utility: read file into memory */
static char* read_file_text(const char* path, size_t * out_len) {
    FILE* f = fopen(path, "rb"); if (!f) { fprintf(stderr, "Failed open %s\n", path); return NULL; }
    fseek(f, 0, SEEK_END); long len = ftell(f); fseek(f, 0, SEEK_SET);
    char* b = malloc(len + 1); fread(b, 1, len, f); b[len] = 0; if (out_len) *out_len = (size_t)len; fclose(f); return b;
}
/* read SPIR-V binary */
static uint32_t* read_file_bin_u32(const char* path, size_t * out_words) {
    FILE* f = fopen(path, "rb"); if (!f) { fprintf(stderr, "Failed open %s\n", path); return NULL; }
    fseek(f, 0, SEEK_END); long len = ftell(f); fseek(f, 0, SEEK_SET);
    uint32_t* b = malloc(len); fread(b, 1, len, f); if (out_words) *out_words = (size_t)(len / 4); fclose(f); return b;
}

/* Minimal error helper */
static const char* vk_result_name(VkResult r) {
    switch (r) {
    case VK_SUCCESS: return "VK_SUCCESS";
    case VK_NOT_READY: return "VK_NOT_READY";
    case VK_TIMEOUT: return "VK_TIMEOUT";
    case VK_EVENT_SET: return "VK_EVENT_SET";
    case VK_EVENT_RESET: return "VK_EVENT_RESET";
    case VK_INCOMPLETE: return "VK_INCOMPLETE";
    case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
    case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
    case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
    case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
    case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
    case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
    case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
    case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
    case VK_ERROR_UNKNOWN: return "VK_ERROR_UNKNOWN";
    case VK_ERROR_OUT_OF_POOL_MEMORY: return "VK_ERROR_OUT_OF_POOL_MEMORY";
    case VK_ERROR_INVALID_EXTERNAL_HANDLE: return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
    case VK_ERROR_FRAGMENTATION: return "VK_ERROR_FRAGMENTATION";
    case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS: return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
    case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
    case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
    case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
    case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
    case VK_ERROR_VALIDATION_FAILED_EXT: return "VK_ERROR_VALIDATION_FAILED_EXT";
    case VK_ERROR_INVALID_SHADER_NV: return "VK_ERROR_INVALID_SHADER_NV";
    case VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR: return "VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR";
    case VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR";
    case VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR";
    case VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR";
    case VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR";
    case VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR";
    default: return "UNKNOWN_VK_RESULT";
    }
}

static const char* vk_result_description(VkResult r) {
    switch (r) {
    case VK_ERROR_OUT_OF_HOST_MEMORY: return "Host system ran out of memory while fulfilling the request.";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "GPU memory was insufficient for the requested allocation or object.";
    case VK_ERROR_INITIALIZATION_FAILED: return "Driver rejected initialization, often due to invalid parameters or missing prerequisites.";
    case VK_ERROR_DEVICE_LOST: return "The GPU stopped responding or was reset; usually caused by device removal or timeout.";
    case VK_ERROR_MEMORY_MAP_FAILED: return "Mapping the requested memory range failed (invalid offset/size or unsupported).";
    case VK_ERROR_LAYER_NOT_PRESENT: return "Requested validation layer is not available on this system.";
    case VK_ERROR_EXTENSION_NOT_PRESENT: return "Requested Vulkan extension is not supported by the implementation.";
    case VK_ERROR_FEATURE_NOT_PRESENT: return "A required device feature is unavailable on the selected GPU.";
    case VK_ERROR_INCOMPATIBLE_DRIVER: return "The installed driver does not support the requested Vulkan version.";
    case VK_ERROR_TOO_MANY_OBJECTS: return "Implementation-specific object limit exceeded (try freeing unused resources).";
    case VK_ERROR_FORMAT_NOT_SUPPORTED: return "Chosen image/format combination is unsupported for the requested usage.";
    case VK_ERROR_FRAGMENTED_POOL: return "Pool allocation failed because the pool became internally fragmented.";
    case VK_ERROR_OUT_OF_POOL_MEMORY: return "Descriptor or command pool cannot satisfy the allocation request.";
    case VK_ERROR_INVALID_EXTERNAL_HANDLE: return "External handle provided is not valid for this driver or platform.";
    case VK_ERROR_FRAGMENTATION: return "Allocation failed due to excessive fragmentation of the available memory.";
    case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS: return "Opaque capture address is invalid or already in use.";
    case VK_ERROR_SURFACE_LOST_KHR: return "The presentation surface became invalid (resized, moved, or destroyed).";
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "Surface creation failed because the window is already bound to another surface.";
    case VK_ERROR_OUT_OF_DATE_KHR: return "Swapchain no longer matches the surface; recreate swapchain to continue.";
    case VK_SUBOPTIMAL_KHR: return "Swapchain is still usable but no longer matches the surface optimally (consider recreating).";
    case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "Requested display configuration is incompatible with the selected display.";
    case VK_ERROR_VALIDATION_FAILED_EXT: return "Validation layers found an error; check validation output for details.";
    case VK_ERROR_INVALID_SHADER_NV: return "Shader failed to compile or link for the driver; inspect SPIR-V or compile options.";
    case VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR: return "Requested image usage flags are unsupported for this surface format.";
    case VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR: return "Video profile does not support the requested picture layout.";
    case VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR: return "Video profile does not support the requested operation.";
    case VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR: return "Video profile does not support the requested pixel format.";
    case VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR: return "Video profile does not support the requested codec.";
    case VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR: return "Requested video standard version is not supported.";
    default: return "Consult validation output or driver logs for more details.";
    }
}

static void fatal(const char* msg) {
    fprintf(stderr, "Fatal: %s\n", msg);
    exit(1);
}

static void fatal_vk(const char* msg, VkResult r) {
    const char* name = vk_result_name(r);
    const char* desc = vk_result_description(r);
    fprintf(stderr, "Fatal: %s failed with %s. %s\n", msg, name, desc);
    exit(1);
}

static void log_gpu_info(VkPhysicalDevice dev) {
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(dev, &props);

    const char* type = "Unknown";
    switch (props.deviceType) {
    case VK_PHYSICAL_DEVICE_TYPE_OTHER: type = "Other"; break;
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: type = "Integrated"; break;
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: type = "Discrete"; break;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: type = "Virtual"; break;
    case VK_PHYSICAL_DEVICE_TYPE_CPU: type = "CPU"; break;
    default: break;
    }

    printf("Using GPU: %s (%s) vendor=0x%04x device=0x%04x driver=0x%x api=%u.%u.%u\n",
           props.deviceName,
           type,
           props.vendorID,
           props.deviceID,
           props.driverVersion,
           VK_VERSION_MAJOR(props.apiVersion),
           VK_VERSION_MINOR(props.apiVersion),
           VK_VERSION_PATCH(props.apiVersion));
}

/* ---------- Vulkan setup (minimal, not exhaustive checks) ---------- */
static void create_instance(void) {
    VkApplicationInfo ai = { .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO, .pApplicationName = "vk_gui", .apiVersion = VK_API_VERSION_1_0 };
    VkInstanceCreateInfo ici = { .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &ai };
    /* request glfw extensions */
    uint32_t extc = 0; const char** exts = glfwGetRequiredInstanceExtensions(&extc);
    ici.enabledExtensionCount = extc; ici.ppEnabledExtensionNames = exts;
    VkResult res = vkCreateInstance(&ici, NULL, &instance);
    if (res != VK_SUCCESS) fatal_vk("vkCreateInstance", res);
}

static void pick_physical_and_create_device(void) {
    uint32_t pc = 0; vkEnumeratePhysicalDevices(instance, &pc, NULL); if (pc == 0) fatal("No physical dev");
    VkPhysicalDevice* list = malloc(sizeof(VkPhysicalDevice) * pc); vkEnumeratePhysicalDevices(instance, &pc, list);
    physical = list[0]; free(list);

    log_gpu_info(physical);

    /* find queue family with graphics + present */
    uint32_t qcount = 0; vkGetPhysicalDeviceQueueFamilyProperties(physical, &qcount, NULL);
    VkQueueFamilyProperties* qprops = malloc(sizeof(VkQueueFamilyProperties) * qcount); vkGetPhysicalDeviceQueueFamilyProperties(physical, &qcount, qprops);
    int found = -1;
    for (uint32_t i = 0; i < qcount; i++) {
        VkBool32 pres = false; vkGetPhysicalDeviceSurfaceSupportKHR(physical, i, surface, &pres);
        if ((qprops[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && pres) { found = (int)i; break; }
    }
    if (found < 0) fatal("No suitable queue family");
    graphics_family = (uint32_t)found;

    float prio = 1.0f;
    VkDeviceQueueCreateInfo qci = { .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueFamilyIndex = graphics_family, .queueCount = 1, .pQueuePriorities = &prio };
    const char* dev_ext[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    VkDeviceCreateInfo dci = { .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, .queueCreateInfoCount = 1, .pQueueCreateInfos = &qci, .enabledExtensionCount = 1, .ppEnabledExtensionNames = dev_ext };
    res = vkCreateDevice(physical, &dci, NULL, &device);
    if (res != VK_SUCCESS) fatal_vk("vkCreateDevice", res);
    vkGetDeviceQueue(device, graphics_family, 0, &queue);
}

typedef struct {
    VkBool32 color_attachment;
    VkBool32 blend;
} FormatSupport;

static FormatSupport get_format_support(VkFormat fmt) {
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(physical, fmt, &props);
    FormatSupport support = {
        .color_attachment = (props.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) != 0,
        .blend = (props.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT) != 0
    };
    return support;
}

/* create swapchain and imageviews */
static void create_swapchain_and_views(VkSwapchainKHR old_swapchain) {
    /* choose format */
    uint32_t fc = 0; vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &fc, NULL); if (fc == 0) fatal("no surface formats");
    VkSurfaceFormatKHR* fmts = malloc(sizeof(VkSurfaceFormatKHR) * fc); vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &fc, fmts);
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
        FormatSupport support = get_format_support(fmts[i].format);
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

        if (fmts[i].format == VK_FORMAT_B8G8R8A8_SRGB && fmts[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
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

    if (!chosen_support.color_attachment) fatal("no color attachment format for swapchain");

    /* Some platforms advertise VK_FORMAT_UNDEFINED to indicate flexibility. Choose a
       concrete format so swapchain creation succeeds on implementations that reject
       VK_FORMAT_UNDEFINED. */
    if (chosen_fmt.format == VK_FORMAT_UNDEFINED) {
        chosen_fmt.format = VK_FORMAT_B8G8R8A8_UNORM;
    }
    chosen_support = get_format_support(chosen_fmt.format);
    if (!chosen_support.color_attachment) fatal("swapchain format lacks color attachment support");
    swapchain_supports_blend = chosen_support.blend;
    swapchain_format = chosen_fmt.format;
    free(fmts);

    int w, h; glfwGetFramebufferSize(g_window, &w, &h);
    while (w == 0 || h == 0) {
        glfwWaitEvents();
        if (glfwWindowShouldClose(g_window)) return;
        glfwGetFramebufferSize(g_window, &w, &h);
    }

    VkSurfaceCapabilitiesKHR caps; vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical, surface, &caps);

    uint32_t img_count = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && img_count > caps.maxImageCount) img_count = caps.maxImageCount;

    if (caps.currentExtent.width != UINT32_MAX) swapchain_extent = caps.currentExtent;
    else {
        uint32_t clamped_w = (uint32_t)w; uint32_t clamped_h = (uint32_t)h;
        if (clamped_w < caps.minImageExtent.width) clamped_w = caps.minImageExtent.width;
        if (clamped_w > caps.maxImageExtent.width) clamped_w = caps.maxImageExtent.width;
        if (clamped_h < caps.minImageExtent.height) clamped_h = caps.minImageExtent.height;
        if (clamped_h > caps.maxImageExtent.height) clamped_h = caps.maxImageExtent.height;
        swapchain_extent.width = clamped_w; swapchain_extent.height = clamped_h;
    }

    g_transformer.viewport_size = (Vec2){(float)swapchain_extent.width, (float)swapchain_extent.height};

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
    if (!(caps.supportedUsageFlags & usage)) fatal("swapchain color usage unsupported");

    VkSwapchainCreateInfoKHR sci = { .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR, .surface = surface, .minImageCount = img_count, .imageFormat = swapchain_format, .imageColorSpace = chosen_fmt.colorSpace, .imageExtent = swapchain_extent, .imageArrayLayers = 1, .imageUsage = usage, .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE, .preTransform = caps.currentTransform, .compositeAlpha = comp_alpha, .presentMode = VK_PRESENT_MODE_FIFO_KHR, .clipped = VK_TRUE, .oldSwapchain = old_swapchain };
    res = vkCreateSwapchainKHR(device, &sci, NULL, &swapchain);
    if (res != VK_SUCCESS) fatal_vk("vkCreateSwapchainKHR", res);
    vkGetSwapchainImagesKHR(device, swapchain, &swapchain_img_count, NULL);
    swapchain_imgs = malloc(sizeof(VkImage) * swapchain_img_count);
    vkGetSwapchainImagesKHR(device, swapchain, &swapchain_img_count, swapchain_imgs);
    swapchain_imgviews = malloc(sizeof(VkImageView) * swapchain_img_count);
    for (uint32_t i = 0; i < swapchain_img_count; i++) {
        VkImageViewCreateInfo ivci = { .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = swapchain_imgs[i], .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = swapchain_format, .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0,1,0,1 } };
        res = vkCreateImageView(device, &ivci, NULL, &swapchain_imgviews[i]);
        if (res != VK_SUCCESS) fatal_vk("vkCreateImageView", res);
    }
}

/* render pass */
static void create_render_pass(void) {
    VkAttachmentDescription att = { .format = swapchain_format, .samples = VK_SAMPLE_COUNT_1_BIT, .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, .storeOp = VK_ATTACHMENT_STORE_OP_STORE, .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE, .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE, .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED, .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR };
    VkAttachmentReference aref = { .attachment = 0, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkSubpassDescription sub = { .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS, .colorAttachmentCount = 1, .pColorAttachments = &aref };
    VkRenderPassCreateInfo rpci = { .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, .attachmentCount = 1, .pAttachments = &att, .subpassCount = 1, .pSubpasses = &sub };
    res = vkCreateRenderPass(device, &rpci, NULL, &render_pass);
    if (res != VK_SUCCESS) fatal_vk("vkCreateRenderPass", res);
}

static void create_descriptor_layout(void) {
    VkDescriptorSetLayoutBinding binding = { .binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT };
    VkDescriptorSetLayoutCreateInfo lci = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = 1, .pBindings = &binding };
    res = vkCreateDescriptorSetLayout(device, &lci, NULL, &descriptor_layout);
    if (res != VK_SUCCESS) fatal_vk("vkCreateDescriptorSetLayout", res);
}

/* create simple pipeline from provided SPIR-V files (vertex/fragment) */
static VkShaderModule create_shader_module_from_spv(const char* path) {
    size_t words = 0; uint32_t* code = read_file_bin_u32(path, &words);
    if (!code) fatal("read spv");
    VkShaderModuleCreateInfo smci = { .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize = words * 4, .pCode = code };
    VkShaderModule mod; res = vkCreateShaderModule(device, &smci, NULL, &mod);
    if (res != VK_SUCCESS) fatal_vk("vkCreateShaderModule", res);
    free(code); return mod;
}

static void create_pipeline(const char* vert_spv, const char* frag_spv) {
    VkShaderModule vs = create_shader_module_from_spv(vert_spv);
    VkShaderModule fs = create_shader_module_from_spv(frag_spv);
    VkPipelineShaderStageCreateInfo stages[2] = {
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vs, .pName = "main" },
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = fs, .pName = "main" }
    };
    /* vertex input binding */
    VkVertexInputBindingDescription bind = { .binding = 0, .stride = sizeof(Vtx), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX };
    VkVertexInputAttributeDescription attr[4] = {
        {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(Vtx,px) },
        {.location = 1, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(Vtx,u) },
        {.location = 2, .binding = 0, .format = VK_FORMAT_R32_SFLOAT, .offset = offsetof(Vtx,use_tex) },
        {.location = 3, .binding = 0, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = offsetof(Vtx,r) }
    };
    VkPipelineVertexInputStateCreateInfo vxi = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, .vertexBindingDescriptionCount = 1, .pVertexBindingDescriptions = &bind, .vertexAttributeDescriptionCount = 4, .pVertexAttributeDescriptions = attr };

    VkPipelineInputAssemblyStateCreateInfo ia = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST };
    float viewport_w = (swapchain_extent.width == 0) ? 1.0f : (float)swapchain_extent.width;
    float viewport_h = (swapchain_extent.height == 0) ? 1.0f : (float)swapchain_extent.height;
    VkViewport vp = { .x = 0, .y = 0, .width = viewport_w, .height = viewport_h, .minDepth = 0.0f, .maxDepth = 1.0f };
    VkRect2D sc = { .offset = {0,0}, .extent = swapchain_extent };
    VkPipelineViewportStateCreateInfo vpci = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .viewportCount = 1, .pViewports = &vp, .scissorCount = 1, .pScissors = &sc };
    VkPipelineRasterizationStateCreateInfo rs = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, .polygonMode = VK_POLYGON_MODE_FILL, .cullMode = VK_CULL_MODE_NONE, .frontFace = VK_FRONT_FACE_CLOCKWISE, .lineWidth = 1.0f };
    VkPipelineMultisampleStateCreateInfo ms = { .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT };
    VkPipelineDepthStencilStateCreateInfo ds = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, .depthTestEnable = VK_FALSE, .depthWriteEnable = VK_FALSE, .depthBoundsTestEnable = VK_FALSE, .stencilTestEnable = VK_FALSE };
    VkPipelineColorBlendAttachmentState cbatt = { .blendEnable = swapchain_supports_blend, .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA, .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, .colorBlendOp = VK_BLEND_OP_ADD, .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE, .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, .alphaBlendOp = VK_BLEND_OP_ADD, .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT };
    VkPipelineColorBlendStateCreateInfo cb = { .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, .attachmentCount = 1, .pAttachments = &cbatt };
    VkPushConstantRange pcr = { .stageFlags = VK_SHADER_STAGE_VERTEX_BIT, .offset = 0, .size = sizeof(float) * 2 };
    VkPipelineLayoutCreateInfo plci = { .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .setLayoutCount = 1, .pSetLayouts = &descriptor_layout, .pushConstantRangeCount = 1, .pPushConstantRanges = &pcr };
    res = vkCreatePipelineLayout(device, &plci, NULL, &pipeline_layout);
    if (res != VK_SUCCESS) fatal_vk("vkCreatePipelineLayout", res);
    VkGraphicsPipelineCreateInfo gpci = { .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, .stageCount = 2, .pStages = stages, .pVertexInputState = &vxi, .pInputAssemblyState = &ia, .pViewportState = &vpci, .pRasterizationState = &rs, .pMultisampleState = &ms, .pDepthStencilState = &ds, .pColorBlendState = &cb, .layout = pipeline_layout, .renderPass = render_pass, .subpass = 0 };
    res = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &gpci, NULL, &pipeline);
    if (res != VK_SUCCESS) fatal_vk("vkCreateGraphicsPipelines", res);
    vkDestroyShaderModule(device, vs, NULL); vkDestroyShaderModule(device, fs, NULL);
}

/* command pool/buffers/framebuffers/semaphores */
static void create_cmds_and_sync(void) {
    if (sem_img_avail) { vkDestroySemaphore(device, sem_img_avail, NULL); sem_img_avail = VK_NULL_HANDLE; }
    if (sem_render_done) { vkDestroySemaphore(device, sem_render_done, NULL); sem_render_done = VK_NULL_HANDLE; }
    VkCommandPoolCreateInfo cpci = { .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .queueFamilyIndex = graphics_family, .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT };
    res = vkCreateCommandPool(device, &cpci, NULL, &cmdpool);
    if (res != VK_SUCCESS) fatal_vk("vkCreateCommandPool", res);
    cmdbuffers = malloc(sizeof(VkCommandBuffer) * swapchain_img_count);
    VkCommandBufferAllocateInfo cbai = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = cmdpool, .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = swapchain_img_count };
    res = vkAllocateCommandBuffers(device, &cbai, cmdbuffers);
    if (res != VK_SUCCESS) fatal_vk("vkAllocateCommandBuffers", res);

    framebuffers = malloc(sizeof(VkFramebuffer) * swapchain_img_count);
    for (uint32_t i = 0; i < swapchain_img_count; i++) {
        VkFramebufferCreateInfo fci = { .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, .renderPass = render_pass, .attachmentCount = 1, .pAttachments = &swapchain_imgviews[i], .width = swapchain_extent.width, .height = swapchain_extent.height, .layers = 1 };
        res = vkCreateFramebuffer(device, &fci, NULL, &framebuffers[i]);
        if (res != VK_SUCCESS) fatal_vk("vkCreateFramebuffer", res);
    }
    VkSemaphoreCreateInfo sci = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO }; vkCreateSemaphore(device, &sci, NULL, &sem_img_avail); vkCreateSemaphore(device, &sci, NULL, &sem_render_done);
    fences = malloc(sizeof(VkFence) * swapchain_img_count);
    for (uint32_t i = 0; i < swapchain_img_count; i++) {
        VkFenceCreateInfo fci = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT };
        vkCreateFence(device, &fci, NULL, &fences[i]);
    }
}

static void cleanup_swapchain(bool keep_swapchain_handle) {
    if (cmdbuffers) {
        vkFreeCommandBuffers(device, cmdpool, swapchain_img_count, cmdbuffers);
        free(cmdbuffers);
        cmdbuffers = NULL;
    }
    if (cmdpool) {
        vkDestroyCommandPool(device, cmdpool, NULL);
        cmdpool = VK_NULL_HANDLE;
    }
    if (framebuffers) {
        for (uint32_t i = 0; i < swapchain_img_count; i++) vkDestroyFramebuffer(device, framebuffers[i], NULL);
        free(framebuffers);
        framebuffers = NULL;
    }
    if (fences) {
        for (uint32_t i = 0; i < swapchain_img_count; i++) vkDestroyFence(device, fences[i], NULL);
        free(fences);
        fences = NULL;
    }
    if (swapchain_imgviews) {
        for (uint32_t i = 0; i < swapchain_img_count; i++) vkDestroyImageView(device, swapchain_imgviews[i], NULL);
        free(swapchain_imgviews);
        swapchain_imgviews = NULL;
    }
    if (swapchain_imgs) {
        free(swapchain_imgs);
        swapchain_imgs = NULL;
    }
    if (!keep_swapchain_handle && swapchain) {
        vkDestroySwapchainKHR(device, swapchain, NULL);
        swapchain = VK_NULL_HANDLE;
    }
    if (pipeline) {
        vkDestroyPipeline(device, pipeline, NULL);
        pipeline = VK_NULL_HANDLE;
    }
    if (pipeline_layout) {
        vkDestroyPipelineLayout(device, pipeline_layout, NULL);
        pipeline_layout = VK_NULL_HANDLE;
    }
    if (render_pass) {
        vkDestroyRenderPass(device, render_pass, NULL);
        render_pass = VK_NULL_HANDLE;
    }
    swapchain_img_count = 0;
}

static void destroy_device_resources(void) {
    cleanup_swapchain(false);

    if (descriptor_pool) { vkDestroyDescriptorPool(device, descriptor_pool, NULL); descriptor_pool = VK_NULL_HANDLE; }
    if (descriptor_layout) { vkDestroyDescriptorSetLayout(device, descriptor_layout, NULL); descriptor_layout = VK_NULL_HANDLE; }
    if (font_sampler) { vkDestroySampler(device, font_sampler, NULL); font_sampler = VK_NULL_HANDLE; }
    if (font_image_view) { vkDestroyImageView(device, font_image_view, NULL); font_image_view = VK_NULL_HANDLE; }
    if (font_image) { vkDestroyImage(device, font_image, NULL); font_image = VK_NULL_HANDLE; }
    if (font_image_mem) { vkFreeMemory(device, font_image_mem, NULL); font_image_mem = VK_NULL_HANDLE; }
    if (vertex_buffer) { vkDestroyBuffer(device, vertex_buffer, NULL); vertex_buffer = VK_NULL_HANDLE; }
    if (vertex_memory) { vkFreeMemory(device, vertex_memory, NULL); vertex_memory = VK_NULL_HANDLE; }
    vertex_capacity = 0;
    if (sem_img_avail) { vkDestroySemaphore(device, sem_img_avail, NULL); sem_img_avail = VK_NULL_HANDLE; }
    if (sem_render_done) { vkDestroySemaphore(device, sem_render_done, NULL); sem_render_done = VK_NULL_HANDLE; }
}

static void recreate_instance_and_surface(void) {
    if (surface) { vkDestroySurfaceKHR(instance, surface, NULL); surface = VK_NULL_HANDLE; }
    if (instance) { vkDestroyInstance(instance, NULL); instance = VK_NULL_HANDLE; }

    create_instance();
    res = glfwCreateWindowSurface(instance, g_window, NULL, &surface);
    if (res != VK_SUCCESS) fatal_vk("glfwCreateWindowSurface", res);
}

static void recreate_swapchain(void) {
    vkDeviceWaitIdle(device);

    VkSwapchainKHR old_swapchain = swapchain;
    cleanup_swapchain(true);

    create_swapchain_and_views(old_swapchain);
    if (!swapchain) {
        if (old_swapchain) vkDestroySwapchainKHR(device, old_swapchain, NULL);
        return;
    }

    create_render_pass();
    create_pipeline(g_vert_spv, g_frag_spv);
    create_cmds_and_sync();

    if (old_swapchain) vkDestroySwapchainKHR(device, old_swapchain, NULL);
}

/* create a simple host-visible vertex buffer */
static uint32_t find_mem_type(uint32_t typeFilter, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties mp; vkGetPhysicalDeviceMemoryProperties(physical, &mp);
    for (uint32_t i = 0; i < mp.memoryTypeCount; i++) {
        if ((typeFilter & (1u << i)) && (mp.memoryTypes[i].propertyFlags & props) == props) return i;
    }
    fatal("no mem type");
    return 0;
}
static void create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkBuffer* out_buf, VkDeviceMemory* out_mem) {
    VkBufferCreateInfo bci = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = size, .usage = usage, .sharingMode = VK_SHARING_MODE_EXCLUSIVE };
    res = vkCreateBuffer(device, &bci, NULL, out_buf);
    if (res != VK_SUCCESS) fatal_vk("vkCreateBuffer", res);
    VkMemoryRequirements mr; vkGetBufferMemoryRequirements(device, *out_buf, &mr);
    VkMemoryAllocateInfo mai = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = mr.size, .memoryTypeIndex = find_mem_type(mr.memoryTypeBits, props) };
    res = vkAllocateMemory(device, &mai, NULL, out_mem);
    if (res != VK_SUCCESS) fatal_vk("vkAllocateMemory", res);
    vkBindBufferMemory(device, *out_buf, *out_mem, 0);
}
static VkCommandBuffer begin_single_time_commands(void) {
    VkCommandBufferAllocateInfo ai = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = cmdpool, .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 1 };
    VkCommandBuffer cb;
    vkAllocateCommandBuffers(device, &ai, &cb);
    VkCommandBufferBeginInfo bi = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
    vkBeginCommandBuffer(cb, &bi);
    return cb;
}
static void end_single_time_commands(VkCommandBuffer cb) {
    vkEndCommandBuffer(cb);
    VkSubmitInfo si = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &cb };
    vkQueueSubmit(queue, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);
    vkFreeCommandBuffers(device, cmdpool, 1, &cb);
}
static void create_vertex_buffer(size_t bytes) {
    if (vertex_buffer != VK_NULL_HANDLE && vertex_capacity >= bytes) {
        return;
    }

    if (vertex_buffer) {
        vkDestroyBuffer(device, vertex_buffer, NULL);
        vertex_buffer = VK_NULL_HANDLE;
    }
    if (vertex_memory) {
        vkFreeMemory(device, vertex_memory, NULL);
        vertex_memory = VK_NULL_HANDLE;
        vertex_capacity = 0;
    }

    create_buffer(bytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &vertex_buffer, &vertex_memory);
    vertex_capacity = bytes;
}

static void transition_image_layout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkCommandBuffer cb = begin_single_time_commands();
    VkImageMemoryBarrier barrier = { .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, .oldLayout = oldLayout, .newLayout = newLayout, .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .image = image, .subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1 } };
    VkPipelineStageFlags src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    vkCmdPipelineBarrier(cb, src_stage, dst_stage, 0, 0, NULL, 0, NULL, 1, &barrier);
    end_single_time_commands(cb);
}

static void copy_buffer_to_image(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
    VkCommandBuffer cb = begin_single_time_commands();
    VkBufferImageCopy copy = { .bufferOffset = 0, .bufferRowLength = 0, .bufferImageHeight = 0, .imageSubresource = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1 }, .imageOffset = {0,0,0}, .imageExtent = { width, height, 1 } };
    vkCmdCopyBufferToImage(cb, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    end_single_time_commands(cb);
}

/* update vertex buffer host-side */
static void upload_vertices(void) {
    if (vtx_count == 0) {
        if (vertex_buffer) {
            vkDestroyBuffer(device, vertex_buffer, NULL);
            vertex_buffer = VK_NULL_HANDLE;
        }
        if (vertex_memory) {
            vkFreeMemory(device, vertex_memory, NULL);
            vertex_memory = VK_NULL_HANDLE;
        }
        vertex_capacity = 0;
        return;
    }
    size_t bytes = vtx_count * sizeof(Vtx);
    create_vertex_buffer(bytes);
    void* dst = NULL; vkMapMemory(device, vertex_memory, 0, bytes, 0, &dst);
    memcpy(dst, vtx_buf, bytes);
    vkUnmapMemory(device, vertex_memory);
}

static bool ensure_vtx_capacity(size_t required)
{
    if (required <= vtx_capacity) {
        return true;
    }

    size_t new_capacity = vtx_capacity == 0 ? required : vtx_capacity * 2;
    while (new_capacity < required) {
        new_capacity *= 2;
    }

    Vtx *expanded = realloc(vtx_buf, new_capacity * sizeof(Vtx));
    if (!expanded) {
        return false;
    }

    vtx_buf = expanded;
    vtx_capacity = new_capacity;
    return true;
}

/* record commands per framebuffer (we will re-record each frame in this simple example) */
/* ---------- GUI building: convert g_widgets -> vertex list (rects + textured glyphs) ---------- */
static unsigned char* ttf_buffer = NULL;
static stbtt_fontinfo fontinfo;
static unsigned char* atlas = NULL;
static int atlas_w = 1024, atlas_h = 1024;
static float font_scale = 0.0f;
static int ascent = 0;
static int descent = 0;
typedef struct { float u0, v0, u1, v1; float xoff, yoff; float w, h; float advance; } Glyph;
#define GLYPH_CAPACITY 2048
static Glyph glyphs[GLYPH_CAPACITY];
static unsigned char glyph_valid[GLYPH_CAPACITY];

typedef struct {
    GlyphQuad *items;
    size_t count;
    size_t capacity;
} GlyphQuadArray;

static int utf8_decode(const char* s, int* out_advance) {
    unsigned char c = (unsigned char)*s;
    if (c < 0x80) { *out_advance = 1; return c; }
    if ((c >> 5) == 0x6) { *out_advance = 2; return ((int)(c & 0x1F) << 6) | ((int)(s[1] & 0x3F)); }
    if ((c >> 4) == 0xE) { *out_advance = 3; return ((int)(c & 0x0F) << 12) | (((int)s[1] & 0x3F) << 6) | ((int)(s[2] & 0x3F)); }
    if ((c >> 3) == 0x1E) { *out_advance = 4; return ((int)(c & 0x07) << 18) | (((int)s[1] & 0x3F) << 12) |
                                        (((int)s[2] & 0x3F) << 6) | ((int)(s[3] & 0x3F)); }
    *out_advance = 1;
    return '?';
}

static const Glyph* get_glyph(int codepoint) {
    if (codepoint >= 0 && codepoint < GLYPH_CAPACITY && glyph_valid[codepoint]) {
        return &glyphs[codepoint];
    }
    if (glyph_valid['?']) return &glyphs['?'];
    return NULL;
}

static int apply_clip_rect(const Widget* widget, const Rect* input, Rect* out) {
    if (!widget || !input || !out) return 0;
    *out = *input;
    if (!widget->has_clip) return 1;
    float x0 = fmaxf(input->x, widget->clip.x);
    float y0 = fmaxf(input->y, widget->clip.y);
    float x1 = fminf(input->x + input->w, widget->clip.x + widget->clip.w);
    float y1 = fminf(input->y + input->h, widget->clip.y + widget->clip.h);
    if (x1 <= x0 || y1 <= y0) return 0;
    out->x = x0;
    out->y = y0;
    out->w = x1 - x0;
    out->h = y1 - y0;
    return 1;
}

static void glyph_quad_array_reserve(GlyphQuadArray *arr, size_t required)
{
    if (required <= arr->capacity) {
        return;
    }

    size_t new_capacity = arr->capacity == 0 ? required : arr->capacity * 2;
    while (new_capacity < required) {
        new_capacity *= 2;
    }

    GlyphQuad *expanded = realloc(arr->items, new_capacity * sizeof(GlyphQuad));
    if (!expanded) {
        return;
    }

    arr->items = expanded;
    arr->capacity = new_capacity;
}

static void build_font_atlas(void) {
    if (!g_font_path) fatal("Font path is null");
    FILE* f = fopen(g_font_path, "rb");
    if (!f) { fprintf(stderr, "Fatal: font not found at %s\n", g_font_path); fatal("font load"); }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    ttf_buffer = malloc(sz); fread(ttf_buffer, 1, sz, f); fclose(f);
    stbtt_InitFont(&fontinfo, ttf_buffer, 0);

    atlas_w = 1024; atlas_h = 1024;
    atlas = malloc(atlas_w * atlas_h);
    memset(atlas, 0, atlas_w * atlas_h);
    memset(glyph_valid, 0, sizeof(glyph_valid));
    font_scale = stbtt_ScaleForPixelHeight(&fontinfo, 32.0f);
    int raw_ascent = 0, raw_descent = 0;
    stbtt_GetFontVMetrics(&fontinfo, &raw_ascent, &raw_descent, NULL);
    ascent = (int)roundf(raw_ascent * font_scale);
    descent = (int)roundf(raw_descent * font_scale);

    int ranges[][2] = { {32, 126}, {0x0400, 0x04FF} };
    int range_count = (int)(sizeof(ranges) / sizeof(ranges[0]));

    int x = 0, y = 0, rowh = 0;
    for (int r = 0; r < range_count; r++) {
        for (int c = ranges[r][0]; c <= ranges[r][1] && c < GLYPH_CAPACITY; c++) {
            int aw, ah, bx, by;
            unsigned char* bitmap = stbtt_GetCodepointBitmap(&fontinfo, 0, font_scale, c, &aw, &ah, &bx, &by);
            if (x + aw >= atlas_w) { x = 0; y += rowh; rowh = 0; }
            if (y + ah >= atlas_h) { fprintf(stderr, "atlas too small\n"); stbtt_FreeBitmap(bitmap, NULL); break; }
            for (int yy = 0; yy < ah; yy++) {
                for (int xx = 0; xx < aw; xx++) {
                    atlas[(y + yy) * atlas_w + (x + xx)] = bitmap[yy * aw + xx];
                }
            }
            stbtt_FreeBitmap(bitmap, NULL);
            int advance, lsb;
            stbtt_GetCodepointHMetrics(&fontinfo, c, &advance, &lsb);
            int box_x0, box_y0, box_x1, box_y1;
            stbtt_GetCodepointBitmapBox(&fontinfo, c, font_scale, font_scale, &box_x0, &box_y0, &box_x1, &box_y1);
            glyphs[c].advance = advance * font_scale;
            glyphs[c].xoff = (float)box_x0;
            glyphs[c].yoff = (float)box_y0;
            glyphs[c].w = (float)(box_x1 - box_x0);
            glyphs[c].h = (float)(box_y1 - box_y0);
            glyphs[c].u0 = (float)x / (float)atlas_w;
            glyphs[c].v0 = (float)y / (float)atlas_h;
            glyphs[c].u1 = (float)(x + aw) / (float)atlas_w;
            glyphs[c].v1 = (float)(y + ah) / (float)atlas_h;
            glyph_valid[c] = 1;
            x += aw + 1;
            if (ah > rowh) rowh = ah;
        }
    }
}

static void create_font_texture(void) {
    if (!atlas) fatal("font atlas not built");
    VkImageCreateInfo ici = { .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, .imageType = VK_IMAGE_TYPE_2D, .format = VK_FORMAT_R8_UNORM, .extent = { (uint32_t)atlas_w, (uint32_t)atlas_h, 1 }, .mipLevels = 1, .arrayLayers = 1, .samples = VK_SAMPLE_COUNT_1_BIT, .tiling = VK_IMAGE_TILING_OPTIMAL, .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, .sharingMode = VK_SHARING_MODE_EXCLUSIVE, .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED }; 
    res = vkCreateImage(device, &ici, NULL, &font_image);
    if (res != VK_SUCCESS) fatal_vk("vkCreateImage", res);
    VkMemoryRequirements mr; vkGetImageMemoryRequirements(device, font_image, &mr);
    VkMemoryAllocateInfo mai = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = mr.size, .memoryTypeIndex = find_mem_type(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) };
    res = vkAllocateMemory(device, &mai, NULL, &font_image_mem);
    if (res != VK_SUCCESS) fatal_vk("vkAllocateMemory", res);
    vkBindImageMemory(device, font_image, font_image_mem, 0);

    VkBuffer staging = VK_NULL_HANDLE; VkDeviceMemory staging_mem = VK_NULL_HANDLE;
    create_buffer(atlas_w * atlas_h, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &staging, &staging_mem);
    void* mapped = NULL; vkMapMemory(device, staging_mem, 0, VK_WHOLE_SIZE, 0, &mapped); memcpy(mapped, atlas, atlas_w * atlas_h); vkUnmapMemory(device, staging_mem);

    transition_image_layout(font_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copy_buffer_to_image(staging, font_image, (uint32_t)atlas_w, (uint32_t)atlas_h);
    transition_image_layout(font_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(device, staging, NULL);
    vkFreeMemory(device, staging_mem, NULL);

    VkImageViewCreateInfo ivci = { .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = font_image, .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = VK_FORMAT_R8_UNORM, .subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1 } };
    res = vkCreateImageView(device, &ivci, NULL, &font_image_view);
    if (res != VK_SUCCESS) fatal_vk("vkCreateImageView", res);

    VkSamplerCreateInfo sci = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .magFilter = VK_FILTER_LINEAR, .minFilter = VK_FILTER_LINEAR, .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK, .unnormalizedCoordinates = VK_FALSE, .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST };
    res = vkCreateSampler(device, &sci, NULL, &font_sampler);
    if (res != VK_SUCCESS) fatal_vk("vkCreateSampler", res);
}

static void create_descriptor_pool_and_set(void) {
    VkDescriptorPoolSize pool = { .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1 };
    VkDescriptorPoolCreateInfo dpci = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, .maxSets = 1, .poolSizeCount = 1, .pPoolSizes = &pool };
    res = vkCreateDescriptorPool(device, &dpci, NULL, &descriptor_pool);
    if (res != VK_SUCCESS) fatal_vk("vkCreateDescriptorPool", res);

    VkDescriptorSetAllocateInfo dsai = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, .descriptorPool = descriptor_pool, .descriptorSetCount = 1, .pSetLayouts = &descriptor_layout };
    res = vkAllocateDescriptorSets(device, &dsai, &descriptor_set);
    if (res != VK_SUCCESS) fatal_vk("vkAllocateDescriptorSets", res);

    VkDescriptorImageInfo dii = { .sampler = font_sampler, .imageView = font_image_view, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    VkWriteDescriptorSet w = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = descriptor_set, .dstBinding = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &dii };
    vkUpdateDescriptorSets(device, 1, &w, 0, NULL);
}

/* build vtxs from g_widgets each frame */
static void build_vertices_from_widgets(void) {
    vtx_count = 0;

    if (g_widgets.count == 0 || swapchain_extent.width == 0 || swapchain_extent.height == 0) {
        return;
    }

    size_t slider_extras = 0;
    for (size_t i = 0; i < g_widgets.count; ++i) {
        if (g_widgets.items[i].type == W_HSLIDER) {
            slider_extras += 2;
        }
    }

    size_t view_model_capacity = g_widgets.count * 4 + slider_extras;
    ViewModel *view_models = calloc(view_model_capacity, sizeof(ViewModel));
    GlyphQuadArray glyph_quads = {0};
    if (!view_models) {
        return;
    }

    size_t view_model_count = 0;
    for (size_t i = 0; i < g_widgets.count; ++i) {
        const Widget *widget = &g_widgets.items[i];

        float effective_offset = widget->scroll_static ? 0.0f : -widget->scroll_offset;
        Rect widget_rect = { widget->rect.x, widget->rect.y + effective_offset, widget->rect.w, widget->rect.h };
        Rect inner_rect = widget_rect;
        if (widget->border_thickness > 0.0f) {
            inner_rect.x += widget->border_thickness;
            inner_rect.y += widget->border_thickness;
            inner_rect.w -= widget->border_thickness * 2.0f;
            inner_rect.h -= widget->border_thickness * 2.0f;
            if (inner_rect.w < 0.0f) inner_rect.w = 0.0f;
            if (inner_rect.h < 0.0f) inner_rect.h = 0.0f;
            Rect clipped_border;
            if (apply_clip_rect(widget, &widget_rect, &clipped_border)) {
                view_models[view_model_count++] = (ViewModel){
                    .id = widget->id,
                    .logical_box = { {clipped_border.x, clipped_border.y}, {clipped_border.w, clipped_border.h} },
                    .z_index = (int)view_model_count,
                    .color = widget->border_color,
                };
            }
        }

        if (widget->type == W_HSLIDER) {
            float track_height = fmaxf(inner_rect.h * 0.35f, 6.0f);
            float track_y = inner_rect.y + (inner_rect.h - track_height) * 0.5f;
            float track_x = inner_rect.x;
            float track_w = inner_rect.w;
            float denom = widget->maxv - widget->minv;
            float t = denom != 0.0f ? (widget->value - widget->minv) / denom : 0.0f;
            if (t < 0.0f) t = 0.0f; else if (t > 1.0f) t = 1.0f;

            int base_z = (int)view_model_count;

            Color track_color = widget->color;
            track_color.a *= 0.35f;
            Rect track_rect = { track_x, track_y, track_w, track_height };
            Rect clipped_track;
            if (apply_clip_rect(widget, &track_rect, &clipped_track)) {
                view_models[view_model_count++] = (ViewModel){
                    .id = widget->id,
                    .logical_box = { {clipped_track.x, clipped_track.y}, {clipped_track.w, clipped_track.h} },
                    .z_index = base_z,
                    .color = track_color,
                };
            }

            float fill_w = track_w * t;
            Rect fill_rect = { track_x, track_y, fill_w, track_height };
            Rect clipped_fill;
            if (apply_clip_rect(widget, &fill_rect, &clipped_fill)) {
                view_models[view_model_count++] = (ViewModel){
                    .id = widget->id,
                    .logical_box = { {clipped_fill.x, clipped_fill.y}, {clipped_fill.w, clipped_fill.h} },
                    .z_index = base_z + 1,
                    .color = widget->color,
                };
            }

            float knob_w = fmaxf(track_height, inner_rect.h * 0.3f);
            float knob_x = track_x + fill_w - knob_w * 0.5f;
            if (knob_x < track_x) knob_x = track_x;
            float knob_max = track_x + track_w - knob_w;
            if (knob_x > knob_max) knob_x = knob_max;
            float knob_h = track_height * 1.5f;
            float knob_y = track_y + (track_height - knob_h) * 0.5f;
            Color knob_color = widget->text_color;
            if (knob_color.a <= 0.0f) knob_color = (Color){1.0f, 1.0f, 1.0f, 1.0f};
            Rect knob_rect = { knob_x, knob_y, knob_w, knob_h };
            Rect clipped_knob;
            if (apply_clip_rect(widget, &knob_rect, &clipped_knob)) {
                view_models[view_model_count++] = (ViewModel){
                    .id = widget->id,
                    .logical_box = { {clipped_knob.x, clipped_knob.y}, {clipped_knob.w, clipped_knob.h} },
                    .z_index = base_z + 2,
                    .color = knob_color,
                };
            }
            continue;
        }

        Rect clipped_fill;
        if (apply_clip_rect(widget, &inner_rect, &clipped_fill)) {
            view_models[view_model_count++] = (ViewModel){
                .id = widget->id,
                .logical_box = { {clipped_fill.x, clipped_fill.y}, {clipped_fill.w, clipped_fill.h} },
                .z_index = (int)view_model_count,
                .color = widget->color,
            };
        }

        if (widget->scrollbar_enabled && widget->show_scrollbar && widget->scroll_viewport > 0.0f &&
            widget->scroll_content > widget->scroll_viewport + 1.0f) {
            float track_w = widget->scrollbar_width > 0.0f ? widget->scrollbar_width : fmaxf(4.0f, inner_rect.w * 0.02f);
            float track_h = inner_rect.h - widget->padding * 2.0f;
            float track_x = inner_rect.x + inner_rect.w - track_w - widget->padding * 0.5f;
            float track_y = inner_rect.y + widget->padding;
            Color track_color = widget->scrollbar_track_color;
            Rect scroll_track = { track_x, track_y, track_w, track_h };
            Rect clipped_track;
            const int scrollbar_z = 1000000;

            if (apply_clip_rect(widget, &scroll_track, &clipped_track)) {
                view_models[view_model_count++] = (ViewModel){
                    .id = widget->id,
                    .logical_box = { {clipped_track.x, clipped_track.y}, {clipped_track.w, clipped_track.h} },
                    .z_index = scrollbar_z,
                    .color = track_color,
                };
            }

            float thumb_ratio = widget->scroll_viewport / widget->scroll_content;
            float thumb_h = fmaxf(track_h * thumb_ratio, 12.0f);
            float max_offset = widget->scroll_content - widget->scroll_viewport;
            float clamped_offset = widget->scroll_offset;
            if (clamped_offset < 0.0f) clamped_offset = 0.0f;
            if (clamped_offset > max_offset) clamped_offset = max_offset;
            float offset_t = (max_offset != 0.0f) ? (clamped_offset / max_offset) : 0.0f;
            float thumb_y = track_y + offset_t * (track_h - thumb_h);
            Color thumb_color = widget->scrollbar_thumb_color;

            Rect thumb_rect = { track_x, thumb_y, track_w, thumb_h };
            Rect clipped_thumb;
            if (apply_clip_rect(widget, &thumb_rect, &clipped_thumb)) {
                view_models[view_model_count++] = (ViewModel){
                    .id = widget->id,
                    .logical_box = { {clipped_thumb.x, clipped_thumb.y}, {clipped_thumb.w, clipped_thumb.h} },
                    .z_index = scrollbar_z + 1,
                    .color = thumb_color,
                };
            }
        }
    }
    int glyph_z_base = (int)view_model_count;
    for (size_t i = 0; i < g_widgets.count; ++i) {
        const Widget *widget = &g_widgets.items[i];

        if (!widget->text || !*widget->text) {
            continue;
        }

        float effective_offset = widget->scroll_static ? 0.0f : -widget->scroll_offset;
        float pen_x = widget->rect.x + widget->padding;
        float pen_y = widget->rect.y + effective_offset + widget->padding + (float)ascent;

        for (const char *c = widget->text; *c; ) {
            int adv = 0;
            int codepoint = utf8_decode(c, &adv);
            if (adv <= 0) break;
            if (codepoint < 32) { c += adv; continue; }

            const Glyph *g = get_glyph(codepoint);
            if (!g) { c += adv; continue; }
            float x0 = pen_x + g->xoff;
            float y0 = pen_y + g->yoff;
            Rect glyph_rect = { x0, y0, g->w, g->h };
            Rect clipped_rect;
            if (!apply_clip_rect(widget, &glyph_rect, &clipped_rect)) { pen_x += g->advance; c += adv; continue; }

            float u0 = g->u0;
            float v0 = g->v0;
            float u1 = g->u1;
            float v1 = g->v1;
            if (clipped_rect.x > glyph_rect.x && glyph_rect.w > 0.0f) {
                float t = (clipped_rect.x - glyph_rect.x) / glyph_rect.w;
                u0 += (u1 - u0) * t;
            }
            if (clipped_rect.y > glyph_rect.y && glyph_rect.h > 0.0f) {
                float t = (clipped_rect.y - glyph_rect.y) / glyph_rect.h;
                v0 += (v1 - v0) * t;
            }
            float glyph_x1 = glyph_rect.x + glyph_rect.w;
            float glyph_y1 = glyph_rect.y + glyph_rect.h;
            if (clipped_rect.x + clipped_rect.w < glyph_x1 && glyph_rect.w > 0.0f) {
                float t = (glyph_x1 - (clipped_rect.x + clipped_rect.w)) / glyph_rect.w;
                u1 -= (u1 - u0) * t;
            }
            if (clipped_rect.y + clipped_rect.h < glyph_y1 && glyph_rect.h > 0.0f) {
                float t = (glyph_y1 - (clipped_rect.y + clipped_rect.h)) / glyph_rect.h;
                v1 -= (v1 - v0) * t;
            }

            glyph_quad_array_reserve(&glyph_quads, glyph_quads.count + 1);
            if (glyph_quads.capacity < glyph_quads.count + 1) {
                break;
            }

            glyph_quads.items[glyph_quads.count++] = (GlyphQuad){
                .min = {clipped_rect.x, clipped_rect.y},
                .max = {clipped_rect.x + clipped_rect.w, clipped_rect.y + clipped_rect.h},
                .uv0 = {u0, v0},
                .uv1 = {u1, v1},
                .color = widget->text_color,
                .z_index = glyph_z_base + (int)glyph_quads.count,
            };

            pen_x += g->advance;
            c += adv;
        }
    }

    float projection[16] = {0};
    projection[0] = 1.0f;
    projection[5] = 1.0f;
    projection[10] = 1.0f;
    projection[15] = 1.0f;

    CoordinateTransformer transformer = g_transformer;
    transformer.viewport_size = (Vec2){(float)swapchain_extent.width, (float)swapchain_extent.height};

    RenderContext context;
    render_context_init(&context, &transformer, projection);

    Renderer renderer;
    renderer_init(&renderer, &context, view_model_count);

    UiVertexBuffer background_buffer;
    ui_vertex_buffer_init(&background_buffer, view_model_count * 6);
    renderer_fill_background_vertices(&renderer, view_models, view_model_count, &background_buffer);

    UiTextVertexBuffer text_buffer;
    ui_text_vertex_buffer_init(&text_buffer, glyph_quads.count * 6);
    renderer_fill_text_vertices(&context, glyph_quads.items, glyph_quads.count, &text_buffer);

    size_t total_vertices = background_buffer.count + text_buffer.count;
    if (total_vertices > 0 && ensure_vtx_capacity(total_vertices)) {
        size_t cursor = 0;
        for (size_t i = 0; i < background_buffer.count; ++i) {
            UiVertex v = background_buffer.vertices[i];
            vtx_buf[cursor++] = (Vtx){v.position[0], v.position[1], 0.0f, 0.0f, 0.0f, v.color.r, v.color.g, v.color.b, v.color.a};
        }

        for (size_t i = 0; i < text_buffer.count; ++i) {
            UiTextVertex v = text_buffer.vertices[i];
            vtx_buf[cursor++] = (Vtx){v.position[0], v.position[1], v.uv[0], v.uv[1], 1.0f, v.color.r, v.color.g, v.color.b, v.color.a};
        }

        vtx_count = cursor;
    }

    ui_text_vertex_buffer_dispose(&text_buffer);
    ui_vertex_buffer_dispose(&background_buffer);
    renderer_dispose(&renderer);
    free(glyph_quads.items);
    free(view_models);
}

static bool recover_device_loss(void) {
    fprintf(stderr, "Device lost detected; tearing down and recreating logical device and swapchain resources...\n");
    if (device) vkDeviceWaitIdle(device);
    destroy_device_resources();
    if (device) {
        vkDestroyDevice(device, NULL);
        device = VK_NULL_HANDLE;
    }

    recreate_instance_and_surface();

    pick_physical_and_create_device();
    create_swapchain_and_views(VK_NULL_HANDLE);
    if (!swapchain) return false;
    create_render_pass();
    create_descriptor_layout();
    create_pipeline(g_vert_spv, g_frag_spv);
    create_cmds_and_sync();
    create_font_texture();
    create_descriptor_pool_and_set();
    build_vertices_from_widgets();
    return true;
}

static void record_cmdbuffer(uint32_t idx) {
    VkCommandBuffer cb = cmdbuffers[idx];
    vkResetCommandBuffer(cb, 0);
    VkCommandBufferBeginInfo binfo = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    vkBeginCommandBuffer(cb, &binfo);

    VkClearValue clr = { .color = {{0.9f,0.9f,0.9f,1.0f}} };
    VkRenderPassBeginInfo rpbi = { .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, .renderPass = render_pass, .framebuffer = framebuffers[idx], .renderArea = {.offset = {0,0}, .extent = swapchain_extent }, .clearValueCount = 1, .pClearValues = &clr };
    vkCmdBeginRenderPass(cb, &rpbi, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    ViewConstants pc = { .viewport = { (float)swapchain_extent.width, (float)swapchain_extent.height } };
    vkCmdPushConstants(cb, pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ViewConstants), &pc);
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &descriptor_set, 0, NULL);
    if (vertex_buffer != VK_NULL_HANDLE && vtx_count > 0) {
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cb, 0, 1, &vertex_buffer, &offset);
        /* simple draw: vertices are triangles (every 3 vertices) */
        vkCmdDraw(cb, (uint32_t)vtx_count, 1, 0, 0);
    }

    vkCmdEndRenderPass(cb);
    vkEndCommandBuffer(cb);
}

/* present frame */
static void draw_frame(void) {
    build_vertices_from_widgets();
    if (swapchain == VK_NULL_HANDLE) return;
    uint32_t img_idx;
    VkResult acq = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, sem_img_avail, VK_NULL_HANDLE, &img_idx);
    if (acq == VK_ERROR_DEVICE_LOST) { if (!recover_device_loss()) fatal_vk("vkAcquireNextImageKHR", acq); return; }
    if (acq == VK_ERROR_OUT_OF_DATE_KHR || acq == VK_SUBOPTIMAL_KHR) { recreate_swapchain(); return; }
    if (acq != VK_SUCCESS) fatal_vk("vkAcquireNextImageKHR", acq);
    vkWaitForFences(device, 1, &fences[img_idx], VK_TRUE, UINT64_MAX);
    vkResetFences(device, 1, &fences[img_idx]);

    /* re-upload vertices & record */
    upload_vertices();
    record_cmdbuffer(img_idx);

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo si = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .waitSemaphoreCount = 1, .pWaitSemaphores = &sem_img_avail, .pWaitDstStageMask = &waitStage, .commandBufferCount = 1, .pCommandBuffers = &cmdbuffers[img_idx], .signalSemaphoreCount = 1, .pSignalSemaphores = &sem_render_done };
    VkResult submit = vkQueueSubmit(queue, 1, &si, fences[img_idx]);
    if (submit == VK_ERROR_DEVICE_LOST) {
        if (!recover_device_loss()) fatal_vk("vkQueueSubmit", submit);
        return;
    }
    if (submit != VK_SUCCESS) fatal_vk("vkQueueSubmit", submit);
    VkPresentInfoKHR pi = { .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, .waitSemaphoreCount = 1, .pWaitSemaphores = &sem_render_done, .swapchainCount = 1, .pSwapchains = &swapchain, .pImageIndices = &img_idx };
    VkResult present = vkQueuePresentKHR(queue, &pi);
    if (present == VK_ERROR_DEVICE_LOST) { if (!recover_device_loss()) fatal_vk("vkQueuePresentKHR", present); return; }
    if (present == VK_ERROR_OUT_OF_DATE_KHR || present == VK_SUBOPTIMAL_KHR) { recreate_swapchain(); return; }
    if (present != VK_SUCCESS) fatal_vk("vkQueuePresentKHR", present);
}

bool vk_renderer_init(GLFWwindow* window, const char* vert_spv, const char* frag_spv, const char* font_path, WidgetArray widgets, const CoordinateTransformer* transformer) {
    g_window = window;
    g_widgets = widgets;
    g_vert_spv = vert_spv;
    g_frag_spv = frag_spv;
    g_font_path = font_path;

    if (transformer) {
        g_transformer = *transformer;
    } else {
        coordinate_transformer_init(&g_transformer, 1.0f, 1.0f, (Vec2){0.0f, 0.0f});
    }

    create_instance();
    res = glfwCreateWindowSurface(instance, g_window, NULL, &surface);
    if (res != VK_SUCCESS) return false;

    pick_physical_and_create_device();
    create_swapchain_and_views(VK_NULL_HANDLE);
    create_render_pass();
    create_descriptor_layout();
    create_pipeline(g_vert_spv, g_frag_spv);
    create_cmds_and_sync();

    build_font_atlas();
    create_font_texture();
    create_descriptor_pool_and_set();
    build_vertices_from_widgets();
    return true;
}

void vk_renderer_update_transformer(const CoordinateTransformer* transformer) {
    if (!transformer) return;
    g_transformer = *transformer;
    g_transformer.viewport_size = (Vec2){(float)swapchain_extent.width, (float)swapchain_extent.height};
}

void vk_renderer_draw_frame(void) {
    draw_frame();
}

void vk_renderer_cleanup(void) {
    if (device) vkDeviceWaitIdle(device);
    free(atlas);
    atlas = NULL;
    free(ttf_buffer);
    ttf_buffer = NULL;
    free(vtx_buf);
    vtx_buf = NULL;
    vtx_capacity = 0;
    destroy_device_resources();
    if (device) { vkDestroyDevice(device, NULL); device = VK_NULL_HANDLE; }
    if (surface) { vkDestroySurfaceKHR(instance, surface, NULL); surface = VK_NULL_HANDLE; }
    if (instance) { vkDestroyInstance(instance, NULL); instance = VK_NULL_HANDLE; }
}

