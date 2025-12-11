#define _POSIX_C_SOURCE 200809L
#include "render/vulkan/vulkan_renderer.h"

#include "vulkan/vulkan.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "coordinate_systems/coordinate_systems.h"
#include "platform/platform.h"
#include "render/common/render_composition.h"
#include "render/common/ui_mesh_builder.h"
#include "ui/ui_config.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

typedef struct { float viewport[2]; } ViewConstants;

/* Vertex format for GUI: pos.xyz, uv.xy, use_tex, color.rgba */
typedef struct { float px, py, pz; float u, v; float use_tex; float r, g, b, a; } Vtx;
typedef struct {
    float z;
    size_t order;
    Vtx vertices[6];
} Primitive;
typedef enum {
    FRAME_AVAILABLE,
    FRAME_FILLING,
    FRAME_SUBMITTED,
} FrameStage;

typedef struct {
    UiVertex *background_vertices;
    UiTextVertex *text_vertices;
    Vtx *vertices;
    size_t background_capacity;
    size_t text_capacity;
    size_t vertex_capacity;
} FrameCpuArena;

typedef struct {
    FrameCpuArena cpu;
    VkBuffer vertex_buffer;
    VkDeviceMemory vertex_memory;
    VkDeviceSize vertex_capacity;
    size_t vertex_count;
    FrameStage stage;
    VkFence inflight_fence;
} FrameResources;

typedef struct VulkanRendererState {
    PlatformWindow* window;
    PlatformSurface* platform_surface;
    WidgetArray widgets;
    VkInstance g_instance;
    VkPhysicalDevice g_physical;
    VkDevice g_device;
    uint32_t g_graphics_family;
    VkQueue g_queue;
    VkSurfaceKHR g_surface;
    VkSwapchainKHR g_swapchain;
    const char* vert_spv;
    const char* frag_spv;
    const char* font_path;
    uint32_t g_swapchain_img_count;
    VkImage* g_swapchain_imgs;
    VkImageView* g_swapchain_imgviews;
    VkFormat g_swapchain_format;
    VkExtent2D g_swapchain_extent;
    VkBool32 g_swapchain_supports_blend;
    VkRenderPass g_render_pass;
    VkPipelineLayout g_pipeline_layout;
    VkPipeline g_pipeline;
    VkCommandPool g_cmdpool;
    VkCommandBuffer* g_cmdbuffers;
    VkFramebuffer* g_framebuffers;
    VkResult g_res;
    VkSemaphore g_sem_img_avail;
    VkSemaphore g_sem_render_done;
    VkFence* g_fences;
    FrameResources g_frame_resources[2];
    uint32_t g_current_frame_cursor;
    int* g_image_frame_owner;
    VkImage g_depth_image;
    VkDeviceMemory g_depth_memory;
    VkImageView g_depth_image_view;
    VkFormat g_depth_format;
    VkImage g_font_image;
    VkDeviceMemory g_font_image_mem;
    VkImageView g_font_image_view;
    VkSampler g_font_sampler;
    VkDescriptorSetLayout g_descriptor_layout;
    VkDescriptorPool g_descriptor_pool;
    VkDescriptorSet g_descriptor_set;
    CoordinateSystem2D transformer;
    RenderLogger* logger;
    bool (*get_required_instance_extensions)(const char*** names, uint32_t* count);
    bool (*create_surface)(PlatformWindow* window, VkInstance instance, const VkAllocationCallbacks* alloc,
                           PlatformSurface* out_surface);
    void (*destroy_surface)(VkInstance instance, const VkAllocationCallbacks* alloc, PlatformSurface* surface);
    PlatformWindowSize (*get_framebuffer_size)(PlatformWindow* window);
    void (*wait_events)(void);
} VulkanRendererState;

static VulkanRendererState g_vk = {0};
static RendererBackend g_vulkan_backend;

#define g_window (g_vk.window)
#define g_widgets (g_vk.widgets)
#define g_platform_surface (g_vk.platform_surface)
#define g_get_required_instance_extensions (g_vk.get_required_instance_extensions)
#define g_create_surface (g_vk.create_surface)
#define g_destroy_surface (g_vk.destroy_surface)
#define g_get_framebuffer_size (g_vk.get_framebuffer_size)
#define g_wait_events (g_vk.wait_events)
#define g_instance (g_vk.g_instance)
#define g_physical (g_vk.g_physical)
#define g_device (g_vk.g_device)
#define g_graphics_family (g_vk.g_graphics_family)
#define g_queue (g_vk.g_queue)
#define g_surface (g_vk.g_surface)
#define g_swapchain (g_vk.g_swapchain)
#define g_vert_spv (g_vk.vert_spv)
#define g_frag_spv (g_vk.frag_spv)
#define g_font_path (g_vk.font_path)
#define g_swapchain_img_count (g_vk.g_swapchain_img_count)
#define g_swapchain_imgs (g_vk.g_swapchain_imgs)
#define g_swapchain_imgviews (g_vk.g_swapchain_imgviews)
#define g_swapchain_format (g_vk.g_swapchain_format)
#define g_swapchain_extent (g_vk.g_swapchain_extent)
#define g_swapchain_supports_blend (g_vk.g_swapchain_supports_blend)
#define g_render_pass (g_vk.g_render_pass)
#define g_pipeline_layout (g_vk.g_pipeline_layout)
#define g_pipeline (g_vk.g_pipeline)
#define g_cmdpool (g_vk.g_cmdpool)
#define g_cmdbuffers (g_vk.g_cmdbuffers)
#define g_framebuffers (g_vk.g_framebuffers)
#define g_res (g_vk.g_res)
#define g_sem_img_avail (g_vk.g_sem_img_avail)
#define g_sem_render_done (g_vk.g_sem_render_done)
#define g_fences (g_vk.g_fences)
#define g_frame_resources (g_vk.g_frame_resources)
#define g_current_frame_cursor (g_vk.g_current_frame_cursor)
#define g_image_frame_owner (g_vk.g_image_frame_owner)
#define g_depth_image (g_vk.g_depth_image)
#define g_depth_memory (g_vk.g_depth_memory)
#define g_depth_image_view (g_vk.g_depth_image_view)
#define g_depth_format (g_vk.g_depth_format)
#define g_font_image (g_vk.g_font_image)
#define g_font_image_mem (g_vk.g_font_image_mem)
#define g_font_image_view (g_vk.g_font_image_view)
#define g_font_sampler (g_vk.g_font_sampler)
#define g_descriptor_layout (g_vk.g_descriptor_layout)
#define g_descriptor_pool (g_vk.g_descriptor_pool)
#define g_descriptor_set (g_vk.g_descriptor_set)
#define g_transformer (g_vk.transformer)
#define g_logger (g_vk.logger)

enum {
    LAYER_STRIDE = 16,
    Z_LAYER_BORDER = 0,
    Z_LAYER_FILL = 1,
    Z_LAYER_SLIDER_TRACK = 2,
    Z_LAYER_SLIDER_FILL = 3,
    Z_LAYER_SLIDER_KNOB = 4,
    Z_LAYER_TEXT = 5,
    Z_LAYER_SCROLLBAR_TRACK = 14,
    Z_LAYER_SCROLLBAR_THUMB = 15,
};


static VkFormat choose_depth_format(void)
{
    VkFormat candidates[] = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D16_UNORM,
    };

    for (size_t i = 0; i < sizeof(candidates)/sizeof(candidates[0]); ++i) {
        VkFormat format = candidates[i];
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(g_physical, format, &props);
        if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            return format;
        }
    }

    return VK_FORMAT_UNDEFINED;
}

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
    case VK_ERROR_DEVICE_LOST: return "The GPU stopped responding or was reset; usually caused by g_device removal or timeout.";
    case VK_ERROR_MEMORY_MAP_FAILED: return "Mapping the requested memory range failed (invalid offset/size or unsupported).";
    case VK_ERROR_LAYER_NOT_PRESENT: return "Requested validation layer is not available on this system.";
    case VK_ERROR_EXTENSION_NOT_PRESENT: return "Requested Vulkan extension is not supported by the implementation.";
    case VK_ERROR_FEATURE_NOT_PRESENT: return "A required g_device feature is unavailable on the selected GPU.";
    case VK_ERROR_INCOMPATIBLE_DRIVER: return "The installed driver does not support the requested Vulkan version.";
    case VK_ERROR_TOO_MANY_OBJECTS: return "Implementation-specific object limit exceeded (try freeing unused resources).";
    case VK_ERROR_FORMAT_NOT_SUPPORTED: return "Chosen image/format combination is unsupported for the requested usage.";
    case VK_ERROR_FRAGMENTED_POOL: return "Pool allocation failed because the pool became internally fragmented.";
    case VK_ERROR_OUT_OF_POOL_MEMORY: return "Descriptor or command pool cannot satisfy the allocation request.";
    case VK_ERROR_INVALID_EXTERNAL_HANDLE: return "External handle provided is not valid for this driver or platform.";
    case VK_ERROR_FRAGMENTATION: return "Allocation failed due to excessive fragmentation of the available memory.";
    case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS: return "Opaque capture address is invalid or already in use.";
    case VK_ERROR_SURFACE_LOST_KHR: return "The presentation g_surface became invalid (resized, moved, or destroyed).";
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "Surface creation failed because the window is already bound to another g_surface.";
    case VK_ERROR_OUT_OF_DATE_KHR: return "Swapchain no longer matches the g_surface; recreate g_swapchain to continue.";
    case VK_SUBOPTIMAL_KHR: return "Swapchain is still usable but no longer matches the g_surface optimally (consider recreating).";
    case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "Requested display configuration is incompatible with the selected display.";
    case VK_ERROR_VALIDATION_FAILED_EXT: return "Validation layers found an error; check validation output for details.";
    case VK_ERROR_INVALID_SHADER_NV: return "Shader failed to compile or link for the driver; inspect SPIR-V or compile options.";
    case VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR: return "Requested image usage flags are unsupported for this g_surface format.";
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

static double vk_now_ms(void) {
#ifdef CLOCK_MONOTONIC
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
#else
    return platform_get_time_ms();
#endif
}

static void vk_log_command(const char* command, const char* params, double start_ms) {
    if (!g_logger) return;
    render_logger_log(g_logger, command, params, vk_now_ms() - start_ms);
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

    printf("Using GPU: %s (%s) vendor=0x%04x g_device=0x%04x driver=0x%x api=%u.%u.%u\n",
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
    /* request platform extensions */
    uint32_t extc = 0; const char** exts = NULL;
    if (!g_get_required_instance_extensions || !g_get_required_instance_extensions(&exts, &extc)) {
        fatal("Failed to query platform Vulkan extensions");
    }
    ici.enabledExtensionCount = extc; ici.ppEnabledExtensionNames = exts;
    double start = vk_now_ms();
    g_res = vkCreateInstance(&ici, NULL, &g_instance);
    vk_log_command("vkCreateInstance", "application", start);
    if (g_res != VK_SUCCESS) fatal_vk("vkCreateInstance", g_res);
}

static void pick_physical_and_create_device(void) {
    uint32_t pc = 0; vkEnumeratePhysicalDevices(g_instance, &pc, NULL); if (pc == 0) fatal("No g_physical dev");
    VkPhysicalDevice* list = malloc(sizeof(VkPhysicalDevice) * pc); vkEnumeratePhysicalDevices(g_instance, &pc, list);
    g_physical = list[0]; free(list);

    log_gpu_info(g_physical);

    /* find g_queue family with graphics + present */
    uint32_t qcount = 0; vkGetPhysicalDeviceQueueFamilyProperties(g_physical, &qcount, NULL);
    VkQueueFamilyProperties* qprops = malloc(sizeof(VkQueueFamilyProperties) * qcount); vkGetPhysicalDeviceQueueFamilyProperties(g_physical, &qcount, qprops);
    int found = -1;
    for (uint32_t i = 0; i < qcount; i++) {
        VkBool32 pres = false; vkGetPhysicalDeviceSurfaceSupportKHR(g_physical, i, g_surface, &pres);
        if ((qprops[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && pres) { found = (int)i; break; }
    }
    if (found < 0) fatal("No suitable g_queue family");
    g_graphics_family = (uint32_t)found;

    float prio = 1.0f;
    VkDeviceQueueCreateInfo qci = { .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueFamilyIndex = g_graphics_family, .queueCount = 1, .pQueuePriorities = &prio };
    const char* dev_ext[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    VkDeviceCreateInfo dci = { .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, .queueCreateInfoCount = 1, .pQueueCreateInfos = &qci, .enabledExtensionCount = 1, .ppEnabledExtensionNames = dev_ext };
    g_res = vkCreateDevice(g_physical, &dci, NULL, &g_device);
    if (g_res != VK_SUCCESS) fatal_vk("vkCreateDevice", g_res);
    vkGetDeviceQueue(g_device, g_graphics_family, 0, &g_queue);
}

typedef struct {
    VkBool32 color_attachment;
    VkBool32 blend;
} FormatSupport;

static FormatSupport get_format_support(VkFormat fmt) {
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(g_physical, fmt, &props);
    FormatSupport support = {
        .color_attachment = (props.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) != 0,
        .blend = (props.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT) != 0
    };
    return support;
}

/* create g_swapchain and imageviews */
static void create_swapchain_and_views(VkSwapchainKHR old_swapchain) {
    /* choose format */
    uint32_t fc = 0; vkGetPhysicalDeviceSurfaceFormatsKHR(g_physical, g_surface, &fc, NULL); if (fc == 0) fatal("no g_surface formats");
    VkSurfaceFormatKHR* fmts = malloc(sizeof(VkSurfaceFormatKHR) * fc); vkGetPhysicalDeviceSurfaceFormatsKHR(g_physical, g_surface, &fc, fmts);
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

    if (!chosen_support.color_attachment) fatal("no color attachment format for g_swapchain");

    /* Some platforms advertise VK_FORMAT_UNDEFINED to indicate flexibility. Choose a
       concrete format so g_swapchain creation succeeds on implementations that reject
       VK_FORMAT_UNDEFINED. */
    if (chosen_fmt.format == VK_FORMAT_UNDEFINED) {
        chosen_fmt.format = VK_FORMAT_B8G8R8A8_UNORM;
    }
    chosen_support = get_format_support(chosen_fmt.format);
    if (!chosen_support.color_attachment) fatal("g_swapchain format lacks color attachment support");
    g_swapchain_supports_blend = chosen_support.blend;
    g_swapchain_format = chosen_fmt.format;
    free(fmts);

    PlatformWindowSize fb_size = g_get_framebuffer_size ? g_get_framebuffer_size(g_window) : (PlatformWindowSize){0};
    int w = fb_size.width;
    int h = fb_size.height;
    while (w == 0 || h == 0) {
        if (g_wait_events) {
            g_wait_events();
        } else {
            platform_wait_events();
        }
        if (platform_window_should_close(g_window)) return;
        fb_size = g_get_framebuffer_size ? g_get_framebuffer_size(g_window) : (PlatformWindowSize){0};
        w = fb_size.width;
        h = fb_size.height;
    }

    VkSurfaceCapabilitiesKHR caps; vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_physical, g_surface, &caps);

    uint32_t img_count = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && img_count > caps.maxImageCount) img_count = caps.maxImageCount;

    if (caps.currentExtent.width != UINT32_MAX) g_swapchain_extent = caps.currentExtent;
    else {
        uint32_t clamped_w = (uint32_t)w; uint32_t clamped_h = (uint32_t)h;
        if (clamped_w < caps.minImageExtent.width) clamped_w = caps.minImageExtent.width;
        if (clamped_w > caps.maxImageExtent.width) clamped_w = caps.maxImageExtent.width;
        if (clamped_h < caps.minImageExtent.height) clamped_h = caps.minImageExtent.height;
        if (clamped_h > caps.maxImageExtent.height) clamped_h = caps.maxImageExtent.height;
        g_swapchain_extent.width = clamped_w; g_swapchain_extent.height = clamped_h;
    }

    g_transformer.viewport_size = (Vec2){(float)g_swapchain_extent.width, (float)g_swapchain_extent.height};

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
    if (!(caps.supportedUsageFlags & usage)) fatal("g_swapchain color usage unsupported");

    VkSwapchainCreateInfoKHR sci = { .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR, .surface = g_surface, .minImageCount = img_count, .imageFormat = g_swapchain_format, .imageColorSpace = chosen_fmt.colorSpace, .imageExtent = g_swapchain_extent, .imageArrayLayers = 1, .imageUsage = usage, .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE, .preTransform = caps.currentTransform, .compositeAlpha = comp_alpha, .presentMode = VK_PRESENT_MODE_FIFO_KHR, .clipped = VK_TRUE, .oldSwapchain = old_swapchain };
    double swapchain_start = vk_now_ms();
    g_res = vkCreateSwapchainKHR(g_device, &sci, NULL, &g_swapchain);
    vk_log_command("vkCreateSwapchainKHR", "g_swapchain setup", swapchain_start);
    if (g_res != VK_SUCCESS) fatal_vk("vkCreateSwapchainKHR", g_res);
    vkGetSwapchainImagesKHR(g_device, g_swapchain, &g_swapchain_img_count, NULL);
    g_swapchain_imgs = malloc(sizeof(VkImage) * g_swapchain_img_count);
    vkGetSwapchainImagesKHR(g_device, g_swapchain, &g_swapchain_img_count, g_swapchain_imgs);
    g_swapchain_imgviews = malloc(sizeof(VkImageView) * g_swapchain_img_count);
    for (uint32_t i = 0; i < g_swapchain_img_count; i++) {
        VkImageViewCreateInfo ivci = { .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = g_swapchain_imgs[i], .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = g_swapchain_format, .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0,1,0,1 } };
        g_res = vkCreateImageView(g_device, &ivci, NULL, &g_swapchain_imgviews[i]);
        if (g_res != VK_SUCCESS) fatal_vk("vkCreateImageView", g_res);
    }
}

static void destroy_depth_resources(void) {
    if (g_depth_image_view) {
        vkDestroyImageView(g_device, g_depth_image_view, NULL);
        g_depth_image_view = VK_NULL_HANDLE;
    }
    if (g_depth_image) {
        vkDestroyImage(g_device, g_depth_image, NULL);
        g_depth_image = VK_NULL_HANDLE;
    }
    if (g_depth_memory) {
        vkFreeMemory(g_device, g_depth_memory, NULL);
        g_depth_memory = VK_NULL_HANDLE;
    }
}

static void create_depth_resources(void) {
    destroy_depth_resources();

    g_depth_format = choose_depth_format();
    if (g_depth_format == VK_FORMAT_UNDEFINED) {
        fatal("No supported depth format found");
    }

    VkImageCreateInfo image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = g_depth_format,
        .extent = { g_swapchain_extent.width, g_swapchain_extent.height, 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    g_res = vkCreateImage(g_device, &image_info, NULL, &g_depth_image);
    if (g_res != VK_SUCCESS) fatal_vk("vkCreateImage (depth)", g_res);

    VkMemoryRequirements mem_req;
    vkGetImageMemoryRequirements(g_device, g_depth_image, &mem_req);
    VkMemoryAllocateInfo alloc_info = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = mem_req.size };
    VkPhysicalDeviceMemoryProperties mem_props; vkGetPhysicalDeviceMemoryProperties(g_physical, &mem_props);
    uint32_t mem_type = UINT32_MAX;
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
        if ((mem_req.memoryTypeBits & (1u << i)) && (mem_props.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            mem_type = i; break; }
    }
    if (mem_type == UINT32_MAX) fatal("No suitable memory type for depth buffer");
    alloc_info.memoryTypeIndex = mem_type;
    g_res = vkAllocateMemory(g_device, &alloc_info, NULL, &g_depth_memory);
    if (g_res != VK_SUCCESS) fatal_vk("vkAllocateMemory (depth)", g_res);
    vkBindImageMemory(g_device, g_depth_image, g_depth_memory, 0);

    VkImageViewCreateInfo view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = g_depth_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = g_depth_format,
        .subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 }
    };

    g_res = vkCreateImageView(g_device, &view_info, NULL, &g_depth_image_view);
    if (g_res != VK_SUCCESS) fatal_vk("vkCreateImageView (depth)", g_res);
}

/* render pass */
static void create_render_pass(void) {
    if (g_depth_format == VK_FORMAT_UNDEFINED) {
        g_depth_format = choose_depth_format();
        if (g_depth_format == VK_FORMAT_UNDEFINED) fatal("No supported depth format found");
    }

    VkAttachmentDescription attachments[2] = {
        { .format = g_swapchain_format, .samples = VK_SAMPLE_COUNT_1_BIT, .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, .storeOp = VK_ATTACHMENT_STORE_OP_STORE, .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE, .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE, .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED, .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR },
        { .format = g_depth_format, .samples = VK_SAMPLE_COUNT_1_BIT, .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE, .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE, .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE, .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED, .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL }
    };
    VkAttachmentReference color_ref = { .attachment = 0, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkAttachmentReference depth_ref = { .attachment = 1, .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
    VkSubpassDescription sub = { .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS, .colorAttachmentCount = 1, .pColorAttachments = &color_ref, .pDepthStencilAttachment = &depth_ref };
    VkRenderPassCreateInfo rpci = { .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, .attachmentCount = 2, .pAttachments = attachments, .subpassCount = 1, .pSubpasses = &sub };
    g_res = vkCreateRenderPass(g_device, &rpci, NULL, &g_render_pass);
    if (g_res != VK_SUCCESS) fatal_vk("vkCreateRenderPass", g_res);
}

static void create_descriptor_layout(void) {
    VkDescriptorSetLayoutBinding binding = { .binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT };
    VkDescriptorSetLayoutCreateInfo lci = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = 1, .pBindings = &binding };
    g_res = vkCreateDescriptorSetLayout(g_device, &lci, NULL, &g_descriptor_layout);
    if (g_res != VK_SUCCESS) fatal_vk("vkCreateDescriptorSetLayout", g_res);
}

/* create simple g_pipeline from provided SPIR-V files (vertex/fragment) */
static VkShaderModule create_shader_module_from_spv(const char* path) {
    size_t words = 0; uint32_t* code = read_file_bin_u32(path, &words);
    if (!code) fatal("read spv");
    VkShaderModuleCreateInfo smci = { .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize = words * 4, .pCode = code };
    VkShaderModule mod; g_res = vkCreateShaderModule(g_device, &smci, NULL, &mod);
    if (g_res != VK_SUCCESS) fatal_vk("vkCreateShaderModule", g_res);
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
        {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vtx,px) },
        {.location = 1, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(Vtx,u) },
        {.location = 2, .binding = 0, .format = VK_FORMAT_R32_SFLOAT, .offset = offsetof(Vtx,use_tex) },
        {.location = 3, .binding = 0, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = offsetof(Vtx,r) }
    };
    VkPipelineVertexInputStateCreateInfo vxi = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, .vertexBindingDescriptionCount = 1, .pVertexBindingDescriptions = &bind, .vertexAttributeDescriptionCount = 4, .pVertexAttributeDescriptions = attr };

    VkPipelineInputAssemblyStateCreateInfo ia = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST };
    float viewport_w = (g_swapchain_extent.width == 0) ? 1.0f : (float)g_swapchain_extent.width;
    float viewport_h = (g_swapchain_extent.height == 0) ? 1.0f : (float)g_swapchain_extent.height;
    VkViewport vp = { .x = 0, .y = 0, .width = viewport_w, .height = viewport_h, .minDepth = 0.0f, .maxDepth = 1.0f };
    VkRect2D sc = { .offset = {0,0}, .extent = g_swapchain_extent };
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
    VkPipelineColorBlendAttachmentState cbatt = { .blendEnable = g_swapchain_supports_blend, .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA, .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, .colorBlendOp = VK_BLEND_OP_ADD, .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE, .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, .alphaBlendOp = VK_BLEND_OP_ADD, .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT };
    VkPipelineColorBlendStateCreateInfo cb = { .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, .attachmentCount = 1, .pAttachments = &cbatt };
    VkPushConstantRange pcr = { .stageFlags = VK_SHADER_STAGE_VERTEX_BIT, .offset = 0, .size = sizeof(float) * 2 };
    VkPipelineLayoutCreateInfo plci = { .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .setLayoutCount = 1, .pSetLayouts = &g_descriptor_layout, .pushConstantRangeCount = 1, .pPushConstantRanges = &pcr };
    g_res = vkCreatePipelineLayout(g_device, &plci, NULL, &g_pipeline_layout);
    if (g_res != VK_SUCCESS) fatal_vk("vkCreatePipelineLayout", g_res);
    VkGraphicsPipelineCreateInfo gpci = { .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, .stageCount = 2, .pStages = stages, .pVertexInputState = &vxi, .pInputAssemblyState = &ia, .pViewportState = &vpci, .pRasterizationState = &rs, .pMultisampleState = &ms, .pDepthStencilState = &ds, .pColorBlendState = &cb, .layout = g_pipeline_layout, .renderPass = g_render_pass, .subpass = 0 };
    g_res = vkCreateGraphicsPipelines(g_device, VK_NULL_HANDLE, 1, &gpci, NULL, &g_pipeline);
    if (g_res != VK_SUCCESS) fatal_vk("vkCreateGraphicsPipelines", g_res);
    vkDestroyShaderModule(g_device, vs, NULL); vkDestroyShaderModule(g_device, fs, NULL);
}

/* command pool/buffers/g_framebuffers/semaphores */
static void create_cmds_and_sync(void) {
    if (g_sem_img_avail) { vkDestroySemaphore(g_device, g_sem_img_avail, NULL); g_sem_img_avail = VK_NULL_HANDLE; }
    if (g_sem_render_done) { vkDestroySemaphore(g_device, g_sem_render_done, NULL); g_sem_render_done = VK_NULL_HANDLE; }
    VkCommandPoolCreateInfo cpci = { .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .queueFamilyIndex = g_graphics_family, .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT };
    g_res = vkCreateCommandPool(g_device, &cpci, NULL, &g_cmdpool);
    if (g_res != VK_SUCCESS) fatal_vk("vkCreateCommandPool", g_res);
    g_cmdbuffers = malloc(sizeof(VkCommandBuffer) * g_swapchain_img_count);
    VkCommandBufferAllocateInfo cbai = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = g_cmdpool, .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = g_swapchain_img_count };
    g_res = vkAllocateCommandBuffers(g_device, &cbai, g_cmdbuffers);
    if (g_res != VK_SUCCESS) fatal_vk("vkAllocateCommandBuffers", g_res);

    g_framebuffers = malloc(sizeof(VkFramebuffer) * g_swapchain_img_count);
    for (uint32_t i = 0; i < g_swapchain_img_count; i++) {
        VkImageView attachments[2] = { g_swapchain_imgviews[i], g_depth_image_view };
        VkFramebufferCreateInfo fci = { .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, .renderPass = g_render_pass, .attachmentCount = 2, .pAttachments = attachments, .width = g_swapchain_extent.width, .height = g_swapchain_extent.height, .layers = 1 };
        g_res = vkCreateFramebuffer(g_device, &fci, NULL, &g_framebuffers[i]);
        if (g_res != VK_SUCCESS) fatal_vk("vkCreateFramebuffer", g_res);
    }
    VkSemaphoreCreateInfo sci = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO }; vkCreateSemaphore(g_device, &sci, NULL, &g_sem_img_avail); vkCreateSemaphore(g_device, &sci, NULL, &g_sem_render_done);
    g_fences = malloc(sizeof(VkFence) * g_swapchain_img_count);
    for (uint32_t i = 0; i < g_swapchain_img_count; i++) {
        VkFenceCreateInfo fci = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT };
        vkCreateFence(g_device, &fci, NULL, &g_fences[i]);
    }

    free(g_image_frame_owner);
    g_image_frame_owner = calloc(g_swapchain_img_count, sizeof(int));
    if (g_image_frame_owner) {
        for (uint32_t i = 0; i < g_swapchain_img_count; ++i) {
            g_image_frame_owner[i] = -1;
        }
    }
    g_current_frame_cursor = 0;
}

static void cleanup_swapchain(bool keep_swapchain_handle) {
    if (g_cmdbuffers) {
        vkFreeCommandBuffers(g_device, g_cmdpool, g_swapchain_img_count, g_cmdbuffers);
        free(g_cmdbuffers);
        g_cmdbuffers = NULL;
    }
    if (g_cmdpool) {
        vkDestroyCommandPool(g_device, g_cmdpool, NULL);
        g_cmdpool = VK_NULL_HANDLE;
    }
    if (g_framebuffers) {
        for (uint32_t i = 0; i < g_swapchain_img_count; i++) vkDestroyFramebuffer(g_device, g_framebuffers[i], NULL);
        free(g_framebuffers);
        g_framebuffers = NULL;
    }
    if (g_fences) {
        for (uint32_t i = 0; i < g_swapchain_img_count; i++) vkDestroyFence(g_device, g_fences[i], NULL);
        free(g_fences);
        g_fences = NULL;
    }
    for (size_t i = 0; i < 2; ++i) {
        g_frame_resources[i].stage = FRAME_AVAILABLE;
        g_frame_resources[i].inflight_fence = VK_NULL_HANDLE;
    }
    free(g_image_frame_owner);
    g_image_frame_owner = NULL;
    if (g_swapchain_imgviews) {
        for (uint32_t i = 0; i < g_swapchain_img_count; i++) vkDestroyImageView(g_device, g_swapchain_imgviews[i], NULL);
        free(g_swapchain_imgviews);
        g_swapchain_imgviews = NULL;
    }
    if (g_swapchain_imgs) {
        free(g_swapchain_imgs);
        g_swapchain_imgs = NULL;
    }
    destroy_depth_resources();
    if (!keep_swapchain_handle && g_swapchain) {
        double destroy_start = vk_now_ms();
        vkDestroySwapchainKHR(g_device, g_swapchain, NULL);
        vk_log_command("vkDestroySwapchainKHR", "cleanup", destroy_start);
        g_swapchain = VK_NULL_HANDLE;
    }
    if (g_pipeline) {
        vkDestroyPipeline(g_device, g_pipeline, NULL);
        g_pipeline = VK_NULL_HANDLE;
    }
    if (g_pipeline_layout) {
        vkDestroyPipelineLayout(g_device, g_pipeline_layout, NULL);
        g_pipeline_layout = VK_NULL_HANDLE;
    }
    if (g_render_pass) {
        vkDestroyRenderPass(g_device, g_render_pass, NULL);
        g_render_pass = VK_NULL_HANDLE;
    }
    g_swapchain_img_count = 0;
}

static void destroy_device_resources(void) {
    cleanup_swapchain(false);

    if (g_descriptor_pool) { vkDestroyDescriptorPool(g_device, g_descriptor_pool, NULL); g_descriptor_pool = VK_NULL_HANDLE; }
    if (g_descriptor_layout) { vkDestroyDescriptorSetLayout(g_device, g_descriptor_layout, NULL); g_descriptor_layout = VK_NULL_HANDLE; }
    if (g_font_sampler) { vkDestroySampler(g_device, g_font_sampler, NULL); g_font_sampler = VK_NULL_HANDLE; }
    if (g_font_image_view) { vkDestroyImageView(g_device, g_font_image_view, NULL); g_font_image_view = VK_NULL_HANDLE; }
    if (g_font_image) { vkDestroyImage(g_device, g_font_image, NULL); g_font_image = VK_NULL_HANDLE; }
    if (g_font_image_mem) { vkFreeMemory(g_device, g_font_image_mem, NULL); g_font_image_mem = VK_NULL_HANDLE; }
    for (size_t i = 0; i < 2; ++i) {
        if (g_frame_resources[i].vertex_buffer) { vkDestroyBuffer(g_device, g_frame_resources[i].vertex_buffer, NULL); g_frame_resources[i].vertex_buffer = VK_NULL_HANDLE; }
        if (g_frame_resources[i].vertex_memory) { vkFreeMemory(g_device, g_frame_resources[i].vertex_memory, NULL); g_frame_resources[i].vertex_memory = VK_NULL_HANDLE; }
        g_frame_resources[i].vertex_capacity = 0;
        g_frame_resources[i].vertex_count = 0;
        g_frame_resources[i].stage = FRAME_AVAILABLE;
        g_frame_resources[i].inflight_fence = VK_NULL_HANDLE;
    }
    if (g_sem_img_avail) { vkDestroySemaphore(g_device, g_sem_img_avail, NULL); g_sem_img_avail = VK_NULL_HANDLE; }
    if (g_sem_render_done) { vkDestroySemaphore(g_device, g_sem_render_done, NULL); g_sem_render_done = VK_NULL_HANDLE; }
}

static void recreate_instance_and_surface(void) {
    if (g_platform_surface && g_destroy_surface && g_instance) {
        g_destroy_surface(g_instance, NULL, g_platform_surface);
    } else if (g_surface && g_instance) {
        vkDestroySurfaceKHR(g_instance, g_surface, NULL);
    }
    g_surface = VK_NULL_HANDLE;
    if (g_instance) { vkDestroyInstance(g_instance, NULL); g_instance = VK_NULL_HANDLE; }

    create_instance();
    if (!g_create_surface || !g_platform_surface ||
        !g_create_surface(g_window, g_instance, NULL, g_platform_surface)) {
        fatal("Failed to recreate platform surface");
    }
    g_surface = (VkSurfaceKHR)(g_platform_surface ? g_platform_surface->handle : NULL);
}

static void recreate_swapchain(void) {
    vkDeviceWaitIdle(g_device);

    VkSwapchainKHR old_swapchain = g_swapchain;
    cleanup_swapchain(true);

    create_swapchain_and_views(old_swapchain);
    if (!g_swapchain) {
        if (old_swapchain) {
            double destroy_start = vk_now_ms();
            vkDestroySwapchainKHR(g_device, old_swapchain, NULL);
            vk_log_command("vkDestroySwapchainKHR", "old g_swapchain", destroy_start);
        }
        return;
    }

    create_depth_resources();
    create_render_pass();
    create_pipeline(g_vert_spv, g_frag_spv);
    create_cmds_and_sync();

    if (old_swapchain) vkDestroySwapchainKHR(g_device, old_swapchain, NULL);
}

/* create a simple host-visible vertex buffer */
static uint32_t find_mem_type(uint32_t typeFilter, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties mp; vkGetPhysicalDeviceMemoryProperties(g_physical, &mp);
    for (uint32_t i = 0; i < mp.memoryTypeCount; i++) {
        if ((typeFilter & (1u << i)) && (mp.memoryTypes[i].propertyFlags & props) == props) return i;
    }
    fatal("no mem type");
    return 0;
}
static void create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkBuffer* out_buf, VkDeviceMemory* out_mem) {
    VkBufferCreateInfo bci = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = size, .usage = usage, .sharingMode = VK_SHARING_MODE_EXCLUSIVE };
    g_res = vkCreateBuffer(g_device, &bci, NULL, out_buf);
    if (g_res != VK_SUCCESS) fatal_vk("vkCreateBuffer", g_res);
    VkMemoryRequirements mr; vkGetBufferMemoryRequirements(g_device, *out_buf, &mr);
    VkMemoryAllocateInfo mai = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = mr.size, .memoryTypeIndex = find_mem_type(mr.memoryTypeBits, props) };
    g_res = vkAllocateMemory(g_device, &mai, NULL, out_mem);
    if (g_res != VK_SUCCESS) fatal_vk("vkAllocateMemory", g_res);
    vkBindBufferMemory(g_device, *out_buf, *out_mem, 0);
}
static VkCommandBuffer begin_single_time_commands(void) {
    VkCommandBufferAllocateInfo ai = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = g_cmdpool, .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 1 };
    VkCommandBuffer cb;
    vkAllocateCommandBuffers(g_device, &ai, &cb);
    VkCommandBufferBeginInfo bi = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
    vkBeginCommandBuffer(cb, &bi);
    return cb;
}
static void end_single_time_commands(VkCommandBuffer cb) {
    vkEndCommandBuffer(cb);
    VkSubmitInfo si = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &cb };
    vkQueueSubmit(g_queue, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(g_queue);
    vkFreeCommandBuffers(g_device, g_cmdpool, 1, &cb);
}
static bool create_vertex_buffer(FrameResources *frame, size_t bytes) {
    if (frame->vertex_buffer != VK_NULL_HANDLE && frame->vertex_capacity >= bytes) {
        return true;
    }

    if (frame->vertex_buffer) {
        vkDestroyBuffer(g_device, frame->vertex_buffer, NULL);
        frame->vertex_buffer = VK_NULL_HANDLE;
    }
    if (frame->vertex_memory) {
        vkFreeMemory(g_device, frame->vertex_memory, NULL);
        frame->vertex_memory = VK_NULL_HANDLE;
        frame->vertex_capacity = 0;
    }

    VkBuffer new_buffer = VK_NULL_HANDLE;
    VkDeviceMemory new_memory = VK_NULL_HANDLE;
    VkResult create = vkCreateBuffer(g_device, &(VkBufferCreateInfo){ .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = bytes, .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, .sharingMode = VK_SHARING_MODE_EXCLUSIVE }, NULL, &new_buffer);
    if (create != VK_SUCCESS) {
        fprintf(stderr, "vkCreateBuffer failed for vertex buffer: %s\n", vk_result_name(create));
        return false;
    }

    VkMemoryRequirements mr; vkGetBufferMemoryRequirements(g_device, new_buffer, &mr);
    VkMemoryAllocateInfo mai = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = mr.size, .memoryTypeIndex = find_mem_type(mr.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) };
    VkResult alloc = vkAllocateMemory(g_device, &mai, NULL, &new_memory);
    if (alloc != VK_SUCCESS) {
        fprintf(stderr, "vkAllocateMemory failed for vertex buffer: %s\n", vk_result_name(alloc));
        vkDestroyBuffer(g_device, new_buffer, NULL);
        return false;
    }

    vkBindBufferMemory(g_device, new_buffer, new_memory, 0);
    frame->vertex_buffer = new_buffer;
    frame->vertex_memory = new_memory;
    frame->vertex_capacity = bytes;
    return true;
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
static bool ensure_cpu_vertex_capacity(FrameCpuArena *cpu, size_t background_vertices, size_t text_vertices, size_t final_vertices) {
    UiVertex *new_background = cpu->background_vertices;
    UiTextVertex *new_text = cpu->text_vertices;
    Vtx *new_final = cpu->vertices;

    if (background_vertices > cpu->background_capacity) {
        size_t cap = cpu->background_capacity == 0 ? background_vertices : cpu->background_capacity * 2;
        while (cap < background_vertices) cap *= 2;
        new_background = realloc(cpu->background_vertices, cap * sizeof(UiVertex));
        if (!new_background) return false;
        cpu->background_capacity = cap;
    }

    if (text_vertices > cpu->text_capacity) {
        size_t cap = cpu->text_capacity == 0 ? text_vertices : cpu->text_capacity * 2;
        while (cap < text_vertices) cap *= 2;
        new_text = realloc(cpu->text_vertices, cap * sizeof(UiTextVertex));
        if (!new_text) return false;
        cpu->text_capacity = cap;
    }

    if (final_vertices > cpu->vertex_capacity) {
        size_t cap = cpu->vertex_capacity == 0 ? final_vertices : cpu->vertex_capacity * 2;
        while (cap < final_vertices) cap *= 2;
        new_final = realloc(cpu->vertices, cap * sizeof(Vtx));
        if (!new_final) return false;
        cpu->vertex_capacity = cap;
    }

    cpu->background_vertices = new_background;
    cpu->text_vertices = new_text;
    cpu->vertices = new_final;
    return true;
}

static bool ensure_vtx_capacity(FrameCpuArena *cpu, size_t required)
{
    return ensure_cpu_vertex_capacity(cpu, cpu->background_capacity, cpu->text_capacity, required);
}

static bool upload_vertices(FrameResources *frame) {
    if (frame->vertex_count == 0) {
        if (frame->vertex_buffer) {
            vkDestroyBuffer(g_device, frame->vertex_buffer, NULL);
            frame->vertex_buffer = VK_NULL_HANDLE;
        }
        if (frame->vertex_memory) {
            vkFreeMemory(g_device, frame->vertex_memory, NULL);
            frame->vertex_memory = VK_NULL_HANDLE;
        }
        frame->vertex_capacity = 0;
        return true;
    }

    size_t bytes = frame->vertex_count * sizeof(Vtx);
    if (!create_vertex_buffer(frame, bytes)) {
        return false;
    }
    void* dst = NULL;
    VkResult map = vkMapMemory(g_device, frame->vertex_memory, 0, bytes, 0, &dst);
    if (map != VK_SUCCESS) {
        fprintf(stderr, "Failed to map vertex memory: %s\n", vk_result_name(map));
        return false;
    }
    memcpy(dst, frame->cpu.vertices, bytes);
    vkUnmapMemory(g_device, frame->vertex_memory);
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

static float snap_to_pixel(float value) {
    return floorf(value + 0.5f);
}

static int apply_clip_rect_to_bounds(const Rect *clip, const Rect *input, Rect *out) {
    if (!input || !out) return 0;
    *out = *input;
    if (!clip) return 1;

    float clip_x0 = ceilf(clip->x);
    float clip_y0 = ceilf(clip->y);
    float clip_x1 = floorf(clip->x + clip->w);
    float clip_y1 = floorf(clip->y + clip->h);

    float x0 = fmaxf(input->x, clip_x0);
    float y0 = fmaxf(input->y, clip_y0);
    float x1 = fminf(input->x + input->w, clip_x1);
    float y1 = fminf(input->y + input->h, clip_y1);
    if (x1 <= x0 || y1 <= y0) return 0;
    out->x = x0;
    out->y = y0;
    out->w = x1 - x0;
    out->h = y1 - y0;
    return 1;
}

static int apply_clip_rect(const Widget* widget, const Rect* input, Rect* out) {
    return apply_clip_rect_to_bounds(widget && widget->has_clip ? &widget->clip : NULL, input, out);
}

static void apply_widget_clip_to_view_model(const Widget *widget, ViewModel *vm) {
    if (!widget || !vm) return;
    if (!widget->has_clip) return;
    vm->has_clip = 1;
    vm->clip.origin = (Vec2){widget->clip.x, widget->clip.y};
    vm->clip.size = (Vec2){widget->clip.w, widget->clip.h};
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

static bool ensure_view_model_capacity(ViewModel **view_models, size_t *capacity, size_t required, bool *ok)
{
    if (!ok || !*ok) {
        return false;
    }
    if (required <= *capacity) {
        return true;
    }

    size_t new_capacity = *capacity == 0 ? required : (*capacity * 2);
    while (new_capacity < required) {
        new_capacity *= 2;
    }

    ViewModel *expanded = realloc(*view_models, new_capacity * sizeof(ViewModel));
    if (!expanded) {
        *ok = false;
        return false;
    }

    memset(expanded + *capacity, 0, (new_capacity - *capacity) * sizeof(ViewModel));
    *view_models = expanded;
    *capacity = new_capacity;
    return true;
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
    g_res = vkCreateImage(g_device, &ici, NULL, &g_font_image);
    if (g_res != VK_SUCCESS) fatal_vk("vkCreateImage", g_res);
    VkMemoryRequirements mr; vkGetImageMemoryRequirements(g_device, g_font_image, &mr);
    VkMemoryAllocateInfo mai = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = mr.size, .memoryTypeIndex = find_mem_type(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) };
    g_res = vkAllocateMemory(g_device, &mai, NULL, &g_font_image_mem);
    if (g_res != VK_SUCCESS) fatal_vk("vkAllocateMemory", g_res);
    vkBindImageMemory(g_device, g_font_image, g_font_image_mem, 0);

    VkBuffer staging = VK_NULL_HANDLE; VkDeviceMemory staging_mem = VK_NULL_HANDLE;
    create_buffer(atlas_w * atlas_h, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &staging, &staging_mem);
    void* mapped = NULL; vkMapMemory(g_device, staging_mem, 0, VK_WHOLE_SIZE, 0, &mapped); memcpy(mapped, atlas, atlas_w * atlas_h); vkUnmapMemory(g_device, staging_mem);

    transition_image_layout(g_font_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copy_buffer_to_image(staging, g_font_image, (uint32_t)atlas_w, (uint32_t)atlas_h);
    transition_image_layout(g_font_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(g_device, staging, NULL);
    vkFreeMemory(g_device, staging_mem, NULL);

    VkImageViewCreateInfo ivci = { .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = g_font_image, .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = VK_FORMAT_R8_UNORM, .subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1 } };
    g_res = vkCreateImageView(g_device, &ivci, NULL, &g_font_image_view);
    if (g_res != VK_SUCCESS) fatal_vk("vkCreateImageView", g_res);

    VkSamplerCreateInfo sci = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .magFilter = VK_FILTER_LINEAR, .minFilter = VK_FILTER_LINEAR, .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK, .unnormalizedCoordinates = VK_FALSE, .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST };
    g_res = vkCreateSampler(g_device, &sci, NULL, &g_font_sampler);
    if (g_res != VK_SUCCESS) fatal_vk("vkCreateSampler", g_res);
}

static void create_descriptor_pool_and_set(void) {
    VkDescriptorPoolSize pool = { .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1 };
    VkDescriptorPoolCreateInfo dpci = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, .maxSets = 1, .poolSizeCount = 1, .pPoolSizes = &pool };
    g_res = vkCreateDescriptorPool(g_device, &dpci, NULL, &g_descriptor_pool);
    if (g_res != VK_SUCCESS) fatal_vk("vkCreateDescriptorPool", g_res);

    VkDescriptorSetAllocateInfo dsai = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, .descriptorPool = g_descriptor_pool, .descriptorSetCount = 1, .pSetLayouts = &g_descriptor_layout };
    g_res = vkAllocateDescriptorSets(g_device, &dsai, &g_descriptor_set);
    if (g_res != VK_SUCCESS) fatal_vk("vkAllocateDescriptorSets", g_res);

    VkDescriptorImageInfo dii = { .sampler = g_font_sampler, .imageView = g_font_image_view, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    VkWriteDescriptorSet w = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = g_descriptor_set, .dstBinding = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &dii };
    vkUpdateDescriptorSets(g_device, 1, &w, 0, NULL);
}

/* build vtxs from g_widgets each frame */
static bool build_vertices_from_widgets(FrameResources *frame) {
    if (!frame) return false;
    frame->vertex_count = 0;

    if (g_widgets.count == 0 || g_swapchain_extent.width == 0 || g_swapchain_extent.height == 0) {
        return true;
    }

    size_t slider_extras = 0;
    for (size_t i = 0; i < g_widgets.count; ++i) {
        if (g_widgets.items[i].type == W_HSLIDER) {
            slider_extras += 2;
        }
    }

    /* Each widget can contribute up to: 4 border segments + 1 fill + 2 scrollbar parts. */
    size_t view_model_capacity = g_widgets.count * 8 + slider_extras;
    ViewModel *view_models = calloc(view_model_capacity, sizeof(ViewModel));
    GlyphQuadArray glyph_quads = {0};
    bool view_models_ok = view_models != NULL;
    if (!view_models) {
        return false;
    }

    size_t *widget_ordinals = g_widgets.count > 0 ? calloc(g_widgets.count, sizeof(size_t)) : NULL;
    if (g_widgets.count > 0 && !widget_ordinals) {
        free(view_models);
        return false;
    }

    size_t view_model_count = 0;
    for (size_t i = 0; i < g_widgets.count; ++i) {
        const Widget *widget = &g_widgets.items[i];
        size_t widget_order = g_widgets.count - 1 - i;
        int base_z = widget->z_index * LAYER_STRIDE;
        int scrollbar_z = (widget->z_index + UI_Z_ORDER_SCALE) * LAYER_STRIDE;
        int text_z = base_z + Z_LAYER_TEXT;

        float scroll_offset = widget->scroll_static ? 0.0f : widget->scroll_offset;
        float snapped_scroll_pixels = -snap_to_pixel(scroll_offset * g_transformer.dpi_scale);
        float effective_offset = snapped_scroll_pixels / g_transformer.dpi_scale;
        Rect widget_rect = { widget->rect.x, widget->rect.y + effective_offset, widget->rect.w, widget->rect.h };
        Rect inner_rect = widget_rect;
        if (widget->border_thickness > 0.0f) {
            float b = widget->border_thickness;
            inner_rect.x += b;
            inner_rect.y += b;
            inner_rect.w -= b * 2.0f;
            inner_rect.h -= b * 2.0f;
            if (inner_rect.w < 0.0f) inner_rect.w = 0.0f;
            if (inner_rect.h < 0.0f) inner_rect.h = 0.0f;

            Rect borders[4] = {
                { widget_rect.x, widget_rect.y, widget_rect.w, b },
                { widget_rect.x, widget_rect.y + widget_rect.h - b, widget_rect.w, b },
                { widget_rect.x, widget_rect.y + b, b, widget_rect.h - b * 2.0f },
                { widget_rect.x + widget_rect.w - b, widget_rect.y + b, b, widget_rect.h - b * 2.0f },
            };

            Rect border_clip = {0};
            const Rect *clip_bounds = NULL;
            if (widget->has_clip) {
                border_clip = (Rect){
                    widget->clip.x - b,
                    widget->clip.y - b,
                    widget->clip.w + b * 2.0f,
                    widget->clip.h + b * 2.0f,
                };
                clip_bounds = &border_clip;
            }

            for (size_t edge = 0; edge < 4; ++edge) {
                if (borders[edge].w <= 0.0f || borders[edge].h <= 0.0f) continue;
                Rect clipped_border;
                if (apply_clip_rect_to_bounds(clip_bounds, &borders[edge], &clipped_border) &&
                    ensure_view_model_capacity(&view_models, &view_model_capacity, view_model_count + 1, &view_models_ok)) {
                    ViewModel vm = {
                        .id = widget->id,
                        .logical_box = { {clipped_border.x, clipped_border.y}, {clipped_border.w, clipped_border.h} },
                        .layer = base_z + Z_LAYER_BORDER,
                        .phase = RENDER_PHASE_BACKGROUND,
                        .widget_order = widget_order,
                        .ordinal = widget_ordinals[i]++,
                        .color = widget->border_color,
                    };
                    if (clip_bounds) {
                        vm.has_clip = 1;
                        vm.clip = (LayoutBox){ {clip_bounds->x, clip_bounds->y}, {clip_bounds->w, clip_bounds->h} };
                    } else {
                        apply_widget_clip_to_view_model(widget, &vm);
                    }
                    view_models[view_model_count++] = vm;
                }
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

            Color track_color = widget->color;
            track_color.a *= 0.35f;
            Rect track_rect = { track_x, track_y, track_w, track_height };
            Rect clipped_track;
            if (apply_clip_rect(widget, &track_rect, &clipped_track) &&
                ensure_view_model_capacity(&view_models, &view_model_capacity, view_model_count + 1, &view_models_ok)) {
                view_models[view_model_count++] = (ViewModel){
                    .id = widget->id,
                    .logical_box = { {clipped_track.x, clipped_track.y}, {clipped_track.w, clipped_track.h} },
                    .layer = base_z + Z_LAYER_SLIDER_TRACK,
                    .phase = RENDER_PHASE_BACKGROUND,
                    .widget_order = widget_order,
                    .ordinal = widget_ordinals[i]++,
                    .color = track_color,
                };
                apply_widget_clip_to_view_model(widget, &view_models[view_model_count - 1]);
            }

            float fill_w = track_w * t;
            Rect fill_rect = { track_x, track_y, fill_w, track_height };
            Rect clipped_fill;
            if (apply_clip_rect(widget, &fill_rect, &clipped_fill) &&
                ensure_view_model_capacity(&view_models, &view_model_capacity, view_model_count + 1, &view_models_ok)) {
                view_models[view_model_count++] = (ViewModel){
                    .id = widget->id,
                    .logical_box = { {clipped_fill.x, clipped_fill.y}, {clipped_fill.w, clipped_fill.h} },
                    .layer = base_z + Z_LAYER_SLIDER_FILL,
                    .phase = RENDER_PHASE_BACKGROUND,
                    .widget_order = widget_order,
                    .ordinal = widget_ordinals[i]++,
                    .color = widget->color,
                };
                apply_widget_clip_to_view_model(widget, &view_models[view_model_count - 1]);
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
            if (apply_clip_rect(widget, &knob_rect, &clipped_knob) &&
                ensure_view_model_capacity(&view_models, &view_model_capacity, view_model_count + 1, &view_models_ok)) {
                view_models[view_model_count++] = (ViewModel){
                    .id = widget->id,
                    .logical_box = { {clipped_knob.x, clipped_knob.y}, {clipped_knob.w, clipped_knob.h} },
                    .layer = base_z + Z_LAYER_SLIDER_KNOB,
                    .phase = RENDER_PHASE_BACKGROUND,
                    .widget_order = widget_order,
                    .ordinal = widget_ordinals[i]++,
                    .color = knob_color,
                };
                apply_widget_clip_to_view_model(widget, &view_models[view_model_count - 1]);
            }
            continue;
        }

        Rect clipped_fill;
        if (apply_clip_rect(widget, &inner_rect, &clipped_fill) &&
            ensure_view_model_capacity(&view_models, &view_model_capacity, view_model_count + 1, &view_models_ok)) {
            view_models[view_model_count++] = (ViewModel){
                .id = widget->id,
                .logical_box = { {clipped_fill.x, clipped_fill.y}, {clipped_fill.w, clipped_fill.h} },
                .layer = base_z + Z_LAYER_FILL,
                .phase = RENDER_PHASE_BACKGROUND,
                .widget_order = widget_order,
                .ordinal = widget_ordinals[i]++,
                .color = widget->color,
            };
            apply_widget_clip_to_view_model(widget, &view_models[view_model_count - 1]);
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
            if (apply_clip_rect(widget, &scroll_track, &clipped_track) &&
                ensure_view_model_capacity(&view_models, &view_model_capacity, view_model_count + 1, &view_models_ok)) {
                view_models[view_model_count++] = (ViewModel){
                    .id = widget->id,
                    .logical_box = { {clipped_track.x, clipped_track.y}, {clipped_track.w, clipped_track.h} },
                    .layer = scrollbar_z + Z_LAYER_SCROLLBAR_TRACK,
                    .phase = RENDER_PHASE_BACKGROUND,
                    .widget_order = widget_order,
                    .ordinal = widget_ordinals[i]++,
                    .color = track_color,
                };
                apply_widget_clip_to_view_model(widget, &view_models[view_model_count - 1]);
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
            if (apply_clip_rect(widget, &thumb_rect, &clipped_thumb) &&
                ensure_view_model_capacity(&view_models, &view_model_capacity, view_model_count + 1, &view_models_ok)) {
                view_models[view_model_count++] = (ViewModel){
                    .id = widget->id,
                    .logical_box = { {clipped_thumb.x, clipped_thumb.y}, {clipped_thumb.w, clipped_thumb.h} },
                    .layer = scrollbar_z + Z_LAYER_SCROLLBAR_THUMB,
                    .phase = RENDER_PHASE_BACKGROUND,
                    .widget_order = widget_order,
                    .ordinal = widget_ordinals[i]++,
                    .color = thumb_color,
                };
                apply_widget_clip_to_view_model(widget, &view_models[view_model_count - 1]);
            }
        }
    }
    if (!view_models_ok) {
        free(glyph_quads.items);
        free(view_models);
        free(widget_ordinals);
        return false;
    }

    for (size_t i = 0; i < g_widgets.count; ++i) {
        const Widget *widget = &g_widgets.items[i];
        size_t widget_order = g_widgets.count - 1 - i;

        if (!widget->text || !*widget->text) {
            continue;
        }

        int text_z = widget->z_index * LAYER_STRIDE + Z_LAYER_TEXT;

        float scroll_offset = widget->scroll_static ? 0.0f : widget->scroll_offset;
        float snapped_scroll_pixels = -snap_to_pixel(scroll_offset * g_transformer.dpi_scale);
        float effective_offset = snapped_scroll_pixels / g_transformer.dpi_scale;
        float pen_x = widget->rect.x + widget->padding;
        float pen_y = widget->rect.y + effective_offset + widget->padding + (float)ascent;

        for (const char *c = widget->text; *c; ) {
            int adv = 0;
            int codepoint = utf8_decode(c, &adv);
            if (adv <= 0) break;
            if (codepoint < 32) { c += adv; continue; }

            const Glyph *g = get_glyph(codepoint);
            if (!g) { c += adv; continue; }
            float snapped_pen_x = floorf(pen_x + 0.5f);
            float snapped_pen_y = floorf(pen_y + 0.5f);
            float x0 = snapped_pen_x + g->xoff;
            float y0 = snapped_pen_y + g->yoff;
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
                .widget_id = widget->id,
                .widget_order = widget_order,
                .layer = text_z,
                .phase = RENDER_PHASE_OVERLAY,
                .ordinal = widget_ordinals[i]++,
                .has_clip = widget->has_clip,
                .clip = { {widget->clip.x, widget->clip.y}, {widget->clip.w, widget->clip.h} },
            };

            pen_x += g->advance;
            c += adv;
        }
    }

    Mat4 projection = mat4_identity();

    CoordinateSystem2D transformer = g_transformer;
    transformer.viewport_size = (Vec2){(float)g_swapchain_extent.width, (float)g_swapchain_extent.height};

    RenderContext context;
    render_context_init(&context, &transformer, &projection);

    Renderer renderer;
    renderer_init(&renderer, &context, view_model_count);
    size_t expected_background = view_model_count * 6;
    size_t expected_text = glyph_quads.count * 6;
    size_t expected_final = renderer.command_list.count * 6;

    if (!ensure_cpu_vertex_capacity(&frame->cpu, expected_background, expected_text, expected_final)) {
        renderer_dispose(&renderer);
        free(glyph_quads.items);
        free(view_models);
        free(widget_ordinals);
        return false;
    }

    UiVertexBuffer background_buffer = {
        .vertices = frame->cpu.background_vertices,
        .count = 0,
        .capacity = frame->cpu.background_capacity,
    };

    UiTextVertexBuffer text_buffer = {
        .vertices = frame->cpu.text_vertices,
        .count = 0,
        .capacity = frame->cpu.text_capacity,
    };

    renderer_fill_vertices(&renderer, view_models, view_model_count, glyph_quads.items, glyph_quads.count, &background_buffer, &text_buffer);

    size_t primitive_count = renderer.command_list.count;

    int min_layer = 0;
    int max_layer = 0;
    if (primitive_count > 0) {
        min_layer = renderer.command_list.commands[0].key.layer;
        max_layer = min_layer;
        for (size_t i = 1; i < renderer.command_list.count; ++i) {
            int layer = renderer.command_list.commands[i].key.layer;
            if (layer < min_layer) min_layer = layer;
            if (layer > max_layer) max_layer = layer;
        }
    }

    Primitive *primitives = primitive_count > 0 ? calloc(primitive_count, sizeof(Primitive)) : NULL;
    bool success = primitive_count == 0 || primitives != NULL;
    size_t prim_idx = 0;
    size_t background_quad_idx = 0;
    size_t text_quad_idx = 0;

    if (primitives) {
        for (size_t c = 0; c < renderer.command_list.count; ++c) {
            const RenderCommand *cmd = &renderer.command_list.commands[c];
            Primitive *p = &primitives[prim_idx++];
            p->order = cmd->key.ordinal;
            p->z = (float)cmd->key.layer;

            if (cmd->primitive == RENDER_PRIMITIVE_BACKGROUND) {
                UiVertex *base = &background_buffer.vertices[background_quad_idx++ * 6];
                for (int i = 0; i < 6; ++i) {
                    UiVertex v = base[i];
                    p->vertices[i] = (Vtx){v.position[0], v.position[1], 0.0f, 0.0f, 0.0f, 0.0f, v.color.r, v.color.g, v.color.b, v.color.a};
                }
            } else {
                UiTextVertex *base = &text_buffer.vertices[text_quad_idx++ * 6];
                for (int i = 0; i < 6; ++i) {
                    UiTextVertex v = base[i];
                    p->vertices[i] = (Vtx){v.position[0], v.position[1], 0.0f, v.uv[0], v.uv[1], 1.0f, v.color.r, v.color.g, v.color.b, v.color.a};
                }
            }
        }

        size_t total_vertices = primitive_count * 6;
        if (ensure_vtx_capacity(&frame->cpu, total_vertices)) {
            size_t cursor = 0;
            float layer_span = (float)(max_layer - min_layer + 1);
            float step = (primitive_count > 0 && layer_span > 0.0f) ? (1.0f / ((float)primitive_count + 1.0f)) / layer_span : 0.0f;
            for (size_t i = 0; i < primitive_count; ++i) {
                float layer_offset = (float)((int)primitives[i].z - min_layer);
                float depth = 1.0f - (layer_offset + 0.5f) / layer_span - step * (float)i;
                if (depth < 0.0f) depth = 0.0f;
                if (depth > 1.0f) depth = 1.0f;
                for (int v = 0; v < 6; ++v) {
                    Vtx vertex = primitives[i].vertices[v];
                    vertex.pz = depth;
                    frame->cpu.vertices[cursor++] = vertex;
                }
            }
            frame->vertex_count = cursor;
        } else {
            frame->vertex_count = 0;
            success = false;
        }
    } else {
        frame->vertex_count = 0;
        if (primitive_count > 0) {
            success = false;
        }
    }

    renderer_dispose(&renderer);
    free(glyph_quads.items);
    free(primitives);
    free(view_models);
    free(widget_ordinals);

    return success;
}

static bool recover_device_loss(void) {
    fprintf(stderr, "Device lost detected; tearing down and recreating logical g_device and g_swapchain resources...\n");
    if (g_device) vkDeviceWaitIdle(g_device);
    destroy_device_resources();
    if (g_device) {
        vkDestroyDevice(g_device, NULL);
        g_device = VK_NULL_HANDLE;
    }

    recreate_instance_and_surface();

    pick_physical_and_create_device();
    create_swapchain_and_views(VK_NULL_HANDLE);
    if (!g_swapchain) return false;
    create_depth_resources();
    create_render_pass();
    create_descriptor_layout();
    create_pipeline(g_vert_spv, g_frag_spv);
    create_cmds_and_sync();
    create_font_texture();
    create_descriptor_pool_and_set();
    for (size_t i = 0; i < 2; ++i) {
        g_frame_resources[i].stage = FRAME_AVAILABLE;
        if (build_vertices_from_widgets(&g_frame_resources[i])) {
            upload_vertices(&g_frame_resources[i]);
        }
    }
    return true;
}

static void record_cmdbuffer(uint32_t idx, const FrameResources *frame) {
    VkCommandBuffer cb = g_cmdbuffers[idx];
    vkResetCommandBuffer(cb, 0);
    VkCommandBufferBeginInfo binfo = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    vkBeginCommandBuffer(cb, &binfo);

    VkClearValue clr[2];
    clr[0].color = (VkClearColorValue){{0.9f,0.9f,0.9f,1.0f}};
    clr[1].depthStencil = (VkClearDepthStencilValue){1.0f, 0};
    VkRenderPassBeginInfo rpbi = { .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, .renderPass = g_render_pass, .framebuffer = g_framebuffers[idx], .renderArea = {.offset = {0,0}, .extent = g_swapchain_extent }, .clearValueCount = 2, .pClearValues = clr };
    vkCmdBeginRenderPass(cb, &rpbi, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, g_pipeline);
    ViewConstants pc = { .viewport = { (float)g_swapchain_extent.width, (float)g_swapchain_extent.height } };
    vkCmdPushConstants(cb, g_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ViewConstants), &pc);
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, g_pipeline_layout, 0, 1, &g_descriptor_set, 0, NULL);
    if (frame && frame->vertex_buffer != VK_NULL_HANDLE && frame->vertex_count > 0) {
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cb, 0, 1, &frame->vertex_buffer, &offset);
        /* simple draw: vertices are triangles (every 3 vertices) */
        vkCmdDraw(cb, (uint32_t)frame->vertex_count, 1, 0, 0);
    }

    vkCmdEndRenderPass(cb);
    vkEndCommandBuffer(cb);
}

/* present frame */
static void draw_frame(void) {
    if (g_swapchain == VK_NULL_HANDLE) return;
    uint32_t img_idx;
    VkResult acq = vkAcquireNextImageKHR(g_device, g_swapchain, UINT64_MAX, g_sem_img_avail, VK_NULL_HANDLE, &img_idx);
    if (acq == VK_ERROR_DEVICE_LOST) { if (!recover_device_loss()) fatal_vk("vkAcquireNextImageKHR", acq); return; }
    if (acq == VK_ERROR_OUT_OF_DATE_KHR || acq == VK_SUBOPTIMAL_KHR) { recreate_swapchain(); return; }
    if (acq != VK_SUCCESS) fatal_vk("vkAcquireNextImageKHR", acq);
    vkWaitForFences(g_device, 1, &g_fences[img_idx], VK_TRUE, UINT64_MAX);
    vkResetFences(g_device, 1, &g_fences[img_idx]);

    if (g_image_frame_owner && g_image_frame_owner[img_idx] >= 0) {
        int idx = g_image_frame_owner[img_idx];
        if (idx >= 0 && idx < 2) {
            FrameResources* owner = &g_frame_resources[idx];
            VkFence tracked_fence = owner->inflight_fence;
            if (tracked_fence && tracked_fence != g_fences[img_idx]) {
                vkWaitForFences(g_device, 1, &tracked_fence, VK_TRUE, UINT64_MAX);
            }
            owner->stage = FRAME_AVAILABLE;
            owner->inflight_fence = VK_NULL_HANDLE;
            g_image_frame_owner[img_idx] = -1;
        }
    }

    FrameResources *frame = &g_frame_resources[g_current_frame_cursor % 2];
    g_current_frame_cursor = (g_current_frame_cursor + 1) % 2;

    if (frame->stage == FRAME_SUBMITTED && frame->inflight_fence) {
        if (frame->inflight_fence != g_fences[img_idx]) {
            vkWaitForFences(g_device, 1, &frame->inflight_fence, VK_TRUE, UINT64_MAX);
        }
        frame->stage = FRAME_AVAILABLE;
        frame->inflight_fence = VK_NULL_HANDLE;
        if (g_image_frame_owner) {
            int frame_idx = (int)(frame - g_frame_resources);
            for (uint32_t i = 0; i < g_swapchain_img_count; ++i) {
                if (g_image_frame_owner[i] == frame_idx) g_image_frame_owner[i] = -1;
            }
        }
    }
    frame->stage = FRAME_FILLING;

    bool built = build_vertices_from_widgets(frame);
    if (!built) {
        frame->vertex_count = 0;
    }

    /* re-upload vertices & record */
    if (!upload_vertices(frame)) {
        frame->vertex_count = 0;
    }
    record_cmdbuffer(img_idx, frame);

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo si = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .waitSemaphoreCount = 1, .pWaitSemaphores = &g_sem_img_avail, .pWaitDstStageMask = &waitStage, .commandBufferCount = 1, .pCommandBuffers = &g_cmdbuffers[img_idx], .signalSemaphoreCount = 1, .pSignalSemaphores = &g_sem_render_done };
    double submit_start = vk_now_ms();
    VkResult submit = vkQueueSubmit(g_queue, 1, &si, g_fences[img_idx]);
    vk_log_command("vkQueueSubmit", "draw", submit_start);
    if (submit == VK_ERROR_DEVICE_LOST) {
        if (!recover_device_loss()) fatal_vk("vkQueueSubmit", submit);
        return;
    }
    if (submit != VK_SUCCESS) fatal_vk("vkQueueSubmit", submit);

    frame->stage = FRAME_SUBMITTED;
    frame->inflight_fence = g_fences[img_idx];
    if (g_image_frame_owner) {
        int frame_idx = (int)(frame - g_frame_resources);
        for (uint32_t i = 0; i < g_swapchain_img_count; ++i) {
            if (g_image_frame_owner[i] == frame_idx) g_image_frame_owner[i] = -1;
        }
        g_image_frame_owner[img_idx] = frame_idx;
    }
    VkPresentInfoKHR pi = { .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, .waitSemaphoreCount = 1, .pWaitSemaphores = &g_sem_render_done, .swapchainCount = 1, .pSwapchains = &g_swapchain, .pImageIndices = &img_idx };
    double present_start = vk_now_ms();
    VkResult present = vkQueuePresentKHR(g_queue, &pi);
    vk_log_command("vkQueuePresentKHR", "present", present_start);
    if (present == VK_ERROR_DEVICE_LOST) { if (!recover_device_loss()) fatal_vk("vkQueuePresentKHR", present); return; }
    if (present == VK_ERROR_OUT_OF_DATE_KHR || present == VK_SUBOPTIMAL_KHR) { recreate_swapchain(); return; }
    if (present != VK_SUCCESS) fatal_vk("vkQueuePresentKHR", present);
}

static bool vk_backend_init(RendererBackend* backend, const RenderBackendInit* init) {
    if (!backend || !init) return false;
    render_logger_init(&backend->logger, init->logger_config, backend->id);
    g_logger = &backend->logger;

    g_window = init->window;
    g_platform_surface = init->surface;
    g_get_required_instance_extensions = init->get_required_instance_extensions;
    g_create_surface = init->create_surface;
    g_destroy_surface = init->destroy_surface;
    g_get_framebuffer_size = init->get_framebuffer_size;
    g_wait_events = init->wait_events;
    g_widgets = init->widgets;
    g_vert_spv = init->vert_spv;
    g_frag_spv = init->frag_spv;
    g_font_path = init->font_path;

    if (!g_window || !g_platform_surface || !g_get_required_instance_extensions || !g_create_surface ||
        !g_destroy_surface || !g_get_framebuffer_size || !g_wait_events) {
        fprintf(stderr, "Vulkan renderer missing platform callbacks.\n");
        return false;
    }

    if (init->transformer) {
        g_transformer = *init->transformer;
    } else {
        coordinate_system2d_init(&g_transformer, 1.0f, 1.0f, (Vec2){0.0f, 0.0f});
    }

    g_current_frame_cursor = 0;
    for (size_t i = 0; i < 2; ++i) {
        g_frame_resources[i].stage = FRAME_AVAILABLE;
        g_frame_resources[i].inflight_fence = VK_NULL_HANDLE;
        g_frame_resources[i].vertex_count = 0;
    }

    create_instance();
    if (!g_create_surface(g_window, g_instance, NULL, g_platform_surface)) return false;
    g_surface = (VkSurfaceKHR)(g_platform_surface ? g_platform_surface->handle : NULL);

    pick_physical_and_create_device();
    create_swapchain_and_views(VK_NULL_HANDLE);
    create_depth_resources();
    create_render_pass();
    create_descriptor_layout();
    create_pipeline(g_vert_spv, g_frag_spv);
    create_cmds_and_sync();

    build_font_atlas();
    create_font_texture();
    create_descriptor_pool_and_set();
    for (size_t i = 0; i < 2; ++i) {
        g_frame_resources[i].stage = FRAME_AVAILABLE;
        build_vertices_from_widgets(&g_frame_resources[i]);
        upload_vertices(&g_frame_resources[i]);
    }
    return true;
}

static void vk_backend_update_transformer(RendererBackend* backend, const CoordinateTransformer* transformer) {
    (void)backend;
    if (!transformer) return;
    g_transformer = *transformer;
    g_transformer.viewport_size = (Vec2){(float)g_swapchain_extent.width, (float)g_swapchain_extent.height};
}

static void vk_backend_draw(RendererBackend* backend) {
    (void)backend;
    draw_frame();
}

static void vk_backend_cleanup(RendererBackend* backend) {
    if (g_device) vkDeviceWaitIdle(g_device);
    free(atlas);
    atlas = NULL;
    free(ttf_buffer);
    ttf_buffer = NULL;
    for (size_t i = 0; i < 2; ++i) {
        free(g_frame_resources[i].cpu.background_vertices);
        free(g_frame_resources[i].cpu.text_vertices);
        free(g_frame_resources[i].cpu.vertices);
        g_frame_resources[i].cpu.background_vertices = NULL;
        g_frame_resources[i].cpu.text_vertices = NULL;
        g_frame_resources[i].cpu.vertices = NULL;
        g_frame_resources[i].cpu.background_capacity = 0;
        g_frame_resources[i].cpu.text_capacity = 0;
        g_frame_resources[i].cpu.vertex_capacity = 0;
    }
    destroy_device_resources();
    if (g_device) { vkDestroyDevice(g_device, NULL); g_device = VK_NULL_HANDLE; }
    if (g_platform_surface && g_destroy_surface && g_instance) {
        g_destroy_surface(g_instance, NULL, g_platform_surface);
    } else if (g_surface && g_instance) {
        vkDestroySurfaceKHR(g_instance, g_surface, NULL);
    }
    g_surface = VK_NULL_HANDLE;
    if (g_instance) { vkDestroyInstance(g_instance, NULL); g_instance = VK_NULL_HANDLE; }
    render_logger_cleanup(g_logger);
    g_logger = NULL;
}

RendererBackend* vulkan_renderer_backend(void) {
    g_vulkan_backend.id = "vulkan";
    g_vulkan_backend.state = &g_vk;
    g_vulkan_backend.init = vk_backend_init;
    g_vulkan_backend.update_transformer = vk_backend_update_transformer;
    g_vulkan_backend.draw = vk_backend_draw;
    g_vulkan_backend.cleanup = vk_backend_cleanup;
    return &g_vulkan_backend;
}

