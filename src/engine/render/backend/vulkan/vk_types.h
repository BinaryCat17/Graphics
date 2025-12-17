#ifndef VK_TYPES_H
#define VK_TYPES_H

#include <stdbool.h>
#include <stdint.h>
#include <vulkan/vulkan.h>

#include "foundation/math/coordinate_systems.h"
#include "foundation/platform/platform.h"
#include "engine/render/backend/vulkan/vulkan_renderer.h"

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
} FrameResources;

typedef struct VulkanRendererState {
    PlatformWindow* window;
    PlatformSurface* platform_surface;
    // UiDrawList ui_draw_list; // Removed
    VkInstance instance;
    VkPhysicalDevice physical_device;
    VkDevice device;
    uint32_t graphics_family;
    VkQueue queue;
    VkSurfaceKHR surface;
    VkSwapchainKHR swapchain;
    const char* vert_spv;
    const char* frag_spv;
    const char* font_path;
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
    VkDescriptorSet descriptor_set;
    CoordinateSystem2D transformer;
    RenderLogger* logger;
    
    // Unified Resources
    VkBuffer unit_quad_buffer;
    VkDeviceMemory unit_quad_memory;
    
    // Instancing
    VkBuffer instance_buffer;
    VkDeviceMemory instance_memory;
    void* instance_mapped;
    VkDescriptorSetLayout instance_layout;
    VkDescriptorSet instance_set;
    size_t instance_capacity; // In element count

    bool (*get_required_instance_extensions)(const char*** names, uint32_t* count);
    bool (*create_surface)(PlatformWindow* window, VkInstance instance, const VkAllocationCallbacks* alloc,
                           PlatformSurface* out_surface);
    void (*destroy_surface)(VkInstance instance, const VkAllocationCallbacks* alloc, PlatformSurface* surface);
    PlatformWindowSize (*get_framebuffer_size)(PlatformWindow* window);
    void (*wait_events)(void);
} VulkanRendererState;

#endif // VK_TYPES_H
