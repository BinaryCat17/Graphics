#ifndef PIPELINE_H
#define PIPELINE_H

#include <stdint.h>
#include <stdbool.h>
#include "graphics_types.h"

// Max limits for static arrays to avoid complex allocation in config structs
#define PIPELINE_MAX_NAME_LENGTH 64
#define PIPELINE_MAX_RESOURCES 32
#define PIPELINE_MAX_PASSES 32
#define PIPELINE_MAX_ATTACHMENTS 8
#define PIPELINE_MAX_TAGS 8

typedef enum PipelineResourceType {
    PIPELINE_RESOURCE_IMAGE_2D,
    PIPELINE_RESOURCE_BUFFER
} PipelineResourceType;

typedef enum PipelinePassType {
    PIPELINE_PASS_GRAPHICS,
    PIPELINE_PASS_COMPUTE
} PipelinePassType;

// Defines a resource (Image or Buffer)
typedef struct PipelineResourceDef {
    char name[PIPELINE_MAX_NAME_LENGTH];
    PipelineResourceType type;
    
    // Format for images
    PixelFormat format;
    
    // Size configuration
    // If scale > 0, size is calculated relative to window (e.g. 1.0 = full width)
    // If scale == 0, fixed_size is used
    float scale_x; 
    float scale_y;
    uint32_t fixed_width;
    uint32_t fixed_height;
} PipelineResourceDef;

// Defines a single pass
typedef struct PipelinePassDef {
    char name[PIPELINE_MAX_NAME_LENGTH];
    PipelinePassType type;
    
    // For Graphics: Input attachments (textures to sample/read)
    char inputs[PIPELINE_MAX_ATTACHMENTS][PIPELINE_MAX_NAME_LENGTH];
    uint32_t input_count;
    
    // For Graphics: Output attachments (render targets)
    char outputs[PIPELINE_MAX_ATTACHMENTS][PIPELINE_MAX_NAME_LENGTH];
    uint32_t output_count;
    
    // Tags for RenderBatches (e.g., "UIBatches", "SceneBatches")
    char draw_lists[PIPELINE_MAX_TAGS][PIPELINE_MAX_NAME_LENGTH];
    uint32_t draw_list_count;

    // For Compute: Shader to execute
    char shader_path[PIPELINE_MAX_NAME_LENGTH];
    
    // Optional clear color for outputs (RGBA)
    // If should_clear is true, attachments are cleared before rendering
    float clear_color[4];
    bool should_clear;
} PipelinePassDef;

// The full pipeline definition
typedef struct PipelineDefinition {
    PipelineResourceDef resources[PIPELINE_MAX_RESOURCES];
    uint32_t resource_count;

    PipelinePassDef passes[PIPELINE_MAX_PASSES];
    uint32_t pass_count;
} PipelineDefinition;

#endif // PIPELINE_H
