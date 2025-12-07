/* Minimal Vulkan GUI renderer(C99) with:
-GLFW for window + Vulkan surface
- Simple Vulkan bootstrap(instance / device / swapchain / renderpass)
- Graphics pipeline using SPIR - V vertex & fragment shaders(extern.spv files)
- Simple vertex buffer drawing quads for widgets
- Bitmap font atlas generation using stb_truetype(embedded)
- JSON UI description using embedded jsmn(minimal subset)
Notes:
-Compile shaders with glslc(see instructions below).
- This code is compact for readability; in production add more error checks and cleanup.
Build example(Linux) :
    glslc shader.vert - o shader.vert.spv
    glslc shader.frag - o shader.frag.spv
    gcc - std = c99 - O2 vk_gui.c - o vk_gui `pkg-config --cflags --libs glfw3` - lvulkan - lm
    Run :
. / vk_gui ui.json shader.vert.spv shader.frag.spv
*/

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

/* ---------- Embedded stb_truetype.h (minimal needed parts) ---------- */
/* We include the single-file stb_truetype implementation. For brevity, we declare the functions we use and
   include the implementation below via STB_TRUETYPE_IMPLEMENTATION.
   (License: public domain / MIT-like) */
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

   /* ---------- Embedded jsmn (very small parser) ---------- */
    typedef enum { JSMN_UNDEFINED = 0, JSMN_OBJECT = 1, JSMN_ARRAY = 2, JSMN_STRING = 3, JSMN_PRIMITIVE = 4 } jsmntype_t;
typedef struct { jsmntype_t type; int start; int end; int size; } jsmntok_t;
typedef struct { unsigned int pos; unsigned int toknext; int toksuper; } jsmn_parser;
static void jsmn_init(jsmn_parser * p) { p->pos = 0; p->toknext = 0; p->toksuper = -1; }
static int jsmn_alloc(jsmn_parser * p, jsmntok_t * toks, size_t nt) {
    if (p->toknext >= nt) return -1;
    toks[p->toknext].start = toks[p->toknext].end = -1;
    toks[p->toknext].size = 0;
    toks[p->toknext].type = JSMN_UNDEFINED;
    return p->toknext++;
}
static int jsmn_parse(jsmn_parser * p, const char* js, size_t len, jsmntok_t * toks, size_t nt) {
    int r;
    for (size_t i = p->pos; i < len; i++) {
        char c = js[i];
        switch (c) {
        case '{': case '[': {
            int tk = jsmn_alloc(p, toks, nt);
            if (tk < 0) return -1;
            toks[tk].type = (c == '{') ? JSMN_OBJECT : JSMN_ARRAY;
            toks[tk].start = i;
            toks[tk].size = 0;
            p->toksuper = tk;
            break;
        }
        case '}': case ']': {
            jsmntype_t want = (c == '}') ? JSMN_OBJECT : JSMN_ARRAY;
            int found = -1;
            for (int j = p->toknext - 1; j >= 0; j--) {
                if (toks[j].start != -1 && toks[j].end == -1 && toks[j].type == want) { found = j; break; }
            }
            if (found == -1) return -1;
            toks[found].end = i + 1;
            p->toksuper = -1;
            break;
        }
        case '\"': {
            int tk = jsmn_alloc(p, toks, nt);
            if (tk < 0) return -1;
            toks[tk].type = JSMN_STRING;
            toks[tk].start = i + 1;
            i++;
            while (i < (int)len && js[i] != '\"') i++;
            if (i >= (int)len) return -1;
            toks[tk].end = i;
            break;
        }
        default:
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == ':' || c == ',') break;
            {
                int tk = jsmn_alloc(p, toks, nt);
                if (tk < 0) return -1;
                toks[tk].type = JSMN_PRIMITIVE;
                toks[tk].start = i;
                while (i < (int)len && js[i] != ',' && js[i] != ']' && js[i] != '}' && js[i] != '\n' && js[i] != '\r' && js[i] != '\t' && js[i] != ' ') i++;
                toks[tk].end = i;
                i--;
            }
        }
    }
    p->pos = (unsigned int)len;
    return 0;
}
static char* tok_copy(const char* js, jsmntok_t * t) {
    int n = t->end - t->start;
    char* s = malloc(n + 1);
    memcpy(s, js + t->start, n);
    s[n] = 0; return s;
}

/* ---------- Small helper types ---------- */
typedef struct { float x, y, w, h; } Rect;
typedef struct { float r, g, b, a; } Color;
typedef enum { W_PANEL, W_LABEL, W_BUTTON, W_HSLIDER } WidgetType;
typedef struct Widget {
    WidgetType type;
    Rect rect;
    Color color;
    char* text; /* for labels/buttons */
    float minv, maxv, value;
    char* id;
    struct Widget* next;
} Widget;
static Widget* widgets = NULL;

/* ---------- Simple JSON loader (very forgiving) ---------- */
static void free_widgets(void) {
    while (widgets) {
        Widget* n = widgets->next;
        if (widgets->text) free(widgets->text);
        if (widgets->id) free(widgets->id);
        free(widgets);
        widgets = n;
    }
}
static void add_widget(Widget * w) {
    w->next = widgets; widgets = w;
}
static void parse_ui_json(const char* json) {
    jsmn_parser p; jsmn_init(&p);
    size_t tokc = 2048;
    jsmntok_t* toks = malloc(sizeof(jsmntok_t) * tokc);
    for (size_t i = 0; i < tokc; i++) { toks[i].start = toks[i].end = -1; toks[i].size = 0; toks[i].type = JSMN_UNDEFINED; }
    if (jsmn_parse(&p, json, strlen(json), toks, tokc) < 0) { fprintf(stderr, "JSON parse problem\n"); free(toks); return; }
    /* naive: find tokens where string == "ui" or "widgets" and read array following */
    for (unsigned int i = 0; i < p.toknext; i++) {
        if (toks[i].type == JSMN_STRING) {
            char* k = tok_copy(json, &toks[i]);
            if (strcmp(k, "ui") == 0 || strcmp(k, "widgets") == 0) {
                free(k);
                if (i + 1 < p.toknext && toks[i + 1].type == JSMN_ARRAY) {
                    int arrstart = toks[i + 1].start, arrend = toks[i + 1].end;
                    
                    /* find object tokens inside array */
                    for (unsigned int j = 0; j < p.toknext; j++) {
                        if (toks[j].type == JSMN_OBJECT && toks[j].start > arrstart && toks[j].end < arrend) {
                            
                            /* scan this object for keys */
                            Widget* w = calloc(1, sizeof(Widget));
                            w->color.r = w->color.g = w->color.b = 0.6f; w->color.a = 1.0f;
                            w->rect.x = w->rect.y = w->rect.w = w->rect.h = 0;
                            w->minv = 0; w->maxv = 1; w->value = 0;
                            unsigned int kidx = j + 1;
                            while (kidx < p.toknext && toks[kidx].start >= toks[j].start && toks[kidx].end <= toks[j].end) {
                                if (toks[kidx].type == JSMN_STRING) {
                                    char* key = tok_copy(json, &toks[kidx]);
                                    if (kidx + 1 >= p.toknext) { free(key); break; }
                                    jsmntok_t* val = &toks[kidx + 1];
                                    if (strcmp(key, "type") == 0 && val->type == JSMN_STRING) {
                                        char* s = tok_copy(json, val);
                                        if (strcmp(s, "panel") == 0) w->type = W_PANEL;
                                        else if (strcmp(s, "label") == 0) w->type = W_LABEL;
                                        else if (strcmp(s, "button") == 0) w->type = W_BUTTON;
                                        else if (strcmp(s, "hslider") == 0) w->type = W_HSLIDER;
                                        free(s);
                                    }
                                    else if (strcmp(key, "x") == 0) { char* s = tok_copy(json, val); w->rect.x = atof(s); free(s); }
                                    else if (strcmp(key, "y") == 0) { char* s = tok_copy(json, val); w->rect.y = atof(s); free(s); }
                                    else if (strcmp(key, "w") == 0) { char* s = tok_copy(json, val); w->rect.w = atof(s); free(s); }
                                    else if (strcmp(key, "h") == 0) { char* s = tok_copy(json, val); w->rect.h = atof(s); free(s); }
                                    else if (strcmp(key, "id") == 0 && val->type == JSMN_STRING) { char* s = tok_copy(json, val); w->id = s; }
                                    else if (strcmp(key, "text") == 0 && val->type == JSMN_STRING) { char* s = tok_copy(json, val); w->text = s; }
                                    else if (strcmp(key, "min") == 0) { char* s = tok_copy(json, val); w->minv = atof(s); free(s); }
                                    else if (strcmp(key, "max") == 0) { char* s = tok_copy(json, val); w->maxv = atof(s); free(s); }
                                    else if (strcmp(key, "value") == 0) { char* s = tok_copy(json, val); w->value = atof(s); free(s); }
                                    else if (strcmp(key, "color") == 0 && val->type == JSMN_ARRAY) {
                                        /* gather next primitives inside array */
                                        int idx = kidx + 2; /* val is at kidx+1, its children tokens start after that in this parser variant */
                                        float cols[4] = { 0.6f,0.6f,0.6f,1.0f }; int cc = 0;
                                        for (unsigned int z = kidx + 1; z < p.toknext && toks[z].start >= val->start && toks[z].end <= val->end; z++) {
                                            if (toks[z].type == JSMN_PRIMITIVE) {
                                                char* s = tok_copy(json, &toks[z]);
                                                cols[cc++] = atof(s);
                                                free(s);
                                            }
                                        }
                                        w->color.r = cols[0]; w->color.g = cols[1]; w->color.b = cols[2];
                                    }
                                    free(key);
                                    kidx += 2;
                                }
                                else kidx++;
                            }
                            add_widget(w);
                        }
                    }
                }
            }
            else free(k);
        }
    }
    free(toks);
}

/* ---------- Vulkan helpers & global state ---------- */
static GLFWwindow* window = NULL;
static VkInstance instance = VK_NULL_HANDLE;
static VkPhysicalDevice physical = VK_NULL_HANDLE;
static VkDevice device = VK_NULL_HANDLE;
static uint32_t graphics_family = (uint32_t)-1;
static VkQueue queue = VK_NULL_HANDLE;
static VkSurfaceKHR surface = VK_NULL_HANDLE;
static VkSwapchainKHR swapchain = VK_NULL_HANDLE;
static const char* g_vert_spv = NULL;
static const char* g_frag_spv = NULL;
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

/* GPU-side buffers (vertex) simple staging omitted: we'll create host-visible vertex buffer */
static VkBuffer vertex_buffer = VK_NULL_HANDLE;
static VkDeviceMemory vertex_memory = VK_NULL_HANDLE;

/* Texture atlas for font */
static VkImage font_image = VK_NULL_HANDLE;
static VkDeviceMemory font_image_mem = VK_NULL_HANDLE;
static VkImageView font_image_view = VK_NULL_HANDLE;
static VkSampler font_sampler = VK_NULL_HANDLE;
static VkDescriptorSetLayout descriptor_layout = VK_NULL_HANDLE;
static VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
static VkDescriptorSet descriptor_set = VK_NULL_HANDLE;

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

    int w, h; glfwGetFramebufferSize(window, &w, &h);
    while (w == 0 || h == 0) {
        glfwWaitEvents();
        if (glfwWindowShouldClose(window)) return;
        glfwGetFramebufferSize(window, &w, &h);
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
    if (sem_img_avail) { vkDestroySemaphore(device, sem_img_avail, NULL); sem_img_avail = VK_NULL_HANDLE; }
    if (sem_render_done) { vkDestroySemaphore(device, sem_render_done, NULL); sem_render_done = VK_NULL_HANDLE; }
}

static void recreate_instance_and_surface(void) {
    if (surface) { vkDestroySurfaceKHR(instance, surface, NULL); surface = VK_NULL_HANDLE; }
    if (instance) { vkDestroyInstance(instance, NULL); instance = VK_NULL_HANDLE; }

    create_instance();
    res = glfwCreateWindowSurface(instance, window, NULL, &surface);
    if (res != VK_SUCCESS) fatal_vk("glfwCreateWindowSurface", res);
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
    if (vertex_buffer) {
        vkDestroyBuffer(device, vertex_buffer, NULL);
        vertex_buffer = VK_NULL_HANDLE;
    }
    if (vertex_memory) {
        vkFreeMemory(device, vertex_memory, NULL);
        vertex_memory = VK_NULL_HANDLE;
    }
    create_buffer(bytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &vertex_buffer, &vertex_memory);
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
        return;
    }
    size_t bytes = vtx_count * sizeof(Vtx);
    create_vertex_buffer(bytes);
    void* dst = NULL; vkMapMemory(device, vertex_memory, 0, bytes, 0, &dst);
    memcpy(dst, vtx_buf, bytes);
    vkUnmapMemory(device, vertex_memory);
}

/* record commands per framebuffer (we will re-record each frame in this simple example) */
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
    if (swapchain == VK_NULL_HANDLE) return;
    uint32_t img_idx;
    VkResult acq = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, sem_img_avail, VK_NULL_HANDLE, &img_idx);
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
    if (present == VK_ERROR_OUT_OF_DATE_KHR || present == VK_SUBOPTIMAL_KHR) { recreate_swapchain(); return; }
    if (present != VK_SUCCESS) fatal_vk("vkQueuePresentKHR", present);
}

/* ---------- GUI building: convert widgets -> vertex list (rects + textured glyphs) ---------- */
static unsigned char* ttf_buffer = NULL;
static stbtt_fontinfo fontinfo;
static unsigned char* atlas = NULL;
static int atlas_w = 512, atlas_h = 512;
static float font_scale = 0.0f;
static int ascent = 0;
typedef struct { float u0, v0, u1, v1; float xoff, yoff; float w, h; float advance; } Glyph;
static Glyph glyphs[128];

static void build_font_atlas(void) {
    FILE* f = fopen("font.ttf", "rb");
    if (!f) { fatal("font.ttf not found in cwd - place a TTF named font.ttf"); }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    ttf_buffer = malloc(sz); fread(ttf_buffer, 1, sz, f); fclose(f);
    stbtt_InitFont(&fontinfo, ttf_buffer, 0);

    atlas_w = 512; atlas_h = 512;
    atlas = malloc(atlas_w * atlas_h);
    memset(atlas, 0, atlas_w * atlas_h);
    font_scale = stbtt_ScaleForPixelHeight(&fontinfo, 32.0f);
    stbtt_GetFontVMetrics(&fontinfo, &ascent, NULL, NULL);
    ascent = (int)roundf(ascent * font_scale);

    int x = 0, y = 0, rowh = 0;
    for (int c = 32; c < 128; c++) {
        int aw, ah, bx, by;
        unsigned char* bitmap = stbtt_GetCodepointBitmap(&fontinfo, 0, font_scale, c, &aw, &ah, &bx, &by);
        if (x + aw >= atlas_w) { x = 0; y += rowh; rowh = 0; }
        if (y + ah >= atlas_h) { fprintf(stderr, "atlas too small\n"); break; }
        for (int yy = 0; yy < ah; yy++) {
            for (int xx = 0; xx < aw; xx++) {
                atlas[(y + yy) * atlas_w + (x + xx)] = bitmap[yy * aw + xx];
            }
        }
        stbtt_FreeBitmap(bitmap, NULL);
        int advance, lsb;
        stbtt_GetCodepointHMetrics(&fontinfo, c, &advance, &lsb);
        glyphs[c].advance = advance * font_scale;
        glyphs[c].xoff = (float)bx;
        glyphs[c].yoff = (float)by;
        glyphs[c].w = (float)aw;
        glyphs[c].h = (float)ah;
        glyphs[c].u0 = (float)x / (float)atlas_w;
        glyphs[c].v0 = (float)y / (float)atlas_h;
        glyphs[c].u1 = (float)(x + aw) / (float)atlas_w;
        glyphs[c].v1 = (float)(y + ah) / (float)atlas_h;
        x += aw + 1;
        if (ah > rowh) rowh = ah;
    }
}

static void append_quad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, Color col, float use_tex) {
    size_t base = vtx_count;
    vtx_buf = realloc(vtx_buf, (vtx_count + 6) * sizeof(Vtx));
    vtx_buf[base + 0] = (Vtx){ x0, y0, u0, v0, use_tex, col.r, col.g, col.b, col.a };
    vtx_buf[base + 1] = (Vtx){ x1, y0, u1, v0, use_tex, col.r, col.g, col.b, col.a };
    vtx_buf[base + 2] = (Vtx){ x1, y1, u1, v1, use_tex, col.r, col.g, col.b, col.a };
    vtx_buf[base + 3] = (Vtx){ x0, y0, u0, v0, use_tex, col.r, col.g, col.b, col.a };
    vtx_buf[base + 4] = (Vtx){ x1, y1, u1, v1, use_tex, col.r, col.g, col.b, col.a };
    vtx_buf[base + 5] = (Vtx){ x0, y1, u0, v1, use_tex, col.r, col.g, col.b, col.a };
    vtx_count += 6;
}

static void append_rect(float x, float y, float w, float h, Color col) {
    append_quad(x, y, x + w, y + h, 0.0f, 0.0f, 0.0f, 0.0f, col, 0.0f);
}

static void append_text(const char* text, float x, float y, Color col) {
    if (!text || !atlas) return;
    float pen_x = x;
    float base_y = y + (float)ascent;
    for (const char* p = text; *p; ++p) {
        unsigned char c = (unsigned char)*p;
        if (c < 32 || c >= 128) { pen_x += 8.0f; continue; }
        Glyph* g = &glyphs[c];
        float x0 = pen_x + g->xoff;
        float y0 = base_y + g->yoff;
        float x1 = x0 + g->w;
        float y1 = y0 + g->h;
        append_quad(x0, y0, x1, y1, g->u0, g->v0, g->u1, g->v1, col, 1.0f);
        pen_x += g->advance;
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

/* build vtxs from widgets each frame */
static void build_vertices_from_widgets(void) {
    free(vtx_buf); vtx_buf = NULL; vtx_count = 0;
    for (Widget* w = widgets; w; w = w->next) {
        append_rect(w->rect.x, w->rect.y, w->rect.w, w->rect.h, w->color);
        if (w->text) {
            Color cc = { 1.0f,1.0f,1.0f,1.0f };
            append_text(w->text, w->rect.x + 6.0f, w->rect.y + 6.0f, cc);
        }
    }
}

/* ---------- Main & glue ---------- */
int main(int argc, char** argv) {
    if (argc < 4) { fprintf(stderr, "Usage: %s ui.json shader.vert.spv shader.frag.spv\n", argv[0]); return 1; }
    const char* json_path = argv[1];
    g_vert_spv = argv[2];
    g_frag_spv = argv[3];

    size_t json_len = 0; char* json_text = read_file_text(json_path, &json_len);
    if (!json_text) return 1;
    parse_ui_json(json_text);
    free(json_text);

    if (!glfwInit()) fatal("glfwInit");
    if (!glfwVulkanSupported()) fatal("glfw Vulkan not supported");
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window = glfwCreateWindow(1024, 640, "vk_gui (Vulkan)", NULL, NULL);
    if (!window) fatal("glfwCreateWindow");

    create_instance();
    res = glfwCreateWindowSurface(instance, window, NULL, &surface);
    if (res != VK_SUCCESS) fatal_vk("glfwCreateWindowSurface", res);

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

    /* main loop */
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        draw_frame();
    }

    vkDeviceWaitIdle(device);

    /* cleanup (minimal) */
    free_widgets();
    free(atlas);
    free(ttf_buffer);
    free(vtx_buf);
    destroy_device_resources();
    vkDestroyDevice(device, NULL);
    vkDestroySurfaceKHR(instance, surface, NULL);
    vkDestroyInstance(instance, NULL);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}