#include "scene_service.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cad_scene_yaml.h"
#include "module_yaml_loader.h"

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

static void free_schemas(AppServices* services) {
    module_schema_free(&services->ui_schema);
    module_schema_free(&services->global_schema);
}

bool scene_service_load(AppServices* services, const char* assets_dir, const char* scene_path) {
    if (!services || !assets_dir || !scene_path) return false;

    ConfigError schema_err = {0};
    state_manager_init(&services->state_manager, 8, 32);

    char* ui_dir = join_path(assets_dir, "ui");
    char* global_dir = join_path(assets_dir, "global_state");
    char* ui_schema_path = join_path(ui_dir, "schema.yaml");
    char* global_schema_path = join_path(global_dir, "schema.yaml");

    if (ui_schema_path && module_schema_load(ui_schema_path, &services->ui_schema, &schema_err)) {
        module_schema_register(&services->state_manager, &services->ui_schema, NULL);
        char* ui_config = join_path(ui_dir, "config");
        module_load_configs(&services->ui_schema, ui_config, &services->state_manager);
        free(ui_config);
    } else {
        fprintf(stderr, "UI schema error %s:%d:%d %s\n", ui_schema_path ? ui_schema_path : "(null)", schema_err.line, schema_err.column, schema_err.message);
    }

    schema_err = (ConfigError){0};
    if (global_schema_path && module_schema_load(global_schema_path, &services->global_schema, &schema_err)) {
        module_schema_register(&services->state_manager, &services->global_schema, NULL);
        char* global_config = join_path(global_dir, "config");
        module_load_configs(&services->global_schema, global_config, &services->state_manager);
        free(global_config);
    } else {
        fprintf(stderr, "Global schema error %s:%d:%d %s\n", global_schema_path ? global_schema_path : "(null)", schema_err.line, schema_err.column, schema_err.message);
    }

    free(ui_dir);
    free(global_dir);
    free(ui_schema_path);
    free(global_schema_path);

    SceneError scene_err = {0};
    if (!parse_scene_yaml(scene_path, &services->scene, &scene_err)) {
        fprintf(stderr, "Failed to load scene %s:%d:%d %s\n", scene_path, scene_err.line, scene_err.column, scene_err.message);
        scene_service_unload(services);
        return false;
    }

    if (!load_assets(assets_dir, &services->assets)) {
        scene_service_unload(services);
        return false;
    }

    services->model = parse_model_config(services->assets.model_doc.root, services->assets.model_path);
    if (services->model) {
        scene_ui_bind_model(services->model, &services->scene, scene_path);
    }

    return true;
}

void scene_service_unload(AppServices* services) {
    if (!services) return;

    if (services->model) {
        save_model(services->model);
        free_model(services->model);
        services->model = NULL;
    }

    free_assets(&services->assets);
    scene_dispose(&services->scene);
    free_schemas(services);
    state_manager_dispose(&services->state_manager);
}
