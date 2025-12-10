#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "ui/scene_ui.h"
#include "config/config_io.h"
#include "ui/ui_json.h"

static UiNode* find_by_id(UiNode* node, const char* id)
{
    if (!node || !id) return NULL;
    if (node->id && strcmp(node->id, id) == 0) return node;
    for (size_t i = 0; i < node->child_count; ++i) {
        UiNode* f = find_by_id(&node->children[i], id);
        if (f) return f;
    }
    return NULL;
}

static Scene make_sample_scene(void)
{
    Scene scene = {0};
    scene.metadata.name = strdup("Demo");
    scene.metadata.author = strdup("User");

    scene.material_count = 1;
    scene.materials = calloc(scene.material_count, sizeof(Material));
    scene.materials[0].id = strdup("steel");
    scene.materials[0].density = 7800.0f;
    scene.materials[0].young_modulus = 2.1e11f;
    scene.materials[0].poisson_ratio = 0.3f;

    scene.part_count = 1;
    scene.parts = calloc(scene.part_count, sizeof(Part));
    scene.parts[0].id = strdup("base");
    scene.parts[0].material = &scene.materials[0];
    GeometryNode* geo = calloc(1, sizeof(GeometryNode));
    geo->kind = GEO_PRIMITIVE;
    geo->data.primitive.type = GEO_PRIM_BOX;
    geo->data.primitive.size[0] = 1.0f;
    geo->data.primitive.size[1] = 2.0f;
    geo->data.primitive.size[2] = 3.0f;
    scene.parts[0].geometry = geo;

    scene.joint_count = 1;
    scene.joints = calloc(scene.joint_count, sizeof(Joint));
    scene.joints[0].id = strdup("j1");
    scene.joints[0].parent = &scene.parts[0];
    scene.joints[0].child = &scene.parts[0];
    scene.joints[0].type = JOINT_REVOLUTE;

    scene.assembly_count = 0;
    scene.analysis_count = 0;
    scene.motion_count = 0;
    return scene;
}

static void test_tree_population(void)
{
    const char* styles = "{\"styles\":{\"panelPrimary\":{\"padding\":4},\"panelSecondary\":{\"padding\":4},"
                          "\"divider\":{\"padding\":1},\"treeItem\":{\"padding\":4},\"treeHeader\":{\"padding\":4}}}";
    const char* layout = "{\"widgets\":{\"treeRow\":{\"type\":\"row\"},\"treeLabel\":{\"type\":\"label\",\"style\":\"treeItem\"},"
                         "\"treeHeader\":{\"type\":\"label\",\"style\":\"treeHeader\"},\"treeSpacer\":{\"type\":\"spacer\"}},"
                         "\"layout\":{\"type\":\"column\",\"children\":[{\"type\":\"column\",\"id\":\"sceneHierarchy\"}]}}";

    Scene scene = make_sample_scene();
    ConfigNode* styles_root = NULL;
    ConfigNode* config_layout_root = NULL;
    ConfigError err = {0};
    assert(parse_config_text(styles, CONFIG_FORMAT_JSON, &styles_root, &err));
    err = (ConfigError){0};
    assert(parse_config_text(layout, CONFIG_FORMAT_JSON, &config_layout_root, &err));
    Style* parsed_styles = parse_styles_config(styles_root);
    UiNode* root = parse_layout_config(config_layout_root, NULL, parsed_styles, NULL, &scene);
    config_node_free(styles_root);
    config_node_free(config_layout_root);
    assert(root);

    UiNode* tree = find_by_id(root, "sceneHierarchy");
    assert(tree);
    /* header (3) + materials (2) + parts (2) + joints (2) + assemblies (1) + analysis (1) + motion (1) */
    assert(tree->child_count == 12);

    UiNode* first_row = &tree->children[0];
    assert(first_row->child_count >= 2);
    assert(strcmp(first_row->children[1].text, "Сцена: Demo") == 0);

    LayoutNode* layout_root = build_layout_tree(root);
    measure_layout(layout_root);
    assign_layout(layout_root, 0.0f, 0.0f);
    WidgetArray widgets = materialize_widgets(layout_root);
    assert(widgets.count >= tree->child_count);

    free_widgets(widgets);
    free_layout_tree(layout_root);
    free_ui_tree(root);
    free_styles(parsed_styles);
    scene_dispose(&scene);
}

int main(void)
{
    test_tree_population();
    printf("scene_ui_tests passed\n");
    return 0;
}
