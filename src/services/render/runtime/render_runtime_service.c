#include "services/render/runtime/render_runtime_service.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/platform/platform.h"
#include "services/render/backend/common/renderer_backend.h"
#include "services/render/backend/vulkan/vulkan_renderer.h"
#include "services/render/runtime/runtime.h"
#include "core/service_manager/service_events.h"

static ServiceDescriptor g_render_runtime_service_descriptor;

static void render_packet_free_resources(RenderFramePacket* packet) {
    if (!packet) return;
    if (packet->display_list.items) {
        free(packet->display_list.items);
        packet->display_list.items = NULL;
    }
    packet->display_list.count = 0;
}

static void try_sync_packet(RenderRuntimeServiceContext* context) {
    if (!context) return;
    
    mtx_lock(&context->packet_mutex);
    
    RenderFramePacket* dest = &context->packets[context->back_packet_index];
    
    // 1. Copy UI Data (Widgets)
    dest->widgets = context->widgets;

    // 2. Copy Display List (Deep Copy)
    render_packet_free_resources(dest);
    if (context->display_list.items && context->display_list.count > 0) {
        dest->display_list.items = (DisplayItem*)malloc(context->display_list.count * sizeof(DisplayItem));
        if (dest->display_list.items) {
            memcpy(dest->display_list.items, context->display_list.items, context->display_list.count * sizeof(DisplayItem));
            dest->display_list.count = context->display_list.count;
        } else {
            fprintf(stderr, "Render Packet: Failed to allocate display list.\n");
        }
    }

    // 3. Copy Transformer
    if (context->render) {
        dest->transformer = context->render->transformer;
    }

    context->packet_ready = true;
    mtx_unlock(&context->packet_mutex);
}

const RenderFramePacket* render_runtime_service_acquire_packet(RenderRuntimeServiceContext* context) {
    if (!context) return NULL;

    mtx_lock(&context->packet_mutex);
    if (context->packet_ready) {
        // Swap indices
        int temp = context->front_packet_index;
        context->front_packet_index = context->back_packet_index;
        context->back_packet_index = temp;
        
        // Reset flag so we don't swap again until new data arrives
        context->packet_ready = false;
        
        // Clear the new back buffer (old front) to be ready for writing? 
        // Not strictly necessary if we overwrite everything, but good for safety.
    }
    mtx_unlock(&context->packet_mutex);

    return &context->packets[context->front_packet_index];
}

static void render_runtime_free_display_list(RenderRuntimeServiceContext* context) {
    if (!context || !context->display_list.items) return;
    free(context->display_list.items);
    context->display_list = (DisplayList){0};
}

static void render_runtime_copy_display_list(RenderRuntimeServiceContext* context, DisplayList source) {
    if (!context) return;
    render_runtime_free_display_list(context);
    if (!source.items || source.count == 0) {
        context->display_list = (DisplayList){0};
        return;
    }

    DisplayItem* copy = (DisplayItem*)malloc(source.count * sizeof(DisplayItem));
    if (!copy) {
        fprintf(stderr, "Render runtime failed to copy display list.\n");
        context->display_list = (DisplayList){0};
        return;
    }

    memcpy(copy, source.items, source.count * sizeof(DisplayItem));
    context->display_list.items = copy;
    context->display_list.count = source.count;
}

static void render_runtime_service_reset(RenderRuntimeServiceContext* context, AppServices* services) {
    if (!context) return;
    
    render_packet_free_resources(&context->packets[0]);
    render_packet_free_resources(&context->packets[1]);
    render_runtime_free_display_list(context);

    *context = (RenderRuntimeServiceContext){
        .render = services ? &services->render : NULL,
        .assets = services ? &services->core.assets : NULL,
        .ui = services ? &services->ui : NULL,
        .model = services ? services->core.model : NULL,
        .state_manager = services ? &services->state_manager : NULL,
        .assets_type_id = services ? services->type_id_assets : -1,
        .ui_type_id = services ? services->type_id_uiruntime : -1,
        .model_type_id = services ? services->type_id_model : -1,
        .render_ready_type_id = services ? services->type_id_renderready : -1,
        .renderer_ready = false,
        .render_ready = false,
        .backend = context->backend,
        .logger_config = context->logger_config,
        .front_packet_index = 0,
        .back_packet_index = 1,
        .packet_ready = false
    };
    // Re-init mutex after memset/reset? 
    // Dangerous if reset is called during runtime. 
    // Assuming reset is only called on stop/start.
    mtx_init(&context->packet_mutex, mtx_plain);
}

static void try_bootstrap_renderer(RenderRuntimeServiceContext* context) {
    if (!context) return;
    if (context->renderer_ready) return; // Already ready
    
    if (!context->render_ready) return;
    if (!context->render || !context->render->window) return;
    if (!context->assets) return;
    if (!context->widgets.items) return;
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
        .display_list = context->display_list,
        .transformer = &context->render->transformer,
        .logger_config = &context->logger_config,
    };

    context->renderer_ready = context->backend->init(context->backend, &init);
    
    if (context->renderer_ready) {
        try_sync_packet(context);
    }
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
    
    // Update Master State
    context->ui = component->ui;
    context->widgets = component->widgets;
    render_runtime_copy_display_list(context, component->display_list);
    
    // Sync to Render Thread (Back Buffer)
    if (context->renderer_ready) {
        try_sync_packet(context);
    }
    
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
    
    // Only update render context and readiness flag. 
    // Do NOT overwrite assets/ui/widgets as they might be stale in this event payload 
    // compared to what we received directly from their respective services.
    context->render = component->render;
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
    
    // Sync to Render Thread (Back Buffer)
    try_sync_packet(context);
}

bool render_runtime_service_prepare(RenderRuntimeServiceContext* context) {
    if (!context) return false;

    if (!runtime_init(context)) {
        fprintf(stderr, "Render runtime initialization failed.\n");
        return false;
    }

    RenderReadyComponent ready = {.render = context->render,
                                  .assets = context->assets,
                                  .ui = context->ui,
                                  .widgets = context->widgets,
                                  .display_list = context->display_list,
                                  .model = context->model,
                                  .ready = true};
    state_manager_publish(context->state_manager, STATE_EVENT_COMPONENT_ADDED, context->render_ready_type_id, "active",
                          &ready, sizeof(ready));

    state_manager_dispatch(context->state_manager, 0);
    return true;
}

static bool render_runtime_service_init(void* ptr, const ServiceConfig* config) {
    AppServices* services = (AppServices*)ptr;
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

static bool render_runtime_service_start(void* ptr, const ServiceConfig* config) {
    AppServices* services = (AppServices*)ptr;
    (void)config;
    if (!services || !services->render_runtime_context) {
        fprintf(stderr, "Render runtime service start received null services context.\n");
        return false;
    }
    return true;
}

static void render_runtime_service_stop(void* ptr) {
    AppServices* services = (AppServices*)ptr;
    if (!services || !services->render_runtime_context) return;

    RenderRuntimeServiceContext* context = services->render_runtime_context;
    if (context->backend && context->backend->cleanup) {
        context->backend->cleanup(context->backend);
    }
    runtime_shutdown(context);
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
