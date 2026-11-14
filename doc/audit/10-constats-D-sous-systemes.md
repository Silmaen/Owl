# Audit — axe D : sous-systèmes

> Statut : revu le 2026-11-12 sur `Feature/sample-games-new` b4281d72. Lecture seule ; les mesures (D-20, D-22, D-27)
> datent du 2026-10-20 (sonde jetable hors dépôt, compilée dans l'image Docker contre `libOwlEngine.so`, clang 22,
> `-O2`, i9-13950HX, 3 répétitions) et n'ont pas été refaites : chiffres indicatifs, ceux de D-22 portent sur Lua 5.5,
> remplacé depuis par Luau. **confirmé** = vu dans le code et/ou reproduit ; **plausible** = déduit du code, non
> reproduit. Les comparaisons externes sont de l'opinion informée (`30-etat-de-l-art.md`).

## Résumé

- Briques tierces à jour et cachées derrière des pimpl ; Jolt remplace Box2D pour la 2D et la 3D, monde physique et
  VM de script portés par la scène (D-19, D-21, D-22, D-28, D-29) ; voxel solide (D-20).
- Simulation déterministe et rejouable, scripts visuels compilés vers Lua et vérifiés, IA testée (D-30, D-32, D-33).
- Son : bus et mixeur en place, mais une source OpenAL par lecture, décodage intégral, pas de streaming (D-13).
- Reliquats : deux mécanismes de déclencheurs, pack sans contrôle d'intégrité, état global résiduel, deux pools de
  threads (D-14, D-10, D-15, D-31).
- Outillage : tracker mémoire à mutex global, profiler « chrome » qui flushe à chaque scope (D-11, D-12).
- 18 constats : 0 haut, 7 moyens, 11 bas ; 9 forces, 6 faiblesses, 0 risque, 3 étrangetés.

## Constats — gravité moyenne

```text
ID             : D-13 — faiblesse, moyenne, confirmé (son)
Constat        : le mixeur (bus Master / Music / SFX / Ambient / Voice, fondus, volumes utilisateur) est en place, mais
                 le backend reste naïf : un `alGenSources` par lecture (recyclage seulement sur `AL_OUT_OF_MEMORY`),
                 fichier entier décodé, pas de streaming (musique comprise), `sound.play` bloquant sur le thread
                 principal, aucune reprise sur perte de périphérique.
Preuve         : openal/SoundAPI.cpp:184-197 ; openal/SoundData.cpp:317 ; LuaBindings.cpp:4427 (`blocking<luaSoundPlay>`) ;
                 sound/Mixer.cpp:56-235.
Recommandation : corriger, effort M (pool de sources, streaming musique, préchargement ; évaluer miniaudio).
```

```text
ID             : D-19 — force, moyenne, confirmé (physique)
Constat        : Jolt 5.6.0 sert la 2D (`EAllowedDOFs::Plane2D`) et la 3D : un monde par scène, système de jobs sur
                 Taskflow, capteurs, couches / masques, joints cassables, requêtes, assemblages, gravité par scène,
                 `saveState` / `restoreState`. Frontière publique propre (aucun en-tête Jolt exposé). Sur x86_64, Jolt
                 exige SSE4.2 : sans lui la physique est désactivée avec un avertissement.
Preuve         : conanfile.py:154 ; physics/JoltWorld.cpp:158 (`TaskflowJobSystem`), 581-586 ;
                 physics/PhysicCommand.cpp:1198-1216 ; test/unit/physics_tests (15 fichiers).
Recommandation : garder
```

```text
ID             : D-20 — force, moyenne, confirmé (voxel)
Constat        : greedy meshing préservant l'AO (correction d'anisotropie), tri arrière → avant du transparent, culling
                 par frustum, génération déterministe sans état partagé, DDA Amanatides & Woo. Mesuré (2026-10-20) :
                 génération 63–107 µs, maillage 350–560 µs par chunk 16³.
Preuve         : data/voxel/ChunkMesher.cpp ; TerrainGenerator.cpp ; VoxelRaycast.cpp ; test/unit/voxel_tests.
Recommandation : garder
```

```text
ID             : D-21 — force, moyenne, confirmé (tâches)
Constat        : le contrat « Scheduler main-thread-only, workers sans état partagé » est respecté : décodage de texture et
                 maillage voxel sur worker, résultat appliqué sur le thread principal, Taskflow caché derrière un pimpl ;
                 `parallelFor` borné à 7 aides pour ne pas payer plus de réveils que de travail.
Preuve         : core/task/SchedulerImpl.h:63 ; core/task/Scheduler.cpp:25-26, 101-120 ; renderer/gpu/Texture.cpp.
Recommandation : garder
```

```text
ID             : D-22 — force, moyenne, confirmé (script)
Constat        : Luau sobre et robuste : appels sous `lua_pcall`, une `lua_State` par instance, bac à sable, quotas mémoire
                 et temps par interruption (le script fautif est désactivé), erreurs répétées journalisées une fois puis
                 comptées. Mesuré sous Lua 5.5 (1 000 instances, 2026-10-20) : création 26–32 µs et 24–25 KiB, `on_update`
                 vide 80–111 ns ; non remesuré sous Luau.
Preuve         : script/LuaEngine.cpp:282, 357, 441-477, 544, 813-830 ; script/ScriptInstance.cpp:46-48, 80-89.
Recommandation : garder (remesurer sous Luau)
```

```text
ID             : D-30 — force, moyenne, confirmé (simulation)
Constat        : simulation déterministe de bout en bout : flux aléatoires par système, fonctions élémentaires aux mêmes
                 bits sur toutes les cibles, hash du monde, replay image par image avec points de contrôle, rembobinage
                 dans Owl Nest, Jolt bit-exact quel que soit le nombre de workers, hash figé en test.
Preuve         : math/RandomStream.h ; math/elementaryFunctions.h ; scene/WorldHash.h ; scene/Replay.h ;
                 physics_tests/WorldHashGolden_test.cpp, JoltDeterminism_test.cpp, PhysicMultiThread_test.cpp,
                 Replay_test.cpp.
Recommandation : garder
```

```text
ID             : D-32 — force, moyenne, confirmé (script visuel)
Constat        : les graphes visuels sont compilés en Lua (pas d'interpréteur de graphe), vérifiés par le type checker Luau
                 contre l'API générée du registre de liaisons ; les tests compilent, chargent et vérifient chaque graphe de
                 la démo et des jeux d'exemple.
Preuve         : script/visual/VisualScriptCompiler.cpp (2 152 lignes) ; script/ScriptTypeChecker.cpp ;
                 script_tests/VisualScriptDemo_test.cpp, VisualScriptCodegen_test.cpp, ScriptTypeChecker_test.cpp.
Recommandation : garder
```

## Constats — gravité basse

```text
ID             : D-10 — étrangeté, basse, confirmé (.owlpack)
Constat        : le pack est lu en mémoire (plus d'extraction), mais l'« obfuscation » XOR reste sans valeur de protection
                 et aucune entrée ne porte de somme de contrôle : une corruption n'est vue que si zstd échoue.
                 `extractPack` reste une API publique que le moteur n'appelle plus (tests et fuzz seulement).
Preuve         : PackFormat.cpp:30, 61-66 ; PackFormat.h:107-120 (`TocEntry` sans checksum) ; app/Application.cpp:115-120,
                 732-757 ; PackExtractor.h:56-75.
Recommandation : corriger, effort S (checksum xxhash par entrée, déjà en dépendance ; retirer ou rendre interne
                 `extractPack`).
```

```text
ID             : D-11 — faiblesse, basse, confirmé (mémoire)
Constat        : tracker (`OWL_ENABLE_MEMORY_TRACKER`, désactivé par défaut) : mutex global par `new`/`delete`, ensemble
                 vivant doublé, `globals()` lu sans verrou (data race), `operator new` renvoie le `nullptr` de `malloc`
                 au lieu de lever `bad_alloc`.
Preuve         : debug/Tracker.cpp:42, 106-111, 242 ; CMakeLists.txt:24.
Recommandation : remplacer, effort S/M (Tracy, déjà intégré en option, ou heaptrack).
```

```text
ID             : D-12 — faiblesse, basse, confirmé (profilage)
Constat        : Tracy est disponible par l'option Conan `tracy` (désactivée par défaut, `OWL_PROFILER=none`) ; le backend
                 « chrome » formate, verrouille et flushe à chaque scope, son coût domine les zones courtes.
Preuve         : debug/Profiler.cpp:110-126 ; conanfile.py:55, 163-164, 192 ; CMakeLists.txt:26.
Recommandation : corriger, effort S (tampon et flush en fin de session pour « chrome »).
```

```text
ID             : D-14 — étrangeté, basse, confirmé (physique)
Constat        : deux mécanismes de déclencheurs coexistent : les capteurs Jolt de `PhysicBody` et le composant `Trigger`
                 testé en AABB maison ; une tilemap reste une boîte par cellule collidable (un seul corps composé, sans
                 fusion) ; le joueur voxel garde sa propre collision AABB.
Preuve         : scene/systems/EngineSystems.cpp:82-91 ; physics/PhysicCommand.cpp:806-842 ;
                 data/voxel/VoxelCollision.cpp.
Recommandation : surveiller, effort M (faire de `Trigger` un capteur, fusionner les cellules en rectangles).
```

```text
ID             : D-15 — étrangeté, basse, confirmé (couplage)
Constat        : physique et scripts sont désormais portés par la scène ; restent globaux la configuration et l'état de
                 frame de `RendererVoxel`, la scène inspectée de l'inspecteur et le périphérique OpenAL (légitime).
                 Deux scènes ne peuvent pas avoir deux réglages voxel.
Preuve         : physics/PhysicCommand.cpp:1205-1206 ; script/LuaBindings.h:150-166 ; renderer/RendererVoxel.cpp:110-127 ;
                 gui/component/render.cpp:336-339 ; sound/openal/SoundAPI.cpp:35.
Recommandation : surveiller, effort S (config voxel portée par la scène ou le cache).
```

```text
ID             : D-24 — faiblesse, basse, confirmé (tâches)
Constat        : la file de tâches reste FIFO : si l'action lève, la terminaison s'exécute comme un succès, attente
                 active par `yield`, au moins une frame de latence, ni priorité ni annulation (chunks hors rayon générés
                 puis déchargés). `parallelFor` est, lui, un vrai découpage parallèle.
Preuve         : core/task/Task.cpp:29-36 ; core/task/Scheduler.cpp:61-75.
Recommandation : corriger, effort S (propager les exceptions, priorité, annulation ; garder Taskflow).
```

```text
ID             : D-27 — faiblesse, basse, confirmé (voxel)
Constat        : empreinte non optimisée : 8 KiB d'identifiants et 8 KiB de métadonnées denses par chunk même vide,
                 vertex CPU de 44 octets. Mesuré (2026-10-20) : 417 vertex et ~26 KiB GPU par chunk ; monde éditeur r=8,
                 h=4 (2 601 chunks) ≈ 42 MiB CPU. Suffisant aujourd'hui, frein au streaming lointain.
Preuve         : data/voxel/Chunk.cpp:39-42 ; Chunk.h:266-270 ; Block.h:24, 106 ; ChunkMesher.h:32-45.
Recommandation : surveiller, effort M (palette ou RLE, chunk vide sans tableau, vertex compactés).
```

```text
ID             : D-28 — force, basse, confirmé (.owlpack)
Constat        : format simple, versionné, compressé zstd, parseur de TOC défensif (magic, version, bornes, chemins `..`
                 rejetés, erreurs typées, garde contre les collisions de hash), lecture sous mutex, fuzzé.
Preuve         : PackReader.cpp:226-236, 290-296 ; PackFormat.cpp:33-50 ; io_tests/packHardening_test.cpp ;
                 test/fuzz/pack_reader_fuzzer.cpp.
Recommandation : garder
```

```text
ID             : D-29 — force, basse, confirmé (raycast)
Constat        : raycaster GPU (DDA en compute Slang), repli CPU, grille renvoyée seulement en cas de changement.
Preuve         : renderer/utils/RaycastDDAPass.cpp ; renderer/RendererRaycast.cpp.
Recommandation : garder
```

```text
ID             : D-31 — faiblesse, basse, confirmé (tâches, physique)
Constat        : chaque scène simulée crée son propre `tf::Executor` de solveur (jusqu'à la moitié des cœurs) à côté de
                 celui du `Scheduler` (tous les cœurs) : sursouscription de 1,5× les cœurs, davantage avec plusieurs
                 scènes. Impact non mesuré.
Preuve         : core/task/SchedulerImpl.h:63 ; physics/SolverTaskPool.cpp:21 ; physics/PhysicCommand.cpp:1212-1213 ;
                 scene/PhysicsSettings.cpp:33-39.
Recommandation : surveiller, effort S (mesurer ; partager un exécuteur en gardant l'ordre déterministe).
```

```text
ID             : D-33 — force, basse, confirmé (IA)
Constat        : steering sur grille de voisinage triée par UUID (ordre déterministe), arbres de comportement avec
                 blackboard, navmesh 3D cuite (A*, funnel, ORCA, liens), chaque brique testée et jouée par un scénario.
                 Le steering réalloue ses tampons à chaque tick.
Preuve         : scene/systems/SteeringSystems.cpp:136-180, 296, 318 ; scene/ai/BehaviourTree*.cpp ; scene/nav/*.cpp ;
                 scene_tests/Steering_test.cpp, BehaviourTree_test.cpp, NavMesh_test.cpp, NavAgent_test.cpp.
Recommandation : garder (réutiliser les tampons du steering entre ticks).
```
