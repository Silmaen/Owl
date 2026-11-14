# Mesures de performance du moteur

> Revu le 2026-11-12 sur `Feature/sample-games-new` b4281d72. Instantané : campagne du 2026-10-05 (§1 à §8), chantier
> du 2026-10-09 (§9), campagne du 2026-11-12 (§3.1, §3.4, §3.6), physique du 2026-11-13 (§3.7), même protocole ;
> la mesure la plus récente fait foi.
> Harnais : `test/bench/` (`owl_bench`, option CMake `OWL_BENCHMARK`, `test/bench/README.md`), baseline CI dans
> `test/bench/baseline/linux-bench.json`. Données brutes (`results/`, `callgrind/`) : non versionnées.

## 1. Résumé

1. Le harnais est maison (médiane, quartiles, CSV / JSON) ; deux campagnes complètes concordent (écart médian 0,5 %).
2. ECS : itération d'une vue à 2 composants 2,6 à 3,2 ns/entité, `findEntityByUUID` 11 à 25 ns, `Scene::copy`
   (passage en Play) 3,1 ms pour 10 000 entités.
3. Frame 2D : 0,33 ms pour 10 000 sprites (backend Null), 0,46 ms en forêt de hiérarchies ; un cache dense des
   matrices monde remplace les tables de hachage par frame (§9.5).
4. Renderer2D : un draw call pour 20 000 quads, 9,4 ns par quad sur le chemin `worldIndex`, 14 à 24 ns sur le chemin
   transitoire (§9.5).
5. Le point le plus lent est l'écriture YAML : 24 µs par entité (yaml-cpp), contre 3,5 à 8,3 µs au chargement
   (rapidyaml) et 1,9 à 3,9 µs depuis le binaire cuit ; un snapshot d'undo coûte 20 µs à l'écriture.
6. Voxel : 350 µs de maillage par chunk 16³ de terrain (1,7 ms au pire), sur les workers avec un upload budgété (pic de
   frame en streaming 0,35 ms, §3.10) ; aucun raccourci pour les chunks uniformes.
7. Luau : 71 ns par `on_update` vide, 189 ns avec `get_position` + `set_position`, mais 46 µs et **126 Kio** par
   `ScriptInstance` (un `lua_State` par entité, P-11).
8. Jolt en 2D : 1,27 ms par pas pour 1 000 boîtes qui tombent (5,6 fois Box2D v3), 0,61 ms à 4 workers, 0,81 ms avec
   un seul pas de collision ; 5 000 boîtes : 1,31 ms à 4 workers avec un pas de collision (P-12).
9. Slang 2026.19 : 165 ms à froid, 21 à 35 ms par shader en Release ; shaders du moteur précompilés au build
   (`OwlShaderBake`, §9.6).
10. Frame bench GPU (§8) : le runner Vulkan ne vide pas la file (0 vidage, 1 soumission par frame) ; sur NVIDIA, frame
    Vulkan 0,48 à 0,79 ms contre 0,33 à 0,59 ms en OpenGL (§9.3).
11. Simulation et IA : le hash du monde coûte 28 µs par corps et une frame enregistrée +37 % (P-13) ; une volée de
    1 000 agents prend 9,8 ms par frame (P-14).

## 2. Protocole

| Élément          | Valeur                                                                                                           |
|------------------|------------------------------------------------------------------------------------------------------------------|
| Machine          | Intel Core i9-13950HX, 32 threads (CPU 0-15 : 8 P-cores HT, 16-31 : E-cores), Linux 7.0                          |
| Conteneur        | `./docker_run.sh` (image `devel-ubuntu2604`), clang 22.1.2, preset `linux-clang-release` (`-O3`, libs partagées) |
| Dossier          | `output/build/linux-clang-release-bench`, `-DOWL_BENCHMARK=ON`                                                   |
| Épinglage        | `taskset -c 6` (un thread de P-core), processus entier                                                           |
| Backend          | Application factice (`isDummy`), rendu Null, son Null, log coupé (`Level::Off`)                                  |
| Échantillons     | 2 de chauffe puis 15 mesurés ; lot calibré pour qu'un échantillon dure au moins 10 ms                            |
| Statistiques     | médiane par appel, IQR (p75 - p25) en % de la médiane, minimum ; « par item » = médiane / nombre d'items         |
| Reproductibilité | deux campagnes (run1, run2) : écart médian 0,5 %, p90 2,5 %, maximum 10 % (`world_transform/*`)                  |
| Charge           | machine partagée avec d'autres agents : load average 10,7 à 11,2 ; 15 à 18 le 2026-11-12 (§3.7 : jusqu'à 285)    |

Les campagnes du §9 répètent chaque configuration 3 fois (médiane des médianes) à load average 3 à 12.

```bash
./docker_run.sh cmake --preset linux-clang-release -S . -B output/build/linux-clang-release-bench -DOWL_BENCHMARK=ON
./docker_run.sh cmake --build output/build/linux-clang-release-bench --target owl_bench
./docker_run.sh bash -c 'cd output/build/linux-clang-release-bench/bin && taskset -c 6 ./owl_bench --csv=../results/run1.csv --json=../results/run1.json'
```

Scènes synthétiques (`test/bench/cases/SceneBench.cpp`, `makeSpriteScene`) : entités `Transform` + `SpriteRenderer`
(plus les composants obligatoires), chacune translatée de +1 en x dans l'espace de son parent. Formes : `flat` (toutes
racines), `chain` (chaque entité fille de la précédente), `wide` (une racine, N - 1 enfants directs), `forest` (racines
de 100 enfants).

## 3. Résultats

### 3.1 Scène et ECS

Campagne du 2026-11-12 (`linux-bench`, `taskset -c 6`, load average 16 à 18).

| Cas                                          | Médiane | Par item | IQR   | Remarque                                    |
|----------------------------------------------|---------|----------|-------|---------------------------------------------|
| `scene/create_entities/1000`                 | 112 µs  | 112 ns   | 1,8 % | 5 composants + UUID + index UUID            |
| `scene/create_entities/100000`               | 15,1 ms | 151 ns   | 7,3 % |                                             |
| `scene/memory/bytes_per_sprite_entity`       | 295 o   |          |       | delta `mallinfo2`, scène de 100 000 sprites |
| `scene/view_iterate/transform_sprite/10000`  | 25,8 µs | 2,6 ns   | 2,3 % | vue EnTT à 2 composants                     |
| `scene/view_iterate/transform_sprite/100000` | 319 µs  | 3,2 ns   | 2,1 % |                                             |
| `scene/find_entity_by_uuid/1000`             | 11,0 µs | 11,0 ns  | 0,7 % | `unordered_map` UUID → entité               |
| `scene/find_entity_by_uuid/100000`           | 2,51 ms | 25,1 ns  | 3,8 % | sorties de cache                            |
| `scene/copy/1000`                            | 270 µs  | 270 ns   | 4,8 % | `Scene::copy` (Play), forêt                 |
| `scene/copy/10000`                           | 3,14 ms | 314 ns   | 1,4 % |                                             |
| `scene/prepare_world_transforms/flat10000`   | 205 µs  | 20,5 ns  | 1,0 % | passe par frame (§9.5)                      |
| `scene/prepare_world_transforms/forest10000` | 454 µs  | 45,4 ns  | 4,2 % |                                             |
| `scene/world_transform/forest10000_all`      | 398 µs  | 39,8 ns  | 2,9 % | `getWorldTransform` hors frame              |
| `scene/world_transform/chain1000_all`        | 5,99 ms | 6,0 µs   | 0,9 % | O(profondeur) par lecture                   |
| `scene/set_parent/chain1000_build`           | 6,20 ms | 6,2 µs   | 2,8 % |                                             |

La passe par frame `prepare_world_transforms` coûte 1,5 à 1,7 fois sa valeur du 2026-10-05 (139 et 272 µs), à charge
comparable ; cause non profilée, à surveiller.

### 3.2 Frame complète (backend Null)

`frame/editor_update` appelle `Scene::onUpdateEditor` (préparation des transforms, rendu 2D, HUD) ;
`frame/runtime_update` appelle `Scene::onUpdateRuntime` après `onStartRuntime`, avec (`render_`) ou sans (`logic_`)
rendu.

| Cas                                     | Médiane  | Par sprite |
|-----------------------------------------|----------|------------|
| `frame/editor_update/flat10000`         | 0,330 ms | 33 ns      |
| `frame/editor_update/forest10000`       | 0,462 ms | 46 ns      |
| `frame/editor_update/chain1000`         | 38,7 µs  | 39 ns      |
| `frame/runtime_update/logic_flat10000`  | 0,140 ms | 14 ns      |
| `frame/runtime_update/render_flat10000` | 0,334 ms | 33 ns      |

### 3.3 Renderer2D (backend Null)

Une frame = `beginScene`, N appels, `endScene` (flush compris).

| Cas                                         | Médiane | Par quad | Draw calls |
|---------------------------------------------|---------|----------|------------|
| `renderer2d/transform_to_matrix` (par quad) |         | 4,2 ns   |            |
| `renderer2d/quads_world_index/10000`        | 94 µs   | 9,4 ns   | 1          |
| `renderer2d/quads_color/10000`              | 168 µs  | 16,8 ns  | 3          |
| `renderer2d/quads_16_textures/10000`        | 242 µs  | 24,2 ns  | 3          |
| `renderer2d/circles/10000`                  | 138 µs  | 13,8 ns  | 3          |
| `renderer2d/text/100_strings_54_chars`      | 1,08 ms | 200 ns   | par glyphe |

Le chemin transitoire coupe un lot tous les 4 096 transforms (`g_maxTransientWorldsPerBatch`), le chemin `worldIndex`
tous les 20 000 quads ; il paie l'écriture d'une mat4 de 64 o par quad.

### 3.4 Sérialisation YAML, undo, prefab

Campagne du 2026-11-12, sauf `prefab/*` (2026-10-05).

| Cas                                          | Médiane | Par entité | Remarque                             |
|----------------------------------------------|---------|------------|--------------------------------------|
| `serialize/scene_to_string/10000`            | 241 ms  | 24,1 µs    | écriture yaml-cpp                    |
| `serialize/scene_from_string/10000`          | 82,6 ms | 8,3 µs     | lecture rapidyaml                    |
| `serialize/scene_from_string/1000`           | 3,5 ms  | 3,5 µs     |                                      |
| `serialize/scene_parse_only/10000`           | 65 ms   | 6,5 µs     | `parseBuffer` seul                   |
| `serialize/scene_from_binary/10000`          | 39 ms   | 3,9 µs     | binaire cuit (`toBinary`), 2,3 Mo    |
| `serialize/scene_from_binary/1000`           | 1,9 ms  | 1,9 µs     |                                      |
| `serialize/entity_to_string/scene1`          | 20,5 µs |            | snapshot d'undo, 202 o               |
| `serialize/entity_to_string/scene10000`      | 20,6 µs |            | indépendant de la taille de la scène |
| `serialize/entity_from_string/scene1`        | 3,4 µs  |            | restauration d'une entité            |
| `serialize/entity_from_string/scene10000`    | 22,4 µs |            | IQR 17 %                             |
| `prefab/instantiate/10_entities/scene0`      | 49 µs   | 4,9 µs     |                                      |
| `prefab/instantiate/100_entities/scene10000` | 388 µs  | 3,9 µs     | indépendant de la scène cible        |
| `sample/load/voxel_terrain`                  | 81 µs   |            | scène de la démo                     |
| `sample/load/main_menu`                      | 493 µs  |            |                                      |
| `sample/load/raycast_demo`                   | 1,05 ms |            |                                      |
| `sample/load/world_map`                      | 1,27 ms |            |                                      |
| `sample/load/tavern`                         | 1,75 ms |            |                                      |
| `sample/load/platformer_house`               | 1,79 ms |            |                                      |
| `sample/load/iso_town`                       | 3,90 ms |            | plus grosse scène de la démo         |

L'écriture reste en yaml-cpp : `YAML::Emitter::Write` pèse l'essentiel de `serializeEntityToString`
(`IsValidPlainScalar` teste chaque chaîne par expressions régulières). Mécanisme de la lecture : §9.7.

### 3.5 Voxel

| Cas                                     | Médiane | Quads  | Maillage | Remarque                       |
|-----------------------------------------|---------|--------|----------|--------------------------------|
| `voxel/generate_chunk/sky`              | 12,9 µs |        |          | chunk d'air                    |
| `voxel/generate_chunk/surface`          | 36,1 µs |        |          | Perlin 2D + grottes 3D         |
| `voxel/generate_chunk/underground`      | 108 µs  |        |          | bruit 3D sur tout le volume    |
| `voxel/mesh_by_kind/full` (sans AO)     | 395 µs  | 6      | 1,1 Ko   | aucun raccourci chunk uniforme |
| `voxel/mesh_by_kind/full` (AO)          | 476 µs  | 6      | 1,1 Ko   |                                |
| `voxel/mesh_by_kind/terrain_surface`    | 282 µs  | 301    | 54 Ko    | sans AO                        |
| `voxel/mesh_by_kind/terrain_surface` AO | 350 µs  | 301    | 54 Ko    | AO : +24 %                     |
| `voxel/mesh_by_kind/random50` AO        | 1,33 ms | 6 071  | 1,1 Mo   |                                |
| `voxel/mesh_by_kind/checkerboard` AO    | 1,67 ms | 12 288 | 2,2 Mo   | pire cas théorique             |
| `voxel/encode_rle/terrain_surface`      | 4,6 µs  |        |          | encodage RLE de sauvegarde     |

Un chunk stocke 16 Ko (blocs + métadonnées, 2 × 4 096 × 2 o). Un sommet `VoxelVertex` fait 40 o, soit 184 o par quad
avec les indices : le maillage d'un chunk de surface pèse 3,4 fois ses données. Profil de `terrain_surface/ao` :
`Chunk::getBlock` (non inline, avec contrôle de bornes) 18,9 % et `BlockRegistry::isOpaque` 13,9 % des instructions.

### 3.6 Luau

Campagne du 2026-11-12 (Luau 0.746).

| Cas                                                | Médiane   | Par instance |
|----------------------------------------------------|-----------|--------------|
| `script/create_instance/empty_script`              | 46,1 µs   |              |
| `script/memory/bytes_per_instance`                 | 129 041 o |              |
| `script/on_update/empty/1_instance`                | 71,2 ns   | 71,2 ns      |
| `script/on_update/empty/1000_instances`            | 215 µs    | 215 ns       |
| `script/on_update/get_set_position/1_instance`     | 189 ns    | 189 ns       |
| `script/on_update/get_set_position/1000_instances` | 521 µs    | 521 ns       |
| `script/on_update/arith_100/1000_instances`        | 789 µs    | 789 ns       |

Chaque instance a son propre `lua_State` (`ScriptInstance.cpp:46-48`) : la mémoire est un delta `mallinfo2` sur 1 000
instances d'un script vide, bindings compris ; à 1 000 instances le coût par appel triple (données dispersées).

### 3.7 Physique (Jolt 5.6.0, 2D)

Boîtes dynamiques 1 × 1 en grille de 200 colonnes, sol statique, corps 2D verrouillés dans le plan
(`EAllowedDOFs::Plane2D`) ; `PhysicCommand::frame` à pas fixe de 16,7 ms. `step_falling` : les 60 premières frames
après `init` ; `step_settled` : 60 frames après 600 frames de stabilisation. Campagne du 2026-11-13 : un thread sur
`taskset -c 6`, les workers sur `taskset -c 2,4,6,8,10` (un thread par P-core), load average 3 à 15 ; `init` et
`step_settled` du 2026-11-12. Box2D 3.1.1 : même cas, même protocole, 2026-10-05.

| Cas (par pas)                          | 100 corps | 500 corps | 1 000 corps | 5 000 corps | Box2D (100 / 1 000 / 5 000) |
|----------------------------------------|-----------|-----------|-------------|-------------|-----------------------------|
| `physics/init` (total)                 | 182 µs    |           | 859 µs      | 3,55 ms     | 54 µs / 586 µs / 3,33 ms    |
| `step_falling`, 1 thread, 2 pas        | 109 µs    | 690 µs    | 1,27 ms     | 3,51 ms     | 22,9 µs / 228 µs / 1,12 ms  |
| `step_falling`, 4 workers, 2 pas       | 100 µs    | 349 µs    | 0,61 ms     | 2,12 ms     |                             |
| `step_falling`, 1 thread, 1 pas        |           |           | 0,81 ms     | 2,29 ms     |                             |
| `step_falling`, 4 workers, 1 pas       |           |           | 0,37 ms     | 1,31 ms     |                             |
| `step_settled`, 1 thread, 2 pas        | 4,0 µs    | 13 µs     | 25 µs       | 8,2 ms      | 2,9 µs / 28 µs / 4,88 ms    |

Profil callgrind de `step_falling/1000_bodies` (Ir) : Jolt 96 % du pas (recherche des collisions 39 % dont EPA 11 %,
solveur de vitesses 32 %, de positions 14 %, broad phase 10 %, sommeil et boîtes 10 %), copie des poses vers les
entités 3 %, contacts et champs de gravité moins de 1 %. Une boîte 2D est une dalle 3D : le contact de deux faces
garde quatre points, deux pour Box2D. Réglages essayés sans gain mesurable : suppression renforcée des arêtes
internes, réduction de variété, distance spéculative, durée avant sommeil, itérations de position ; 6 itérations de
vitesse au lieu de 10 : -12 %. Un pas de collision au lieu de deux : -36 %, mais la simulation change (hash de
référence, replay de la démo, quatre tests) : le défaut reste à 2, `solverSubSteps` se règle par scène. Le seuil du
nombre automatique de workers passe à 250 corps dynamiques : 4 workers divisent le pas par 2 dès 500 corps, sans effet
à 100 (le résultat ne dépend pas du nombre de workers). `physics/frame_empty` : 2,2 µs par frame à 60 Hz, 1,2 µs à
144 Hz.

### 3.8 Scènes de `engine_demo/` (backend Null)

| Scène              | Entités | Fichier | Chargement | Frame éditeur | Quads |
|--------------------|---------|---------|------------|---------------|-------|
| `game_over`        | 7       | 4,2 Ko  | 1,18 ms    | 10,5 µs       | 33    |
| `main_menu`        | 17      | 10,3 Ko | 2,86 ms    | 34,2 µs       | 115   |
| `platformer_house` | 34      | 19,0 Ko | 5,40 ms    | 28,7 µs       | 93    |
| `raycast_demo`     | 110     | 55,3 Ko | 15,5 ms    | 44,3 µs       | 99    |
| `settings_menu`    | 10      | 6,1 Ko  | 1,71 ms    | 20,0 µs       | 71    |
| `victory`          | 6       | 3,3 Ko  | 0,96 ms    | 10,1 µs       | 32    |
| `voxel_terrain`    | 2       | 2,5 Ko  | 0,61 ms    | 14,9 µs       | 0     |
| `world_map`        | 17      | 7,3 Ko  | 2,30 ms    | 25,2 µs       | 77    |

Les scènes d'exemple (110 entités au plus) ne stressent pas le CPU du moteur ; elles servent au frame bench GPU (§8).
Le chargement ci-dessus est celui de la campagne du 2026-10-05 ; les chargements courants sont au §3.4. `voxel_terrain`
ne maille rien : sans couche `RendererVoxel` (application factice), le streaming éditeur ne produit aucun maillage.

### 3.9 Démarrage et shaders

| Cas                                                 | Médiane |
|-----------------------------------------------------|---------|
| `app/startup_ms/dummy_null_backend`                 | 236 ms  |
| `slang/compile_cold/quad_vulkan_first_in_process`   | 165 ms  |
| `slang/compile_warm/quad_vulkan`                    | 21,1 ms |
| `slang/compile_warm/voxel_vulkan`                   | 35,3 ms |
| `slang/compile_warm/all_engine_shaders_vulkan` (12) | 270 ms  |
| `slang/cache_hit/quad_validate_and_read`            | 5,4 µs  |

La compilation à froid comprend la création de la session globale Slang (`shaderFileUtils.cpp:185-194`). Les shaders du
moteur sont précompilés au build par `OwlShaderBake` et lus avant toute compilation : Slang n'est sur le chemin de
démarrage que pour un shader modifié, un hot reload ou un shader de jeu (§9.6). Démarrage des runners GPU : §9.8.

### 3.10 Streaming voxel par frame

`voxel/streaming/*` (`test/bench/cases/VoxelStreamingBench.cpp`) : monde procédural, couche `RendererVoxel` réelle
(backend Null : l'upload ne coûte rien, seul le CPU est mesuré), frames cadencées à 60 Hz, temps CPU du thread
principal par frame (`CLOCK_THREAD_CPUTIME_ID`, scène + `Scheduler::frame`), maillage sur les workers. `taskset -c 6`,
médiane de 3 passes (min-max entre crochets), load average 18 à 32.

| Scénario                                     | Mesure                       | Valeur                |
|----------------------------------------------|------------------------------|-----------------------|
| `runtime_walk/walk` (Play, r = 4, h = 2,     | pic de frame                 | 0,35 ms [0,34-0,57]   |
| 32 chunks à 1 chunk / 16 frames, 518 frames) | p99 / p50                    | 0,34 / 0,08 ms        |
|                                              | frames > 5 ms                | 0                     |
|                                              | CPU thread principal (total) | 55 ms                 |
|                                              | CPU processus (total)        | 828 ms                |
|                                              | maillage, tous threads       | 651 ms, 1 152 uploads |
|                                              | latence d'apparition moyenne | 29 ms (1,75 frame)    |
| `runtime_walk/initial_load` (405 chunks)     | pic / frames > 5 ms          | 0,48 ms / 0           |
|                                              | latence moyenne / max        | 58 / 183 ms           |
| `editor_load` (r = 8, h = 4, 2 601 chunks,   | pic / p50                    | 0,49 / 0,31 ms        |
| 168 frames)                                  | frames > 5 ms                | 0                     |
|                                              | CPU thread principal (total) | 51 ms                 |
|                                              | CPU processus (total)        | 1 606 ms              |
|                                              | meshes finaux / uploads      | 1 492 / 1 974         |
|                                              | latence moyenne / max        | 422 ms / 2,0 s        |

Sur 4 cœurs (`taskset -c 6-9`), pic de la marche 0,54 ms, latences identiques : la latence n'est pas bornée par le CPU.
Un chunk attend que ses voisins en cours de génération soient installés (16 chunks générés par frame), puis passe au
plus 32 uploads par frame. L'arrivée d'un voisin remaille le chunk déjà maillé ; les faces de bord cachées par des
voisins arrivés après coup ne sont pas émises. Le thread principal ne fait que capturer, téléverser et dessiner.

## 4. Constats

```text
ID             : P-03
Axe            : C
Nature         : faiblesse
Gravité        : basse
Preuve         : Scene.cpp:4074-4105 (`isEffectivelyVisible`) ; frame/editor_update/chain1000 = 38,7 µs (§9.5).
Constat        : Pendant une passe de mise à jour, la visibilité héritée est précalculée par entité dans
                 `prepareWorldTransforms` (un bit par mode). Hors passe, `isEffectivelyVisible` remonte la chaîne de
                 parents (un contrôle de visibilité par niveau), en O(profondeur).
Comparaison    : Godot propage la visibilité dans l'arbre ; Bevy calcule InheritedVisibility dans une passe descendante.
Recommandation : surveiller, effort S : un appel hors passe sur une hiérarchie très profonde est le seul cas touché.
Statut         : confirmé (lecture du code)
```

```text
ID             : P-05
Axe            : B
Nature         : force
Gravité        : basse
Preuve         : Renderer2D.cpp:461-492 et :310-376 ; renderer2d/quads_world_index/10000 = 9,4 ns par quad, 1 draw call
                 pour 10 000 quads, 5 pour 100 000.
Constat        : Le batching par instances (SSBO) est efficace : un draw call par type de primitive jusqu'à 20 000
                 instances, et un coût CPU de 9 à 10 ns par quad quand le monde vient de la scène.
Comparaison    : Opinion : au niveau des batchers 2D courants (Hazel-like, raylib) ou mieux, puisque les sommets ne sont
                 pas dupliqués sur CPU.
Recommandation : garder.
Statut         : confirmé (mesuré)
```

```text
ID             : P-07
Axe            : C / E (sérialisation, undo, prefab)
Nature         : faiblesse
Gravité        : basse
Preuve         : serialize/scene_to_string/10000 = 24,1 µs par entité (yaml-cpp), scene_from_string/10000 = 8,3 µs
                 (rapidyaml), scene_from_binary/10000 = 3,9 µs ; entity_to_string 20,5 µs (§3.4, 2026-11-12).
Constat        : La lecture est rapide (rapidyaml, binaire cuit dans le .owlpack), mais l'écriture reste en yaml-cpp :
                 le snapshot d'undo, la sauvegarde et la fusion de prefab paient 20 à 24 µs par entité, 3 à 6 fois la
                 lecture.
Comparaison    : Godot sérialise en texte (.tscn) mais cuit en binaire (.scn) à l'export ; Bevy instancie depuis un
                 monde en mémoire.
Recommandation : surveiller, effort M : émetteur rapidyaml ou maison pour l'undo et la sauvegarde (C-10, A-22).
Statut         : confirmé (mesuré)
```

```text
ID             : P-08
Axe            : C
Nature         : force
Gravité        : basse
Preuve         : scene/view_iterate = 2,6-3,2 ns par entité ; find_entity_by_uuid 11-25 ns ; scene/copy/10000 = 3,14 ms ;
                 create_entities 112-151 ns (§3.1, 2026-11-12).
Constat        : Les primitives ECS sont au niveau attendu d'EnTT ; le passage en Play (Scene::copy) coûte 3 ms pour
                 10 000 entités, sans aller-retour YAML.
Comparaison    : Conforme aux ordres de grandeur publiés par EnTT pour l'itération de vues (à sourcer).
Recommandation : garder.
Statut         : confirmé (mesuré)
```

```text
ID             : P-09
Axe            : D (voxel)
Nature         : faiblesse
Gravité        : basse
Preuve         : voxel/mesh_by_kind : 282-350 µs (surface), 395-476 µs (chunk plein, 6 quads), 1,67 ms (damier) ;
                 callgrind (2026-10-05) : Chunk::getBlock (data/voxel/Chunk.h:133, non inline) 18,9 %,
                 BlockRegistry::isOpaque 13,9 % ;
                 voxel/streaming : pic de frame 0,35 ms (§3.10).
Constat        : Un chunk uniforme (plein ou souterrain) coûte autant qu'un chunk de surface, faute de raccourci. Le
                 maillage tourne sur les workers avec un upload budgété : il ne pèse pas sur le thread principal.
Comparaison    : Opinion : les mailleurs gloutons par masques de bits traitent des chunks bien plus gros dans le même
                 temps.
Recommandation : corriger, effort M : raccourci pour les chunks uniformes, accès direct au tableau avec bordure
                 recopiée (au lieu d'un std::function par voisin).
Statut         : confirmé (mesuré)
```

```text
ID             : P-10
Axe            : D (voxel)
Nature         : faiblesse
Gravité        : basse
Preuve         : data/voxel/ChunkMesher.h:32-45 (VoxelVertex = 44 o avec le champ `surface`) ;
                 voxel/mesh_by_kind/terrain_surface/mesh_bytes = 55 Ko pour 16 Ko de données (2026-10-05).
Constat        : Le format de sommet est large (positions et normales en float, UV en float) et il est recopié une fois
                 de plus à l'upload : 184 o par quad sur CPU, davantage sur GPU.
Comparaison    : Les moteurs voxel compressent couramment un sommet en 4 à 8 octets (position entière dans le chunk,
                 face, AO, index de texture).
Recommandation : surveiller, effort M : à traiter avec P-09 si la mémoire GPU devient un sujet (frame bench, §8).
Statut         : confirmé (mesuré)
```

```text
ID             : P-11
Axe            : D (script)
Nature         : faiblesse
Gravité        : moyenne
Preuve         : script/memory/bytes_per_instance = 129 041 o ; create_instance 46,1 µs ; on_update/empty 71 ns
                 (1 instance), 215 ns (1 000) ; get_set_position 189-521 ns (§3.6, 2026-11-12, Luau 0.746) ;
                 ScriptInstance.cpp:46-48 (un LuaEngine, donc un lua_State, par instance) ; Lua 5.5 le 2026-10-05 :
                 9 020 o et 28 µs.
Constat        : L'appel reste bon marché, mais un lua_State Luau par entité coûte 126 Kio, 14 fois l'état Lua 5.5
                 qu'il remplace : 1 000 entités scriptées pèsent 126 Mo, 10 000 en pèsent 1,26 Go. Le coût par appel
                 triple à 1 000 instances (données dispersées).
Comparaison    : Luau est conçu pour une VM partagée avec un thread et un environnement bac à sable par script
                 (lua_newthread + luaL_sandboxthread, modèle Roblox) ; Defold partage aussi une VM.
Recommandation : surveiller, effort S ; décidé le 2026-11-12 : un lua_State par entité est gardé (isolation) ;
                 mesurer ce qui pèse dans les 126 Kio (bindings recopiés par état ?) pour le réduire.
Statut         : confirmé (mesuré)
```

```text
ID             : P-12
Axe            : D (physique)
Nature         : faiblesse
Gravité        : moyenne
Preuve         : §3.7 (2026-11-13) : step_falling 109 µs / 1,27 ms / 3,51 ms pour 100 / 1 000 / 5 000 boîtes 2D sur un
                 thread, contre 22,9 µs / 228 µs / 1,12 ms avec Box2D 3.1.1 (même cas) ; 0,61 ms / 2,12 ms à
                 4 workers ; avec un pas de collision 0,81 ms / 2,29 ms, 1,31 ms à 5 000 corps et 4 workers ; Jolt
                 fait 96 % du pas ; recette Jolt sans AVX ni FMA (conan/recipes/joltphysics/all/conanfile.py:57-61).
Constat        : Jolt, seul moteur physique, fait la 2D avec des corps 3D verrouillés dans un plan : une scène 2D
                 dynamique coûte 3 à 6 fois plus par pas qu'avec Box2D sur un thread, le contact de deux dalles
                 gardant quatre points. La cible « pas < 1,5 ms à 5 000 corps » n'est tenue qu'à 4 workers avec un
                 pas de collision (1,31 ms) ; au défaut (2 pas) : 2,12 ms à 4 workers, 3,5 ms sur un thread. Une
                 pile endormie reste aussi rapide (25 µs à 1 000 corps).
Comparaison    : Godot garde un moteur 2D dédié à côté du 3D ; le déterminisme multiplateforme de Jolt est rare.
Recommandation : surveiller ; décidé le 2026-11-12 : Jolt reste le moteur 2D. Profil fait (§3.7) : aucun réglage
                 de Jolt ne gagne sans changer la simulation, hors nombre de workers (seuil automatique à 250 corps) ;
                 une scène 2D dense règle `solverSubSteps` à 1. Cibles : 1 000 corps < 1,5 ms sur un thread,
                 5 000 corps < 1,5 ms à 4 workers avec un pas de collision.
Statut         : confirmé (mesuré)
```

```text
ID             : P-13
Axe            : D (simulation, replay)
Nature         : faiblesse
Gravité        : moyenne
Preuve         : `owl_bench` (`test/bench/cases/Cases.h`, `runReplayBenches`, 2026-11-13) : `replay/world_hash/1000_bodies`
                 28 ms (28 µs par corps, budget 5 ms) ; `replay/live`, `replay/record` et `replay/playback` à 100 corps :
                 132 µs, 180 µs et 179 µs par frame.
Constat        : `computeWorldHash` sérialise chaque entité en YAML pour la hacher : 28 µs par corps. Le hachage revient
                 toutes les 60 ticks pendant l'enregistrement et la relecture, donc à chaque Play dans l'éditeur (~3 ms
                 à 100 corps) ; une frame enregistrée ou rejouée coûte 37 % de plus qu'une frame vivante (budget : 10 %).
Comparaison    : les moteurs à relecture déterministe hachent l'état binaire des composants, pas une forme texte.
Recommandation : corriger, effort M : hacher l'état binaire des composants au lieu du YAML (change les hachages de
                 référence des scénarios rejoués).
Statut         : confirmé (mesuré)
```

```text
ID             : P-14
Axe            : D (IA)
Nature         : faiblesse
Gravité        : moyenne
Preuve         : `owl_bench` (`runAiBenches`, 2026-11-13) : `ai/steering/flock/1000_agents` 9,8 ms par frame (budget
                 2 ms) ; `scene/systems/SteeringSystems.cpp`.
Constat        : la cellule de la grille de voisinage vient de la portée d'évitement (~10 m) : chaque agent parcourt et
                 trie environ 400 voisins, alors que le rayon de voisinage de la volée est bien plus petit.
Comparaison    : les boids sur grille uniforme prennent une cellule égale au rayon de voisinage.
Recommandation : corriger, effort S : cellule tirée du rayon de voisinage, k plus proches voisins sans tri complet.
Statut         : confirmé (mesuré)
```

## 5. Le harnais

- `test/bench/harness/Bench.{h,cpp}` : `Runner::measure` (lot calibré), `measureWithSetup` (préparation hors chrono),
  `measureOnce` (chemins froids), `metric` (tailles, compteurs, contrôles de correction), sorties CSV et JSON avec la
  charge de la machine au début et à la fin.
- `test/bench/cases/*.cpp` : un fichier par groupe (`SceneBench`, `SerializeBench`, `Renderer2DBench`, `FrameBench`,
  `VoxelBench`, `VoxelStreamingBench`, `ScriptBench`, `PhysicsBench`, `SampleBench`, `SlangBench`, `HudBench`,
  `IsometricBench`, `TilemapBench`, `PathBench`, `McpBench`, `AiBench`, `ReplayBench`, …).
- `CodeStyle` scanne `test/bench` (`SOURCE_ROOTS`, `ci/actions/code_style.py`).
- Google Benchmark n'est pas utilisé : un paquet Conan `benchmark` n'apporterait que `--benchmark_repetitions`, les
  compteurs utilisateur et la comparaison de runs (`compare.py`).

## 6. Limites des mesures

- **P-core contre E-core.** Sans épinglage et à load average ~45, les temps sont 2 à 3 fois plus longs
  (`scene/create_entities/10000` : 295 ns contre 110 ns par entité). Toute comparaison épingle sur un P-core.
- **Backend Null.** `null::StorageBuffer::setData` fait un vrai `memcpy` (remplissage des SSBO compté) ;
  `VertexBuffer::setData` et `Texture2D::setData` Null sont vides : ni coût de pilote ni coût GPU.
- **Isolation.** Les autres agents tournent sur les autres cœurs : bande passante mémoire et L3 partagés ; les IQR
  restent sous 2 % pour presque tous les cas.
- **Profils.** `perf` n'est pas exécutable dans l'image : profils d'instructions avec
  `valgrind --tool=callgrind --toggle-collect=<fonction>` sur une itération. Les pourcentages sont des instructions
  (Ir), pas du temps.

## 7. Mesures restant à faire

1. **Voxel par frame** (M5, M6) : coût GPU de l'upload et mémoire GPU par chunk (le CPU est au §3.10).
2. **Debug + couverture** : `slang/compile_cold` dans `linux-clang-debug`.
3. **Mémoire** : pic et allocations par frame (heaptrack absent de l'image), contenu des 126 Kio d'un état Luau (P-11) ; `OWL_ENABLE_MEMORY_TRACKER` (M10).
4. **Lua** : `find_entity` depuis un script (M1).
5. **Physique** : 10 000 corps, corps dans une hiérarchie, tilemap collidable (M3).
6. **Chargement** : ouverture et extraction d'un `.owlpack` (M8), premier et second lancement réels.
7. **Profileur et logs** (M11, M12) : coût d'un `OWL_PROFILE_SCOPE` actif et d'une trace désactivée en boucle chaude.
8. **Comparaison externe** : chiffres publiés d'EnTT, Box2D v3, yaml-cpp / rapidyaml, mailleurs voxel, sourcés et
   datés, dans `30-etat-de-l-art.md`.
9. **Chaîne de build** (second rang) : build à froid, build incrémental après modification de `Scene.h`.

## 8. Frame bench GPU

### 8.1 Protocole

| Élément      | Valeur                                                                                                           |
|--------------|------------------------------------------------------------------------------------------------------------------|
| Outil        | `OwlRunner --frame-bench <scène> --backend <b> --frames 1000 --warmup 120 --out <json>` (`test/bench/README.md`) |
| Build        | `linux-clang-release` dans `./docker_run.sh --gui`, clang 22, `-O3`                                              |
| Machine      | i9-13950HX, portable hybride : Intel UHD (RPL-S) principal, RTX 5000 Ada Laptop en PRIME offload                 |
| Affichage    | session Wayland (`WAYLAND_DISPLAY=wayland-0`), fenêtre GLFW via XWayland, 1280 × 720                             |
| Épinglage    | `taskset -c 2-7` (P-cores), processus entier                                                                     |
| Déterminisme | pas fixe 16,667 ms, entrées null, caméra primaire de la scène, son null                                          |
| Présentation | Vulkan `immediate` (vsync coupée), OpenGL `swap-interval-0`                                                      |
| Répétitions  | 3 à 5 par configuration, 1 000 frames mesurées chacune ; médiane des médianes                                    |

Sélection du GPU, vérifiée par le champ `device` de chaque rapport :

| Configuration   | Variables                                                         | `device` rapporté                                   |
|-----------------|-------------------------------------------------------------------|-----------------------------------------------------|
| Vulkan / NVIDIA | `VK_ICD_FILENAMES=/etc/vulkan/icd.d/nvidia_icd.json`              | NVIDIA RTX 5000 Ada Generation Laptop GPU           |
| OpenGL / NVIDIA | `__NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia`    | NVIDIA RTX 5000 Ada Generation Laptop GPU/PCIe/SSE2 |
| Vulkan / Intel  | `VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/intel_icd.json`         | Intel(R) Graphics (RPL-S)                           |
| OpenGL / Intel  | `__GLX_VENDOR_LIBRARY_NAME=mesa MESA_LOADER_DRIVER_OVERRIDE=iris` | Mesa Intel(R) Graphics (RPL-S)                      |

Mesures par frame : temps mur entre deux débuts de frame (`cpu_total`) et ses phases ; temps GPU par timestamps
(Vulkan : `vkCmdWriteTimestamp` autour de **chaque** command buffer soumis, `gpu_busy` = somme des intervalles,
`gpu_span` = premier au dernier timestamp ; OpenGL : `glQueryCounter(GL_TIMESTAMP)` à `beginFrame` et `endFrame`, un
seul intervalle, attente du pilote comprise). Les deux `gpu_busy` ne se comparent pas : travail GPU pur en Vulkan, borne
haute en OpenGL. Compteurs : draw calls, soumissions, `vkQueueWaitIdle`, `vkDeviceWaitIdle`, `fence_wait` (Vulkan,
`vulkan::internal::FrameProfiler`).

Scènes : `main_menu` (UI 2D), `world_map` (tilemap vue de dessus), `platformer_house` (tilemap, 27 sprites, HUD UI,
physique, 17 scripts Lua), `raycast_demo` (raycaster GPU + sprites billboard), `voxel_terrain` (voxel).

### 8.2 Résultats (médianes, ms par frame)

Vulkan : 0 vidage de file (`vkQueueWaitIdle` + `vkDeviceWaitIdle`), 1 soumission et 0 attente de fence par frame, sur
les cinq scènes. Draw calls : 4 (`main_menu`), 6 (`world_map`), 5 (`platformer_house`), 3 (`raycast_demo`), 1
(`voxel_terrain`), identiques entre backends. « Travail » = `cpu_total` moins l'attente de présentation
(`cpu_pace_wait_ms` en Vulkan, `cpu_present_ms` en OpenGL).

| Scène              | GPU    | Vulkan : frame | OpenGL : frame | Travail Vulkan / OpenGL |
|--------------------|--------|----------------|----------------|-------------------------|
| `main_menu`        | NVIDIA | 0,475          | 0,329          | 0,140 / 0,100           |
| `main_menu`        | Intel  | 0,350          | 0,232          | 0,116 / 0,086           |
| `world_map`        | NVIDIA | 0,496          | 0,341          | 0,143 / 0,086           |
| `world_map`        | Intel  | 0,519          | 0,370          | 0,158 / 0,117           |
| `platformer_house` | NVIDIA | 0,494          | 0,336          | 0,162 / 0,104           |
| `platformer_house` | Intel  | 0,466          | 0,247          | 0,184 / 0,099           |
| `raycast_demo`     | NVIDIA | 0,519          | 0,359          | 0,358 / 0,321           |
| `raycast_demo`     | Intel  | 0,418          | 0,623          | 0,370 / 0,342           |
| `voxel_terrain`    | NVIDIA | 0,483          | 0,331          | 0,119 / 0,171           |
| `voxel_terrain`    | Intel  | 0,351          | 0,359          | 0,193 / 0,081           |

GPU Intel des scènes 2D : 0,32 à 0,48 ms de travail par frame.

### 8.3 Constats

1. **Les scènes d'exemple ne chargent pas le GPU** : elles mesurent le socle. `voxel_terrain` n'émet qu'un draw call ;
   ni le maillage ni le streaming n'y sont couverts (M5, M6 restent à faire avec une trajectoire de caméra).
2. **Vulkan reste au-dessus d'OpenGL au temps mur** sauf en raycast sur Intel (0,42 contre 0,62 ms) : cause au §9.3.
3. **Wayland / vsync.** Sans vsync, aucun blocage observé (200 runs, Vulkan et OpenGL, NVIDIA et Intel). Avec
   `--vsync`, Vulkan choisit `mailbox` et tourne ; OpenGL (`swap-interval-1`, NVIDIA offload) se fige au premier
   `swapBuffers` de l'écran de chargement, avec ou sans `WAYLAND_DISPLAY`. Reproductible ; non remesuré sur le code
   courant.
4. **Sensibilité à la charge.** À load average 64 à 164, les temps CPU sont 1,5 à 4 fois plus longs. Toute comparaison
   se fait à charge basse, épinglée, et cite la load average.
5. **Dispersion.** Écart entre répétitions (max - min des médianes CPU) : 1 à 3 % sur Vulkan / NVIDIA pour les scènes
   2D, jusqu'à 78 % sur les configurations les plus courtes ou les plus chargées ; IQR intra-run sous 0,1 ms sur NVIDIA.

### 8.4 Relancer la matrice

```bash
./docker_run.sh --gui env VK_ICD_FILENAMES=/etc/vulkan/icd.d/nvidia_icd.json taskset -c 2-7 \
    output/build/linux-clang-release/bin/OwlRunner --frame-bench engine_demo/scenes/platformer_house.owl \
    --backend vulkan --frames 1000 --warmup 120 --out output/frame-bench/platformer_house.vulkan.nvidia.json
```

Boucle scènes × configurations × répétitions, puis médiane des médianes par série (`summary.<série>.median` de chaque
JSON) ; comparer `cpu_total_ms`, `gpu_busy_ms`, `gpu_span_ms`, `queue_wait_idle` et `device_wait_idle`.

## 9. Cibles de performance : mécanismes et valeurs courantes

Protocole : celui du §2 (`owl_bench`, `taskset -c 6`, 15 échantillons) et du §8.1 (frame bench), 3 répétitions par
configuration, médiane des médianes ; physique multi-thread sur `taskset -c 0-15`. Les valeurs courantes sont dans les
tableaux du §3 et du §8.2 ; chaque sous-section décrit ce qui les produit. Démarrage de l'application factice Null :
236 ms (§3.9).

### 9.1 Cibles et état

| Cible                                  | Valeur courante                                               | Statut             |
|----------------------------------------|---------------------------------------------------------------|--------------------|
| Frame à 10 000 sprites                 | 0,33 ms, 0,46 ms en forêt (§9.5)                              | tenue              |
| Coût par quad Renderer2D < 10 ns       | 9,4 ns sur `worldIndex`, 14 à 24 ns hors `worldIndex` (§9.5)  | gardée en l'état   |
| 0 vidage de file par frame             | 0 dans le runner (§8.2) et dans l'éditeur (§9.10)             | tenue              |
| Frame runner Vulkan ≤ OpenGL           | 0,03 à 0,11 ms de travail CPU au-dessus d'OpenGL (§9.3)       | acceptée en l'état |
| Chargement de scène < 10 µs par entité | 8,3 µs à 10 000 entités, 3,9 µs en binaire (§3.4)             | tenue              |
| Pas physique 2D, 1 000 corps < 1,5 ms  | 1,27 ms sur un thread, 0,61 ms à 4 workers (§3.7)             | tenue              |
| Pas physique 2D, 5 000 corps < 1,5 ms  | 1,31 ms à 4 workers et 1 pas de collision, 2,12 ms au défaut  | tenue (P-12)       |
| Hash du monde, 1 000 corps < 5 ms      | 28 ms, 28 µs par corps (P-13)                                 | non tenue (P-13)   |
| Frame enregistrée ou rejouée ≤ +10 %   | +37 % à 100 corps (P-13)                                      | non tenue (P-13)   |
| Volée de 1 000 agents < 2 ms par frame | 9,8 ms (P-14)                                                 | non tenue (P-14)   |
| Script, coût par instance              | 46 µs, 126 Kio, 71 ns par appel vide (§3.6)                   | acceptée (P-11)    |
| Démarrage à froid jusqu'à la 1re frame | 270 à 440 ms, indépendant du cache de shaders (§9.6, §9.8)    | tenue              |
| SPIR-V stocké                          | validé avant le pilote, recompilé au besoin (§9.8)            | validée            |

### 9.2 Luau

Le profil callgrind du 2026-10-09 portait sur Lua 5.5 et n'est pas refait sous Luau. Mécanismes encore en place : bindings
en fonctions C légères `guarded<fn>`, scène liée et delta dans le `Quota` de l'état, noms de callbacks internés une fois.

### 9.3 Vulkan contre OpenGL au frame bench

Mesures par chronos dans `VulkanHandler::startFrame` et `INTEL_MEASURE=draw` (temps GPU par événement, pilotes anv et
iris). Le `cpu_begin_frame_ms` Vulkan est l'attente du rythme de présentation, pas du travail moteur : sur NVIDIA (PRIME
vers l'écran Intel, Wayland, `immediate`), `vkAcquireNextImageKHR` bloque 0,35 ms ; avec 4 à 6 images au lieu de 3,
l'attente passe sur la fence de frame ; `vkcube --present_mode 0` en 1280 × 720 tourne à 0,48 ms par frame : c'est le
plancher du chemin de présentation, qu'OpenGL atteint plus vite (EGL bloque 0,23 ms dans le swap, compté dans
`cpu_present_ms`). Sur Intel, la frame attend le GPU : 338 µs de travail par frame `main_menu` en Vulkan contre 218 µs
en OpenGL, dont 39 µs pour un `vkCmdClearAttachments` redondant et la profondeur et l'attachement d'identifiant que
porte la cible principale Vulkan (le framebuffer par défaut OpenGL n'en a pas).

Le raycast calcule le DDA sur le CPU pour les profondeurs de colonne et les statistiques (`cpuWalkColumn`, le même que
le backend Null, sans émission) au lieu de relire la passe GPU par `StorageBuffer::getData` ; les lots successifs sur un
même framebuffer partagent une passe de rendu (2 passes par frame 2D) ; `RenderCommand::clear` ne réefface pas une passe
qui vient d'effacer au chargement ; le frame bench rapporte `cpu_pace_wait_ms` (fence de frame + acquisition d'image).
Images de référence inchangées (`ctest -L render`), validation sans message sur NVIDIA et lavapipe.

« Vulkan ≤ OpenGL » au temps mur n'est pas atteignable sous Wayland sans toucher à la présentation : le travail CPU
Vulkan reste 0,03 à 0,11 ms au-dessus d'OpenGL (sauf `voxel_terrain` NVIDIA). Pistes si la cible devait être tenue :
la mesurer sur le travail (hors attente de présentation), ou alléger la cible principale Vulkan (profondeur et
identifiant seulement quand un calque les utilise).

### 9.5 Frame à 10 000 sprites et Renderer2D

Valeurs : §3.1 à §3.3. Mécanisme (callgrind sur `onUpdateEditor` et `runQuads`) : un cache dense des matrices monde
par entité (indexé par `entt::to_entity`, persistant entre frames), sans crochet d'écriture : une entrée reste valide
tant que son `Transform` local, son parent (handle versionné et UUID) et le tampon de version du parent sont
inchangés, vérifiés en remontant la chaîne (quelques comparaisons par niveau, sans limite de profondeur ni récursion,
boucles détectées) ; une entrée recomposée prend un nouveau tampon, ses descendants suivent. Pendant
`prepareWorldTransforms` et la fin de tick armée, une entrée vérifiée dans la passe est reprise telle quelle (O(1)).
Une racine rend son `Transform` local exact, un enfant une décomposition mise en cache par tampon. La visibilité
héritée des deux modes est calculée dans la même passe. `Transform::operator()` est en forme fermée T·Rz·Ry·Rx·S
(sin / cos sautés pour un angle nul). Renderer2D : le chemin `worldIndex` n'appelle pas `operator()` ; le slot de
texture se cherche par pointeur avant la comparaison du backend.

Reste hors cible : le chemin transitoire paie l'écriture d'une mat4 de 64 o par quad (sous 10 ns il faudrait passer le
TRS au shader), le chemin texturé deux opérations atomiques de `shared_ptr` chez l'appelant (8 ns, `Quad2DData::texture`
possédant ; une référence non possédante changerait l'API publique), et les chaînes restent O(profondeur) par lecture
hors frame (11 ns par niveau) faute de crochet d'écriture sur `Transform`.

### 9.6 Démarrage à froid et compilation Slang

Le coût de Slang 2026.19 (§3.9) est celui de la bibliothèque : avec `libslang-compiler` 2026.1 et le même binaire, les
mêmes shaders donnent `compile_cold` 93,6 ms, `quad_vulkan` 16,5 ms, `voxel_vulkan` 20,1 ms,
`all_engine_shaders_vulkan` (12) 176 ms. Callgrind par compilation de `voxel` : 261 M instructions en 2026.19 contre
162 M en 2026.1, dont l'édition de liens IR (`cloneInst` 161 contre 46 M, `specializeModule` 48 contre 10 M) et la
vérification sémantique (+65 %). Une session Slang partagée entre compilations ne change rien, `-O0` gagne 7 % pour un
code produit différent. 2026.19 écrit `warning[E41012]`, que le filtre de diagnostics reconnaît.

Slang n'est pas sur le chemin de démarrage : `OwlShaderBake` compile au build les 12 shaders du moteur pour Vulkan et
OpenGL (42 étages, 0,7 s) dans `bin/assets/shaders/<renderer>/spirv/<api>/`, installés dans les composants Engine et
Nest et embarqués dans un `.owlpack` (l'`AssetScanner` prend tout `shaders/`). `loadOrCompileSpirv` lit ce SPIR-V, puis
le cache, et ne compile qu'à défaut de clé correspondant à la source (shader modifié, hot reload, shader de jeu). Les
compute shaders, sans cache, sont recompilés à chaque démarrage (création de la session globale comprise). Le GLSL du
repli OpenGL est traduit par spirv-cross au chargement, sans Slang. Le runner du composant Nest lit les 42 fichiers de
son `assets/` (`Using precompiled … from <paquet>/assets`), Vulkan et OpenGL, cache vide ou chaud ; un jeu exporté par
`OwlNest --export` extrait les 42 fichiers de son pack. Valeurs de démarrage : §9.8.

### 9.7 Chargement de scène, prefabs et instantanés

Valeurs : §3.4. Mécanisme (callgrind sur `applyParsed`, `parseBuffer` et `PrefabSerializer::instantiate`) : rapidyaml
0.15.2 (ConanCenter, privé au moteur, statique) lit les scènes, prefabs et instantanés ; yaml-cpp écrit, le format est
inchangé. `core::YamlNode` est une vue en lecture seule qui reprend les règles de yaml-cpp (clé absente fausse, valeur
vide ou `~` nulle et lue `null` en chaîne, booléens `yes` / `On` / `TRUE`, entiers décimaux ou `0x`, `.inf` / `.nan`,
indexer un scalaire lève une erreur) ; `YamlDocument` possède l'arbre. Les clés d'une entité sont lues une fois puis
servies aux descripteurs dans l'ordre du registre, avec un seul `Serializer` réutilisé. Un document à la version
courante, ou de format 1 sans `EntityLink` nommé sans identifiant, est lu tel quel ; les autres passent par
`upgradeDocumentText` (yaml-cpp) puis sont relus. `applyEntityFromString` compare les composants par structure
(`isSameAs`). Le prefab copie ses composants par le registre (la sérialisation ne sert qu'à un composant de jeu non
copiable) et ne lie que les enfants de l'instance. `EnabledRenderers` et `EnabledRenderersConfig::fromYaml` lisent aussi
par rapidyaml.

Scénarios (`OwlRunner --scenario`, `taskset -c 2-7`, temps mur du processus) : 162-178 ms, `world_map_walk` 83 ms ; le
démarrage domine. `parse_only/1000` est bimodal selon que l'arbre retombe sur des pages déjà servies ; à 10 000
entités, l'arbre (~27 Mo) est remappé et refauté à chaque chargement : avec `GLIBC_TUNABLES=glibc.malloc.mmap_threshold=…`
(aucun `munmap`), `scene_from_string/10000` tombe à 3,7 µs et le parsing à 2,3 µs. `entity_from_string/scene10000`
garde le parcours de la scène qui retrouve les enfants de l'entité recréée (`linkIntoHierarchy`).

La cible (< 10 µs par entité à 10 000 entités) est tenue : 7,3 µs, dont 5,9 µs de parsing et 1,4 µs de création
d'entités et de composants. Un format binaire cuit dans les `.owlpack` (non fait) ne gagnerait que le parsing, ~1,5 à
2 µs par entité (×4), au prix d'un second format à versionner et à migrer à côté du YAML que l'éditeur garde ; à
trancher par le mainteneur, sans urgence au vu de la cible.

### 9.8 Shaders recompilés au besoin, démarrage Vulkan sur NVIDIA

**Politique des shaders.** Le SPIR-V stocké (cache du répertoire de travail, puis sortie d'`OwlShaderBake`) passe par
`checkSpirv` avant tout pilote : en-tête, instructions qui remplissent le fichier, `OpFunctionEnd` en dernier,
`OpMemoryModel` et un point d'entrée de l'étage attendu. Un fichier absent, illisible, tronqué, d'une autre clé (source
ou version de Slang) ou d'un autre étage est recompilé depuis la source Slang et mis en cache ; un refus du pilote
(Vulkan `vkCreateShaderModule`, `vkCreateGraphicsPipelines`, `vkCreateComputePipelines` ; OpenGL compilation, édition
de liens ou échec de spirv-cross pour le repli GLSL) déclenche une recompilation unique (`recompileSpirv`), jamais pour
un SPIR-V qui sort du compilateur. Le cache est lu avant la sortie précompilée, pour qu'un précompilé refusé ne le soit
qu'une fois ; une entrée de cache refusée est effacée. Le journal donne le fichier, la raison et la recompilation
(`stored SPIR-V rejected, …/line.vert.spv: truncated: the instruction at word 297 needs 4 words, 2 left` puis
`recompiled, the cache now holds the new SPIR-V`). Vérifié sur quatre configurations (`platformer_house`, cache vide
puis chaud, repli GLSL `OWL_OPENGL_SHADERS=glsl` sur les deux pilotes OpenGL) avec `quad` aux étages permutés,
`circle.frag` aléatoire, `line.vert` tronqué, `text` absent et `raycast_dda.comp` remplacé par un fragment : tout est
recompilé, rendu et relu du cache au lancement suivant. Sans le contrôle d'étage, NVIDIA et Mesa refusent proprement
une permutation, mais ANV meurt en SIGFPE dans le `vkCreateComputePipelines` suivant le refus d'un module invalide
(comportement indéfini, d'où le contrôle en amont). Tests : `ShaderFileUtils.{Corrupt,Truncated,Absent}CachedSpirvIsRecompiled`,
`CachedSpirvOf{AnotherKey,AnotherStage}IsRecompiled`, `CorruptPrecompiledSpirvIsRecompiled`,
`RecompileSpirvReplacesRefusedStoredOutput`, `CheckSpirvRejectsMalformedModules`. Validation Vulkan (`--validation`,
NVIDIA et Intel, `platformer_house`, `raycast_demo`, `voxel_terrain`, cache vide et chaud) sans message.

**Démarrage Vulkan NVIDIA.** Chronos ciblés, `platformer_house`, cache chaud, médiane de 3, ms :

| Étape                                              | NVIDIA | Intel | Écart |
|----------------------------------------------------|--------|-------|-------|
| Fenêtre GLFW (chargement de l'ICD compris)         | 167    | 132   | +35   |
| `vkCreateInstance`                                 | 8,1    | 1,7   | +6    |
| Choix du périphérique physique                     | 7,8    | 10,3  | −3    |
| `vkCreateDevice`                                   | 89     | 9,1   | +80   |
| Swapchain                                          | 43     | 8,7   | +34   |
| Shaders et pipelines (`initShaders`)               | 30     | 26    | +4    |
| dont `vkCreate{Graphics,Compute}Pipelines` (7 + 1) | 1,0    | 0,8   | 0     |
| Police, scène, prêt → première frame               | 78     | 88    | −10   |
| Première frame                                     | 438    | 279   | +159  |

Les ~160 ms d'écart sont dans le pilote : création du périphérique, swapchain en PRIME offload, chargement de l'ICD.
Les pipelines coûtent 1 ms parce que les pilotes ont leur propre cache disque (`~/.cache/nvidia`, cache Mesa). Un
`VkPipelineCache` applicatif ne change rien avec les caches pilote (0,9 → 0,8 ms) et coûte 10 ms au premier lancement
sur NVIDIA (un cache applicatif vide court-circuite celui du pilote) ; sans eux (`__GL_SHADER_DISK_CACHE=0`,
`MESA_SHADER_CACHE_DISABLE=true`), les pipelines passent de 96 à 0,9 ms sur NVIDIA et de 55 à 0,2 ms sur Intel, cas
qu'un poste réel ne rencontre qu'après une purge du cache pilote : rien n'est ajouté. Le premier lancement après une
veille du GPU NVIDIA (D3cold) ajoute ~2 s à la fenêtre ; la médiane écarte ces valeurs.

`startup_ms` (`engine_ready` / `first_frame`, ms), `taskset -c 2-7`, 300 frames, 3 répétitions (5 pour OpenGL), médiane ;
« froid » : `bin/cache/shader` vidé :

| Configuration   | Froid     | Chaud     |
|-----------------|-----------|-----------|
| Vulkan / NVIDIA | 394 / 414 | 399 / 420 |
| OpenGL / NVIDIA | 324 / 353 | 314 / 342 |
| Vulkan / Intel  | 252 / 279 | 252 / 282 |
| OpenGL / Intel  | 278 / 302 | 279 / 305 |

La recompilation au besoin ne coûte rien au démarrage nominal ; l'écart NVIDIA restant relève du pilote, pas des
pipelines.

### 9.9 Décisions du mainteneur (2026-10-10)

| Cible                            | Décision                                                                                                             |
|----------------------------------|----------------------------------------------------------------------------------------------------------------------|
| Frame runner Vulkan ≤ OpenGL     | Acceptée en l'état : plancher de présentation PRIME sous Wayland, travail hors attente 0,03-0,11 ms au-dessus (§9.3) |
| SPIR-V stocké                    | Validé : le cache est lu avant le SPIR-V précompilé, un précompilé refusé ne l'est qu'une fois (§9.8)                |
| Coût par quad Renderer2D < 10 ns | Gardé : les quads hors `worldIndex` restent à 14-24 ns, sans TRS passé au shader ni API non possédante (§9.5)        |

Les autres cibles sont tenues.

### 9.10 Vidages GPU dans l'éditeur (2026-10-10)

Mesure manuelle du mainteneur dans le panneau *Stats* (Vulkan, NVIDIA, Wayland, `engine_demo`), valeurs « frame /
max 2 s » : au repos, en jeu (voxel, ~1 200 fps), au survol, au gizmo et dans le content browser, 1 soumission,
`queueWaitIdle` = `deviceWaitIdle` = 0 et aucune attente de fence ; un clic de sélection fait 1 attente de fence
(lecture du picking), un chargement de scène jusqu'à 7, puis retour à 0. `paceWaitMs` en jeu : 0,2 ms par frame,
3,17 ms au pire sur 2 s. Cible « 0 vidage par frame » tenue dans l'éditeur comme dans le runner.
