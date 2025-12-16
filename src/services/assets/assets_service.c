#include "services/assets/assets_service.h"
#include "core/platform/platform.h"
#include "app/app_services.h"
#include "core/service_manager/service_events.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char* join_path(const char* dir, const char* leaf) {
    if (!dir || !leaf) return NULL;
    size_t dir_len = strlen(dir);
    while (dir_len > 0 && dir[dir_len - 1] == '/') dir_len--;
    size_t leaf_len = strlen(leaf);
    size_t total = dir_len + 1 + leaf_len + 1;
    char* out = (char*)malloc(total);
    if (!out) return NULL;
    memcpy(out, dir, dir_len);
    out[dir_len] = '/';
    memcpy(out + dir_len + 1, leaf, leaf_len);
    out[total - 1] = 0;
    return out;
}

static void free_paths(Assets* assets) {
    free(assets->ui_path);
    free(assets->vert_spv_path);
    free(assets->frag_spv_path);
    free(assets->font_path);
    assets->ui_path = NULL;
    assets->vert_spv_path = NULL;
    assets->frag_spv_path = NULL;
    assets->font_path = NULL;
}

int load_assets(const char* assets_dir, const char* ui_config_path, Assets* out_assets) {
    if (!out_assets) return 0;

    // Do not memset 0 here if it's already initialized or if we want to preserve some state?
    // Actually, clean slate is better.
    // Ensure we don't leak if called twice (though start is called once).
    free_paths(out_assets);
    config_document_free(&out_assets->ui_doc);
    memset(out_assets, 0, sizeof(*out_assets));

    if (ui_config_path && ui_config_path[0]) {
        out_assets->ui_path = platform_strdup(ui_config_path);
    } else {
        out_assets->ui_path = join_path(assets_dir, "ui/ui.yaml");
    }
    out_assets->vert_spv_path = join_path(assets_dir, "shaders/shader.vert.spv");
    out_assets->frag_spv_path = join_path(assets_dir, "shaders/shader.frag.spv");
    out_assets->font_path = join_path(assets_dir, "fonts/font.ttf");

    if (!out_assets->ui_path || !out_assets->vert_spv_path || !out_assets->frag_spv_path || !out_assets->font_path) {
        fprintf(stderr, "Fatal: failed to compose asset paths for directory '%s'\n", assets_dir);
        free_assets(out_assets);
        return 0;
    }

    ConfigError err = {0};
    if (!load_config_document(out_assets->ui_path, CONFIG_FORMAT_YAML, &out_assets->ui_doc, &err)) {
        fprintf(stderr, "Failed to load %s: %s\n", out_assets->ui_path, err.message);
        free_assets(out_assets);
        return 0;
    }
    return 1;
}

void free_assets(Assets* assets) {
    if (!assets) return;
    free_paths(assets);
    config_document_free(&assets->ui_doc);
    memset(assets, 0, sizeof(*assets));
}

// --- Service Implementation ---

static bool assets_service_init(void* ptr, const ServiceConfig* config) {
    (void)ptr; (void)config;
    return true; // Nothing to init strictly, memory is zeroed by manager usually
}

static bool assets_service_start(void* ptr, const ServiceConfig* config) {
    AppServices* services = (AppServices*)ptr;
    if (!services || !config || !config->assets_dir) return false;

    // Use the context allocated in AppServices (GeneratedServicesContext)
    Assets* assets_ctx = &services->assets;

    if (!load_assets(config->assets_dir, config->ui_config_path, assets_ctx)) {
        return false;
    }

    // Publish component
    AssetsComponent comp = { .assets = assets_ctx };
    state_manager_publish(&services->state_manager, STATE_EVENT_COMPONENT_ADDED, 
                          services->type_id_assets, "active", &comp, sizeof(comp));

    return true;
}

static void assets_service_stop(void* ptr) {
    AppServices* services = (AppServices*)ptr;
    if (!services) return;
    free_assets(&services->assets);
}

static const ServiceDescriptor g_assets_service_descriptor = {
    .name = "AssetsService",
    .init = assets_service_init,
    .start = assets_service_start,
    .stop = assets_service_stop,
    // Dependencies? None strictly (except config).
};

const ServiceDescriptor* assets_service_descriptor(void) {
    return &g_assets_service_descriptor;
}