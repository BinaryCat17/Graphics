#include "render_runtime_service.h"

#include <stdio.h>
#include <string.h>

#include "platform/platform.h"
#include "render/common/renderer_backend.h"
#include "render/vulkan/vulkan_renderer.h"
#include "runtime/runtime.h"
#include "services/service_events.h"

static ServiceDescriptor g_render_runtime_service_descriptor;

static void render_runtime_service_reset(RenderRuntimeServiceContext* context, AppServices* services) {
    if (!context) return;
    *context = (RenderRuntimeServiceContext){
        .render = services ? &services->render : NULL,
        .state_manager = services ? &services->state_manager : NULL,
        .assets_type_id = services ? services->assets_type_id : -1,
        .ui_type_id = services ? services->ui_type_id : -1,
        .model_type_id = services ? services->model_type_id : -1,
        .render_ready_type_id = services ? services->render_ready_type_id : -1,
        .renderer_ready = false,
        .render_ready = false,
        .backend = context->backend,
        .logger_config = context->logger_config,
    };
}

static void try_bootstrap_renderer(RenderRuntimeServiceContext* context) {
    if (!context || context->renderer_ready || !context->render_ready) return;
    if (!context->render || !context->render->window || !context->assets || !context->widgets.items) return;
    if (!context->backend) return;

    RenderBackendInit init = {
        .window = context->render->window,
        .surface = &context->render->surface,
        .get_required_instance_extensions = platform_get_required_vulkan_instance_extensions,
        .create_surface = platform_create_vulkan_surface,
        .destroy_surface = platform_destroy_vulkan_surface,
        .get_framebuffer_size = platform_get_framebuffer_size,
        .wait_events = platform_wait_events,
        .poll_events = platform_poll_events,
        .vert_spv = context->assets->vert_spv_path,
        .frag_spv = context->assets->frag_spv_path,
        .font_path = context->assets->font_path,
        .widgets = context->widgets,
        .transformer = &context->render->transformer,
        .logger_config = &context->logger_config,
    };

    context->renderer_ready = context->backend->init(context->backend, &init);
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

static void on_render_ready_event(const StateEvent* event, void* user_data) {
    RenderRuntimeServiceContext* context = (RenderRuntimeServiceContext*)user_data;
    if (!context || !event || !event->payload) return;
    const RenderReadyComponent* component = (const RenderReadyComponent*)event->payload;
    context->render = component->render;
    context->assets = component->assets;
    context->ui = component->ui;
    context->widgets = component->widgets;
    context->model = component->model;
    context->render_ready = component->ready;
    try_bootstrap_renderer(context);
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
    if (context->render_ready_type_id >= 0) {
        state_manager_subscribe(context->state_manager, context->render_ready_type_id, "active", on_render_ready_event,
                                context);
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
    if (context->backend && context->backend->update_transformer) {
        context->backend->update_transformer(context->backend, &render->transformer);
    }
}

bool render_runtime_service_prepare(RenderRuntimeServiceContext* context, AppServices* services) {
    if (!context || !services) return false;

    if (!runtime_init(services)) {
        fprintf(stderr, "Render runtime initialization failed.\\n");
        return false;
    }

    RenderReadyComponent ready = {.render = &services->render,
                                  .assets = &services->core.assets,
                                  .ui = &services->ui,
                                  .widgets = services->ui.widgets,
                                  .model = services->core.model,
                                  .ready = true};
    state_manager_publish(&services->state_manager, STATE_EVENT_COMPONENT_ADDED, services->render_ready_type_id, "active",
                          &ready, sizeof(ready));

    state_manager_dispatch(&services->state_manager, 0);
    return true;
}

static bool render_runtime_service_init(AppServices* services, const ServiceConfig* config) {
    if (!services) return false;

    static RenderRuntimeServiceContext g_context = {0};
    g_render_runtime_service_descriptor.context = &g_context;
    services->render_runtime_context = &g_context;

    renderer_backend_register(vulkan_renderer_backend());
    const char* backend_id = config ? config->renderer_backend : NULL;
    g_context.backend = renderer_backend_get(backend_id);

    RenderLogSinkType sink_type = RENDER_LOG_SINK_STDOUT;
    if (config && config->render_log_sink) {
        if (strcmp(config->render_log_sink, "file") == 0) {
            sink_type = RENDER_LOG_SINK_FILE;
        } else if (strcmp(config->render_log_sink, "ring") == 0) {
            sink_type = RENDER_LOG_SINK_RING_BUFFER;
        }
    }

    g_context.logger_config = (RenderLoggerConfig){
        .sink_type = sink_type,
        .sink_target = config ? config->render_log_target : NULL,
        .ring_capacity = 0,
        .enabled = config ? config->render_log_enabled : false,
    };

    return render_runtime_service_bind(&g_context, services);
}

static bool render_runtime_service_start(AppServices* services, const ServiceConfig* config) {
    (void)config;
    if (!services || !services->render_runtime_context) {
        fprintf(stderr, "Render runtime service start received null services context.\n");
        return false;
    }
    return true;
}

static void render_runtime_service_stop(AppServices* services) {
    if (!services || !services->render_runtime_context) return;

    RenderRuntimeServiceContext* context = services->render_runtime_context;
    if (context->backend && context->backend->cleanup) {
        context->backend->cleanup(context->backend);
    }
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
