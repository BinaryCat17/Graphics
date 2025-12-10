#include "render_runtime_service.h"

#include <GLFW/glfw3.h>
#include <stdio.h>

#include "render/vulkan_renderer.h"
#include "runtime/runtime.h"
#include "service_events.h"

static ServiceDescriptor g_render_runtime_service_descriptor;

static void render_runtime_service_reset(RenderRuntimeServiceContext* context, AppServices* services) {
    if (!context) return;
    *context = (RenderRuntimeServiceContext){
        .render = services ? &services->render : NULL,
        .state_manager = services ? &services->state_manager : NULL,
        .assets_type_id = services ? services->assets_type_id : -1,
        .ui_type_id = services ? services->ui_type_id : -1,
        .model_type_id = services ? services->model_type_id : -1,
    };
}

static void try_bootstrap_renderer(RenderRuntimeServiceContext* context) {
    if (!context || context->renderer_ready) return;
    if (!context->render || !context->render->window || !context->assets || !context->widgets.items) return;

    context->renderer_ready = vk_renderer_init(context->render->window, context->assets->vert_spv_path,
                                               context->assets->frag_spv_path, context->assets->font_path,
                                               context->widgets, &context->render->transformer);
}

static void on_assets_event(const StateEvent* event, void* user_data) {
    RenderRuntimeServiceContext* context = (RenderRuntimeServiceContext*)user_data;
    if (!context || !event || !event->payload) return;
    const AssetsComponent* component = (const AssetsComponent*)event->payload;
    context->assets = component->assets;
    try_bootstrap_renderer(context);
}

static void on_ui_event(const StateEvent* event, void* user_data) {
    RenderRuntimeServiceContext* context = (RenderRuntimeServiceContext*)user_data;
    if (!context || !event || !event->payload) return;
    const UiRuntimeComponent* component = (const UiRuntimeComponent*)event->payload;
    context->ui = component->ui;
    context->widgets = component->widgets;
    try_bootstrap_renderer(context);
}

static void on_model_event(const StateEvent* event, void* user_data) {
    RenderRuntimeServiceContext* context = (RenderRuntimeServiceContext*)user_data;
    if (!context || !event || !event->payload) return;
    const ModelComponent* component = (const ModelComponent*)event->payload;
    context->model = component->model;
}

static bool render_runtime_service_bind(RenderRuntimeServiceContext* context, AppServices* services) {
    if (!context || !services) {
        fprintf(stderr, "Render runtime service bind received invalid arguments.\n");
        return false;
    }
    render_runtime_service_reset(context, services);

    if (context->assets_type_id >= 0) {
        state_manager_subscribe(context->state_manager, context->assets_type_id, "active", on_assets_event, context);
    }
    if (context->ui_type_id >= 0) {
        state_manager_subscribe(context->state_manager, context->ui_type_id, "active", on_ui_event, context);
    }
    if (context->model_type_id >= 0) {
        state_manager_subscribe(context->state_manager, context->model_type_id, "active", on_model_event, context);
    }
    return true;
}

RenderRuntimeServiceContext* render_runtime_service_context(const ServiceDescriptor* descriptor) {
    if (!descriptor) return NULL;
    return (RenderRuntimeServiceContext*)descriptor->context;
}

void render_runtime_service_update_transformer(RenderRuntimeServiceContext* context, RenderRuntimeContext* render) {
    if (!context || !render) return;
    if (!context->renderer_ready) return;
    vk_renderer_update_transformer(&render->transformer);
}

static bool render_runtime_service_init(AppServices* services, const ServiceConfig* config) {
    (void)config;
    if (!services) return false;

    static RenderRuntimeServiceContext g_context = {0};
    g_render_runtime_service_descriptor.context = &g_context;
    services->render_runtime_context = &g_context;

    return render_runtime_service_bind(&g_context, services);
}

static bool render_runtime_service_start(AppServices* services, const ServiceConfig* config) {
    (void)config;
    if (!services || !services->render_runtime_context) {
        fprintf(stderr, "Render runtime service start received null services context.\n");
        return false;
    }

    if (!runtime_init(services)) {
        fprintf(stderr, "Render runtime initialization failed.\n");
        return false;
    }

    state_manager_dispatch(&services->state_manager, 0);
    try_bootstrap_renderer(services->render_runtime_context);
    return true;
}

static void render_runtime_service_stop(AppServices* services) {
    if (!services || !services->render_runtime_context) return;

    vk_renderer_cleanup();
    runtime_shutdown(services);
    render_runtime_service_reset(services->render_runtime_context, services);
}

static const char* g_render_runtime_dependencies[] = {"scene", "ui"};

static ServiceDescriptor g_render_runtime_service_descriptor = {
    .name = "render-runtime",
    .dependencies = g_render_runtime_dependencies,
    .dependency_count = sizeof(g_render_runtime_dependencies) / sizeof(g_render_runtime_dependencies[0]),
    .init = render_runtime_service_init,
    .start = render_runtime_service_start,
    .stop = render_runtime_service_stop,
    .context = NULL,
    .thread_handle = NULL,
};

const ServiceDescriptor* render_runtime_service_descriptor(void) {
    return &g_render_runtime_service_descriptor;
}
