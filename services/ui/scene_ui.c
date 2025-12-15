#include "ui/scene_ui.h"
#include "platform/platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static UiNode* create_node(void)
{
    UiNode* n = (UiNode*)calloc(1, sizeof(UiNode));
    return n;
}

static void append_child(UiNode* parent, UiNode* child)
{
    if (!parent || !child) return;
    UiNode* expanded = (UiNode*)realloc(parent->children, sizeof(UiNode) * (parent->child_count + 1));
    if (!expanded) return;
    parent->children = expanded;
    parent->children[parent->child_count] = *child;
    parent->child_count += 1;
    free(child);
}

static UiNode* find_by_id(UiNode* node, const char* id)
{
    if (!node || !id) return NULL;
    if (node->id && strcmp(node->id, id) == 0) return node;
    for (size_t i = 0; i < node->child_count; ++i) {
        UiNode* found = find_by_id(&node->children[i], id);
        if (found) return found;
    }
    return NULL;
}

static UiNode* make_label_row(const char* text, int depth, const char* style_use)
{
    UiNode* row = create_node();
    if (!row) return NULL;
    row->type = platform_strdup("row");
    row->use = platform_strdup("components/treeRow");
    row->layout = UI_LAYOUT_ROW;
    row->has_spacing = 1;
    row->spacing = 6.0f;

    UiNode* spacer = create_node();
    UiNode* label = create_node();
    if (!spacer || !label) {
        free(row);
        free(spacer);
        free(label);
        return NULL;
    }
    spacer->type = platform_strdup("spacer");
    spacer->use = platform_strdup("components/treeSpacer");
    spacer->widget_type = W_SPACER;
    spacer->has_w = 1;
    spacer->rect.w = (float)(depth * 16);
    spacer->has_h = 1;
    spacer->rect.h = 18.0f;

    label->type = platform_strdup("label");
    label->use = style_use ? platform_strdup(style_use) : platform_strdup("components/treeLabel");
    label->widget_type = W_LABEL;
    if (text) label->text = platform_strdup(text);

    append_child(row, spacer);
    append_child(row, label);
    return row;
}

static void append_materials(UiNode* container, const Scene* scene)
{
    if (!container || !scene) return;
    char header[128];
    snprintf(header, sizeof(header), "Материалы (%zu)", scene->material_count);
    append_child(container, make_label_row(header, 1, "components/treeHeader"));
    for (size_t i = 0; i < scene->material_count; ++i) {
        const Material* m = &scene->materials[i];
        char line[256];
        snprintf(line, sizeof(line), "Материал %s: ρ=%.3g, E=%.3g, ν=%.3g", m->id ? m->id : "<id>",
                 m->density, m->young_modulus, m->poisson_ratio);
        append_child(container, make_label_row(line, 2, NULL));
    }
}

static void describe_geometry(const GeometryNode* geo, char* out, size_t cap)
{
    if (!geo || !out || cap == 0) return;
    if (geo->kind == GEO_PRIMITIVE) {
        switch (geo->data.primitive.type) {
        case GEO_PRIM_BOX:
            snprintf(out, cap, "box %.2fx%.2fx%.2f", geo->data.primitive.size[0], geo->data.primitive.size[1], geo->data.primitive.size[2]);
            break;
        case GEO_PRIM_CYLINDER:
            snprintf(out, cap, "cylinder r=%.2f h=%.2f", geo->data.primitive.radius, geo->data.primitive.height);
            break;
        case GEO_PRIM_SPHERE:
            snprintf(out, cap, "sphere r=%.2f", geo->data.primitive.radius);
            break;
        case GEO_PRIM_EXTRUDE:
            snprintf(out, cap, "extrude h=%.2f", geo->data.primitive.height);
            break;
        default:
            snprintf(out, cap, "primitive");
        }
    } else if (geo->kind == GEO_SKETCH) {
        snprintf(out, cap, "скетч %s", geo->data.sketch.path ? geo->data.sketch.path : "<path>");
    } else if (geo->kind == GEO_STEP) {
        snprintf(out, cap, "STEP %s (x%.2f)", geo->data.step.path ? geo->data.step.path : "<path>", geo->data.step.scale);
    } else if (geo->kind == GEO_BOOLEAN) {
        const char* op = geo->data.boolean.op == GEO_BOOL_DIFFERENCE ? "Разность" :
                         (geo->data.boolean.op == GEO_BOOL_INTERSECTION ? "Пересечение" : "Объединение");
        snprintf(out, cap, "Булево: %s", op);
    } else {
        snprintf(out, cap, "геометрия не задана");
    }
}

static void append_parts(UiNode* container, const Scene* scene)
{
    if (!container || !scene) return;
    char header[128];
    snprintf(header, sizeof(header), "Детали (%zu)", scene->part_count);
    append_child(container, make_label_row(header, 1, "components/treeHeader"));
    for (size_t i = 0; i < scene->part_count; ++i) {
        const Part* p = &scene->parts[i];
        char geo[128];
        describe_geometry(p->geometry, geo, sizeof(geo));
        const char* mat = p->material && p->material->id ? p->material->id : "<материал>";
        char line[256];
        snprintf(line, sizeof(line), "Деталь %s (материал: %s, %s)", p->id ? p->id : "<id>", mat, geo);
        append_child(container, make_label_row(line, 2, NULL));
    }
}

static void append_joints(UiNode* container, const Scene* scene)
{
    if (!container || !scene) return;
    char header[128];
    snprintf(header, sizeof(header), "Соединения (%zu)", scene->joint_count);
    append_child(container, make_label_row(header, 1, "components/treeHeader"));
    for (size_t i = 0; i < scene->joint_count; ++i) {
        const Joint* j = &scene->joints[i];
        const char* t = j->type == JOINT_PRISMATIC ? "Поступ." : (j->type == JOINT_FIXED ? "Фикс." : "Поворот");
        char line[256];
        snprintf(line, sizeof(line), "Шарнир %s [%s]: %s → %s", j->id ? j->id : "<id>", t,
                 j->parent && j->parent->id ? j->parent->id : "—",
                 j->child && j->child->id ? j->child->id : "—");
        append_child(container, make_label_row(line, 2, NULL));
    }
}

static void append_assemblies(UiNode* container, const Scene* scene)
{
    if (!container || !scene) return;
    char header[128];
    snprintf(header, sizeof(header), "Сборки (%zu)", scene->assembly_count);
    append_child(container, make_label_row(header, 1, "components/treeHeader"));
    for (size_t i = 0; i < scene->assembly_count; ++i) {
        const Assembly* a = &scene->assemblies[i];
        const char* root_part = a->root.part && a->root.part->id ? a->root.part->id : "<root>";
        char line[256];
        snprintf(line, sizeof(line), "Сборка %s (корень: %s)", a->id ? a->id : "<id>", root_part);
        append_child(container, make_label_row(line, 2, NULL));
    }
}

static void append_analysis(UiNode* container, const Scene* scene)
{
    if (!container || !scene) return;
    char header[128];
    snprintf(header, sizeof(header), "Нагрузки (%zu)", scene->analysis_count);
    append_child(container, make_label_row(header, 1, "components/treeHeader"));
    for (size_t i = 0; i < scene->analysis_count; ++i) {
        const LoadCase* lc = &scene->analysis[i];
        char line[256];
        snprintf(line, sizeof(line), "Нагрузка %s (%zu целей)", lc->id ? lc->id : "<id>", lc->load_count);
        append_child(container, make_label_row(line, 2, NULL));
    }
}

static void append_motion(UiNode* container, const Scene* scene)
{
    if (!container || !scene) return;
    char header[128];
    snprintf(header, sizeof(header), "Движения (%zu)", scene->motion_count);
    append_child(container, make_label_row(header, 1, "components/treeHeader"));
    for (size_t i = 0; i < scene->motion_count; ++i) {
        const MotionProfile* mp = &scene->motion_profiles[i];
        const char* joint_id = (mp->joint && mp->joint->id) ? mp->joint->id : "—";
        char line[256];
        snprintf(line, sizeof(line), "Профиль %s [%s] → %s", mp->id ? mp->id : "<id>", mp->type ? mp->type : "тип не задан", joint_id);
        append_child(container, make_label_row(line, 2, NULL));
    }
}

static void append_header_info(UiNode* container, const Scene* scene)
{
    if (!container || !scene) return;
    char title[160];
    const char* scene_name = scene->metadata.name ? scene->metadata.name : "Без названия";
    snprintf(title, sizeof(title), "Сцена: %s", scene_name);
    append_child(container, make_label_row(title, 0, "components/treeHeader"));
    if (scene->metadata.author && *scene->metadata.author) {
        char author[160];
        snprintf(author, sizeof(author), "Автор: %s", scene->metadata.author);
        append_child(container, make_label_row(author, 1, NULL));
    }
    char counts[160];
    snprintf(counts, sizeof(counts), "Состав: %zu материалов, %zu деталей, %zu соединений", scene->material_count, scene->part_count, scene->joint_count);
    append_child(container, make_label_row(counts, 1, NULL));
}

static void populate_container(UiNode* container, const Scene* scene)
{
    if (!container || !scene) return;
    append_header_info(container, scene);
    append_materials(container, scene);
    append_parts(container, scene);
    append_joints(container, scene);
    append_assemblies(container, scene);
    append_analysis(container, scene);
    append_motion(container, scene);
}

void scene_ui_inject(UiNode* root, const Scene* scene)
{
    if (!root || !scene) return;
    UiNode* tree = find_by_id(root, "sceneHierarchy");
    if (tree) {
        populate_container(tree, scene);
    }

    UiNode* materials = find_by_id(root, "materialsList");
    if (materials) append_materials(materials, scene);
    UiNode* joints = find_by_id(root, "jointsList");
    if (joints) append_joints(joints, scene);
    UiNode* analysis = find_by_id(root, "analysisList");
    if (analysis) {
        append_assemblies(analysis, scene);
        append_analysis(analysis, scene);
        append_motion(analysis, scene);
    }
}

void scene_ui_bind_model(Model* model, const Scene* scene, const char* scene_path)
{
    if (!model || !scene) return;
    const char* name = scene->metadata.name ? scene->metadata.name : "Без названия";
    model_set_string(model, "sceneName", name);
    if (scene->metadata.author) model_set_string(model, "sceneAuthor", scene->metadata.author);
    if (scene_path) model_set_string(model, "scenePath", scene_path);
    char summary[160];
    snprintf(summary, sizeof(summary), "%zu материалов · %zu деталей · %zu соединений", scene->material_count, scene->part_count, scene->joint_count);
    model_set_string(model, "sceneStats", summary);
}
