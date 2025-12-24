#ifndef MATH_SERIALIZER_H
#define MATH_SERIALIZER_H

#include "math_graph.h"
#include <stdbool.h>

// Saves the graph to a .gdl (Graph Description Language) file.
// Returns true on success.
bool math_serializer_save_graph(MathGraph* graph, const char* filepath);

// Loads a graph from a .gdl file, replacing the current graph content.
// Returns true on success.
bool math_serializer_load_graph(MathGraph* graph, const char* filepath);

#endif // MATH_SERIALIZER_H
