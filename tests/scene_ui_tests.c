#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "test_framework.h"

#include "foundation/platform/platform.h"
#include "engine/ui/scene_ui.h"
#include "foundation/config/config_io.h"
#include "engine/ui/model_style.h"
#include "engine/ui/ui_node.h"
#include "engine/ui/layout_tree.h"
#include "engine/ui/widget_list.h"

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
    scene.metadata.name = platform_strdup("Demo");
    scene.metadata.author = platform_strdup("User");

    scene.material_count = 1;
    scene.materials = calloc(scene.material_count, sizeof(Material));
    scene.materials[0].id = platform_strdup("steel");
    scene.materials[0].density = 7800.0f;
    scene.materials[0].young_modulus = 2.1e11f;
    scene.materials[0].poisson_ratio = 0.3f;

    scene.part_count = 1;
    scene.parts = calloc(scene.part_count, sizeof(Part));
    scene.parts[0].id = platform_strdup("base");
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
    scene.joints[0].id = platform_strdup("j1");
    scene.joints[0].parent = &scene.parts[0];
    scene.joints[0].child = &scene.parts[0];
    scene.joints[0].type = JOINT_REVOLUTE;

    scene.assembly_count = 0;
    scene.analysis_count = 0;
    scene.motion_count = 0;
    return scene;
}

static int test_tree_population(void)
{
    const char* styles = "{\"styles\":{\"panelPrimary\":{\"padding\":4},\"panelSecondary\":{\"padding\":4},\"divider\":{\"padding\":1},\"treeItem\":{\"padding\":4},\"treeHeader\":{\"padding\":4}}}";
    const char* layout = "{\"layout\":{\"type\":\"column\",\"children\":[{\"type\":\"column\",\"id\":\"sceneHierarchy\"}]}}";

    Scene scene = make_sample_scene();
    ConfigNode* styles_root = NULL;
    ConfigNode* config_layout_root = NULL;
    ConfigError err = {0};
    TEST_ASSERT(parse_config_text(styles, CONFIG_FORMAT_JSON, &styles_root, &err));
    err = (ConfigError){0};
    TEST_ASSERT(parse_config_text(layout, CONFIG_FORMAT_JSON, &config_layout_root, &err));
    Style* parsed_styles = ui_config_load_styles(styles_root);
    ConfigDocument layout_doc = {.format = CONFIG_FORMAT_JSON, .root = config_layout_root,
                                 .source_path = "assets/ui/config/layout/ui.yaml"};
    UiNode* root = ui_config_load_layout(&layout_doc, NULL, parsed_styles, &scene);
    config_node_free(styles_root);
    config_node_free(config_layout_root);
    TEST_ASSERT(root);

    UiNode* tree = find_by_id(root, "sceneHierarchy");
    TEST_ASSERT(tree);
    /* header (3) + materials (2) + parts (2) + joints (2) + assemblies (1) + analysis (1) + motion (1) */
    TEST_ASSERT_INT_EQ(12, tree->child_count);

    UiNode* first_row = &tree->children[0];
    TEST_ASSERT(first_row->child_count >= 2);
    TEST_ASSERT_STR_EQ("Сцена: Demo", first_row->children[1].text);

    LayoutNode* layout_root = build_layout_tree(root);
    measure_layout(layout_root, NULL);
    assign_layout(layout_root, 0.0f, 0.0f);
    WidgetArray widgets = materialize_widgets(layout_root);
    TEST_ASSERT(widgets.count >= tree->child_count);

    free_widgets(widgets);
    free_layout_tree(layout_root);
    free_ui_tree(root);
    free_styles(parsed_styles);
    scene_dispose(&scene);
    return 1;
}

static int test_yaml_layout_parsing(void)
{
    const char* styles_yaml =
        "styles:\n"
        "  base:\n"
        "    padding: 3\n"
        "    textColor: [0.2, 0.3, 0.4, 1.0]\n";

    const char* layout_yaml =
        "layout:\n"
        "  type: column\n"
        "  children:\n"
        "    - type: label\n"
        "      text: Example\n"
        "      style: base\n";

    ConfigNode* styles_root = NULL;
    ConfigNode* layout_root = NULL;
    ConfigError err = {0};
    TEST_ASSERT(parse_config_text(styles_yaml, CONFIG_FORMAT_YAML, &styles_root, &err));
    err = (ConfigError){0};
    TEST_ASSERT(parse_config_text(layout_yaml, CONFIG_FORMAT_YAML, &layout_root, &err));

    Style* parsed_styles = ui_config_load_styles(styles_root);
    TEST_ASSERT(parsed_styles);
    ConfigDocument layout_doc = {.format = CONFIG_FORMAT_YAML, .root = layout_root,
                                 .source_path = "assets/ui/config/layout/ui.yaml"};
    UiNode* root = ui_config_load_layout(&layout_doc, NULL, parsed_styles, NULL);
    TEST_ASSERT(root);
    
    // Unwrap the root absolute container
    UiNode* layout_node = root;
    if (root->child_count == 1 && (!root->type || strcmp(root->type, "column") != 0)) {
        layout_node = &root->children[0];
    }
    
    // Verify column
    TEST_ASSERT(layout_node->type && strcmp(layout_node->type, "column") == 0);
    TEST_ASSERT_INT_EQ(1, layout_node->child_count);

    // Verify label
    UiNode* child = &layout_node->children[0];
    TEST_ASSERT(child->type && strcmp(child->type, "label") == 0);
    TEST_ASSERT(child->text && strcmp(child->text, "Example") == 0);
    TEST_ASSERT(child->style_name && strcmp(child->style_name, "base") == 0);

    free_ui_tree(root);
    free_styles(parsed_styles);
    config_node_free(styles_root);
    config_node_free(layout_root);
    return 1;
}

int main(void)
{
    RUN_TEST(test_tree_population);
    RUN_TEST(test_yaml_layout_parsing);
    
    printf("Tests Run: %d, Failed: %d\n", g_tests_run, g_tests_failed);
    return g_tests_failed > 0 ? 1 : 0;
}