#include "ui/ui_context.h"

#include <string.h>

void ui_context_init(UiContext* ui) {
    if (!ui) return;
    memset(ui, 0, sizeof(UiContext));
}

void ui_context_dispose(UiContext* ui) {
    if (!ui) return;

    if (ui->styles) {
        free_styles(ui->styles);
        ui->styles = NULL;
    }
    if (ui->widgets.items) {
        free_widgets(ui->widgets);
        ui->widgets = (WidgetArray){0};
    }
    if (ui->ui_root) {
        free_ui_tree(ui->ui_root);
        ui->ui_root = NULL;
    }
    if (ui->layout_root) {
        free_layout_tree(ui->layout_root);
        ui->layout_root = NULL;
    }
    ui_compositor_free(ui->display_list);
    if (ui->scroll) {
        scroll_free(ui->scroll);
        ui->scroll = NULL;
    }
    if (ui->model) {
        free_model(ui->model);
        ui->model = NULL;
    }
}
