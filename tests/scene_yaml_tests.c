#include <assert.h>
#include "core/platform/platform.h"
#include <stdio.h>
#include <string.h>

#include "services/scene/cad_scene_yaml.h"

static const char *write_temp(const char *name, const char *text)
{
    FILE *f = platform_fopen(name, "wb");
    if (!f) return NULL;
    fputs(text, f);
    fclose(f);
    return name;
}

static void test_valid_scene()
{
    const char *path = write_temp("/tmp/scene_valid.yaml",
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
    assert(path);
    Scene scene;
    SceneError err = {0};
    int ok = parse_scene_yaml(path, &scene, &err);
    assert(ok);
    assert(scene.material_count == 1);
    assert(scene.part_count == 1);
    assert(scene.joint_count == 1);
    assert(scene.analysis_count == 1);
    assert(scene.motion_count == 1);
    assert(scene.parts[0].material == &scene.materials[0]);
    assert(scene.motion_profiles[0].joint == &scene.joints[0]);
    scene_dispose(&scene);
}

static void test_invalid_reference()
{
    const char *path = write_temp("/tmp/scene_invalid.yaml",
        "version: 1\n"
        "materials:\n"
        "  - id: steel_45\n"
        "    density: 1\n"
        "parts:\n"
        "  - id: base\n"
        "    material: missing\n"
    );
    assert(path);
    Scene scene;
    SceneError err = {0};
    int ok = parse_scene_yaml(path, &scene, &err);
    assert(!ok);
    assert(err.message[0] != '\0');
}

int main(void)
{
    test_valid_scene();
    test_invalid_reference();
    return 0;
}
