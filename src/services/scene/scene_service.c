#include "scene_service.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/app_services.h"
#include "services/scene/cad_scene_yaml.h"
#include "core/config/module_yaml_loader.h"
#include "core/service_manager/service_events.h"

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

bool scene_service_load(CoreContext* core, Assets* assets, StateManager* state_manager, int scene_type_id,
                        int model_type_id, const ServiceConfig* config) {
    if (!core || !assets || !state_manager || !config || !config->assets_dir || !config->scene_path) {
        fprintf(stderr, "Scene service load called with invalid arguments.\n");
        return false;
    }

    const char* assets_dir = config->assets_dir;
    const char* scene_path = config->scene_path;

    ConfigError schema_err = {0};

    char* ui_dir = join_path(assets_dir, "ui");
    char* global_config_path = join_path(assets_dir, "config/app.yaml");
    char* ui_schema_path = join_path(assets_dir, "config/ui_schema.yaml");

    if (!ui_dir || !global_config_path || !ui_schema_path) {
        fprintf(stderr, "Failed to allocate memory for schema paths.\n");
        free(ui_dir);
        free(ui_schema_path);
        free(global_config_path);
        return false;
    }

    if (ui_schema_path && module_schema_load(ui_schema_path, &core->ui_schema, &schema_err)) {
        module_schema_register(state_manager, &core->ui_schema, NULL);
        module_load_configs(&core->ui_schema, ui_dir, state_manager);
    } else {
        fprintf(stderr, "UI schema error %s:%d:%d %s\n", ui_schema_path ? ui_schema_path : "(null)", schema_err.line,
                schema_err.column, schema_err.message);
    }

    schema_err = (ConfigError){0};
    if (global_config_path && module_schema_load(global_config_path, &core->global_schema, &schema_err)) {
        module_schema_register(state_manager, &core->global_schema, NULL);
        module_load_configs(&core->global_schema, global_config_path, state_manager);
    } else {
        fprintf(stderr, "Global schema error %s:%d:%d %s\n", global_config_path ? global_config_path : "(null)",
                schema_err.line, schema_err.column, schema_err.message);
    }

    free(ui_dir);
    free(ui_schema_path);
    free(global_config_path);

    SceneError scene_err = {0};
    if (!parse_scene_yaml(scene_path, &core->scene, &scene_err)) {
        fprintf(stderr, "Failed to load scene %s:%d:%d %s\n", scene_path, scene_err.line, scene_err.column,
                scene_err.message);
        scene_service_unload(core);
        return false;
    }

    // Assets are already loaded by AssetsService. We just use them.

    core->model = ui_config_load_model(&assets->ui_doc);
    if (core->model) {
        scene_ui_bind_model(core->model, &core->scene, scene_path);
    }

    SceneComponent scene_component = {.scene = &core->scene, .path = scene_path};
    state_manager_publish(state_manager, STATE_EVENT_COMPONENT_ADDED, scene_type_id, "active",
                          &scene_component, sizeof(scene_component));

    // AssetsComponent is published by AssetsService now.

    ModelComponent model_component = {.model = core->model};
    state_manager_publish(state_manager, STATE_EVENT_COMPONENT_ADDED, model_type_id, "active",
                          &model_component, sizeof(model_component));

    return true;
}

void scene_service_unload(CoreContext* core) {
    if (!core) return;

    if (core->model) {
        free_model(core->model);
        core->model = NULL;
    }

    // Do not free assets here, AssetsService owns them.
    
    scene_dispose(&core->scene);
    free_schemas(core);
}

static bool scene_service_init(void* ptr, const ServiceConfig* config) {
    AppServices* services = (AppServices*)ptr;
    (void)services;
    (void)config;
    return true;
}

static bool scene_service_start(void* ptr, const ServiceConfig* config) {
    AppServices* services = (AppServices*)ptr;
    // Pass pointer to Assets context
    return scene_service_load(&services->core, &services->assets, &services->state_manager, 
                              services->type_id_scene, services->type_id_model, config);
}

static void scene_service_stop(void* ptr) {
    AppServices* services = (AppServices*)ptr;
    scene_service_unload(&services->core);
}

static const char* g_dependencies[] = {"AssetsService"};

static const ServiceDescriptor g_scene_service_descriptor = {
    .name = "scene",
    .dependencies = g_dependencies,
    .dependency_count = 1,
    .init = scene_service_init,
    .start = scene_service_start,
    .stop = scene_service_stop,
    .context = NULL,
    .thread_handle = NULL,
};

const ServiceDescriptor* scene_service_descriptor(void) { return &g_scene_service_descriptor; }