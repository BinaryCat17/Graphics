#ifndef PIPELINE_LOADER_H
#define PIPELINE_LOADER_H

#include "pipeline.h"
#include <stdbool.h>

typedef struct Assets Assets;

bool pipeline_loader_load(Assets* assets, const char* path, PipelineDefinition* out_def);

#endif // PIPELINE_LOADER_H
