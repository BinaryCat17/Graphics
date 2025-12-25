#ifndef VK_TYPES_H
#define VK_TYPES_H

#include <stdbool.h>
#include <stdint.h>
#include <vulkan/vulkan.h>

#include "foundation/math/coordinate_systems.h"
#include "foundation/platform/platform.h"
#include "engine/graphics/internal/backend/vulkan/vulkan_renderer.h"

typedef struct Font Font;
struct VkBufferWrapper; // Forward declaration

typedef struct { float viewport[2]; } ViewConstants;

typedef enum {
    FRAME_AVAILABLE,
    FRAME_FILLING,
    FRAME_SUBMITTED,
} FrameStage;

typedef struct {

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

    // Per-Frame Instance Buffer (Dynamic) removed
    
    VkDescriptorPool frame_descriptor_pool; // For dynamic custom draws
} FrameResources;

typedef struct VulkanRendererState {
    PlatformWindow* window;
    PlatformSurface* platform_surface;
    VkInstance instance;
    VkPhysicalDevice physical_device;
    VkDevice device;
    uint32_t graphics_family;
    VkQueue queue;
    VkSurfaceKHR surface;
    VkSwapchainKHR swapchain;
    
    // Shader Sources (Persisted for Pipeline Recreation)
    struct {
        uint32_t* code;
        size_t size;
    } vert_shader_src;
    
    struct {
        uint32_t* code;
        size_t size;
    } frag_shader_src;

    uint32_t swapchain_img_count;
    VkImage* swapchain_imgs;
    VkImageView* swapchain_imgviews;
    VkFormat swapchain_format;
    VkExtent2D swapchain_extent;
    VkBool32 swapchain_supports_blend;
    VkRenderPass render_pass;
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
    VkCommandPool cmdpool;
    VkCommandBuffer* cmdbuffers;
    VkFramebuffer* framebuffers;
    VkResult res;
    VkSemaphore sem_img_avail;
    VkSemaphore sem_render_done;
    VkFence* fences;
    FrameResources frame_resources[2];
    uint32_t current_frame_cursor;
    int* image_frame_owner;
    VkImage depth_image;
    VkDeviceMemory depth_memory;
    VkImageView depth_image_view;
    VkFormat depth_format;
    VkImage font_image;
    VkDeviceMemory font_image_mem;
    VkImageView font_image_view;
    VkSampler font_sampler;
    VkDescriptorSetLayout descriptor_layout;
    VkDescriptorPool descriptor_pool;
    VkDescriptorSet descriptor_set; // Set 0: Global Textures
    CoordinateSystem2D transformer;
    const Font* font;
    
    // Screenshot State
    bool screenshot_pending;
    char screenshot_path[256];
    void* screenshot_threads_head; // Linked list of active screenshot threads

    // Unified Resources
    struct VkBufferWrapper* unit_quad_buffer;
    struct VkBufferWrapper* unit_quad_index_buffer;
    
    // Instancing (Global Layout, Per-Frame Sets) removed

    // Compute Target (Visualization)
    VkImage compute_target_image;
    VkDeviceMemory compute_target_memory;
    VkImageView compute_target_view;
    VkDescriptorSet compute_target_descriptor; // Set 2 (Sampling)
    VkDescriptorSet compute_write_descriptor;  // Set 0 (Compute Writing)
    VkDescriptorSetLayout compute_write_layout;
    int compute_width;
    int compute_height;
    
    // Compute Sync
    VkFence compute_fence;
    VkCommandBuffer compute_cmd;

    // --- Compute Pipeline Pool ---
#define MAX_COMPUTE_PIPELINES 32
    struct {
        bool active;
        VkPipeline pipeline;
        VkPipelineLayout layout;
        VkDescriptorSetLayout set_layouts[4];
        uint32_t set_layout_count;
    } compute_pipelines[MAX_COMPUTE_PIPELINES];

#define MAX_COMPUTE_BINDINGS 16
    struct {
        struct VkBufferWrapper* buffer; // Pointer to wrapper
    } compute_bindings[MAX_COMPUTE_BINDINGS];
    
    VkDescriptorSetLayout compute_ssbo_layout; // Layout for Set 1 (Buffers)
    VkDescriptorSet compute_ssbo_descriptor;   // Set 1 Instance (For legacy Compute Dispatch)

    // --- Graphics Pipeline Pool ---
#define MAX_GRAPHICS_PIPELINES 32
    struct {
        bool active;
        VkPipeline pipeline;
        VkPipelineLayout layout;
        VkDescriptorSetLayout set_layouts[4];
        uint32_t set_layout_count;
    } graphics_pipelines[MAX_GRAPHICS_PIPELINES];

    struct {
        struct VkBufferWrapper* buffer;
    } graphics_bindings[MAX_COMPUTE_BINDINGS];

    // --- Dynamic Textures ---
#define MAX_DYNAMIC_TEXTURES 64
    struct {
        bool active;
        uint32_t width;
        uint32_t height;
        uint32_t format; // 0=RGBA8, 1=RGBA16F, 2=D32
        
        VkImage image;
        VkDeviceMemory memory;
        VkImageView view;
        VkSampler sampler;
        
        VkDescriptorSet descriptor; // (Optional) Cached descriptor for sampling
    } textures[MAX_DYNAMIC_TEXTURES];

} VulkanRendererState;

#endif // VK_TYPES_H
