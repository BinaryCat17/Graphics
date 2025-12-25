#ifndef MATH_EDITOR_H
#define MATH_EDITOR_H

// Forward Declarations
typedef struct Engine Engine;
typedef struct Scene Scene;
typedef struct Assets Assets;
typedef struct MemoryArena MemoryArena;

// Opaque Handle
typedef struct MathEditor MathEditor;

// API

// Creates the editor instance (allocates memory)
MathEditor* math_editor_create(Engine* engine);

// Updates the editor state (Input, UI Layout, Transpilation)
void math_editor_update(MathEditor* editor, Engine* engine);

// Renders the editor UI to the provided scene
void math_editor_render(MathEditor* editor, Scene* scene, const Assets* assets, MemoryArena* arena);

// Shuts down the editor and frees resources
void math_editor_destroy(MathEditor* editor);

// Feature Factory
#include "engine/core/engine.h"
EngineFeature math_engine_feature(void);

#endif // MATH_EDITOR_H
