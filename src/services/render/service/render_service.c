#include "services/render/service/render_service.h"

#include <stdbool.h>
#include <stdio.h>
#include <threads.h>

#include "app/app_services.h"
#include "core/platform/platform.h"
#include "services/render/runtime/render_runtime_service.h"
#include "services/ui/ui_service.h"

typedef struct RenderServiceContext {
    RenderRuntimeServiceContext* runtime;
    thrd_t thread;
    bool running;
    bool thread_active;
} RenderServiceContext;

static ServiceDescriptor g_render_service_descriptor;

static void render_service_run_loop(RenderServiceContext* service) {
    if (!service || !service->runtime) return;
    RenderRuntimeServiceContext* context = service->runtime;
    if (!context->render || !context->state_manager) return;

    while (service->running && !platform_window_should_close(context->render->window)) {
        state_manager_dispatch(context->state_manager, 0);
        platform_poll_events();
        
        const RenderFramePacket* packet = render_runtime_service_acquire_packet(context);
        
        if (context->renderer_ready && context->backend && packet) {
            // Safe execution on Render Thread
            if (context->backend->update_transformer) {
                context->backend->update_transformer(context->backend, &packet->transformer);
            }
            
            if (context->backend->update_ui) {
                context->backend->update_ui(context->backend, packet->widgets, packet->display_list);
            }

            // Note: ui_frame_update acts on shared UiContext. ideally this should move to UiService
            if (context->ui) {
                ui_frame_update(context->ui);
            }

            if (context->backend->draw) {
                context->backend->draw(context->backend);
            }
        }
    }
}

static RenderServiceContext* render_service_context(void) {
    return (RenderServiceContext*)g_render_service_descriptor.context;
}

static bool render_service_init(void* ptr, const ServiceConfig* config) {
    AppServices* services = (AppServices*)ptr;
    (void)config;
    if (!services || !services->render_runtime_context) {
        fprintf(stderr, "Render service init received invalid runtime context.\n");
        return false;
    }
    static RenderServiceContext g_context = {0};
    g_context.runtime = services->render_runtime_context;
    g_context.running = false;
    g_context.thread_active = false;
    g_render_service_descriptor.context = &g_context;
    return true;
}

static bool render_service_start(void* ptr, const ServiceConfig* config) {
    AppServices* services = (AppServices*)ptr;
    (void)config;
    RenderServiceContext* context = render_service_context();
    if (!context || !services || !services->render_runtime_context) {
        fprintf(stderr, "Render service start received invalid arguments.\n");
        return false;
    }

    if (!render_runtime_service_prepare(context->runtime)) {
        return false;
    }

    context->running = true;
    context->thread_active = true; // reusing this flag to indicate "loop active"
    
    // We run the loop directly here. This blocks until the window closes.
    // This effectively makes the "render" service the "main" service.
    render_service_run_loop(context);
    
    context->running = false;
    context->thread_active = false;
    
    return true;
}

static void render_service_stop(void* ptr) {
    AppServices* services = (AppServices*)ptr;
    (void)services;
    RenderServiceContext* context = render_service_context();
    if (!context) return;

    if (!context->running) return;

    context->running = false;
    if (context->runtime && context->runtime->render && context->runtime->render->window) {
        platform_set_window_should_close(context->runtime->render->window, true);
    }
}

static const char* g_render_service_dependencies[] = {"render-runtime"};

static ServiceDescriptor g_render_service_descriptor = {
    .name = "render",
    .dependencies = g_render_service_dependencies,
    .dependency_count = sizeof(g_render_service_dependencies) / sizeof(g_render_service_dependencies[0]),
    .init = render_service_init,
    .start = render_service_start,
    .stop = render_service_stop,
    .context = NULL,
    .thread_handle = NULL,
};

const ServiceDescriptor* render_service_descriptor(void) {
    return &g_render_service_descriptor;
}
