#include "render_service.h"

#include <GLFW/glfw3.h>
#include "render/vulkan_renderer.h"
#include "service_events.h"
#include "runtime/runtime.h"

typedef struct RenderServiceState {
    RenderRuntimeContext* render;
    const Assets* assets;
    UiContext* ui;
    WidgetArray widgets;
    Model* model;
    StateManager* state_manager;
    int renderer_ready;
} RenderServiceState;

static RenderServiceState g_render_state = {0};

static void try_bootstrap_renderer(void) {
    if (g_render_state.renderer_ready) return;
    if (!g_render_state.render || !g_render_state.render->window || !g_render_state.assets ||
        !g_render_state.widgets.items) {
        return;
    }
    g_render_state.renderer_ready = vk_renderer_init(g_render_state.render->window, g_render_state.assets->vert_spv_path,
                                                     g_render_state.assets->frag_spv_path, g_render_state.assets->font_path,
                                                     g_render_state.widgets, &g_render_state.render->transformer);
}

static void on_assets_event(const StateEvent* event, void* user_data) {
    (void)user_data;
    if (!event || !event->payload) return;
    const AssetsComponent* component = (const AssetsComponent*)event->payload;
    g_render_state.assets = component->assets;
    try_bootstrap_renderer();
}

static void on_ui_event(const StateEvent* event, void* user_data) {
    (void)user_data;
    if (!event || !event->payload) return;
    const UiRuntimeComponent* component = (const UiRuntimeComponent*)event->payload;
    g_render_state.ui = component->ui;
    g_render_state.widgets = component->widgets;
    try_bootstrap_renderer();
}

static void on_model_event(const StateEvent* event, void* user_data) {
    (void)user_data;
    if (!event || !event->payload) return;
    const ModelComponent* component = (const ModelComponent*)event->payload;
    g_render_state.model = component->model;
}

bool render_service_bind(RenderRuntimeContext* render, StateManager* state_manager, int assets_type_id, int ui_type_id,
                        int model_type_id) {
    if (!render || !state_manager) return false;
    g_render_state.render = render;
    g_render_state.state_manager = state_manager;

    if (assets_type_id >= 0)
        state_manager_subscribe(state_manager, assets_type_id, "active", on_assets_event, NULL);
    if (ui_type_id >= 0) state_manager_subscribe(state_manager, ui_type_id, "active", on_ui_event, NULL);
    if (model_type_id >= 0) state_manager_subscribe(state_manager, model_type_id, "active", on_model_event, NULL);
    return true;
}

void render_service_update_transformer(RenderRuntimeContext* render) {
    if (!render) return;
    vk_renderer_update_transformer(&render->transformer);
}

void render_loop(RenderRuntimeContext* render, StateManager* state_manager) {
    if (!render || !state_manager) return;
    while (!glfwWindowShouldClose(render->window)) {
        state_manager_dispatch(state_manager, 0);
        glfwPollEvents();
        if (g_render_state.renderer_ready && g_render_state.ui && g_render_state.model) {
            ui_frame_update(g_render_state.ui);
            vk_renderer_draw_frame();
        }
    }
}

void render_service_shutdown(RenderRuntimeContext* render) {
    (void)render;
    vk_renderer_cleanup();
    g_render_state = (RenderServiceState){0};
}

static bool render_service_init(AppServices* services, const ServiceConfig* config) {
    (void)config;
    if (!services) return false;
    return render_service_bind(&services->render, &services->state_manager, services->assets_type_id,
                               services->ui_type_id, services->model_type_id);
}

static bool render_service_start(AppServices* services, const ServiceConfig* config) {
    (void)config;
    if (!services) return false;
    if (!runtime_init(services)) return false;
    state_manager_dispatch(&services->state_manager, 0);
    render_loop(&services->render, &services->state_manager);
    return true;
}

static void render_service_stop(AppServices* services) {
    if (!services) return;
    render_service_shutdown(&services->render);
    runtime_shutdown(services);
}

static const ServiceDescriptor g_render_service_descriptor = {
    .name = "render",
    .init = render_service_init,
    .start = render_service_start,
    .stop = render_service_stop,
    .context = NULL,
    .thread_handle = NULL,
};

const ServiceDescriptor* render_service_descriptor(void) { return &g_render_service_descriptor; }
