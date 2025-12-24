#include "math_serializer.h"
#include "internal/math_graph_internal.h"
#include "foundation/logger/logger.h"
#include "foundation/platform/fs.h"
#include "foundation/config/simple_yaml.h"
#include "foundation/memory/arena.h"
#include "foundation/meta/reflection.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- Helper: Node Type to String ---

static const char* get_node_type_str(MathNodeType type) {
    const MetaEnum* e = meta_get_enum("MathNodeType");
    if (e) {
        const char* name = meta_enum_get_name(e, (int)type);
        // Skip "MATH_NODE_" prefix if present for cleaner files
        if (name && strncmp(name, "MATH_NODE_", 10) == 0) {
            return name + 10;
        }
        return name ? name : "UNKNOWN";
    }
    return "UNKNOWN";
}

static MathNodeType get_node_type_from_str(const char* str) {
    if (!str) return MATH_NODE_VALUE;
    
    char buf[64];
    snprintf(buf, 64, "MATH_NODE_%s", str);
    
    const MetaEnum* e = meta_get_enum("MathNodeType");
    int val = 0;
    if (e && meta_enum_get_value(e, buf, &val)) {
        return (MathNodeType)val;
    }
    
    // Try without prefix just in case standard changes
    if (e && meta_enum_get_value(e, str, &val)) {
        return (MathNodeType)val;
    }

    return MATH_NODE_VALUE; // Default fallback
}

// --- Helper: YAML Scalar Access ---

static const char* config_node_map_get_scalar(const ConfigNode* map, const char* key, const char* default_val) {
    const ConfigNode* node = config_node_map_get(map, key);
    if (node && node->scalar) {
        return node->scalar;
    }
    return default_val;
}

// --- SAVE IMPLEMENTATION ---

// Helper struct to map Runtime ID -> Unique String Name
typedef struct {
    MathNodeId id;
    char unique_name[64];
} NodeNameEntry;

bool math_serializer_save_graph(MathGraph* graph, const char* filepath) {
    if (!graph || !filepath) return false;

    FILE* f = fopen(filepath, "w");
    if (!f) {
        LOG_ERROR("Serializer: Failed to open file for writing: %s", filepath);
        return false;
    }

    LOG_INFO("Serializer: Saving graph to %s...", filepath);

    // 1. Header
    fprintf(f, "format: \"gdl-1.0\"\n");
    fprintf(f, "nodes:\n");

    // 2. Generate Unique Names map
    // We assume graph->node_capacity is enough for iteration
    // We need a temporary map because multiple nodes might have name "Add".
    // We will append _1, _2 etc.
    
    uint32_t count = 0;
    NodeNameEntry* name_map = (NodeNameEntry*)calloc(graph->node_capacity, sizeof(NodeNameEntry));
    
    // First pass: Collect basic names and handle duplicates
    for (uint32_t i = 0; i < graph->node_capacity; ++i) {
        MathNode* node = graph->node_ptrs[i];
        if (!node || node->type == MATH_NODE_NONE) continue;
        
        count++;
        name_map[i].id = i;
        
        // Base name
        const char* base_name = (node->name[0] != '\0') ? node->name : "Node";
        strncpy(name_map[i].unique_name, base_name, 63);
        
        // Simple O(N^2) duplicate check/resolution (fine for < 1000 nodes)
        int duplicate_idx = 0;
        bool unique = false;
        
        while (!unique) {
            unique = true;
            // Check against all PREVIOUS processed nodes
            for (uint32_t k = 0; k < i; ++k) {
                if (graph->node_ptrs[k] && graph->node_ptrs[k]->type != MATH_NODE_NONE) {
                    if (strcmp(name_map[k].unique_name, name_map[i].unique_name) == 0) {
                        unique = false;
                        duplicate_idx++;
                        // Regenerate name
                        snprintf(name_map[i].unique_name, 63, "%s_%d", base_name, duplicate_idx);
                        break; // Check again with new name
                    }
                }
            }
        }
    }

    // 3. Write Nodes
    for (uint32_t i = 0; i < graph->node_capacity; ++i) {
        MathNode* node = graph->node_ptrs[i];
        if (!node || node->type == MATH_NODE_NONE) continue;

        fprintf(f, "  - name: \"%s\"\n", name_map[i].unique_name);
        fprintf(f, "    type: %s\n", get_node_type_str(node->type));
        
        // Properties
        if (node->type == MATH_NODE_VALUE) {
            fprintf(f, "    value: %f\n", node->value);
        }
        
        // UI Layout (Optional but useful)
        // We need to fetch this from somewhere.
        // Currently MathNode doesn't store X/Y, MathNodeView does.
        // The Graph itself doesn't know about UI positions (Separation of Concerns).
        // BUT, for a file format, we usually want to save positions.
        // COMPROMISE: We will save X/Y if we can find a way, or we accept that 
        // Logic Only serialization loses layout.
        // WAITING: For now, we save logic. The user didn't explicitly ask for UI persistence yet, 
        // but it's implied. 
        // To fix this properly, MathGraph should probably own the layout, or we need to pass a context.
        // For this task, I will stick to Logic.
        
        // Actually, let's look at MathEditor... it holds the Views.
        // The Serializer only takes MathGraph.
        // We will add a TODO for UI positions or require MathGraph to hold metadata.
        // OR: We can change the signature of save to accept optional View data.
        // For now, let's output 0,0 placeholders or skip.
    }

    fprintf(f, "links:\n");

    // 4. Write Links
    // We iterate all nodes and their inputs
    for (uint32_t i = 0; i < graph->node_capacity; ++i) {
        MathNode* node = graph->node_ptrs[i];
        if (!node || node->type == MATH_NODE_NONE) continue;

        for (int k = 0; k < MATH_NODE_MAX_INPUTS; ++k) {
            MathNodeId src_id = node->inputs[k];
            if (src_id != MATH_NODE_INVALID_ID) {
                // Find source name
                // Safe because src_id must be valid if link exists
                if (src_id < graph->node_capacity && graph->node_ptrs[src_id]) {
                     fprintf(f, "  - src: \"%s\"\n", name_map[src_id].unique_name);
                     fprintf(f, "    dst: [\"%s\", %d]\n", name_map[i].unique_name, k);
                }
            }
        }
    }

    free(name_map);
    fclose(f);
    LOG_INFO("Serializer: Saved %u nodes.", count);
    return true;
}

// --- LOAD IMPLEMENTATION ---

bool math_serializer_load_graph(MathGraph* graph, const char* filepath) {
    if (!graph || !filepath) return false;

    // 1. Read File
    // We need a scratch arena for the parser
    MemoryArena scratch;
    arena_init(&scratch, 1024 * 1024); // 1MB

    char* text = fs_read_text(&scratch, filepath);
    if (!text) {
        LOG_ERROR("Serializer: File not found: %s", filepath);
        arena_destroy(&scratch);
        return false;
    }

    ConfigNode* root = NULL;
    ConfigError err = {0};
    if (!simple_yaml_parse(&scratch, text, &root, &err)) {
        LOG_ERROR("Serializer: YAML Error: %s", err.message);
        arena_destroy(&scratch);
        return false;
    }

    // 2. Clear existing graph
    math_graph_clear(graph);

    // 3. Parse Nodes & Build Name Map
    // We need to map Name (String) -> ID (Runtime)
    // Using a simple linear search or hash map. Given N is small (<1000), 
    // we can just store an array of structs in scratch.

    typedef struct {
        char* name;
        MathNodeId id;
    } NameMap;
    
    NameMap* name_map = NULL;
    size_t name_map_count = 0;
    size_t name_map_cap = 0;

    const ConfigNode* nodes_node = config_node_map_get(root, "nodes");
    if (nodes_node && nodes_node->type == CONFIG_NODE_SEQUENCE) {
        name_map_cap = nodes_node->item_count;
        name_map = arena_alloc(&scratch, name_map_cap * sizeof(NameMap));

        for (size_t i = 0; i < nodes_node->item_count; ++i) {
            ConfigNode* n = nodes_node->items[i];
            if (n->type != CONFIG_NODE_MAP) continue;

            // Get Name
            const char* name_str = config_node_map_get_scalar(n, "name", NULL);
            // Get Type
            const char* type_str = config_node_map_get_scalar(n, "type", NULL);
            // Get Value (Optional)
            const char* val_str = config_node_map_get_scalar(n, "value", "0.0");

            MathNodeType type = get_node_type_from_str(type_str);
            
            // Create Node
            MathNodeId id = math_graph_add_node(graph, type);
            if (name_str) math_graph_set_name(graph, id, name_str);
            math_graph_set_value(graph, id, (float)atof(val_str));

            // Store in Map
            if (name_str) {
                name_map[name_map_count].name = (char*)name_str; // Pointer to arena memory
                name_map[name_map_count].id = id;
                name_map_count++;
            }
        }
    }

    // 4. Parse Links
    const ConfigNode* links_node = config_node_map_get(root, "links");
    if (links_node && links_node->type == CONFIG_NODE_SEQUENCE) {
        for (size_t i = 0; i < links_node->item_count; ++i) {
            ConfigNode* l = links_node->items[i];
            
            const char* src_name = config_node_map_get_scalar(l, "src", NULL);
            const ConfigNode* dst_node = config_node_map_get(l, "dst");

            if (!src_name || !dst_node) continue;

            // Find Source ID
            MathNodeId src_id = MATH_NODE_INVALID_ID;
            for(size_t k=0; k<name_map_count; ++k) {
                if (strcmp(name_map[k].name, src_name) == 0) {
                    src_id = name_map[k].id;
                    break;
                }
            }
            if (src_id == MATH_NODE_INVALID_ID) {
                LOG_WARN("Serializer: Unknown source node '%s'", src_name);
                continue;
            }

            // Parse Dst
            const char* dst_name = NULL;
            int dst_slot = 0;

            if (dst_node->type == CONFIG_NODE_SEQUENCE && dst_node->item_count >= 2) {
                dst_name = dst_node->items[0]->scalar;
                if (dst_node->items[1]->scalar) dst_slot = atoi(dst_node->items[1]->scalar);
            }

            if (!dst_name) continue;

            // Find Dst ID
            MathNodeId dst_id = MATH_NODE_INVALID_ID;
            for(size_t k=0; k<name_map_count; ++k) {
                if (strcmp(name_map[k].name, dst_name) == 0) {
                    dst_id = name_map[k].id;
                    break;
                }
            }

            if (dst_id != MATH_NODE_INVALID_ID) {
                math_graph_connect(graph, dst_id, dst_slot, src_id);
            } else {
                LOG_WARN("Serializer: Unknown dest node '%s'", dst_name);
            }
        }
    }

    arena_destroy(&scratch);
    LOG_INFO("Serializer: Loaded graph from %s", filepath);
    return true;
}
