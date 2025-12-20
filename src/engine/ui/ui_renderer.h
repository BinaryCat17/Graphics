#ifndef UI_RENDERER_H
#define UI_RENDERER_H

#include <stdint.h>

// Forward Declarations
typedef struct UiInstance UiInstance;
typedef struct Scene Scene;
typedef struct Assets Assets;
typedef struct MemoryArena MemoryArena;

// --- High-Level Pipeline API ---

typedef float (*UiTextMeasureFunc)(const char* text, void* user_data);

// Layout & Render Pipeline
// frame_number: used to optimize layout caching (recalc only once per frame per node)
void ui_instance_layout(UiInstance* instance, float window_w, float window_h, uint64_t frame_number, UiTextMeasureFunc measure_func, void* measure_data);

// Generates render commands into the Scene.
// arena: Frame allocator for temporary render structures (e.g. overlay lists)
void ui_instance_render(UiInstance* instance, Scene* scene, const Assets* assets, MemoryArena* arena);

#endif // UI_RENDERER_H
