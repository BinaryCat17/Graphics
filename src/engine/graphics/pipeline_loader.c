#include "pipeline_loader.h"
#include "foundation/config/simple_yaml.h"
#include "foundation/memory/arena.h"
#include "engine/assets/assets.h"
#include "foundation/logger/logger.h"
#include <string.h>
#include <stdlib.h>

static PixelFormat parse_pixel_format(const char* str) {
    if (!str) return PIXEL_FORMAT_UNKNOWN;
    if (strcmp(str, "RGBA8") == 0) return PIXEL_FORMAT_RGBA8_UNORM;
    if (strcmp(str, "RGB8") == 0) return PIXEL_FORMAT_RGB8_UNORM;
    if (strcmp(str, "R8") == 0) return PIXEL_FORMAT_R8_UNORM;
    if (strcmp(str, "RGBA16F") == 0) return PIXEL_FORMAT_RGBA16_FLOAT;
    if (strcmp(str, "D32") == 0) return PIXEL_FORMAT_D32_SFLOAT;
    return PIXEL_FORMAT_UNKNOWN;
}

bool pipeline_loader_load(Assets* assets, const char* path, PipelineDefinition* out_def) {
    if (!assets || !path || !out_def) return false;

    AssetData data = assets_load_file(assets, path);
    if (!data.data) {
        LOG_ERROR("PipelineLoader: Failed to load file '%s'", path);
        return false;
    }

    MemoryArena arena;
    if (!arena_init(&arena, 64 * 1024)) {
        LOG_ERROR("PipelineLoader: Failed to initialize parsing arena");
        assets_free_file(&data);
        return false;
    }

    ConfigNode* root = NULL;
    ConfigError err = {0};

    if (simple_yaml_parse(&arena, (const char*)data.data, &root, &err) != 1) {
        LOG_ERROR("PipelineLoader: YAML Parse error in '%s' at line %d: %s", path, err.line, err.message);
        arena_destroy(&arena);
        assets_free_file(&data);
        return false;
    }

    memset(out_def, 0, sizeof(PipelineDefinition));

    const ConfigNode* pipeline_node = config_node_map_get(root, "pipeline");
    if (!pipeline_node) {
        LOG_ERROR("PipelineLoader: Root 'pipeline' node not found in '%s'", path);
        arena_destroy(&arena);
        assets_free_file(&data);
        return false;
    }

    // 1. Parse Resources
    const ConfigNode* resources_node = config_node_map_get(pipeline_node, "resources");
    if (resources_node && resources_node->type == CONFIG_NODE_SEQUENCE) {
        for (size_t i = 0; i < resources_node->item_count && out_def->resource_count < PIPELINE_MAX_RESOURCES; ++i) {
            const ConfigNode* res_item = resources_node->items[i];
            PipelineResourceDef* res_def = &out_def->resources[out_def->resource_count++];

            const ConfigNode* name = config_node_map_get(res_item, "name");
            if (name && name->scalar) strncpy(res_def->name, name->scalar, PIPELINE_MAX_NAME_LENGTH - 1);

            const ConfigNode* type = config_node_map_get(res_item, "type");
            if (type && type->scalar) {
                if (strcmp(type->scalar, "IMAGE_2D") == 0) res_def->type = PIPELINE_RESOURCE_IMAGE_2D;
                else if (strcmp(type->scalar, "BUFFER") == 0) res_def->type = PIPELINE_RESOURCE_BUFFER;
            }

            const ConfigNode* format = config_node_map_get(res_item, "format");
            if (format && format->scalar) res_def->format = parse_pixel_format(format->scalar);

            const ConfigNode* size = config_node_map_get(res_item, "size");
            if (size && size->type == CONFIG_NODE_SEQUENCE && size->item_count >= 2) {
                // Check if they are strings with % or numbers
                // For now, let's assume they are either numbers or special strings
                // We'll simplify: if they look like [window_width, window_height], we set scale to 1.0
                if (size->items[0]->scalar && strcmp(size->items[0]->scalar, "window_width") == 0) {
                    res_def->scale_x = 1.0f;
                } else if (size->items[0]->scalar) {
                    res_def->fixed_width = atoi(size->items[0]->scalar);
                }

                if (size->items[1]->scalar && strcmp(size->items[1]->scalar, "window_height") == 0) {
                    res_def->scale_y = 1.0f;
                } else if (size->items[1]->scalar) {
                    res_def->fixed_height = atoi(size->items[1]->scalar);
                }
            }
        }
    }

    // 2. Parse Passes
    const ConfigNode* passes_node = config_node_map_get(pipeline_node, "passes");
    if (passes_node && passes_node->type == CONFIG_NODE_SEQUENCE) {
        for (size_t i = 0; i < passes_node->item_count && out_def->pass_count < PIPELINE_MAX_PASSES; ++i) {
            const ConfigNode* pass_item = passes_node->items[i];
            PipelinePassDef* pass_def = &out_def->passes[out_def->pass_count++];

            const ConfigNode* name = config_node_map_get(pass_item, "name");
            if (name && name->scalar) strncpy(pass_def->name, name->scalar, PIPELINE_MAX_NAME_LENGTH - 1);

            const ConfigNode* type = config_node_map_get(pass_item, "type");
            if (type && type->scalar) {
                if (strcmp(type->scalar, "GRAPHICS") == 0) pass_def->type = PIPELINE_PASS_GRAPHICS;
                else if (strcmp(type->scalar, "COMPUTE") == 0) pass_def->type = PIPELINE_PASS_COMPUTE;
            }

            const ConfigNode* inputs = config_node_map_get(pass_item, "inputs");
            if (inputs && inputs->type == CONFIG_NODE_SEQUENCE) {
                for (size_t j = 0; j < inputs->item_count && j < PIPELINE_MAX_ATTACHMENTS; ++j) {
                    if (inputs->items[j]->scalar) {
                        strncpy(pass_def->inputs[pass_def->input_count++], inputs->items[j]->scalar, PIPELINE_MAX_NAME_LENGTH - 1);
                    }
                }
            }

            const ConfigNode* outputs = config_node_map_get(pass_item, "outputs");
            if (outputs && outputs->type == CONFIG_NODE_SEQUENCE) {
                for (size_t j = 0; j < outputs->item_count && j < PIPELINE_MAX_ATTACHMENTS; ++j) {
                    if (outputs->items[j]->scalar) {
                        strncpy(pass_def->outputs[pass_def->output_count++], outputs->items[j]->scalar, PIPELINE_MAX_NAME_LENGTH - 1);
                    }
                }
            }

            const ConfigNode* draw_list = config_node_map_get(pass_item, "draw_list");
            if (draw_list) {
                if (draw_list->type == CONFIG_NODE_SCALAR) {
                     strncpy(pass_def->draw_lists[pass_def->draw_list_count++], draw_list->scalar, PIPELINE_MAX_NAME_LENGTH - 1);
                } else if (draw_list->type == CONFIG_NODE_SEQUENCE) {
                    for (size_t j = 0; j < draw_list->item_count && j < PIPELINE_MAX_TAGS; ++j) {
                        if (draw_list->items[j]->scalar) {
                            strncpy(pass_def->draw_lists[pass_def->draw_list_count++], draw_list->items[j]->scalar, PIPELINE_MAX_NAME_LENGTH - 1);
                        }
                    }
                }
            }
            
            const ConfigNode* shader = config_node_map_get(pass_item, "shader");
            if (shader && shader->scalar) strncpy(pass_def->shader_path, shader->scalar, PIPELINE_MAX_NAME_LENGTH - 1);
        }
    }

    arena_destroy(&arena);
    assets_free_file(&data);
    LOG_INFO("PipelineLoader: Successfully loaded '%s' (%u resources, %u passes)", path, out_def->resource_count, out_def->pass_count);
    return true;
}
