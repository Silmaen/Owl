# Mesures de performance du moteur

> Revu le 2026-10-20 sur `main` 4e1a6a10. Instantané : campagne du 2026-10-05 (§1 à §8), chantier du 2026-10-09 (§9) ;
> le §9 fait foi quand il redonne une mesure.
> Harnais : `test/bench/` (`owl_bench`, option CMake `OWL_BENCHMARK`, `test/bench/README.md`), baseline CI dans
> `test/bench/baseline/linux-bench.json`. Données brutes (`results/`, `callgrind/`) : non versionnées.

## 1. Résumé

1. Le harnais est maison (médiane, quartiles, CSV / JSON) ; deux campagnes complètes concordent (écart médian 0,5 %).
2. ECS : itération d'une vue à 2 composants 2,6 ns/entité, `findEntityByUUID` 9 à 23 ns, `Scene::copy` (passage en
   Play) 3 ms pour 10 000 entités.
3. Frame 2D : 0,33 ms pour 10 000 sprites (backend Null), 0,46 ms en forêt de hiérarchies ; un cache dense des
   matrices monde remplace les tables de hachage par frame (§9.5).
4. Renderer2D : un draw call pour 20 000 quads, 9,4 ns par quad sur le chemin `worldIndex`, 14 à 24 ns sur le chemin
   transitoire (§9.5).
5. Le point le plus lent est l'écriture YAML : 23 µs par entité (yaml-cpp), contre 7,3 µs au chargement (rapidyaml,
   §9.7) ; un snapshot d'undo coûte 26 µs à l'écriture et 3 à 14 µs à la relecture.
6. Voxel : 350 µs de maillage par chunk 16³ de terrain (1,7 ms au pire), sur les workers avec un upload budgété (pic de
   frame en streaming 0,35 ms, §3.10) ; aucun raccourci pour les chunks uniformes.
7. Lua : 44 ns par `on_update` vide, 140 ns avec `get_position` + `set_position`, 28 µs et ~9 Ko par `ScriptInstance`
   (un `lua_State` par entité).
8. Box2D 3.1.1 sur un pool de tâches (`SolverTaskPool`) : 5 000 corps au contact coûtent 1,68 ms par pas à 8 workers
   (SSE2), 1,44 ms en AVX2 optionnel (`OWL_PHYSICS_AVX2`, §9.4).
9. Slang 2026.19 : 165 ms à froid, 21 à 35 ms par shader en Release ; shaders du moteur précompilés au build
   (`OwlShaderBake`, §9.6).
10. Frame bench GPU (§8) : le runner Vulkan ne vide pas la file (0 vidage, 1 soumission par frame) ; sur NVIDIA, frame
    Vulkan 0,48 à 0,79 ms contre 0,33 à 0,59 ms en OpenGL (§9.3).

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
| Charge           | machine partagée avec d'autres agents : load average 10,7 à 11,2                                                 |

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

| Cas                                          | Médiane | Par item | IQR   | Remarque                                    |
|----------------------------------------------|---------|----------|-------|---------------------------------------------|
| `scene/create_entities/1000`                 | 109 µs  | 109 ns   | 1,6 % | 5 composants + UUID + index UUID            |
| `scene/create_entities/100000`               | 13,8 ms | 138 ns   | 1,0 % |                                             |
| `scene/memory/bytes_per_sprite_entity`       | 278 o   |          |       | delta `mallinfo2`, scène de 100 000 sprites |
| `scene/view_iterate/transform_sprite/10000`  | 25,6 µs | 2,6 ns   | 0,4 % | vue EnTT à 2 composants                     |
| `scene/view_iterate/transform_sprite/100000` | 277 µs  | 2,8 ns   | 0,3 % |                                             |
| `scene/find_entity_by_uuid/1000`             | 9,4 µs  | 9,4 ns   | 0,7 % | `unordered_map` UUID → entité               |
| `scene/find_entity_by_uuid/100000`           | 2,28 ms | 22,8 ns  | 0,6 % | sorties de cache                            |
| `scene/copy/1000`                            | 248 µs  | 248 ns   | 4,4 % | `Scene::copy` (Play), forêt                 |
| `scene/copy/10000`                           | 2,97 ms | 297 ns   | 3,0 % |                                             |
| `scene/prepare_world_transforms/flat10000`   | 139 µs  | 13,9 ns  |       | passe par frame (§9.5)                      |
| `scene/prepare_world_transforms/forest10000` | 272 µs  | 27,2 ns  |       |                                             |
| `scene/world_transform/forest10000_all`      | 352 µs  | 35,2 ns  |       | `getWorldTransform` hors frame              |
| `scene/world_transform/chain1000_all`        | 5,57 ms | 5,6 µs   |       | O(profondeur) par lecture                   |
| `scene/set_parent/chain1000_build`           | 5,71 ms | 5,7 µs   |       |                                             |

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

| Cas                                          | Médiane | Par entité | Remarque                             |
|----------------------------------------------|---------|------------|--------------------------------------|
| `serialize/scene_to_string/10000`            | 229 ms  | 22,9 µs    | écriture yaml-cpp                    |
| `serialize/scene_from_string/10000`          | 73 ms   | 7,3 µs     | lecture rapidyaml                    |
| `serialize/scene_from_string/1000`           | 2,8 ms  | 2,8 µs     |                                      |
| `serialize/scene_parse_only/10000`           | 59 ms   | 5,9 µs     | `parseBuffer` seul                   |
| `serialize/entity_to_string/scene1`          | 25,8 µs |            | snapshot d'undo, 218 o               |
| `serialize/entity_to_string/scene10000`      | 26,3 µs |            | indépendant de la taille de la scène |
| `serialize/entity_from_string/scene1`        | 2,9 µs  |            | restauration d'une entité            |
| `serialize/entity_from_string/scene10000`    | 14,3 µs |            |                                      |
| `prefab/instantiate/10_entities/scene0`      | 49 µs   | 4,9 µs     |                                      |
| `prefab/instantiate/10_entities/scene10000`  | 58 µs   | 5,8 µs     |                                      |
| `prefab/instantiate/100_entities/scene10000` | 388 µs  | 3,9 µs     | indépendant de la scène cible        |
| `sample/load/voxel_terrain`                  | 26 µs   |            | scène de la démo                     |
| `sample/load/world_map`                      | 75 µs   |            |                                      |
| `sample/load/main_menu`                      | 90 µs   |            |                                      |
| `sample/load/platformer_house`               | 198 µs  |            |                                      |
| `sample/load/raycast_demo`                   | 408 µs  |            | plus grosse scène de la démo         |

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

### 3.6 Lua

| Cas                                                | Médiane | Par instance |
|----------------------------------------------------|---------|--------------|
| `script/create_instance/empty_script`              | 28,1 µs |              |
| `script/memory/bytes_per_instance`                 | 9 020 o |              |
| `script/on_update/empty/1_instance`                | 43,8 ns | 43,8 ns      |
| `script/on_update/empty/1000_instances`            | 80,1 µs | 80,1 ns      |
| `script/on_update/get_set_position/1_instance`     | 140 ns  | 140 ns       |
| `script/on_update/get_set_position/1000_instances` | 258 µs  | 258 ns       |
| `script/on_update/arith_100/1000_instances`        | 615 µs  | 615 ns       |

À 1 000 instances, le coût par appel double presque : chaque instance a son propre `lua_State`
(`ScriptInstance.cpp:41`), donc ses données sont dispersées en mémoire. La mémoire est un delta `mallinfo2` avec un
script vide ; la sonde M1 de `10-constats-D-sous-systemes.md` annonçait 24-25 Kio avec une autre méthode : à
réconcilier. Mécanisme : §9.2.

### 3.7 Physique (Box2D 3.1.1)

Boîtes dynamiques 1 × 1 en grille de 200 colonnes, sol statique ; `PhysicCommand::frame` à pas fixe de 16,7 ms.
`step_falling` : les 60 premières frames après `init` ; `step_settled` : 60 frames après 600 frames de stabilisation.
Mono-thread (`taskset -c 6`) :

| Cas                    | 100 corps | 1 000 corps | 5 000 corps |
|------------------------|-----------|-------------|-------------|
| `physics/init` (total) | 54 µs     | 586 µs      | 3,33 ms     |
| `physics/step_falling` | 22,9 µs   | 228 µs      | 1,12 ms     |
| `physics/step_settled` | 2,9 µs    | 28 µs       | 4,88 ms     |

À 5 000 corps, la pile de 25 rangées ne s'endort pas et reste en contact : 4,9 ms par pas. Profil de
`step_falling/1000` : `b2World_Step` 87,6 %, boucle de synchronisation vers les `Transform` 12,4 %.

#### 3.7.1 Pas fixe et solveur multi-thread

Machine chargée (load average 5 à 12) : chiffres relatifs. `physics/frame_empty` (0 corps) : 284 ns à 60 Hz, 132 ns à
144 Hz. Une frame sans pas (144 Hz, 60 % des frames) ne coûte que la boucle d'interpolation ; une frame vide à 60 Hz
coûte le `b2World_Step` d'un monde vide (~280 ns). Synchronisation et interpolation de 5 000 `Transform` : 0,13 ms par
frame ; événements de contact : 5 à 12 µs par pas.

Multi-thread (`workerCount` réglé, SSE2), `taskset -c 6,8,10,12,14` (cinq cœurs physiques) :

| Cas                        | 1 thread | 2 workers | 4 workers | 8 workers |
|----------------------------|----------|-----------|-----------|-----------|
| `step_falling/100_bodies`  | 23,5 µs  | 47,3 µs   | 46,5 µs   | 49,3 µs   |
| `step_settled/100_bodies`  | 2,1 µs   | 4,7 µs    | 3,6 µs    | 5,0 µs    |
| `step_falling/1000_bodies` | 227 µs   | 212 µs    | 208 µs    | 193 µs    |
| `step_settled/1000_bodies` | 19,2 µs  | 20,3 µs   | 20,6 µs   | 21,9 µs   |
| `step_falling/5000_bodies` | 1,11 ms  | 890 µs    | 726 µs    | 738 µs    |
| `step_settled/5000_bodies` | 5,14 ms  | 3,85 ms   | 3,18 ms   | 2,86 ms   |

Le gain n'apparaît qu'au-delà de quelques milliers de corps ; en dessous, la distribution des tâches coûte plus qu'elle
ne rapporte, d'où le seuil automatique de 2 000 corps dynamiques. AVX2 et décomposition du pas : §9.4.

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
Preuve         : Scene.cpp:1887-1910 (`isEffectivelyVisible`) ; frame/editor_update/chain1000 = 38,7 µs (§9.5).
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
Preuve         : serialize/scene_to_string/10000 = 22,9 µs par entité (yaml-cpp), scene_from_string/10000 = 7,3 µs
                 (rapidyaml) ; entity_to_string 26 µs, entity_from_string 2,9-14 µs ; PrefabSerializer.cpp:321-347
                 (fusion YAML d'une entité de prefab avec son instance) ; §9.7.
Constat        : La lecture est rapide depuis rapidyaml, mais l'écriture reste en yaml-cpp : le snapshot d'undo, la
                 sauvegarde et la fusion de prefab paient 23 à 26 µs par entité. Le format de scène reste du texte ;
                 aucun format binaire cuit n'existe pour le runtime et le .owlpack.
Comparaison    : Godot sérialise en texte (.tscn) mais cuit en binaire (.scn) à l'export ; Bevy instancie depuis un
                 monde en mémoire.
Recommandation : surveiller, effort M à L : format binaire cuit pour le runtime et le .owlpack (v0.6.0), YAML gardé
                 comme format d'édition.
Statut         : confirmé (mesuré)
```

```text
ID             : P-08
Axe            : C
Nature         : force
Gravité        : basse
Preuve         : scene/view_iterate = 2,6-2,8 ns par entité ; find_entity_by_uuid 9-23 ns ; scene/copy/10000 = 2,97 ms ;
                 create_entities 109-138 ns.
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
                 callgrind : Chunk::getBlock (Chunk.cpp:32, non inline) 18,9 %, BlockRegistry::isOpaque 13,9 % ;
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
Preuve         : ChunkMesher.h:28-39 (VoxelVertex = 40 o) ; voxel/mesh_by_kind/terrain_surface/mesh_bytes = 55 Ko pour
                 16 Ko de données ; RendererVoxel.cpp:73-87 (recopie en Mesh3DVertex à l'upload).
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
Nature         : force
Gravité        : basse
Preuve         : script/on_update/empty = 43,8 ns (1 instance), 80 ns (1 000) ; get_set_position 140-258 ns ;
                 create_instance 28,1 µs ; 9 020 o par instance ; ScriptInstance.cpp:41 (un lua_State par entité).
Constat        : Le coût d'appel Lua est faible : 1 000 entités scriptées coûtent 0,1 à 0,3 ms par frame. Le modèle
                 « un lua_State par entité » isole bien, mais coûte ~9 Ko et 28 µs par instance, et disperse la mémoire
                 (coût par appel presque doublé à 1 000 instances).
Comparaison    : Opinion : Defold et la plupart des moteurs Lua partagent une VM et isolent par environnement ; à
                 10 000 entités scriptées, le modèle actuel coûte ~90 Mo et 0,3 s de création.
Recommandation : garder, surveiller au-delà de quelques milliers d'entités scriptées.
Statut         : confirmé (mesuré)
```

## 5. Le harnais

- `test/bench/harness/Bench.{h,cpp}` : `Runner::measure` (lot calibré), `measureWithSetup` (préparation hors chrono),
  `measureOnce` (chemins froids), `metric` (tailles, compteurs, contrôles de correction), sorties CSV et JSON avec la
  charge de la machine au début et à la fin.
- `test/bench/cases/*.cpp` : un fichier par groupe (`SceneBench`, `SerializeBench`, `Renderer2DBench`, `FrameBench`,
  `VoxelBench`, `VoxelStreamingBench`, `ScriptBench`, `PhysicsBench`, `SampleBench`, `SlangBench`, `HudBench`,
  `IsometricBench`, `TilemapBench`, `PathBench`, `McpBench`).
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
3. **Mémoire** : pic et allocations par frame (heaptrack absent de l'image) ; `OWL_ENABLE_MEMORY_TRACKER` (M10).
4. **Lua** : `find_entity` depuis un script (M1).
5. **Physique** : 10 000 corps, corps dans une hiérarchie, tilemap collidable (M3).
6. **Chargement** : ouverture et extraction d'un `.owlpack` (M8), premier et second lancement réels.
7. **Profileur et logs** (M11, M12) : coût d'un `OWL_PROFILE_SCOPE` actif et d'une trace désactivée en boucle chaude.
8. **Comparaison externe** : chiffres publiés d'EnTT, Box2D v3, yaml-cpp / rapidyaml, mailleurs voxel, sourcés et
   datés, dans `30-etat-de-l-art.md`.
9. **Chaîne de build** (second rang) : build à froid, build incrémental après modification de `Scene.h`.

## 8. Frame bench GPU

### 8.1 Protocole

| Élément      | Valeur                                                                                                            |
|--------------|-------------------------------------------------------------------------------------------------------------------|
| Outil        | `OwlRunner --frame-bench <scène> --backend <b> --frames 1000 --warmup 120 --out <json>` (`test/bench/README.md`)  |
| Build        | `linux-clang-release` dans `./docker_run.sh --gui`, clang 22, `-O3`                                               |
| Machine      | i9-13950HX, portable hybride : Intel UHD (RPL-S) principal, RTX 5000 Ada Laptop en PRIME offload                  |
| Affichage    | session Wayland (`WAYLAND_DISPLAY=wayland-0`), fenêtre GLFW via XWayland, 1280 × 720                              |
| Épinglage    | `taskset -c 2-7` (P-cores), processus entier                                                                      |
| Déterminisme | pas fixe 16,667 ms, entrées null, caméra primaire de la scène, son null                                           |
| Présentation | Vulkan `immediate` (vsync coupée), OpenGL `swap-interval-0`                                                       |
| Répétitions  | 3 à 5 par configuration, 1 000 frames mesurées chacune ; médiane des médianes                                     |

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

| Cible                                  | Valeur courante                                                  | Statut             |
|----------------------------------------|------------------------------------------------------------------|--------------------|
| Frame à 10 000 sprites                 | 0,33 ms, 0,46 ms en forêt (§9.5)                                 | tenue              |
| Coût par quad Renderer2D < 10 ns       | 9,4 ns sur `worldIndex`, 14 à 24 ns hors `worldIndex` (§9.5)     | gardée en l'état   |
| 0 vidage de file par frame             | 0 dans le runner (§8.2) et dans l'éditeur (§9.10)                | tenue              |
| Frame runner Vulkan ≤ OpenGL           | 0,03 à 0,11 ms de travail CPU au-dessus d'OpenGL (§9.3)          | acceptée en l'état |
| Chargement de scène < 10 µs par entité | 7,3 µs à 10 000 entités (§9.7)                                   | tenue              |
| Pas Box2D < 1,5 ms, 5 000 corps        | 1,68 ms à 8 workers en SSE2, 1,44 ms en AVX2 optionnel (§9.4)    | écart accepté      |
| Lua, coût par instance                 | 28 µs, 9 020 o, 44 ns par appel vide (§9.2)                      | tenue              |
| Démarrage à froid jusqu'à la 1re frame | 270 à 440 ms, indépendant du cache de shaders (§9.6, §9.8)       | tenue              |
| SPIR-V stocké                          | validé avant le pilote, recompilé au besoin (§9.8)               | validée            |

### 9.2 Lua

Coût par instance et par appel : §3.6. Ce qui le fixe, mesuré par callgrind (`--toggle-collect` sur
`ScriptInstance::onUpdate`, puis sur la création) : Lua 5.5.0 ; chaque binding est une fonction C légère
`guarded<fn>`, sans allocation (une closure par binding coûterait 70 allocations de 64 o, soit 4,5 Kio par état) ;
`coroutine.wrap` est en C sur le `coroutine.resume` du bac à sable (un chunk Lua compilé par état coûterait 16 % des
instructions de la création) ; la scène liée et le delta vivent dans le `Quota` de l'état (plus de
`lua_getfield(REGISTRY, "owl_scene")` par binding) ; les noms de callbacks (16 au plus) sont internés une fois, en
référence de registre ; le gestionnaire d'erreur est poussé avant la fonction et le contexte d'erreur n'est formaté
qu'en cas d'erreur. Quotas, chien de garde, traceback et bac à sable restent actifs. Le `Quota` élargi ajoute 55 o par
instance.

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

### 9.4 Box2D, 5 000 corps au contact

Box2D 3.2 n'existe pas (ni sur ConanCenter ni en amont, dernière étiquette `v3.1.1`) ; la recette ConanCenter 3.1.1 n'a
pas d'option AVX2, d'où une recette locale (`conan/recipes/box2d`, la même plus `avx2`). Décomposition du pas
(`b2World_GetProfile` et chronos dans `PhysicCommand::frame`, 60 pas de `step_settled_mt8`) : 1,36 ms de `b2World_Step`
(paires 0,29, narrow phase 0,33, solveur 0,74 dont contraintes 0,48) et 0,10 ms de copie des poses vers l'ECS, en
série. La pile « tassée » ne l'est pas : après 600 pas, les colonnes de 25 caisses se sont effondrées (vitesse max
25 m/s, 5 000 corps éveillés), d'où des paires nouvelles et une île coupée à chaque pas.

Réglages : Box2D en AVX2 sur x86_64 (`OWL_PHYSICS_AVX2`, option `box2d/*:avx2`, sans objet sur arm64), avec contrôle du
CPU à `PhysicCommand::init` (sans AVX2 : erreur au journal et monde non créé) ; deux threads de plus que `workerCount`
dans l'exécuteur, car la reconstruction d'arbre et la coupe d'île tournent à côté des tâches du solveur, qui s'attendent ;
copie des poses (`captureCurrent`, `writeTransforms`) répartie sur l'exécuteur au-delà de 1 024 corps, les enfants
d'une hiérarchie restant en série. `PhysicMultiThread` et `PhysicFixedStep` restent déterministes (3 passes identiques).

Valeurs, 5 000 corps, `taskset -c 0-15`, load average 5,6 à 5,8 : `step_settled` 4,46 ms mono-thread, 3,28 / 2,00 /
1,44 ms à 2 / 4 / 8 workers en AVX2 ; `step_falling_mt8` 450 µs ; en SSE2, `step_settled_mt8` 1,68 ms. Le nombre
automatique de workers reste plafonné à 4 (2,00 ms) ; le relever est à trancher par le mainteneur.

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

| Configuration   | Froid       | Chaud       |
|-----------------|-------------|-------------|
| Vulkan / NVIDIA | 394 / 414   | 399 / 420   |
| OpenGL / NVIDIA | 324 / 353   | 314 / 342   |
| Vulkan / Intel  | 252 / 279   | 252 / 282   |
| OpenGL / Intel  | 278 / 302   | 279 / 305   |

La recompilation au besoin ne coûte rien au démarrage nominal ; l'écart NVIDIA restant relève du pilote, pas des
pipelines.

### 9.9 Décisions du mainteneur (2026-10-10)

| Cible                            | Décision                                                                                                             |
|----------------------------------|----------------------------------------------------------------------------------------------------------------------|
| Frame runner Vulkan ≤ OpenGL     | Acceptée en l'état : plancher de présentation PRIME sous Wayland, travail hors attente 0,03-0,11 ms au-dessus (§9.3) |
| Pas Box2D < 1,5 ms, 5 000 corps  | Écart accepté : 1,68 ms à 8 workers en SSE2 par défaut, 1,44 ms en AVX2 optionnel (§9.4)                             |
| SPIR-V stocké                    | Validé : le cache est lu avant le SPIR-V précompilé, un précompilé refusé ne l'est qu'une fois (§9.8)                |
| Coût par quad Renderer2D < 10 ns | Gardé : les quads hors `worldIndex` restent à 14-24 ns, sans TRS passé au shader ni API non possédante (§9.5)        |

Les autres cibles sont tenues. Le tableau des cibles de `doc/pages/design/foundations.md` porte ces statuts.

### 9.10 Vidages GPU dans l'éditeur (2026-10-10)

Mesure manuelle du mainteneur dans le panneau *Stats* (Vulkan, NVIDIA, Wayland, `engine_demo`), valeurs « frame /
max 2 s » : au repos, en jeu (voxel, ~1 200 fps), au survol, au gizmo et dans le content browser, 1 soumission,
`queueWaitIdle` = `deviceWaitIdle` = 0 et aucune attente de fence ; un clic de sélection fait 1 attente de fence
(lecture du picking), un chargement de scène jusqu'à 7, puis retour à 0. `paceWaitMs` en jeu : 0,2 ms par frame,
3,17 ms au pire sur 2 s. Cible « 0 vidage par frame » tenue dans l'éditeur comme dans le runner.
