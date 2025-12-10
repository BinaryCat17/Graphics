#include "render_service.h"

#include <GLFW/glfw3.h>
#include <stdbool.h>
#include <stdio.h>
#include <threads.h>

#include "render/vulkan_renderer.h"
#include "render_runtime_service.h"
#include "ui/ui_json.h"

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

    while (service->running && !glfwWindowShouldClose(context->render->window)) {
        state_manager_dispatch(context->state_manager, 0);
        glfwPollEvents();
        if (context->renderer_ready && context->ui && context->model) {
            ui_frame_update(context->ui);
            vk_renderer_draw_frame();
        }
    }
}

static int render_service_thread(void* user_data) {
    RenderServiceContext* context = (RenderServiceContext*)user_data;
    render_service_run_loop(context);
    context->running = false;
    context->thread_active = false;
    return 0;
}

static RenderServiceContext* render_service_context(void) {
    return (RenderServiceContext*)g_render_service_descriptor.context;
}

static bool render_service_init(AppServices* services, const ServiceConfig* config) {
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

static bool render_service_start(AppServices* services, const ServiceConfig* config) {
    (void)config;
    RenderServiceContext* context = render_service_context();
    if (!context || !services || !services->render_runtime_context) {
        fprintf(stderr, "Render service start received invalid arguments.\n");
        return false;
    }

    context->running = true;
    context->thread_active = thrd_create(&context->thread, render_service_thread, context) == thrd_success;
    if (!context->thread_active) {
        fprintf(stderr, "Failed to start render service loop thread.\n");
        context->running = false;
        return false;
    }
    g_render_service_descriptor.thread_handle = &context->thread;
    return true;
}

static void render_service_stop(AppServices* services) {
    (void)services;
    RenderServiceContext* context = render_service_context();
    if (!context) return;

    if (!context->running && !context->thread_active) return;

    context->running = false;
    if (context->runtime && context->runtime->render && context->runtime->render->window) {
        glfwSetWindowShouldClose(context->runtime->render->window, GLFW_TRUE);
    }

    if (context->thread_active) {
        thrd_join(context->thread, NULL);
        context->thread_active = false;
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
