#ifndef MATH_EDITOR_VIEW_H
#define MATH_EDITOR_VIEW_H

#include "math_editor_internal.h"
#include "engine/scene/scene.h"
#include "foundation/memory/arena.h"

// --- View Model Management ---

MathNodeView* math_editor_add_view(MathEditor* editor, MathNodeId id, float x, float y);
MathNodeView* math_editor_find_view(MathEditor* editor, MathNodeId id);

void math_editor_sync_view_data(MathEditor* editor);
void math_editor_sync_wires(MathEditor* editor);

// --- UI / Rendering ---

// Triggers UI rebuild for the Canvas and Inspector
void math_editor_refresh_graph_view(MathEditor* editor);

// Updates selection state and rebuilds Inspector UI
void math_editor_update_selection(MathEditor* editor);

#endif // MATH_EDITOR_VIEW_H
