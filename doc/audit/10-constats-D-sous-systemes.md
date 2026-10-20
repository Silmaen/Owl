# Audit — axe D : sous-systèmes

> Statut : revu le 2026-10-20 sur `main` 4e1a6a10. Lecture seule ; mesures faites avec une sonde jetable hors dépôt,
> compilée dans l'image Docker contre `libOwlEngine.so` (clang 22, `-O2`, i9-13950HX), 3 répétitions : chiffres
> indicatifs. **confirmé** = vu dans le code et/ou reproduit ; **plausible** = déduit du code, non reproduit.
> Les comparaisons externes sont de l'opinion informée (`30-etat-de-l-art.md`).

## Résumé

- Briques tierces à jour et cachées derrière des pimpl (D-19, D-21, D-22, D-28, D-29) ; voxel solide (D-20).
- `.owlpack` : XOR sans effet réel, double extraction en clair, `PackReader` non thread-safe lu depuis un worker
  (D-10, D-09).
- Intégration incomplète : volumes par catégorie sans effet, pas de joints, Box2D réduit à des boîtes (D-07, D-14).
- Son naïf : décodage intégral, un `alGenSources` par lecture (D-13).
- Singletons statiques liés à la scène active (D-15).
- Outillage : tracker mémoire à mutex global, profiler « chrome » qui flushe à chaque scope (D-11, D-12).
- 19 constats : 0 haut, 8 moyens, 11 bas ; 6 forces, 7 faiblesses, 2 risques, 4 étrangetés.

## Constats — gravité moyenne

```text
ID             : D-10 — étrangeté, moyenne, confirmé (.owlpack)
Constat        : l'« obfuscation » XOR (8 bits) est annulée par l'extraction intégrale en clair au démarrage ; les
                 chargeurs « pack-aware » font doublon avec une seconde extraction dans `/tmp/owl_pack_cache` (partagé
                 entre utilisateurs, jamais nettoyé, noms en collision). Un pack modifié sans changement de taille laisse
                 des assets périmés ; l'écriture `ofstream` échoue en silence si le dossier appartient à un autre
                 utilisateur.
Preuve         : PackFormat.cpp:39-44 ; Application.cpp:63-91 ; Texture.cpp:189-195 ; FontLibrary.cpp:61-68 ;
                 architecture.md:305.
Recommandation : remplacer, effort M (lecture en mémoire ou mmap, somme de contrôle par entrée ; garder le XOR en
                 anti-grep).
```

```text
ID             : D-13 — faiblesse, moyenne, confirmé (son)
Constat        : pipeline naïf : un `alGenSources` par lecture (recyclage seulement sur `AL_OUT_OF_MEMORY`), fichier
                 entier décodé, pas de streaming (musique comprise), `sound.play` décode sur le thread principal,
                 scan disque répété pour un son absent, pas de bus ni de reprise sur perte de périphérique.
Preuve         : openal/SoundAPI.cpp:34-77 ; openal/SoundData.cpp:204-216 ; LuaBindings.cpp:203-205 ;
                 AssetLibrary.h:221-268.
Recommandation : corriger, effort M (préchargement, pool de sources, streaming musique, bus ; évaluer miniaudio).
```

```text
ID             : D-14 — faiblesse, moyenne, confirmé (physique)
Constat        : Box2D v3 sert de solveur de boîtes : une boîte par cellule de tilemap, pas de chaînes, triggers en AABB
                 maison au lieu de capteurs, gravité codée en dur, ni `workerCount` ni `enqueueTask`, aucun filtrage de
                 collision. Le joueur voxel a sa propre collision AABB.
Preuve         : PhysicCommand.cpp ; Scene.cpp ; VoxelCollision.cpp.
Recommandation : corriger, effort M (chaînes, capteurs, `enqueueTask` vers Taskflow, gravité par scène).
```

```text
ID             : D-15 — étrangeté, moyenne, confirmé (couplage)
Constat        : plusieurs sous-systèmes sont des singletons statiques liés à la scène active : une seule scène simulée
                 par processus (pas de prévisualisation de prefab en Play, pas de tests parallèles), dépendances
                 invisibles dans les signatures.
Preuve         : ScriptEngine.cpp:26 ; LuaBindings.cpp:30 ; RendererVoxel.cpp:43 ; openal/SoundAPI.cpp:34 ;
                 glfw/Input.cpp:26.
Recommandation : surveiller, effort L (`PhysicsWorld`, `ScriptContext` portés par `Scene` ; avec l'axe A).
```

```text
ID             : D-19 — force, moyenne, confirmé (physique)
Constat        : Box2D 3.1.1 : bon choix de version, usage idiomatique, frontière publique propre (pimpl, aucun en-tête
                 exposé), conversion vers l'espace local avec hiérarchie traitée et testée. Les faiblesses (D-14) viennent
                 de l'intégration.
Preuve         : conanfile.py:150 ; PhysicCommand.cpp ; test/physics_tests.
Recommandation : garder
```

```text
ID             : D-20 — force, moyenne, confirmé (voxel)
Constat        : greedy meshing préservant l'AO (correction d'anisotropie), tri arrière → avant du transparent, culling
                 par frustum, génération déterministe sans état partagé, DDA Amanatides & Woo. Mesuré : génération
                 63–107 µs, maillage 350–560 µs par chunk 16³.
Preuve         : ChunkMesher.cpp ; TerrainGenerator.cpp ; VoxelRaycast.cpp ; test/voxel_tests.
Recommandation : garder
```

```text
ID             : D-21 — force, moyenne, confirmé (tâches)
Constat        : le contrat « Scheduler main-thread-only, workers sans état partagé » est respecté : décodage de texture et
                 maillage voxel sur worker, résultat appliqué sur le thread principal, Taskflow caché derrière un pimpl.
Preuve         : renderer/gpu/Texture.cpp ; Scene.cpp ; SchedulerImpl.h.
Recommandation : garder
```

```text
ID             : D-22 — force, moyenne, confirmé (script)
Constat        : Lua sobre et robuste : appels sous `lua_pcall`, une `lua_State` par instance, requêtes différées
                 (chargement de scène, sauvegarde, destruction). Mesuré (1 000 instances) : création 26–32 µs et
                 24–25 KiB ; `on_update` vide 80–111 ns, avec `get_position` + `set_position` 264–317 ns, soit
                 0,1–0,3 ms par frame.
Preuve         : LuaEngine.cpp ; ScriptInstance.cpp:18-25 ; LuaBindings.cpp.
Recommandation : garder (vérifier D-16)
```

## Constats — gravité basse

```text
ID             : D-07 — étrangeté, basse, confirmé (son, physique)
Constat        : deux fonctions documentées n'existent pas : `volume_music` / `volume_sfx` sont déclarés sans effet
                 (seul `volume_master` est appliqué, `SceneSound::category` jamais lu) et aucun joint Box2D n'est exposé.
Preuve         : SettingsManager.cpp:216 ; gui/component/render.cpp ; architecture.md:214.
Recommandation : corriger, effort S (doc) ou M (bus par catégorie, joints).
```

```text
ID             : D-09 — risque, basse, plausible (tâches / .owlpack)
Constat        : le `PackReader` partagé (`mutable std::ifstream`, seek + read en méthode const) est lu depuis le worker de
                 téléportation sans verrou. Aucune lecture concurrente identifiée dans la fenêtre : course théorique.
Preuve         : RunnerLayer.cpp:406-418, 441-452 ; Application.cpp:531-535 ; PackReader.h:145-146.
Recommandation : corriger, effort S (mutex ou lecture positionnelle ; run TSan sur la transition).
```

```text
ID             : D-11 — faiblesse, basse, confirmé (mémoire)
Constat        : tracker (`OWL_ENABLE_MEMORY_TRACKER`, désactivé par défaut) : mutex global par `new`/`delete`, ensemble
                 vivant doublé, `globals()` lu sans verrou (data race), `operator new` renvoie `nullptr` au lieu de lever
                 `bad_alloc`. Coût à mesurer.
Preuve         : debug/Tracker.cpp.
Recommandation : remplacer, effort S/M (Tracy ou heaptrack).
```

```text
ID             : D-12 — faiblesse, basse, confirmé (profilage)
Constat        : Tracy est sélectionnable derrière `OWL_PROFILE_*` mais le défaut est `none` ; le backend « chrome »
                 formate et flushe à chaque scope, son coût domine les zones courtes.
Preuve         : debug/Profiler.cpp ; CMakeLists.txt:26.
Recommandation : corriger, effort S (Tracy dans l'image de build).
```

```text
ID             : D-16 — risque, basse, plausible (script)
Constat        : Lua est compilé en C : une erreur levée par `luaL_check*` fait un `longjmp` à travers des frames C++ qui
                 saute les destructeurs (indéfini si un objet non trivial est vivant). Les exceptions C++ sont déjà
                 interceptées par `callGuarded`. Liaisons surtout triviales, chemin non revérifié.
Preuve         : core/external/lua.h:14-18 ; LuaEngine.cpp:535-546 ; LuaBindings.cpp.
Recommandation : corriger, effort S (auditer les liaisons `luaL_check*` ; sinon Lua en C++ ou sol2).
```

```text
ID             : D-24 — faiblesse, basse, confirmé (tâches)
Constat        : Taskflow réduit à une file FIFO : si l'action lève, la terminaison s'exécute comme un succès
                 (`broken_promise` avalé), attente active par `yield`, au moins une frame de latence, ni priorité ni
                 annulation (chunks hors rayon générés puis déchargés).
Preuve         : core/task/Scheduler.cpp:28, 42-56, 92-104 ; Task.cpp:31-33 ; Scene.cpp.
Recommandation : corriger, effort S (propager les exceptions, priorité, annulation ; garder Taskflow).
```

```text
ID             : D-25 — faiblesse, basse, confirmé (entrées)
Constat        : les manettes ne sont exposées aux scripts qu'en événements (pas d'état interrogeable) et les codes GLFW
                 sont exposés tels quels.
Preuve         : public/input/Input.h ; .claude/rules/script.md.
Recommandation : corriger, effort S/M (états de manette dans la table d'actions).
```

```text
ID             : D-26 — étrangeté, basse, confirmé (script)
Constat        : `scene.find_entity` est en O(n) sans avertissement ; l'inspecteur exécute le script au niveau global sans
                 liaisons ni limite de temps ; un `on_update` en erreur relogue à chaque frame sans désactivation.
Preuve         : LuaBindings.cpp:240-256 ; gui/component/render.cpp:586-589 ; LuaEngine.cpp.
Recommandation : corriger, effort S.
```

```text
ID             : D-27 — faiblesse, basse, confirmé (voxel)
Constat        : empreinte non optimisée : 16 KiB denses par chunk même vide, vertex CPU 40 octets / GPU 56 octets,
                 copie de chunk au lieu d'un move. Mesuré : 417 vertex et ~26 KiB GPU par chunk ; monde éditeur r=8, h=4
                 (2 601 chunks) ≈ 42 MiB CPU. Suffisant aujourd'hui, frein au streaming lointain.
Preuve         : Chunk.cpp:27-30 ; ChunkMesher.h:28-39 ; Renderer3D.h:32-42 ; RendererVoxel.cpp:77-86 ; Scene.cpp:1198.
Recommandation : surveiller, effort M (palette ou RLE, vertex compactés).
```

```text
ID             : D-28 — force, basse, confirmé (.owlpack)
Constat        : format simple, versionné, compressé zstd, parseur de TOC défensif (magic, version, bornes, chemins `..`
                 rejetés, erreurs typées, garde contre les collisions de hash).
Preuve         : PackReader.cpp ; PackFormat.cpp ; test/io_tests.
Recommandation : garder
```

```text
ID             : D-29 — force, basse, confirmé (raycast)
Constat        : raycaster GPU (DDA en compute Slang), repli CPU, grille renvoyée seulement en cas de changement.
Preuve         : renderer/utils/RaycastDDAPass.cpp ; RendererRaycast.cpp.
Recommandation : garder
```
