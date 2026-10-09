# Mesures de performance du moteur

> Statut : première campagne, 2026-10-05, branche `Feature/Bench` (commit `45e27892` + harnais non commité).
> Harnais : `bench/` (`owl_bench`, option CMake `OWL_BENCHMARK`, voir `bench/README.md`).
> Données brutes : `output/build/linux-clang-release-bench/results/run{1,2}.{txt,csv,json}`, profils callgrind dans
> `output/build/linux-clang-release-bench/callgrind/` (dossier de build, non versionné).
> Vérification croisée (2026-10-05) : 9 constats relus, aucun réfuté ; P-02 et P-07 abaissés de haute à moyenne.

## 1. Résumé

1. Google Benchmark n'existe ni dans le cache DepManager local ni sur le remote `package.argawaen.net` : `bench/` utilise un
   petit harnais maison (médiane, quartiles, CSV / JSON). Les deux campagnes successives concordent (écart médian 0,5 %).
2. Le cœur ECS est sain : itération d'une vue à 2 composants 2,6 ns/entité, `findEntityByUUID` 9 à 23 ns, `Scene::copy`
   (passage en Play) 3 ms pour 10 000 entités.
3. Le coût CPU d'une frame est élevé pour du 2D : environ 250 ns par sprite (2,5 ms pour 10 000 sprites, backend Null).
   `prepareWorldTransforms` en prend 72 % : trois `unordered_map` remplies à chaque frame, avec une allocation par entrée.
4. Bug de correction mesuré : le monde est faux au-delà de 64 niveaux de hiérarchie (feuille d'une chaîne de 1 000 : x = 65
   au lieu de 1 000), et `setParent` y corrompt la position (8 064 au lieu de 999).
5. Le batching Renderer2D est bon : un draw call pour 20 000 quads, 10,5 ns par quad sur le chemin `worldIndex`.
   Le chemin transitoire coûte 86 ns par quad, dont 76 ns dans `Transform::operator()`.
6. La sérialisation YAML est le point le plus lent : 117 à 134 µs par entité au chargement (1,34 s pour 10 000 entités),
   256 µs par entité pour instancier un prefab, 26 µs + 106 µs pour un aller-retour d'entité (undo).
7. Le maillage voxel prend 350 µs par chunk 16³ de terrain (1,7 ms dans le pire cas), sur le thread principal, sans budget
   par frame. La génération (13 à 108 µs) est déjà asynchrone. Après PR-24 (§3.10), il passe sur les workers : pic de
   frame en streaming de 14,3 ms à 0,35 ms, plus aucune frame au-dessus de 5 ms.
8. Lua est raisonnable : 43 ns par `on_update` vide, 133 ns avec `get_position` + `set_position`, 30 µs et ~9 Ko par
   `ScriptInstance` (un `lua_State` par entité).
9. Box2D 3.1 tourne sur un seul thread : 4,9 ms par pas pour 5 000 boîtes empilées. Le solveur parallèle de Box2D v3
   n'est pas branché, alors que Taskflow est déjà là.
10. La consigne « première compilation Slang ≈ 50 s » est fausse d'un facteur ~700 : 74 ms à froid, 17 à 22 ms par shader
    ensuite, 219 ms pour les 13 shaders du moteur.
11. Frame bench GPU (§8, `OwlRunner --frame-bench`) : sur la RTX 5000 Ada, Vulkan coûte 1,2 à 4,6 fois le temps CPU
    d'OpenGL par frame (0,6 à 2,0 ms contre 0,35 à 0,76 ms) pour un GPU occupé 0,01 à 0,09 ms ; le runner vide la file
    2 à 4 fois par frame (`vkQueueWaitIdle`), plus 2 `vkDeviceWaitIdle` sur la scène raycast. lavapipe et llvmpipe ne
    passent pas encore.

## 2. Protocole

| Élément          | Valeur                                                                                                         |
|------------------|----------------------------------------------------------------------------------------------------------------|
| Machine          | Intel Core i9-13950HX, 32 threads (CPU 0-15 : 8 P-cores HT, 16-31 : E-cores), Linux 7.0                        |
| Conteneur        | `docker/run.sh` (image `devel-ubuntu2404`), clang 22.1.8, preset `linux-clang-release` (`-O3`, libs partagées) |
| Dossier          | `output/build/linux-clang-release-bench`, `-DOWL_BENCHMARK=ON`                                                 |
| Épinglage        | `taskset -c 6` (un thread de P-core) ; le processus entier est épinglé                                         |
| Backend          | Application factice (`isDummy`), rendu Null, son Null, log coupé (`Level::Off`)                                |
| Échantillons     | 2 de chauffe puis 15 mesurés ; lot calibré pour qu'un échantillon dure au moins 10 ms                          |
| Statistiques     | médiane par appel, IQR (p75 - p25) en % de la médiane, minimum ; « par item » = médiane / nombre d'items       |
| Reproductibilité | deux campagnes complètes (run1, run2) : écart médian 0,5 %, p90 2,5 %, maximum 10 % (`world_transform/*`)      |
| Charge           | machine partagée avec d'autres agents (builds, sanitizers, clang-tidy) : load average 10,7 à 11,2              |

Points de vigilance :

- **P-core contre E-core.** Une première passe non épinglée, à load average ~45, donnait des temps 2 à 3 fois plus longs
  (par exemple `scene/create_entities/10000` : 295 ns contre 110 ns par entité). Toute comparaison doit épingler sur un
  P-core ; les chiffres ci-dessous sont ceux de run1, épinglé.
- **Backend Null.** `null::StorageBuffer::setData` fait un vrai `memcpy`, donc le coût de remplissage des SSBO est compté.
  Les `VertexBuffer::setData` et `Texture2D::setData` Null sont vides : aucun coût de pilote, aucun coût GPU n'est mesuré.
- **Isolation.** Les autres agents tournaient sur les autres cœurs : bande passante mémoire et cache L3 partagés. Les IQR
  restent sous 2 % pour presque tous les cas.
- **Profils.** `perf` n'est pas exécutable dans l'image : profils d'instructions avec
  `valgrind --tool=callgrind --toggle-collect=<fonction>` sur un cas filtré (une itération). Les pourcentages sont des
  instructions (Ir), pas du temps.

Commandes :

```bash
docker/run.sh cmake --preset linux-clang-release -S . -B output/build/linux-clang-release-bench -DOWL_BENCHMARK=ON
docker/run.sh cmake --build output/build/linux-clang-release-bench --target owl_bench
cd output/build/linux-clang-release-bench/bin
../../../../docker/run.sh taskset -c 6 ./owl_bench --csv=../results/run1.csv --json=../results/run1.json
```

Scènes synthétiques (`bench/cases/SceneBench.cpp`, `makeSpriteScene`) : entités `Transform` + `SpriteRenderer` (plus les
composants obligatoires), chacune translatée de +1 en x dans l'espace de son parent. Formes : `flat` (toutes racines),
`chain` (chaque entité fille de la précédente), `wide` (une racine, N - 1 enfants directs), `forest` (racines de 100 enfants).

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
| `scene/world_transform/flat10000_all`        | 88 µs   | 8,8 ns   | 4,4 % | `getWorldTransform` hors frame, racines     |
| `scene/world_transform/wide10000_all`        | 2,28 ms | 228 ns   | 0,2 % | un parent par entité                        |
| `scene/world_transform/forest10000_all`      | 2,27 ms | 227 ns   | 0,2 % |                                             |
| `scene/world_transform/chain1000_leaf`       | 6,3 µs  |          | 0,1 % | marche plafonnée à 64 parents               |
| `scene/world_transform/chain1000_all`        | 6,43 ms | 6,4 µs   | 0,1 % | O(n × min(profondeur, 64))                  |
| `scene/prepare_world_transforms/flat10000`   | 1,68 ms | 168 ns   | 0,2 % | passe par frame (CPU + upload SSBO)         |
| `scene/prepare_world_transforms/forest10000` | 1,99 ms | 199 ns   | 0,4 % |                                             |
| `scene/prepare_world_transforms/chain1000`   | 194 µs  | 194 ns   | 0,4 % | correcte à toute profondeur côté CPU        |
| `scene/set_parent/wide10000_build`           | 2,83 ms | 283 ns   | 0,7 % | 9 999 `setParent`                           |
| `scene/set_parent/chain1000_build`           | 7,90 ms | 7,9 µs   | 0,3 % | 3 marches de 64 parents par appel           |

Contrôles de correction (métriques) :

| Métrique                                            | Valeur | Attendu |
|-----------------------------------------------------|--------|---------|
| x monde de la feuille de `chain1000`, moteur        | 65     | 1 000   |
| x monde de la feuille de `chain1000`, marche totale | 1 000  | 1 000   |
| x monde de la feuille après 999 `setParent` chaînés | 8 064  | 999     |

### 3.2 Frame complète (backend Null)

`frame/editor_update` appelle `Scene::onUpdateEditor` (préparation des transforms, rendu 2D, HUD) ; `frame/runtime_update`
appelle `Scene::onUpdateRuntime` après `onStartRuntime`, avec (`render_`) ou sans (`logic_`) rendu.

| Cas                                     | Médiane | Par sprite | IQR   |
|-----------------------------------------|---------|------------|-------|
| `frame/editor_update/flat1000`          | 246 µs  | 246 ns     | 0,5 % |
| `frame/editor_update/flat10000`         | 2,52 ms | 252 ns     | 0,7 % |
| `frame/editor_update/forest10000`       | 3,05 ms | 305 ns     | 0,4 % |
| `frame/editor_update/chain1000`         | 1,64 ms | 1,64 µs    | 0,4 % |
| `frame/runtime_update/logic_flat10000`  | 2,00 ms | 200 ns     | 0,4 % |
| `frame/runtime_update/render_flat10000` | 2,52 ms | 252 ns     | 0,3 % |

Profil de `editor_update/flat10000` (callgrind, % des instructions de `onUpdateEditor`) :

| Fonction                                                  | Inclusif |
|-----------------------------------------------------------|----------|
| `Scene::prepareWorldTransforms`                           | 71,7 %   |
| `math::Transform::operator()` (TRS → mat4)                | 26,3 %   |
| `Scene::renderWithStack` / `Scene::render`                | 24,0 %   |
| `operator new` + `malloc` + `free` + `operator delete`    | ~30 %    |
| `Scene::isEffectivelyVisible` (cache `unordered_map`)     | 13,8 %   |
| `math::Transform::Transform(mat4)` (décomposition, atan2) | 12,9 %   |
| `WorldTransformPass::compute` (copie et upload)           | 5,7 %    |

Pour `editor_update/chain1000`, `isEffectivelyVisible` monte à 84,5 % (dont 41,7 % dans `findEntityByUUID`) : chaque
entité remonte jusqu'à 64 parents.

### 3.3 Renderer2D (backend Null)

Une frame = `beginScene`, N appels, `endScene` (flush compris).

| Cas                                    | Médiane | Par quad | Draw calls |
|----------------------------------------|---------|----------|------------|
| `renderer2d/transform_to_matrix/10000` | 758 µs  | 75,8 ns  |            |
| `renderer2d/quads_color/1000`          | 86 µs   | 86,2 ns  | 1          |
| `renderer2d/quads_color/10000`         | 862 µs  | 86,2 ns  | 3          |
| `renderer2d/quads_color/100000`        | 8,65 ms | 86,5 ns  | 25         |
| `renderer2d/quads_world_index/10000`   | 105 µs  | 10,5 ns  | 1          |
| `renderer2d/quads_world_index/100000`  | 1,23 ms | 12,3 ns  | 5          |
| `renderer2d/quads_16_textures/10000`   | 935 µs  | 93,5 ns  | 3          |
| `renderer2d/circles/10000`             | 857 µs  | 85,7 ns  | 3          |
| `renderer2d/text/100_strings_54_chars` | 1,08 ms | 200 ns   | par glyphe |

Le chemin transitoire coupe un lot tous les 4 096 transforms (`g_maxTransientWorldsPerBatch`), le chemin `worldIndex` tous
les 20 000 quads. Profil de `quads_color/10000` : `resolveWorldIndex` 94,5 % de `drawQuad`, dont 77,7 % dans
`Transform::operator()` (trois `rotate` génériques axe-angle et quatre produits de matrices) ; le `memcpy` du flush pèse 12,8 %.

### 3.4 Sérialisation YAML, undo, prefab

| Cas                                          | Médiane | Par entité | Remarque                             |
|----------------------------------------------|---------|------------|--------------------------------------|
| `serialize/scene_to_string/1000`             | 29,9 ms | 29,9 µs    | 315 Ko de YAML                       |
| `serialize/scene_to_string/10000`            | 311 ms  | 31,1 µs    | 3,2 Mo de YAML                       |
| `serialize/scene_from_string/1000`           | 117 ms  | 117 µs     | `deserializeFromBuffer`              |
| `serialize/scene_from_string/10000`          | 1,34 s  | 134 µs     |                                      |
| `serialize/scene_parse_only/10000`           | 651 ms  | 65 µs      | `parseBuffer` seul : ~4,9 Mo/s       |
| `serialize/entity_to_string/scene1`          | 25,8 µs |            | snapshot d'undo, 218 o               |
| `serialize/entity_to_string/scene10000`      | 26,3 µs |            | indépendant de la taille de la scène |
| `serialize/entity_from_string/scene1`        | 106 µs  |            | restauration d'une entité            |
| `serialize/entity_from_string/scene10000`    | 160 µs  |            |                                      |
| `prefab/instantiate/10_entities/scene0`      | 2,55 ms | 255 µs     |                                      |
| `prefab/instantiate/100_entities/scene0`     | 26,0 ms | 260 µs     |                                      |
| `prefab/instantiate/100_entities/scene10000` | 25,7 ms | 257 µs     | indépendant de la scène cible        |

Profils : dans `deserializeFromBuffer`, le parseur yaml-cpp prend 55 % et `deserializeEntity` 31 % ; dans
`serializeEntityToString`, `YAML::Emitter::Write` prend 76 % (dont 73 % dans `IsValidPlainScalar`, qui teste chaque chaîne par
expressions régulières) ; dans `PrefabSerializer::instantiate`, 49 % pour relire le fichier, le reste dans un aller-retour
YAML par entité (`PrefabSerializer.cpp:164-168`).

Les mesures de `10-constats-CE-scene-editeur.md` sur `EntitySnapshot` (restauration en 15,8 / 7,2 ms) portent sur la
commande d'éditeur complète, pas sur `deserializeEntityFromString` seul (106-160 µs ici) : l'écart vient du reste de la
commande et reste à attribuer.

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

Stockage d'un chunk : 16 Ko (blocs + métadonnées, 2 × 4 096 × 2 o). Un sommet `VoxelVertex` fait 40 o, soit 184 o par quad
avec les indices : le maillage d'un chunk de surface pèse 3,4 fois ses données. Profil de `terrain_surface/ao` :
`Chunk::getBlock` (non inline, avec contrôle de bornes) 18,9 % et `BlockRegistry::isOpaque` 13,9 % des instructions.

### 3.6 Lua

| Cas                                                | Médiane | Par instance |
|----------------------------------------------------|---------|--------------|
| `script/create_instance/empty_script`              | 30,5 µs |              |
| `script/memory/bytes_per_instance`                 | 8 965 o |              |
| `script/on_update/empty/1_instance`                | 43,5 ns | 43,5 ns      |
| `script/on_update/empty/1000_instances`            | 84,3 µs | 84,3 ns      |
| `script/on_update/get_set_position/1_instance`     | 133 ns  | 133 ns       |
| `script/on_update/get_set_position/1000_instances` | 261 µs  | 261 ns       |
| `script/on_update/arith_100/1000_instances`        | 597 µs  | 597 ns       |

À 1 000 instances, le coût par appel double (84 ns contre 44) : chaque instance a son propre `lua_State`
(`ScriptInstance.cpp:41`), donc ses données sont dispersées en mémoire. La mémoire est un delta `mallinfo2` avec un
script vide ; la sonde de `10-constats-D-sous-systemes.md` (M1) annonçait 24-25 Kio, avec une autre méthode : à réconcilier.

### 3.7 Physique (Box2D 3.1.1)

Boîtes dynamiques 1 × 1 en grille de 200 colonnes, sol statique ; `PhysicCommand::frame` à pas fixe de 16,7 ms.
`step_falling` : les 60 premières frames après `init` ; `step_settled` : 60 frames après 600 frames de stabilisation.

| Cas                    | 100 corps | 1 000 corps | 5 000 corps |
|------------------------|-----------|-------------|-------------|
| `physics/init` (total) | 54 µs     | 586 µs      | 3,33 ms     |
| `physics/step_falling` | 22,9 µs   | 228 µs      | 1,12 ms     |
| `physics/step_settled` | 2,9 µs    | 28 µs       | 4,88 ms     |

À 5 000 corps, la pile de 25 rangées ne s'endort pas et reste en contact : 4,9 ms par pas. Profil de `step_falling/1000` :
`b2World_Step` 87,6 % (exécuté en série par `b2DefaultAddTaskFcn`), boucle de synchronisation vers les `Transform` 12,4 %.

#### 3.7.1 Après PR-22 (pas fixe, solveur multi-thread) — 2026-10-05

Branche `Feature/FixedStepPhysics`. Machine chargée par d'autres agents (load average 5 à 12, pics à 50 écartés) :
chiffres relatifs, avant / après mesurés alternés dans la même fenêtre. Avant = `Feature/LuaOnCollision` (pas de 16 ms,
`getSeconds()` tronque à la milliseconde) ; après = un pas fixe de 1/60 s par frame de 16,667 ms. Temps par frame.

Mono-thread, `taskset -c 6` :

| Cas                             | Avant (100 / 1 000 / 5 000 corps) | Après (100 / 1 000 / 5 000 corps) |
|---------------------------------|-----------------------------------|-----------------------------------|
| `physics/step_falling`          | 23,0 µs / 230 µs / 1,10 ms        | 22,2 µs / 227 µs / 1,11 ms        |
| `physics/step_settled`          | 2,6 µs / 24,4 µs / 4,86 ms        | 2,0 µs / 18,2 µs / 5,04 ms        |
| `physics/frame_empty` (0 corps) | 355 ns                            | 284 ns (60 Hz) ; 132 ns (144 Hz)  |

Le surcoût de l'accumulateur est nul à l'échelle de la mesure : une frame sans pas (144 Hz, 60 % des frames) ne coûte
que la boucle d'interpolation ; une frame vide à 60 Hz coûte le `b2World_Step` d'un monde vide (~280 ns).
Synchronisation et interpolation des 5 000 `Transform` : 0,13 ms par frame ; événements de contact : 5 à 12 µs par pas.

Multi-thread (`workerCount` réglé), même exécution, `taskset -c 6,8,10,12,14` (cinq cœurs physiques) :

| Cas                        | 1 thread | 2 workers | 4 workers | 8 workers |
|----------------------------|----------|-----------|-----------|-----------|
| `step_falling/100_bodies`  | 23,5 µs  | 47,3 µs   | 46,5 µs   | 49,3 µs   |
| `step_settled/100_bodies`  | 2,1 µs   | 4,7 µs    | 3,6 µs    | 5,0 µs    |
| `step_falling/1000_bodies` | 227 µs   | 212 µs    | 208 µs    | 193 µs    |
| `step_settled/1000_bodies` | 19,2 µs  | 20,3 µs   | 20,6 µs   | 21,9 µs   |
| `step_falling/5000_bodies` | 1,11 ms  | 890 µs    | 726 µs    | 738 µs    |
| `step_settled/5000_bodies` | 5,14 ms  | 3,85 ms   | 3,18 ms   | 2,86 ms   |

Le gain n'apparaît qu'au-delà de quelques milliers de corps ; en dessous, la distribution des tâches coûte plus qu'elle
ne rapporte, d'où le seuil automatique de 2 000 corps dynamiques. L'objectif v0.3.0 (5 000 corps sous 1,5 ms par pas)
n'est pas atteint : 3,2 ms à 4 workers sur cette pile qui ne s'endort pas, dont `b2World_GetProfile` attribue l'essentiel
au solveur de contraintes. Le paquet Box2D 3.1.1 est compilé en SSE2 sans AVX2 (`objdump` : 0 registre `ymm`), ce que
Catto chiffre à ~×1,1 ; le reste est à chercher dans la scène (25 rangées en contact permanent) et la charge de la
machine. Remesurer sur machine calme et après la migration du paquet (AVX2).

### 3.8 Scènes de `sample_project/` (backend Null)

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

Les scènes d'exemple sont petites (au plus 110 entités) : leur coût CPU de frame est négligeable et ne stresse pas le
moteur. `voxel_terrain` ne maille rien ici : sans couche `RendererVoxel` dans la pile de rendu (application factice), le
streaming éditeur ne produit aucun maillage. Ces scènes ne servent qu'au futur frame bench GPU.

### 3.9 Démarrage et shaders

| Cas                                                 | Médiane |
|-----------------------------------------------------|---------|
| `app/startup_ms/dummy_null_backend`                 | 226 ms  |
| `slang/compile_cold/quad_vulkan_first_in_process`   | 74 ms   |
| `slang/compile_warm/quad_vulkan`                    | 17,4 ms |
| `slang/compile_warm/quad_opengl`                    | 17,3 ms |
| `slang/compile_warm/voxel_vulkan`                   | 21,7 ms |
| `slang/compile_warm/all_engine_shaders_vulkan` (13) | 219 ms  |
| `slang/cache_hit/quad_validate_and_read`            | 5,4 µs  |

La compilation « à froid » comprend la création de la session globale Slang (`shaderFileUtils.cpp:185-194`). Une passe non
épinglée sur machine chargée donnait 185 ms : on reste, dans tous les cas, très loin de 50 s.

### 3.10 Streaming voxel par frame (après audit : PR-24)

`voxel/streaming/*` (`bench/cases/VoxelStreamingBench.cpp`) : monde procédural, couche `RendererVoxel` réelle (backend
Null : l'upload ne coûte rien, seul le CPU est mesuré), frames cadencées à 60 Hz comme une boucle synchronisée, temps
CPU du thread principal par frame (`CLOCK_THREAD_CPUTIME_ID`, scène + `Scheduler::frame`). Avant = `Feature/GameExport`
(maillage synchrone dans `prepareWorld`), après = maillage sur les workers. `taskset -c 6`, avant / après alternés,
médiane de 3 passes (min-max entre crochets), machine chargée (load average 18 à 32).

| Scénario                                     | Mesure                       | Avant               | Après                 |
|----------------------------------------------|------------------------------|---------------------|-----------------------|
| `runtime_walk/walk` (Play, r = 4, h = 2,     | pic de frame                 | 14,3 ms [12,4-17,2] | 0,35 ms [0,34-0,57]   |
| 32 chunks à 1 chunk / 16 frames, 518 frames) | p99 / p50                    | 9,06 / 1,17 ms      | 0,34 / 0,08 ms        |
|                                              | frames > 5 ms                | 32                  | 0                     |
|                                              | CPU thread principal (total) | 889 ms              | 55 ms                 |
|                                              | CPU processus (total)        | 1 098 ms            | 828 ms                |
|                                              | maillage, tous threads       | (dans le principal) | 651 ms, 1 152 uploads |
|                                              | latence d'apparition moyenne | 0 (même frame)      | 29 ms (1,75 frame)    |
| `runtime_walk/initial_load` (405 chunks)     | pic / frames > 5 ms          | 11,9 ms / 10        | 0,48 ms / 0           |
|                                              | latence moyenne / max        | 0                   | 58 / 183 ms           |
| `editor_load` (r = 8, h = 4, 2 601 chunks,   | pic / p50                    | 13,7 / 5,94 ms      | 0,49 / 0,31 ms        |
| 168 frames)                                  | frames > 5 ms                | 96                  | 0                     |
|                                              | CPU thread principal (total) | 982 ms              | 51 ms                 |
|                                              | CPU processus (total)        | 1 398 ms            | 1 606 ms              |
|                                              | meshes finaux / uploads      | 1 887 / —           | 1 492 / 1 974         |
|                                              | latence moyenne / max        | 0                   | 422 ms / 2,0 s        |

Sur 4 cœurs (`taskset -c 6-9`, 2 passes), même tableau : pic de la marche 12,0 → 0,54 ms, latences identiques. La latence
n'est donc pas bornée par le CPU : un chunk attend que ses voisins en cours de génération soient installés (la
génération reste à 16 chunks par frame), puis passe au plus 32 uploads par frame. Le travail total augmente dans
l'éditeur (+15 % de CPU processus) parce que l'arrivée d'un voisin remaille désormais le chunk déjà maillé (D-08) ; en
contrepartie, 395 meshes de moins restent à dessiner : leurs faces de bord étaient cachées par des voisins arrivés après
coup et ne sont plus émises. La marche coûte moins au total (828 contre 1 098 ms) : le thread principal ne fait plus
que capturer, téléverser et dessiner.

## 4. Constats

```text
ID             : P-01
Axe            : C (scène, hiérarchie)
Nature         : risque
Gravité        : moyenne
Preuve         : source/owl/private/scene/Scene.cpp:2137 (maxDepth = 64 dans getWorldTransform), :2259 (idem dans
                 isEffectivelyVisible), :2299 (setParent) ; engine_assets/shaders/world_transform/slang/world_transform.slang:57
                 (hops < 64) ; mesure scene/world_transform/chain1000_leaf_x_engine = 65 (attendu 1 000) et
                 scene/set_parent/chain1000_leaf_x_after_reparent = 8 064 (attendu 999).
Constat        : Au-delà de 64 ancêtres, le monde calculé hors frame est faux sans erreur (un simple WARN, coupé en
                 Release), le shader GPU tronque de même, et setParent recalcule la transform locale à partir de ce
                 monde faux : la position de l'entité est corrompue de façon permanente. Seul prepareWorldTransforms
                 (CPU) est juste à toute profondeur.
Comparaison    : Godot et Bevy n'ont pas de plafond de profondeur ; ils propagent les transforms de haut en bas.
Recommandation : corriger, effort S : calculer le monde par propagation descendante (déjà faite dans
                 prepareWorldTransforms) ou par une marche sans plafond avec détection de cycle à l'insertion
                 (setParent la fait déjà) ; fournir au shader les mondes calculés au lieu de les recalculer (P-04).
Statut         : confirmé (mesuré)
Vérification   : Rejoué (owl_bench --filter=chain1000 / scene/set_parent) : 65 et 8 064, et un calcul analytique de
                 la marche plafonnée redonne 8 064 ; le shader plafonne aussi (hops < 64). Seule nuance : le WARN n'est
                 pas « coupé en Release » (Log.h:166, niveau par défaut Trace), et setParent ne journalise rien.
```

```text
ID             : P-02
Axe            : C / B (frame)
Nature         : faiblesse
Gravité        : moyenne
Preuve         : frame/editor_update/flat10000 = 2,52 ms (252 ns par sprite), runtime render 2,52 ms ; callgrind :
                 prepareWorldTransforms 71,7 %, allocations ~30 % ; Scene.cpp:2210-2213 (emplace / insert_or_assign dans
                 m_entityToWorldIndex et m_worldTransformCache, Scene.h:533) et :2284 (m_visibilityCache), vidés à
                 chaque frame ; Scene.cpp:2213 (math::Transform{worldMat} décompose chaque matrice monde, atan2 compris).
Constat        : Le coût CPU par sprite et par frame tient à la préparation des transforms, pas au rendu : trois tables
                 de hachage à nœuds alloués sont reconstruites chaque frame, et chaque matrice monde est décomposée en
                 TRS pour un cache que le chemin de rendu n'utilise pas. Cela contredit la règle « no per-frame
                 allocation » de .claude/rules/renderer.md. À ce rythme, un cœur ne tient qu'environ 60 000 sprites à
                 60 Hz avant tout travail GPU, physique ou script.
Comparaison    : Opinion : un moteur 2D à ECS vise quelques dizaines de ns par sprite côté CPU (tableaux denses indexés
                 par entité, mat4 en cache, propagation par niveaux) ; chiffres externes à sourcer dans 30-etat-de-l-art.md.
Recommandation : corriger, effort M : stocker les mondes en mat4 dans un vecteur dense (ou un composant
                 WorldTransform EnTT), indexer par entité au lieu d'unordered_map, ne décomposer qu'à la demande,
                 réserver une fois. Gain attendu (opinion) : un facteur 3 à 5 sur la frame.
Statut         : confirmé (mesuré)
Vérification   : Les trois unordered_map à nœuds sont bien vidées et remplies à chaque frame (Scene.h:504, :533, :557).
                 Mais le cache TRS n'est pas inutile au rendu : texte, tilemaps, raycast et repli sans worldIndex le
                 lisent (Scene.cpp:1053, :1073, :1093...). Gravité abaissée de haute à moyenne : perte de perf sans
                 bogue, et 2,5 ms pour 10 000 sprites tient à 60 Hz alors que les scènes réelles font au plus 110 entités.
```

```text
ID             : P-03
Axe            : C
Nature         : faiblesse
Gravité        : moyenne
Preuve         : Scene.cpp:2251-2287 ; frame/editor_update/chain1000 = 1,64 ms contre 246 µs pour flat1000 (6,7×) ;
                 callgrind : isEffectivelyVisible 84,5 %, findEntityByUUID 41,7 %.
Constat        : La visibilité héritée est recalculée par entité en remontant la chaîne de parents (une recherche UUID par
                 niveau) ; le cache ne retient que le résultat de l'entité, pas celui des ancêtres. Le coût est en
                 O(n × profondeur), plafonné à 64.
Comparaison    : Godot propage la visibilité dans l'arbre ; Bevy calcule InheritedVisibility dans une passe descendante.
Recommandation : corriger, effort S : calculer la visibilité effective dans la passe pré-ordre de
                 prepareWorldTransforms (un bit par slot), ou mémoriser les ancêtres visités.
Statut         : confirmé (mesuré)
Vérification   : Scene.cpp:2251-2287 remonte bien la chaîne avec un findEntityByUUID par niveau et ne met en cache que
                 la clé de l'entité ; rejoué : editor_update/chain1000 = 1,63 ms.
```

```text
ID             : P-04
Axe            : B
Nature         : étrangeté
Gravité        : basse
Preuve         : Scene.cpp:2166-2216 (le CPU calcule déjà toutes les matrices monde, cpuWorlds) puis
                 WorldTransformPass.cpp:49-94 (upload des locales et des parents, recalcul sur GPU) ;
                 world_transform.slang:50-63 (chaque thread remonte jusqu'à 64 parents).
Constat        : Les transforms monde sont calculées deux fois par frame, sur CPU puis sur GPU, et la version GPU est
                 en O(n × profondeur) avec le plafond de P-01. Le seul gain serait d'éviter l'upload de mat4 monde, mais
                 on uploade des mat4 locales de même taille.
Comparaison    : Les moteurs qui calculent les transforms sur GPU le font justement pour ne pas les calculer sur CPU.
Recommandation : corriger, effort S : uploader cpuWorlds et supprimer la passe compute, ou l'inverse ; mesurer ensuite
                 avec un frame bench GPU (§7).
Statut         : confirmé (lecture du code)
```

```text
ID             : P-05
Axe            : B
Nature         : force
Gravité        : basse
Preuve         : Renderer2D.cpp:461-492 et :310-376 ; renderer2d/quads_world_index/10000 = 10,5 ns par quad, 1 draw call
                 pour 10 000 quads, 5 pour 100 000.
Constat        : Le batching par instances (SSBO) est efficace : un draw call par type de primitive jusqu'à 20 000
                 instances, et un coût CPU de 10 à 12 ns par quad quand le monde vient de la scène.
Comparaison    : Opinion : au niveau des batchers 2D courants (Hazel-like, raylib) ou mieux, puisque les sommets ne sont
                 pas dupliqués sur CPU.
Recommandation : garder.
Statut         : confirmé (mesuré)
```

```text
ID             : P-06
Axe            : B / A (math)
Nature         : faiblesse
Gravité        : moyenne
Preuve         : source/owl/private/math/Transform.cpp:91-95 ; renderer2d/transform_to_matrix = 75,8 ns ;
                 quads_color = 86 ns par quad contre 10,5 ns avec worldIndex ; callgrind : 77,7 % de drawQuad.
Constat        : Transform::operator() construit la matrice par trois rotate() génériques (axe-angle, chacun avec
                 sin/cos et une matrice complète) et quatre produits mat4. Cette fonction pèse aussi 26 % de la frame
                 éditeur (P-02). Tout le texte, l'UI, les gizmos et le debug passent par ce chemin transitoire.
Comparaison    : Une composition TRS directe (sincos des trois angles, 9 termes) coûte typiquement une dizaine de ns
                 (opinion, à mesurer).
Recommandation : corriger, effort S : composition directe, ou cache de la matrice locale invalidé à l'écriture.
Statut         : confirmé (mesuré)
Vérification   : Transform.cpp:91-95 enchaîne bien translate, trois rotate(identity, angle, axe) et scale, soit quatre
                 produits mat4 ; aucun chemin rapide ni cache ne neutralise ce coût.
```

```text
ID             : P-07
Axe            : C / E (sérialisation, undo, prefab)
Nature         : faiblesse
Gravité        : moyenne
Preuve         : serialize/scene_from_string/10000 = 1,34 s (134 µs par entité), scene_to_string/10000 = 311 ms,
                 parse seul 4,9 Mo/s ; prefab/instantiate = 256-260 µs par entité ; entity_to_string 26 µs,
                 entity_from_string 106-160 µs ; PrefabSerializer.cpp:97 et :164-168 ; callgrind : parseur yaml-cpp
                 55 %, IsValidPlainScalar 73 % de l'émission.
Constat        : Tout ce qui copie des entités passe par du texte YAML (chargement, prefab, undo, duplication) : 100
                 entités de prefab coûtent 26 ms (une frame et demie à 60 Hz), une scène de 10 000 entités se charge
                 en plus d'une seconde. Le prefab relit son fichier et refait un aller-retour YAML par entité à chaque
                 instanciation.
Comparaison    : Godot sérialise en texte (.tscn) mais cuit en binaire (.scn) à l'export et instancie depuis une
                 PackedScene déjà chargée ; Bevy instancie depuis un monde en mémoire. Les parseurs YAML/JSON récents
                 (rapidyaml, simdjson) annoncent un débit bien supérieur à yaml-cpp : à sourcer dans 30-etat-de-l-art.md.
Recommandation : remplacer pour les chemins chauds, effort M à L : mettre en cache le prefab chargé (scène temporaire)
                 et copier les composants ECS directement (comme Scene::copy, 300 ns par entité) ; garder YAML comme
                 format d'édition, ajouter un format binaire cuit pour le runtime et le .owlpack.
Statut         : confirmé (mesuré)
Vérification   : Le double aller-retour est confirmé (PrefabSerializer.cpp:97 LoadFile, :164-168). Gravité abaissée de
                 haute à moyenne : PrefabSerializer::instantiate n'est appelé que par l'éditeur (EditorLayer.cpp:2855,
                 rien en Lua), la duplication du moteur copie les composants sans YAML (Scene.cpp:2050, seul le
                 snapshot d'undo passe par YAML), et la plus grosse scène réelle se charge en 15,5 ms.
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
Gravité        : moyenne
Preuve         : voxel/mesh_by_kind : 282-350 µs (surface), 395-476 µs (chunk plein, 6 quads), 1,67 ms (damier) ;
                 RendererVoxel.cpp:149-170 (prepareWorld remaille tous les chunks sales dans la frame, sur le thread
                 principal, sans budget) ; callgrind : Chunk::getBlock (Chunk.cpp:32, non inline) 18,9 %,
                 BlockRegistry::isOpaque 13,9 %.
Constat        : Un chunk uniforme (plein ou souterrain) coûte autant qu'un chunk de surface, faute de raccourci. Le
                 streaming lance jusqu'à 16 chunks par frame (Scene.cpp:1202) : leur maillage peut coûter 5 à 8 ms
                 dans une seule frame. La génération, elle, est asynchrone (Scene.cpp:1229-1235).
Comparaison    : Opinion : les mailleurs gloutons par masques de bits traitent des chunks bien plus gros dans le même
                 temps ; Minecraft et ses dérivés maillent sur des threads de travail.
Recommandation : corriger, effort M : raccourci pour les chunks uniformes, accès direct au tableau avec bordure
                 recopiée (au lieu d'un std::function par voisin), maillage sur les workers Taskflow avec upload budgété.
Statut         : corrigé par PR-24 (maillage sur workers, upload budgété, §3.10) ; le raccourci des chunks uniformes
                 et l'accès direct au tableau restent à faire
Vérification   : RendererVoxel.cpp:149-176 remaille dans la frame tout chunk sale non vide ; le budget de 16
                 (Scene.cpp:1202) porte sur la mise en file de la génération, et l'installation des chunks finis n'est
                 pas bornée (done.swap, Scene.cpp:1184), ce qui peut même dépasser 16 maillages par frame.
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
Recommandation : surveiller, effort M : à traiter avec P-09 si la mémoire GPU devient un sujet (frame bench §7).
Statut         : confirmé (mesuré)
```

```text
ID             : P-11
Axe            : D (script)
Nature         : force
Gravité        : basse
Preuve         : script/on_update/empty = 43,5 ns (1 instance), 84 ns (1 000) ; get_set_position 133-261 ns ;
                 create_instance 30,5 µs ; 8 965 o par instance ; ScriptInstance.cpp:41 (un lua_State par entité).
Constat        : Le coût d'appel Lua est faible : 1 000 entités scriptées coûtent 0,1 à 0,3 ms par frame. Le modèle
                 « un lua_State par entité » isole bien, mais coûte ~9 Ko et 30 µs par instance, et disperse la mémoire
                 (coût par appel doublé à 1 000 instances).
Comparaison    : Opinion : Defold et la plupart des moteurs Lua partagent une VM et isolent par environnement ; à
                 10 000 entités scriptées, le modèle actuel coûte ~90 Mo et 0,3 s de création.
Recommandation : garder, surveiller au-delà de quelques milliers d'entités scriptées.
Statut         : confirmé (mesuré)
```

```text
ID             : P-12
Axe            : D (physique)
Nature         : faiblesse
Gravité        : moyenne
Preuve         : PhysicCommand.cpp:61-62 (b2DefaultWorldDef sans workerCount ni enqueueTask) ; callgrind :
                 b2DefaultAddTaskFcn (exécution en série) ; physics/step_settled/5000 = 4,88 ms par pas,
                 step_falling/1000 = 228 µs ; synchronisation vers les Transform 12,4 % du pas.
Constat        : Box2D v3 est utilisé en mono-thread alors que le moteur embarque déjà Taskflow : une scène de quelques
                 milliers de corps empilés prend un tiers de frame.
Comparaison    : Box2D v3 est conçu pour un solveur multi-thread branché sur le système de tâches de l'hôte
                 (enqueueTask / finishTask) ; recoupe le constat physique de 10-constats-D-sous-systemes.md.
Recommandation : corriger, effort S : brancher enqueueTask sur le Scheduler, puis remesurer step_settled.
Statut         : confirmé (mesuré)
Vérification   : PhysicCommand.cpp:61-63 ne règle que la gravité sur b2DefaultWorldDef, et aucune occurrence de
                 workerCount ni d'enqueueTask n'existe dans source/. « Un tiers de frame » vaut pour 5 000 corps
                 (29 %) ; à 1 000 corps stabilisés, le pas ne coûte que 28 µs.
```

```text
ID             : P-13
Axe            : J (configuration IA) / B
Nature         : étrangeté
Gravité        : moyenne
Preuve         : CLAUDE.md:94, .claude/rules/slang-shaders.md:66, .claude/rules/testing.md:51 et :74 (« ~50 s ») ;
                 mesure slang/compile_cold = 74 ms, compile_warm 17-22 ms, all_engine_shaders 219 ms, cache 5,4 µs.
Constat        : La consigne « la première compilation Slang d'un processus prend ~50 s » est fausse d'environ trois
                 ordres de grandeur en Release dans l'image actuelle. Elle oriente à tort les agents (fixtures
                 SetUpTestSuite, constat B sur le coût de session) et surévalue l'enjeu du cache SPIR-V, qui ne fait
                 gagner qu'environ 0,2 s par lancement.
Comparaison    : —
Recommandation : corriger la consigne, effort S ; mesurer aussi en Debug + couverture avant de conclure que les 50 s
                 n'existent nulle part (§7).
Statut         : confirmé en Release (mesuré), plausible pour les autres builds
Vérification   : Les quatre lignes citées disent bien « ~50 s » au commit 45e27892 (git show HEAD). Elles sont déjà
                 corrigées dans l'arbre de travail non commité ; la mémoire utilisateur (MEMORY.md, « ~50s ») ne l'est pas.
```

```text
ID             : P-14
Axe            : C / F
Nature         : étrangeté
Gravité        : basse
Preuve         : Scene.cpp:2086-2091 (registry.storage<Entity>(), soit le stockage du composant scene::Entity, jamais
                 rempli) ; test/scene_tests/SceneRuntime_test.cpp:396-401 (le test fige le retour à 0) ;
                 SceneSerializer_test.cpp:26, :68, :91 et Scene_test.cpp:76 (comparent 0 à 0).
Constat        : Scene::getEntityCount renvoie toujours 0, un test verrouille ce comportement et quatre assertions
                 d'aller-retour en deviennent vides. Trouvé en écrivant les métriques du harnais.
Comparaison    : —
Recommandation : corriger, effort S : registry.storage<entt::entity>() (compter les entités valides), puis
                 renforcer les tests d'aller-retour.
Statut         : confirmé (lecture du code et mesure)
Vérification   : Aucun code n'ajoute de composant scene::Entity (grep emplace/addComponent<Entity> vide) et aucun appelant
                 de production n'existe ; il y a même cinq comparaisons vides et deux tests figés à 0 (SceneExtra_test.cpp:87,
                 SceneCoverage_test.cpp:77 en plus de ceux cités).
```

## 5. Réponses aux demandes de mesure des autres axes

| Demande                                | Réponse                                                                             |
|----------------------------------------|-------------------------------------------------------------------------------------|
| B : coût de la session Slang « ~50 s » | 74 ms à froid, 17-22 ms par shader ensuite (P-13)                                   |
| D, M1 : Lua par entité                 | 43-261 ns par appel, 30,5 µs et 9 Ko par instance (§3.6) ; `find_entity` non mesuré |
| D, M2 : pas Box2D                      | 100 → 5 000 corps (§3.7) ; 10 000 corps et hiérarchie non mesurés                   |
| D, M4 : génération et maillage voxel   | §3.5, 15 échantillons, IQR < 2 %                                                    |
| D, M5 et M6 : coût voxel par frame     | CPU mesuré en §3.10 (backend Null) ; coût GPU de l'upload : frame bench GPU (§7)    |
| CE : transforms, copie, snapshot       | §3.1 et §3.4 ; chaînes de 1 000 en plus des chaînes de 16                           |

## 6. Le harnais

- `bench/harness/Bench.{h,cpp}` : `Runner::measure` (lot calibré), `measureWithSetup` (préparation hors chrono),
  `measureOnce` (chemins froids), `metric` (tailles, compteurs, contrôles de correction), sorties CSV et JSON avec la
  charge de la machine au début et à la fin.
- `bench/cases/*.cpp` : un fichier par groupe (`scene`, `serialize`, `prefab`, `renderer2d`, `frame`, `voxel`, `script`,
  `physics`, `sample`, `slang`).
- Il respecte les sous-contrôles de `CodeStyle` (vérifiés en pointant le script sur `bench/`), mais `CodeStyle` ne scanne
  pas `bench/` : l'ajouter à `SOURCE_ROOTS` (`ci/actions/code_style.py:58`) est une ligne, hors du périmètre autorisé de
  l'audit.
- Google Benchmark absent : `poetry run depmanager pack ls -p benchmark` (et `google_benchmark`, `googlebenchmark`,
  `gbenchmark`) ne trouve rien en local, et la liste du remote `package.argawaen.net` (48 paquets) n'en contient pas.
  Le harnais maison suffit pour cette campagne ; un paquet `benchmark` dans OwlDependencies apporterait surtout
  `--benchmark_repetitions`, les compteurs utilisateur et la comparaison de runs (`compare.py`).

## 7. Mesures restant à faire

1. **Frame bench GPU (fait, §8).** Le runner n'avait pas de mode de mesure (`source/owlnest/runner/RunnerLayer.cpp`) et
   l'éditeur n'affiche qu'un FPS (`EditorLayer.cpp:935`). Il faut ajouter au runner, dans une PR hors audit, une option
   `--frame-bench <scène> --frames N --warmup M --csv <fichier>` qui charge la scène, passe en Play, caméra fixe ou
   trajectoire scriptée, et journalise par frame le temps CPU (`Timestep`), le temps GPU (requêtes de timestamp :
   `GL_TIMESTAMP` en OpenGL, `vkCmdWriteTimestamp` en Vulkan) et les statistiques Renderer2D. Lancement :
   `docker/run.sh --gui output/build/linux-clang-release/bin/OwlRunner --frame-bench scenes/raycast_demo.owl ...`
   sur les huit scènes de `sample_project/` et sur les scènes synthétiques du harnais exportées en `.owl`, en OpenGL puis
   en Vulkan, sur la RTX 5000 Ada (`__NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia` pour OpenGL), l'Intel UHD
   et llvmpipe, avec présentation non synchronisée (vsync coupée) et p50 / p99 / max sur 1 000 frames.
2. **Voxel par frame** (M5, M6) : `prepareWorld` et streaming avec une couche `RendererVoxel` réelle ; pics de frame
   en mouvement ; mémoire GPU par chunk.
3. **Debug + couverture** : refaire `slang/compile_cold` dans `linux-clang-debug` pour situer l'origine des « 50 s ».
4. **Mémoire** : pic et allocations par frame (heaptrack absent de l'image) ; `OWL_ENABLE_MEMORY_TRACKER` (M10).
5. **Physique** : 10 000 corps, corps dans une hiérarchie, tilemap collidable (M3), après branchement multi-thread.
6. **Chargement** : ouverture et extraction d'un `.owlpack` (M8), premier et second lancement réels.
7. **Profileur et logs** (M11, M12) : coût d'un `OWL_PROFILE_SCOPE` actif et d'une trace désactivée en boucle chaude.
8. **Comparaison externe** : chiffres publiés d'EnTT, Box2D v3, yaml-cpp / rapidyaml, mailleurs voxel, sourcés et datés,
   dans `30-etat-de-l-art.md`.
9. **Chaîne de build** (second rang) : build à froid, build incrémental après modification de `Scene.h`.

## 8. Frame bench GPU — baseline v0.3.0

> Statut : première campagne, 2026-10-05, branche `Feature/RunnerFrameBench` (PR-17). C'est la **référence à battre**
> pour la refonte du socle Vulkan (PR-28, PR-29) : chaque PR de rendu relance la même matrice et compare.
> Données brutes : un JSON par répétition, non versionné (dossier temporaire de la session de mesure) ; une
> première campagne à load average 64 à 164 (`results/`) donnait des temps CPU 1,5 à 4 fois plus longs et sert
> seulement à montrer la sensibilité à la charge.

### 8.1 Protocole

| Élément      | Valeur                                                                                                      |
|--------------|-------------------------------------------------------------------------------------------------------------|
| Outil        | `OwlRunner --frame-bench <scène> --backend <b> --frames 1000 --warmup 120 --out <json>` (`bench/README.md`) |
| Build        | `linux-clang-release` dans `docker/run.sh --gui`, clang 22, `-O3`                                           |
| Machine      | i9-13950HX, portable hybride : Intel UHD (RPL-S) principal, RTX 5000 Ada Laptop en PRIME offload            |
| Affichage    | session Wayland (`WAYLAND_DISPLAY=wayland-0`), fenêtre GLFW via XWayland, 1280 × 720                        |
| Épinglage    | `taskset -c 2-7` (P-cores) pour le processus entier                                                         |
| Déterminisme | pas fixe 16,667 ms, entrées null (ni clavier, ni souris, ni capture), caméra primaire de la scène, son null |
| Présentation | Vulkan `immediate` (vsync coupée), OpenGL `swap-interval-0`                                                 |
| Répétitions  | 5 par configuration, 1 000 frames mesurées chacune ; chiffres = médiane des 5 médianes                      |
| Charge       | load average 7,5 à 33,6 pendant la campagne (machine partagée avec d'autres agents)                         |

Sélection du GPU, vérifiée par le champ `device` de chaque rapport :

| Configuration   | Variables                                                         | `device` rapporté                                   |
|-----------------|-------------------------------------------------------------------|-----------------------------------------------------|
| Vulkan / NVIDIA | `VK_ICD_FILENAMES=/etc/vulkan/icd.d/nvidia_icd.json`              | NVIDIA RTX 5000 Ada Generation Laptop GPU           |
| OpenGL / NVIDIA | `__NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia`    | NVIDIA RTX 5000 Ada Generation Laptop GPU/PCIe/SSE2 |
| Vulkan / Intel  | `VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/intel_icd.json`         | Intel(R) Graphics (RPL-S)                           |
| OpenGL / Intel  | `__GLX_VENDOR_LIBRARY_NAME=mesa MESA_LOADER_DRIVER_OVERRIDE=iris` | Mesa Intel(R) Graphics (RPL-S)                      |

Mesures par frame : temps mur entre deux débuts de frame (`cpu_total`) et ses phases ; temps GPU par timestamps :
Vulkan `vkCmdWriteTimestamp` autour de **chaque** command buffer soumis (batchs et one-shot), `gpu_busy` = somme des
intervalles, `gpu_span` = premier au dernier timestamp ; OpenGL `glQueryCounter(GL_TIMESTAMP)` à `beginFrame` et
`endFrame` (un seul intervalle : `busy` = `span`, temps d'attente du pilote compris). Les deux `gpu_busy` ne se
comparent donc pas directement : en Vulkan c'est du travail GPU pur, en OpenGL une borne haute. Compteurs : draw calls,
soumissions, `vkQueueWaitIdle`, `vkDeviceWaitIdle` (Vulkan seulement, compteur de debug dans
`vulkan::internal::FrameProfiler`).

Scènes : `main_menu` (UI 2D), `world_map` (tilemap vue de dessus), `platformer_house` (mélange : tilemap, 27 sprites,
HUD UI, physique, 17 scripts Lua), `raycast_demo` (raycaster GPU + sprites billboard), `voxel_terrain` (voxel).

### 8.2 Résultats (médianes, ms par frame)

| Scène              | Backend | GPU    | CPU frame | CPU p99 | Prép. rendu | GPU busy | GPU span | Vidages / frame | Soumissions |
|--------------------|---------|--------|-----------|---------|-------------|----------|----------|-----------------|-------------|
| `main_menu`        | Vulkan  | NVIDIA | 1,07      | 3,39    | 0,48        | 0,039    | 0,63     | 3 + 0           | 6           |
| `main_menu`        | OpenGL  | NVIDIA | 0,66      | 0,81    | 0,07        | 0,35     | 0,35     | —               | —           |
| `main_menu`        | Vulkan  | Intel  | 3,11      | 6,05    | 1,38        | 0,96     | 2,85     | 3 + 0           | 6           |
| `main_menu`        | OpenGL  | Intel  | 0,37      | 0,61    | 0,11        | 0,34     | 0,34     | —               | —           |
| `world_map`        | Vulkan  | NVIDIA | 1,55      | 4,46    | 0,96        | 0,054    | 1,10     | 4 + 0           | 7           |
| `world_map`        | OpenGL  | NVIDIA | 0,35      | 0,46    | 0,06        | 0,042    | 0,042    | —               | —           |
| `world_map`        | Vulkan  | Intel  | 3,35      | 6,88    | 2,12        | 1,06     | 3,23     | 4 + 0           | 7           |
| `world_map`        | OpenGL  | Intel  | 0,56      | 0,65    | 0,13        | 0,51     | 0,51     | —               | —           |
| `platformer_house` | Vulkan  | NVIDIA | 1,60      | 4,77    | 0,97        | 0,055    | 1,14     | 4 + 0           | 7           |
| `platformer_house` | OpenGL  | NVIDIA | 0,35      | 0,46    | 0,06        | 0,034    | 0,034    | —               | —           |
| `platformer_house` | Vulkan  | Intel  | 3,54      | 6,74    | 1,88        | 1,29     | 3,38     | 4 + 0           | 7           |
| `platformer_house` | OpenGL  | Intel  | 0,71      | 0,85    | 0,06        | 0,66     | 0,66     | —               | —           |
| `raycast_demo`     | Vulkan  | NVIDIA | 2,04      | 4,96    | 1,12        | 0,094    | 1,55     | 3 + 2           | 8           |
| `raycast_demo`     | OpenGL  | NVIDIA | 0,76      | 0,85    | 0,63        | 0,47     | 0,47     | —               | —           |
| `raycast_demo`     | Vulkan  | Intel  | 2,98      | 7,09    | 2,08        | 1,09     | 2,84     | 3 + 2           | 8           |
| `raycast_demo`     | OpenGL  | Intel  | 1,01      | 1,22    | 0,87        | 0,92     | 0,92     | —               | —           |
| `voxel_terrain`    | Vulkan  | NVIDIA | 0,61      | 2,14    | 0,04        | 0,014    | 0,17     | 2 + 0           | 4           |
| `voxel_terrain`    | OpenGL  | NVIDIA | 0,53      | 0,65    | 0,06        | 0,008    | 0,008    | —               | —           |
| `voxel_terrain`    | Vulkan  | Intel  | 0,74      | 1,00    | 0,08        | 0,16     | 0,63     | 2 + 0           | 4           |
| `voxel_terrain`    | OpenGL  | Intel  | 0,19      | 0,22    | 0,02        | 0,040    | 0,040    | —               | —           |

« Vidages » = `vkQueueWaitIdle` + `vkDeviceWaitIdle` par frame ; constant d'une frame à l'autre (IQR nul). Draw calls :
4 (`main_menu`), 6 (`world_map`), 5 (`platformer_house`), 3 (`raycast_demo`), 1 (`voxel_terrain`), identiques entre
backends. Écart entre répétitions (max - min des médianes CPU) : 1 à 3 % sur Vulkan / NVIDIA pour les scènes 2D, jusqu'à
78 % sur les configurations les plus courtes ou les plus chargées ; l'IQR intra-run reste sous 0,1 ms sur NVIDIA.

### 8.3 Constats

1. **B-01 mesuré dans le runner.** Sans éditeur (pas de `readPixel`, pas de framebuffer viewport), une frame Vulkan
   vide la file 2 à 4 fois (`vkQueueWaitIdle` des `endSingleTimeCommands` : transitions, clears, compute des transforms,
   upload tilemap) et le raycast ajoute 2 `vkDeviceWaitIdle` (`StorageBuffer::getData`). Les 4 à 8 soumissions par
   frame sont donc sérialisées avec le CPU. Cible PR-28 : 0 vidage, 1 à 2 soumissions.
2. **Le GPU attend le CPU.** Sur NVIDIA, le GPU travaille 0,014 à 0,094 ms par frame Vulkan, mais le premier et le
   dernier timestamp sont espacés de 0,17 à 1,55 ms : plus de 90 % du « temps GPU » de la frame est de l'attente
   d'enregistrement côté CPU. Les scènes d'exemple ne chargent pas le GPU ; elles mesurent le socle.
3. **Vulkan est plus lent qu'OpenGL partout**, en temps CPU : facteur 1,2 (`voxel_terrain`) à 4,6 (`platformer_house`)
   sur NVIDIA, 3,0 à 8,3 sur Intel. L'essentiel est la préparation du rendu (enregistrement + soumissions par batch :
   0,5 à 1,1 ms hors voxel, contre 0,06 à 0,63 ms) ; `beginFrame` (attente de fence + acquisition) reste sous 0,05 ms.
4. **Intel Vulkan : GPU 20 à 25 fois plus lent que NVIDIA** sur la même frame (0,96 à 1,29 ms busy en 2D), alors qu'OpenGL
   Intel tient 0,34 à 0,66 ms de span. À regarder avec la sync validation (PM-09) : clears et transitions one-shot
   sont chers sur ce pilote.
5. **Voxel quasi vide.** `voxel_terrain` n'émet qu'un draw call et 0,01 à 0,16 ms de GPU : la mesure couvre le socle,
   pas le maillage ni le streaming (M5, M6 restent à faire avec une trajectoire de caméra).
6. **lavapipe et llvmpipe ne passent pas** (PR-18 en dépend) : lavapipe plante (SIGSEGV dans le code JIT) au premier
   draw ; la validation montre des descripteurs jamais écrits (`VUID-vkCmdDrawIndexed-None-08114` sur `gInstances`,
   `gSceneWorlds`, `gTransientWorlds`), tolérés par NVIDIA et Intel. llvmpipe (OpenGL 4.5) n'expose pas
   `GL_ARB_gl_spirv` : `GLAD: ERROR glSpecializeShader is NULL!` puis plantage.
7. **Wayland / vsync.** Sous la session Wayland, sans vsync, aucun blocage n'a été observé (200 runs, Vulkan et OpenGL,
   NVIDIA et Intel). Avec `--vsync`, Vulkan choisit `mailbox` et tourne ; OpenGL (`swap-interval-1`, NVIDIA offload)
   se fige au premier `swapBuffers` de l'écran de chargement, avec ou sans `WAYLAND_DISPLAY` (XWayland). Reproductible ;
   relève de l'item v0.3.0 « full Wayland », non corrigé ici.
8. **Sensibilité à la charge.** La campagne à load average 64 à 164 donnait 3,9 à 6,4 ms par frame Vulkan / NVIDIA au
   lieu de 1,1 à 2,0 ms. Toute comparaison se fait à charge basse, épinglée, et cite la load average.

### 8.4 Relancer la matrice

```bash
docker/run.sh --gui env VK_ICD_FILENAMES=/etc/vulkan/icd.d/nvidia_icd.json taskset -c 2-7 \
    output/build/linux-clang-release/bin/OwlRunner --frame-bench sample_project/scenes/platformer_house.owl \
    --backend vulkan --frames 1000 --warmup 120 --out output/frame-bench/platformer_house.vulkan.nvidia.json
```

Une boucle scènes × configurations × 5 répétitions, puis la médiane des médianes par série (`summary.<série>.median`
de chaque JSON) ; comparer `cpu_total_ms`, `gpu_busy_ms`, `gpu_span_ms`, `queue_wait_idle` et `device_wait_idle`.

### 8.5 Après le socle Vulkan (PR-28, PR-29)

2026-10-08, branche `Feature/VulkanFoundation` : même protocole, 3 répétitions de 1 000 frames, avant et après mesurés le
même jour (load average 3 à 12), Vulkan en `immediate`. OpenGL / NVIDIA, inchangé, sert de témoin (0,37 à 1,15 ms dans
les deux campagnes). « Vidages » = `vkQueueWaitIdle` + `vkDeviceWaitIdle`, « attentes » = `fence_wait`.

| Scène              | GPU    | CPU avant | CPU après | p99 avant | p99 après | Vidages      | Soumissions | Attentes après |
|--------------------|--------|-----------|-----------|-----------|-----------|--------------|-------------|----------------|
| `main_menu`        | NVIDIA | 0,92      | 0,41      | 3,19      | 0,65      | 2 → 0        | 5 → 1       | 0              |
| `main_menu`        | Intel  | 1,97      | 0,44      | 3,07      | 0,58      | 2 → 0        | 5 → 1       | 0              |
| `world_map`        | NVIDIA | 1,32      | 0,42      | 6,70      | 0,61      | 3 → 0        | 6 → 1       | 0              |
| `world_map`        | Intel  | 3,06      | 0,55      | 4,45      | 0,73      | 3 → 0        | 6 → 1       | 0              |
| `platformer_house` | NVIDIA | 1,32      | 0,43      | 4,42      | 0,63      | 3 → 0        | 6 → 1       | 0              |
| `platformer_house` | Intel  | 2,86      | 0,50      | 3,98      | 0,64      | 3 → 0        | 6 → 1       | 0              |
| `raycast_demo`     | NVIDIA | 0,97      | 0,74      | 1,42      | 0,96      | 4 → 0        | 7 → 2       | 1              |
| `raycast_demo`     | Intel  | 3,06      | 2,43      | 4,58      | 3,32      | 4 → 0        | 7 → 2       | 1              |
| `voxel_terrain`    | NVIDIA | 0,45      | 0,40      | 7,53      | 0,54      | 1 → 0        | 3 → 1       | 0              |
| `voxel_terrain`    | Intel  | 1,45      | 0,47      | 1,96      | 0,65      | 1 → 0        | 3 → 1       | 0              |

Vulkan rejoint OpenGL sur les scènes 2D et voxel ; le GPU Intel passe de 0,79 à 1,06 ms de travail par frame 2D à 0,35 à
0,51 ms (plus de transitions ni de clears one-shot). Reste le raycast : ses deux lectures CPU par frame
(`StorageBuffer::getData`, nombre de touches et z-buffer) vident la frame en cours une fois (B-01, à supprimer).

## 9. Chantier des cibles de perf (2026-10-09)

Branche `Feature/PerformanceTargets` : une sous-section par étape, chacune mesurée avant / après le même jour.

### 9.1 État initial du jour

Protocole : celui du §2 (`owl_bench`, `taskset -c 6`, 15 échantillons, médiane) et du §8.1 (frame bench,
`taskset -c 2-7`, 1 000 frames après 120 de chauffe), avec **3 répétitions** par configuration et la médiane des 3
médianes. Build `linux-clang-release-bench` (clang 22.1.2) de la branche au 2026-10-09 (`599ec352`, code de `main`),
load average 3,3 à 7,3. Physique multi-thread sur `taskset -c 0-15`. Session de bureau verrouillée pendant le frame
bench (présentation `immediate` sans compositeur, sans effet attendu sur le CPU). Données, non versionnées :
`output/bench-initial/`, `output/frame-bench-initial/`.

| Indicateur                                        | Audit                    | Aujourd'hui                        |
|---------------------------------------------------|--------------------------|------------------------------------|
| Frame CPU, 10 000 sprites (`editor_update`)       | 2,52 ms                  | 2,60 ms                            |
| Coût par quad Renderer2D, `worldIndex` / transit. | 10,5 / 86 ns             | 10,4 / 86,1 ns                     |
| Vidages de file par frame Vulkan, runner          | 2 à 4                    | 0 (1 soumission, 2 en raycast)     |
| Frame runner Vulkan / OpenGL, NVIDIA (5 scènes)   | 0,61-2,04 / 0,35-0,76 ms | 0,48-0,79 / 0,33-0,59 ms           |
| Frame runner Vulkan / OpenGL, Intel (5 scènes)    | 0,74-3,54 / 0,19-1,01 ms | 0,40-1,62 / 0,23-0,90 ms           |
| Chargement de scène par entité (10 000 entités)   | 134 µs                   | 122 µs                             |
| Pas Box2D, 5 000 corps au contact                 | 4,88 ms (1 thread)       | 5,03 ms (1 thread), 1,64 ms (8)    |
| Maillage voxel, pic de frame en streaming         | 0,35 ms (PR-24)          | non remesuré                       |
| Démarrage, application factice Null               | 226 ms                   | 236 ms                             |
| Démarrage runner GPU, jusqu'à la première frame   | ~400 ms (lavapipe)       | 415-632 ms (NVIDIA / Intel, Slang) |
| Lua, `create_instance/empty_script`               | 30,5 µs                  | 47,5 µs                            |
| Lua, mémoire par instance                         | 8 965 o                  | 13 457 o                           |
| Lua, `on_update/empty/1_instance`                 | 43,5 ns                  | 67,3 ns                            |
| Lua, `get_set_position`, 1 000 instances          | 261 ns                   | 376 ns                             |

Le socle Vulkan (§8.5) a tenu ses promesses ; restent hors cible la frame à 10 000 sprites, le chargement de scène et
la physique mono-thread, et Lua a régressé de 45 à 55 % depuis l'audit.

### 9.2 Régression Lua

Cause, par callgrind (`--toggle-collect` sur `ScriptInstance::onUpdate`, puis sur la création) : le durcissement du
bac à sable et le registre typé, pas Lua (5.5.0 avant comme après). Par état, chacun des 70 bindings était une closure
C (`guardedBinding` + la fonction en upvalue) : 70 allocations de 64 o, soit les 4,5 Kio de l'écart mémoire ;
`coroutine.wrap` était un chunk Lua compilé à chaque état (16 % des instructions de la création). Par appel, le nom du
callback et la clé `owl_dt` étaient hachés et internés à chaque frame, et chaque binding relisait la scène liée par
`lua_getfield(REGISTRY, "owl_scene")` ; `lua_insert` du gestionnaire d'erreur et `lua_remove` du global coûtaient deux
rotations de pile.

Correction : bindings en fonctions C légères `guarded<fn>` (aucune allocation), `coroutine.wrap` en C sur le
`coroutine.resume` du bac à sable, scène liée et delta stockés dans le `Quota` de l'état, noms des callbacks
internés une fois (référence de registre, 16 noms au plus), gestionnaire d'erreur poussé avant la fonction,
contexte d'erreur formaté seulement en cas d'erreur. Aucune fonctionnalité retirée (quotas, chien de garde,
traceback, bac à sable).

`taskset -c 6`, avant (`599ec352`) / après alternés, médiane de 3 passes, load average 6,8 à 7,5 :

| Cas (par instance)                      | Audit   | Avant    | Après   |
|-----------------------------------------|---------|----------|---------|
| `create_instance/empty_script`          | 30,5 µs | 48,3 µs  | 28,1 µs |
| `memory/bytes_per_instance`             | 8 965 o | 13 452 o | 9 020 o |
| `on_update/empty/1_instance`            | 43,5 ns | 65,4 ns  | 43,8 ns |
| `on_update/empty/1000_instances`        | 84,3 ns | 164 ns   | 80,1 ns |
| `on_update/get_set_position/1_instance` | 133 ns  | 188 ns   | 140 ns  |
| `on_update/get_set_position/1000_inst.` | 261 ns  | 365 ns   | 258 ns  |
| `on_update/arith_100/1000_instances`    | 597 ns  | 657 ns   | 615 ns  |

Lua revient au niveau de l'audit sur la création et l'appel vide ; restent 55 o par instance (+0,6 %, le `Quota` élargi
de la scène, du delta et du cache des noms) et 3 à 5 % sur `get_set_position` et `arith_100`, à la limite du bruit entre
campagnes (load average 7 contre 11 à l'audit).

### 9.3 Vulkan contre OpenGL au frame bench

Cause, par chronos temporaires dans `VulkanHandler::startFrame` puis `INTEL_MEASURE=draw` (temps GPU par événement,
pilotes anv et iris) : le `cpu_begin_frame_ms` Vulkan n'est pas du travail du moteur mais l'attente du rythme de
présentation. Sur NVIDIA (PRIME vers l'écran Intel, Wayland, `immediate`), `vkAcquireNextImageKHR` bloque 0,35 ms ;
avec 4 à 6 images au lieu de 3, l'attente passe sur la fence de frame (la soumission attend le sémaphore de l'image),
et `vkcube --present_mode 0` en 1280 × 720 tourne à 0,48 ms par frame : c'est le plancher du chemin de présentation,
OpenGL l'atteint plus vite (EGL bloque 0,23 ms dans le swap, compté dans `cpu_present_ms`). Sur Intel la frame
attend le GPU : 338 µs de travail par frame `main_menu` en Vulkan contre 218 µs en OpenGL, dont 39 µs pour un
`vkCmdClearAttachments` redondant juste après la passe qui efface au chargement, et la profondeur et l'attachement
d'identifiant que la cible principale Vulkan porte (le framebuffer par défaut OpenGL n'en a pas). Le raycast lisait
deux SSBO par frame (`StorageBuffer::getData`), soit une soumission et une attente de fence en Vulkan et un
`glGetBufferSubData` bloquant en OpenGL.

Corrections : le raycast parcourt le DDA sur le CPU pour les profondeurs de colonne et les statistiques (le même
`cpuWalkColumn` que le backend Null, sans émission) au lieu de relire la passe GPU ; les lots successifs sur un même
framebuffer partagent une passe de rendu (4 à 2 passes par frame 2D) ; `RenderCommand::clear` ne réefface plus une
passe qui vient d'effacer au chargement ; le frame bench rapporte `cpu_pace_wait_ms` (fence de frame + acquisition
d'image, Vulkan). Images de référence inchangées (`ctest -L render`), validation sans message sur NVIDIA et lavapipe.

Avant (`5ee02064`) / après alternés, 3 répétitions, médiane des médianes, ms par frame, load average 7 à 12 ;
« travail » = `cpu_total` moins l'attente de présentation (`cpu_pace_wait_ms` en Vulkan, `cpu_present_ms` en OpenGL) :

| Scène              | GPU    | Vulkan avant | Vulkan après | OpenGL avant | OpenGL après | Travail Vulkan / OpenGL après |
|--------------------|--------|--------------|--------------|--------------|--------------|-------------------------------|
| `main_menu`        | NVIDIA | 0,478        | 0,475        | 0,329        | 0,329        | 0,140 / 0,100                 |
| `main_menu`        | Intel  | 0,390        | 0,350        | 0,231        | 0,232        | 0,116 / 0,086                 |
| `world_map`        | NVIDIA | 0,504        | 0,496        | 0,341        | 0,341        | 0,143 / 0,086                 |
| `world_map`        | Intel  | 0,562        | 0,519        | 0,373        | 0,370        | 0,158 / 0,117                 |
| `platformer_house` | NVIDIA | 0,495        | 0,494        | 0,336        | 0,336        | 0,162 / 0,104                 |
| `platformer_house` | Intel  | 0,505        | 0,466        | 0,247        | 0,247        | 0,184 / 0,099                 |
| `raycast_demo`     | NVIDIA | 0,796        | 0,519        | 0,584        | 0,359        | 0,358 / 0,321                 |
| `raycast_demo`     | Intel  | 1,465        | 0,418        | 0,941        | 0,623        | 0,370 / 0,342                 |
| `voxel_terrain`    | NVIDIA | 0,484        | 0,483        | 0,330        | 0,331        | 0,119 / 0,171                 |
| `voxel_terrain`    | Intel  | 0,368        | 0,351        | 0,329        | 0,359        | 0,193 / 0,081                 |

Raycast : 2 → 1 soumission, 1 → 0 attente de fence, frame divisée par 1,5 à 3,5 sur les deux backends ; GPU Intel des
scènes 2D 0,36-0,52 → 0,32-0,48 ms. Le raycast Vulkan passe sous OpenGL sur Intel (0,42 contre 0,62 ms) ; ailleurs la
frame Vulkan reste au plancher de présentation du pilote (NVIDIA) ou au GPU (Intel), et le travail CPU Vulkan reste
0,03 à 0,11 ms au-dessus d'OpenGL (sauf `voxel_terrain` NVIDIA). « Vulkan ≤ OpenGL » n'est donc pas atteignable au
temps mur sous Wayland sans toucher à la présentation ; à trancher : mesurer la cible sur le travail (hors attente de
présentation), ou alléger la cible principale Vulkan (profondeur et identifiant seulement quand un calque les utilise).
