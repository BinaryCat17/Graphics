#include "scene_service.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scene/cad_scene_yaml.h"
#include "config/module_yaml_loader.h"
#include "services/service_events.h"

static char* join_path(const char* dir, const char* leaf) {
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

static void free_schemas(CoreContext* core) {
    module_schema_free(&core->ui_schema);
    module_schema_free(&core->global_schema);
}

bool scene_service_load(AppServices* services, const ServiceConfig* config) {
    if (!services || !config || !config->assets_dir || !config->scene_path) {
        fprintf(stderr, "Scene service load called with invalid arguments.\n");
        return false;
    }

    const char* assets_dir = config->assets_dir;
    const char* scene_path = config->scene_path;
    CoreContext* core = &services->core;

    ConfigError schema_err = {0};

    char* ui_dir = join_path(assets_dir, "ui");
    char* global_dir = join_path(assets_dir, "global_state");
    char* ui_schema_path = join_path(ui_dir, "schema.yaml");
    char* global_schema_path = join_path(global_dir, "schema.yaml");

    if (!ui_dir || !global_dir || !ui_schema_path || !global_schema_path) {
        fprintf(stderr, "Failed to allocate memory for schema paths.\n");
        free(ui_dir);
        free(global_dir);
        free(ui_schema_path);
        free(global_schema_path);
        return false;
    }

    if (ui_schema_path && module_schema_load(ui_schema_path, &core->ui_schema, &schema_err)) {
        module_schema_register(&services->state_manager, &core->ui_schema, NULL);
        char* ui_config = join_path(ui_dir, "config");
        module_load_configs(&core->ui_schema, ui_config, &services->state_manager);
        free(ui_config);
    } else {
        fprintf(stderr, "UI schema error %s:%d:%d %s\n", ui_schema_path ? ui_schema_path : "(null)", schema_err.line,
                schema_err.column, schema_err.message);
    }

    schema_err = (ConfigError){0};
    if (global_schema_path && module_schema_load(global_schema_path, &core->global_schema, &schema_err)) {
        module_schema_register(&services->state_manager, &core->global_schema, NULL);
        char* global_config = join_path(global_dir, "config");
        module_load_configs(&core->global_schema, global_config, &services->state_manager);
        free(global_config);
    } else {
        fprintf(stderr, "Global schema error %s:%d:%d %s\n", global_schema_path ? global_schema_path : "(null)",
                schema_err.line, schema_err.column, schema_err.message);
    }

    free(ui_dir);
    free(global_dir);
    free(ui_schema_path);
    free(global_schema_path);

    SceneError scene_err = {0};
    if (!parse_scene_yaml(scene_path, &core->scene, &scene_err)) {
        fprintf(stderr, "Failed to load scene %s:%d:%d %s\n", scene_path, scene_err.line, scene_err.column,
                scene_err.message);
        scene_service_unload(services);
        return false;
    }

    if (!load_assets(assets_dir, config->ui_config_path, &core->assets)) {
        fprintf(stderr, "Failed to load assets from '%s'.\n", assets_dir);
        scene_service_unload(services);
        return false;
    }

    core->model = ui_config_load_model(&core->assets.ui_doc);
    if (core->model) {
        scene_ui_bind_model(core->model, &core->scene, scene_path);
    }

    SceneComponent scene_component = {.scene = &core->scene, .path = scene_path};
    state_manager_publish(&services->state_manager, STATE_EVENT_COMPONENT_ADDED, services->scene_type_id, "active",
                          &scene_component, sizeof(scene_component));

    AssetsComponent assets_component = {.assets = &core->assets};
    state_manager_publish(&services->state_manager, STATE_EVENT_COMPONENT_ADDED, services->assets_type_id, "active",
                          &assets_component, sizeof(assets_component));

    ModelComponent model_component = {.model = core->model};
    state_manager_publish(&services->state_manager, STATE_EVENT_COMPONENT_ADDED, services->model_type_id, "active",
                          &model_component, sizeof(model_component));

    return true;
}

void scene_service_unload(AppServices* services) {
    if (!services) return;

    CoreContext* core = &services->core;

    if (core->model) {
        free_model(core->model);
        core->model = NULL;
    }

    free_assets(&core->assets);
    scene_dispose(&core->scene);
    free_schemas(core);
}

static bool scene_service_init(AppServices* services, const ServiceConfig* config) {
    (void)services;
    (void)config;
    return true;
}

static bool scene_service_start(AppServices* services, const ServiceConfig* config) {
    return scene_service_load(services, config);
}

static void scene_service_stop(AppServices* services) {
    scene_service_unload(services);
}

static const ServiceDescriptor g_scene_service_descriptor = {
    .name = "scene",
    .dependencies = NULL,
    .dependency_count = 0,
    .init = scene_service_init,
    .start = scene_service_start,
    .stop = scene_service_stop,
    .context = NULL,
    .thread_handle = NULL,
};

const ServiceDescriptor* scene_service_descriptor(void) { return &g_scene_service_descriptor; }
