# Формат сцен и геометрии на YAML

Документ фиксирует структуру YAML-файлов для описания деталей, материалов, соединений и сценариев расчётов. Цель — дать однозначную схему, которую сможет разбирать минималистичный парсер без внешних зависимостей.

## Общая структура файла `scene.yaml`

```yaml
version: 1
metadata:
  name: "Reducer demo"
  author: "Demo"
units:
  length: mm
  angle: deg
materials:
  - id: steel_45
    density: 7850
    young_modulus: 2.05e11
    poisson_ratio: 0.29
parts:
  - id: base_plate
    material: steel_45
    geometry:
      primitive:
        type: box
        size: [200, 120, 20]
        fillet: 3
  - id: shaft
    material: steel_45
    geometry:
      primitive:
        type: cylinder
        radius: 10
        height: 180
joints:
  - id: j1
    type: revolute
    parent: base_plate
    child: shaft
    origin: [100, 60, 10]
    axis: [0, 0, 1]
assemblies:
  - id: reducer
    root: base_plate
    children:
      - joint: j1
        child: shaft
analysis:
  - id: static_load
    type: static
    loads:
      - target: shaft
        force: [0, 0, -1200]
        point: [100, 60, 180]
motion:
  - id: spinup
    joint: j1
    profile:
      type: trapezoid
      start: 0
      end: 5
      v_max: 15
```

### Обязательные разделы
- `version` — целое, позволяет будущую миграцию формата.
- `materials`, `parts`, `joints`, `assemblies` — словари и списки, задающие структуру изделия.
- `analysis` или `motion` — хотя бы один сценарий расчётов или движения должен присутствовать.

### Геометрия деталей
- `geometry.primitive` — коробка (`box`), цилиндр (`cylinder`), сфера (`sphere`) и экструзия (`extrude`).
- `geometry.boolean` — булево дерево: `union`, `difference`, `intersection`, где каждая ветвь — примитив или поддерево.
- `geometry.sketch` — эскиз: ссылка на внешний файл `path` или inline-массив точек и сегментов для будущего профилирования.
- `geometry.step` — импорт из STEP: `{ path: assets/step/part.step, scale: 0.001 }`.

### Соединения
- `type` — `revolute`, `prismatic`, `fixed`.
- `origin` — точка установки в мировой системе координат (3 числа).
- `axis` — ось вращения/перемещения (3 числа, нормализуются парсером).
- Ограничения: `limits` со свойствами `lower`, `upper`, `velocity`, `accel`.
- Приводы: `actuator` с параметрами `torque_max`, `feedforward`, `profile` (ссылка на `motion`).

### Сборки
- `root` — id базовой детали.
- `children` — список `{ joint, child }`, допускаются вложенные `children` для иерархий.
- Конфигурации: `configs` со списком `{ id, overrides }` для замены деталей или материалов.

### Анализ и нагрузки
- `analysis.type`: `static` или `modal` (пока заглушка).
- `loads`: сила, момент или закрепление (`fixed: true`), привязанные к точке модели.
- `boundary_conditions`: глобальные закрепления по осям.
- `report`: путь для результатов и флаги включения метрик (перемещения, напряжения, запасы прочности).

### Движение
- `profile.type`: `trapezoid`, `sine`, `points` (последовательность { t, value }).
- `sampling`: число шагов или частота дискретизации.
- `output`: пути для трейсов положения/скорости/ускорения.

## Минималистичный парсер YAML
- Лексер: скаляры (числа, строки в кавычках или без), списки `-`, словари `key:`; якоря/алиасы не поддерживаются.
- Рекурсивный спуск для вложенных структур, строгий учёт отступов пробелами.
- Ошибки: вывод строки/столбца, ожидание токена, подсказка по разделу.
- API: `int parse_scene_yaml(const char *path, Scene *out, SceneError *err);`
  - Нормализация единиц: `mm|cm|m`, `deg|rad` с приведением к СИ в памяти.
  - Валидация ссылок (`joint.parent`, `assembly.root`), уникальность id.

## Импорт STEP (AP203/AP214)
- Сущности: `CARTESIAN_POINT`, `DIRECTION`, `AXIS2_PLACEMENT_3D`, `EDGE_CURVE`, `ADVANCED_FACE`, `CLOSED_SHELL`.
- Парсинг строкой: токены `#<id> = <TYPE> ( ... );` с сохранением таблицы сущностей.
- Триангуляция: плоская дискретизация граней по UV-сетке, сглаживание нормалей опционально.
- Свойства: расчёт AABB и массы из плотности материала, сохранение трансформации детали.
- API: `int load_step_mesh(const char *path, float scale, Mesh *out, MeshError *err);`

## Внутренние структуры данных
```c
typedef struct Material {
    const char *id;
    float density;
    float young_modulus;
    float poisson_ratio;
} Material;

typedef struct GeometryNode {
    enum { GEO_PRIMITIVE, GEO_BOOLEAN, GEO_SKETCH, GEO_STEP } kind;
    // дочерние указатели или параметры примитива
} GeometryNode;

typedef struct Part {
    const char *id;
    const Material *material;
    GeometryNode *geometry;
    float transform[16];
} Part;

typedef struct Joint {
    const char *id;
    const char *parent;
    const char *child;
    enum { JOINT_REVOLUTE, JOINT_PRISMATIC, JOINT_FIXED } type;
    float origin[3];
    float axis[3];
} Joint;

typedef struct AssemblyNode {
    const Part *part;
    const Joint *via_joint;
    struct AssemblyNode *children;
    size_t child_count;
} AssemblyNode;

typedef struct LoadCase {
    const char *id;
    // силы/моменты привязанные к Part
} LoadCase;

typedef struct MotionProfile {
    const char *id;
    // выборка по времени
} MotionProfile;
```

## CLI и тесты
- CLI: `app --scene scene.yaml --run analysis|motion --frames 100 --out report.json`.
  - Чтение сцены → построение иерархии → запуск статического решателя или интегратора движения.
  - Стабильный формат отчёта: `{ "analysis": [...], "motion": [...], "logs": [...] }`.
- Тесты:
  - Парсинг YAML: валидные файлы и ошибки отступов/идентификаторов.
  - Загрузчик STEP: импорт простого куба и сравнение AABB.
  - Кинематика: прогон шарнирной пары с аналитическим положением.

## Демонстрационные сцены
Примеры хранятся в `assets/scenes/` и покрывают типовые сценарии: шарнирная пара, редуктор, статический упор.

- `hinge_pair.yaml` — простая шарнирная пара с ограничением угла и синусоидальным приводом.
- `gear_reducer.yaml` — упрощённый двухвальный редуктор с эскизом зубчатой пары и связкой приводных валов.
- `static_bracket.yaml` — консольный кронштейн, нагруженный внешней силой и закреплением на стене.
