#include <stdio.h>
#include <string.h>

#include "test_framework.h"

#include "foundation/platform/platform.h"
#include "domains/cad_model/cad_scene_yaml.h"

static const char *write_temp(const char *name, const char *text)
{
    FILE *f = platform_fopen(name, "wb");
    if (!f) return NULL;
    fputs(text, f);
    fclose(f);
    return name;
}

static int test_valid_scene(void)
{
    const char *path = write_temp("scene_valid.yaml",
        "version: 1\n"
        "materials:\n"
        "  - id: steel_45\n"
        "    density: 7850\n"
        "    young_modulus: 2.05e11\n"
        "    poisson_ratio: 0.29\n"
        "parts:\n"
        "  - id: base\n"
        "    material: steel_45\n"
        "    geometry:\n"
        "      primitive:\n"
        "        type: box\n"
        "        size: [1, 2, 3]\n"
        "joints:\n"
        "  - id: j1\n"
        "    type: revolute\n"
        "    parent: base\n"
        "    child: base\n"
        "    origin: [0,0,0]\n"
        "    axis: [0,0,1]\n"
        "assemblies:\n"
        "  - id: a1\n"
        "    root: base\n"
        "analysis:\n"
        "  - id: c1\n"
        "    type: static\n"
        "    loads:\n"
        "      - target: base\n"
        "        force: [1,0,0]\n"
        "motion:\n"
        "  - id: m1\n"
        "    joint: j1\n"
        "    profile:\n"
        "      type: trapezoid\n"
    );
    TEST_ASSERT(path != NULL);
    Scene scene;
    SceneError err = {0};
    int ok = parse_scene_yaml(path, &scene, &err);
    if (!ok) {
        fprintf(stderr, "Scene parse error: %s\n", err.message);
    }
    TEST_ASSERT(ok);
    TEST_ASSERT_INT_EQ(1, scene.material_count);
    TEST_ASSERT_INT_EQ(1, scene.part_count);
    TEST_ASSERT_INT_EQ(1, scene.joint_count);
    TEST_ASSERT_INT_EQ(1, scene.analysis_count);
    TEST_ASSERT_INT_EQ(1, scene.motion_count);
    TEST_ASSERT(scene.parts[0].material == &scene.materials[0]);
    TEST_ASSERT(scene.motion_profiles[0].joint == &scene.joints[0]);
    scene_dispose(&scene);
    remove("scene_valid.yaml");
    return 1;
}

static int test_invalid_reference(void)
{
    const char *path = write_temp("scene_invalid.yaml",
        "version: 1\n"
        "materials:\n"
        "  - id: steel_45\n"
        "    density: 1\n"
        "parts:\n"
        "  - id: base\n"
        "    material: missing\n"
    );
    TEST_ASSERT(path != NULL);
    Scene scene;
    SceneError err = {0};
    int ok = parse_scene_yaml(path, &scene, &err);
    TEST_ASSERT(!ok);
    TEST_ASSERT(err.message[0] != '\0');
    remove("scene_invalid.yaml");
    return 1;
}

int main(void)
{
    RUN_TEST(test_valid_scene);
    RUN_TEST(test_invalid_reference);
    
    printf("Tests Run: %d, Failed: %d\n", g_tests_run, g_tests_failed);
    return g_tests_failed > 0 ? 1 : 0;
}