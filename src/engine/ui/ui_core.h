#ifndef UI_CORE_H
#define UI_CORE_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "foundation/string/string_id.h"
#include "foundation/math/coordinate_systems.h"
#include "engine/scene/scene.h"

// --- UI CONSTANTS & FLAGS ---

typedef enum UiBindingTarget {
    BINDING_TARGET_NONE = 0,
    BINDING_TARGET_TEXT,
    BINDING_TARGET_VISIBLE,
    BINDING_TARGET_LAYOUT_X,
    BINDING_TARGET_LAYOUT_Y,
    BINDING_TARGET_LAYOUT_WIDTH,
    BINDING_TARGET_LAYOUT_HEIGHT,
    BINDING_TARGET_STYLE_COLOR,
    BINDING_TARGET_TRANSFORM_POS_X,
    BINDING_TARGET_TRANSFORM_POS_Y,
    BINDING_TARGET_TRANSFORM_POS_Z
} UiBindingTarget;

typedef struct UiBinding UiBinding;

// --- UI SYSTEM API ---

void ui_system_init(void);
void ui_system_shutdown(void);

// UI Node Management
SceneNode* ui_node_create(SceneTree* tree, const SceneNodeSpec* spec, void* data, const MetaStruct* meta);
void ui_node_rebuild_children(SceneNode* el, SceneTree* tree);

// UI Update loop (handles animations, data binding sync)
void ui_node_update(SceneNode* element, float dt);

// Layout & Render
typedef Vec2 (*UiTextMeasureFunc)(const char* text, float scale, void* user_data);
void ui_system_layout(SceneTree* tree, float window_w, float window_h, uint64_t frame_number, UiTextMeasureFunc measure_func, void* measure_data);

// Use render_packet.h for Scene*
struct Scene; 
struct Assets;
struct MemoryArena;
void ui_system_render(SceneTree* tree, struct Scene* scene, const struct Assets* assets, struct MemoryArena* arena);

// Helpers
Rect ui_node_get_screen_rect(const SceneNode* node);

// Data Binding API
typedef enum UiBindingTarget UiBindingTarget;
typedef struct UiBinding UiBinding;
const UiBinding* ui_node_get_binding(const SceneNode* node, UiBindingTarget target);
void ui_node_write_binding_float(SceneNode* node, UiBindingTarget target, float value);
void ui_node_write_binding_string(SceneNode* node, UiBindingTarget target, const char* value);

#endif // UI_CORE_H
