#include "engine/core/engine.h"
#include "foundation/logger/logger.h"
#include <string.h>
#include <stdlib.h>

int main(int argc, char** argv) {
    // 1. Config
    EngineConfig config = {
        .width = 1280,
        .height = 720,
        .title = "Graphics Engine",
        .assets_path = "assets",
        .ui_path = "assets/ui/editor.yaml",
        .log_level = LOG_LEVEL_INFO
    };

    logger_init("logs/graphics.log");

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--assets") == 0 && i + 1 < argc) {
            config.assets_path = argv[++i];
        } else if (strcmp(argv[i], "--ui") == 0 && i + 1 < argc) {
            config.ui_path = argv[++i];
        } else if (strcmp(argv[i], "--log-level") == 0 && i + 1 < argc) {
            const char* level_str = argv[++i];
            if (strcmp(level_str, "trace") == 0) config.log_level = LOG_LEVEL_TRACE;
            else if (strcmp(level_str, "debug") == 0) config.log_level = LOG_LEVEL_DEBUG;
            else if (strcmp(level_str, "info") == 0) config.log_level = LOG_LEVEL_INFO;
            else if (strcmp(level_str, "warn") == 0) config.log_level = LOG_LEVEL_WARN;
            else if (strcmp(level_str, "error") == 0) config.log_level = LOG_LEVEL_ERROR;
            else if (strcmp(level_str, "fatal") == 0) config.log_level = LOG_LEVEL_FATAL;
        } else if (strcmp(argv[i], "--log-interval") == 0 && i + 1 < argc) {
             logger_set_trace_interval(atof(argv[++i]));
        }
    }

    // 2. Engine Lifecycle
    Engine engine;
    if (engine_init(&engine, &config)) {
        engine_run(&engine);
        engine_shutdown(&engine);
    } else {
        LOG_FATAL("Engine failed to initialize.");
        return 1;
    }

    logger_shutdown();
    return 0;
}