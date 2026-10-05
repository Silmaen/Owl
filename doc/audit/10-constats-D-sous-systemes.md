# Audit — axe D : sous-systèmes

> Statut : première passe, 2026-10-05, branche `Feature/Bench` (commit `45e27892`). Lecture seule sur le
> code ; mesures faites avec une sonde jetable (`scratchpad/axeD/probe.cpp`, hors dépôt) compilée dans
> l'image Docker contre `output/build/linux-clang-release/bin/libOwlEngine.so` (clang 22, `-O2`), machine
> i9-13950HX 32 threads. Chaque chiffre est indicatif (3 répétitions, plage donnée) ; les campagnes
> rigoureuses relèvent de `20-mesures.md`.
>
> Légende des statuts : **confirmé** = vu dans le code et/ou reproduit ; **plausible** = déduit du code,
> non reproduit. « Mesuré » est précisé dans la preuve. Les comparaisons externes sont de l'**opinion
> informée** tant que l'agent état de l'art ne les a pas sourcées et datées (`30-etat-de-l-art.md`).

## Résumé

1. Les briques tierces sont bien choisies et à jour (Box2D 3.1.1, Lua 5.5.0, Taskflow 4.0.0, OpenAL Soft 1.24.3, zstd 1.5.7) et bien cachées derrière des pimpl.
2. L'intégration, elle, est mince : beaucoup de fonctions documentées n'existent pas (`on_collision`, `other_id`, volumes par catégorie, joints Box2D).
3. **Use-after-free reproduit** : un script qui se détruit lui-même (`coin.lua` du sample) libère sa propre `lua_State` en cours d'exécution (valgrind : 1 896 erreurs).
4. **`.owlpack` dangereux sur entrée malveillante** : extraction au démarrage sans validation de chemin (zip-slip), allocations dimensionnées par l'en-tête (`std::bad_alloc` non rattrapé, mesuré).
5. **Le runner ne maille jamais les mondes voxel** : seul l'éditeur appelle `prepareVoxelRenderData()`.
6. Physique : pas variable non borné, corps Box2D jamais détruits, aucun événement de contact, solveur mono-thread.
7. Lua : isolation et coût corrects (80–317 ns par `on_update`, 24 KiB par instance, mesuré) mais sandbox poreux (`load` + bytecode, ni quota CPU ni mémoire).
8. Voxel : algorithmes solides (greedy meshing avec AO, génération async pure) mais maillage sur le thread principal, sans budget ; les voisins ne sont pas remaillés.
9. Outillage maison hérité de Hazel (profiler JSON qui flush à chaque scope, tracker mémoire à mutex global) : coûteux, donc peu utilisable en charge ; Tracy le remplacerait.
10. 29 constats : 4 hauts, 17 moyens, 8 bas ; 6 forces (après vérification croisée : D-09 abaissé de moyenne à basse, aucun constat réfuté).

| Gravité | Force | Faiblesse | Risque | Étrangeté | Total |
|---------|-------|-----------|--------|-----------|-------|
| Haute   | 0     | 2         | 2      | 0         | 4     |
| Moyenne | 4     | 8         | 2      | 3         | 17    |
| Basse   | 2     | 4         | 1      | 1         | 8     |
| Total   | 6     | 14        | 5      | 4         | 29    |

## Constats — gravité haute

```text
ID             : D-01
Axe            : Sous-systèmes (script / scène)
Nature         : risque
Gravité        : haute
Preuve         : source/owl/private/script/LuaBindings.cpp:270-278 (destroy_entity immédiat) ;
                 source/owl/private/scene/Scene.cpp:315-343 (registry.destroy, pas de file différée) ;
                 source/owl/private/scene/SceneTrigger.cpp:20-26,82-87 (callback Lua appelé depuis la boucle
                 des triggers) ; sample_project/scripts/coin.lua:32 (`scene.destroy_entity(entity_id)`).
                 Mesuré : sonde `probe uaf` sous valgrind → "Invalid read ... luaD_poscall ... Address is
                 inside a block free'd by LuaEngine::~LuaEngine ← Scene::destroyEntity ←
                 luaSceneDestroyEntity", ERROR SUMMARY: 1896 errors from 93 contexts.
Constat        : `scene.destroy_entity` détruit l'entité tout de suite, y compris le composant `LuaScript` qui
                 possède la `lua_State` en train d'exécuter l'appel : le VM continue sur de la mémoire libérée.
                 Côté C++, `Scene.cpp:658-684` réutilise ensuite `trigger`, une référence vers le composant
                 détruit. Le sample (pièces du platformer) déclenche le cas à chaque ramassage.
Comparaison    : Unity (`Destroy` effectif en fin de frame), Godot (`queue_free`), Bevy (`Commands`) diffèrent
                 tous la destruction.
Recommandation : corriger, effort S (file `pendingDestroy` drainée après `onUpdateRuntime`, même patron que
                 `teleportRequest` / `saveLoadRequest`) + test de non-régression sous ASan/valgrind.
Statut         : confirmé
Vérification   : reproduit le 2026-10-05 (sonde recompilée, `docker/run.sh --perf` + valgrind) : 1 896 erreurs, blocs
                 libérés par `LuaEngine::~LuaEngine` ; `uniq<ScriptInstance>` du composant (LuaScript.h:73) détruit
                 dans `destroyEntity`, puis `trigger.setOverlapping` (Scene.cpp:684) sur la référence morte.
```

```text
ID             : D-02
Axe            : Sous-systèmes (.owlpack)
Nature         : risque
Gravité        : haute
Preuve         : source/owl/private/app/Application.cpp:63-91 (chaque entrée du TOC est écrite dans
                 `assetsDir / entryPath` sans normalisation : `../x` ou un chemin absolu sortent du dossier) ;
                 source/owl/private/data/assets/pack/PackReader.cpp:55,123 et PackFormat.cpp:61 (vecteurs
                 dimensionnés par `tocSize`, `dataSize`, `originalSize` lus du fichier, sans borne par la taille
                 du fichier) ; Application.cpp:516 (`open` sans try/catch).
                 Mesuré : en-tête forgé `tocSize = 2^62` → `std::bad_alloc` sort de `tryOpen` ;
                 `tocSize = 4 GiB` → 4 GiB alloués et remplis de zéros avant l'échec de lecture.
Constat        : un `.owlpack` malveillant (mod, téléchargement) peut écrire des fichiers arbitraires avec les
                 droits du joueur (zip-slip), ou faire planter le jeu à l'ouverture. Le parseur du TOC est par
                 ailleurs borné (voir D-28) : ce sont les tailles et les chemins qui ne sont pas validés.
Comparaison    : correctifs zip-slip classiques (rejet des `..` et des chemins absolus, résolution canonique et
                 contrôle de préfixe) ; les formats de pak (UE `.pak`, Godot `.pck`) sont lus en place, sans
                 extraction.
Recommandation : corriger, effort S (valider les chemins, borner les tailles par `file_size`, attraper les
                 exceptions dans `tryOpen`) ; à terme ne plus extraire (D-10). Ajouter un fuzzer sur
                 `deserializeToc` / `tryOpen`.
Statut         : confirmé
Vérification   : `tocSize = 2^62` refait : `std::bad_alloc` sort bien de `tryOpen` ; `assetsDir / entryPath`
                 (Application.cpp:72) sans contrôle, un chemin absolu remplace même `assetsDir` (sémantique de
                 `operator/`).
```

```text
ID             : D-03
Axe            : Sous-systèmes (voxel)
Nature         : faiblesse
Gravité        : haute
Preuve         : source/owlnest/sources/panel/Viewport.cpp:219, seul appelant de `Scene::prepareVoxelRenderData()`
                 (grep exhaustif de `prepareVoxelRenderData` / `prepareWorld` sur `source/`) ;
                 source/owl/private/renderer/RendererVoxel.cpp:184-186 (`drawVoxelWorld` sort si le cache de
                 maillages est vide) ; source/owl/private/renderer/RendererVoxelLayer.cpp:39 (`onRender` vide) ;
                 aucune occurrence de « voxel » dans source/owlnest/runner/.
Constat        : dans OwlRunner (le jeu exporté), aucun chunk n'est jamais maillé, donc rien ne s'affiche ; le
                 voxel, livré comme fonction phare de v0.2.1, ne fonctionnerait que dans l'éditeur.
Comparaison    : —
Recommandation : corriger, effort S (préparer les données voxel dans le pipeline de rendu de la scène, pas dans
                 le panneau Viewport) ; vérifier à l'exécution sur `sample_project/scenes/voxel_terrain.owl`.
Statut         : confirmé
Vérification   : seul appelant `Viewport.cpp:219` (grep refait) et `drawVoxelWorld` sort sans cache
                 (RendererVoxel.cpp:184-186) ; nuance : `RunnerLayer.h:42` mentionne « future voxel », ce qui
                 corrobore l'absence de support dans le runner.
```

```text
ID             : D-04
Axe            : Sous-systèmes (physique)
Nature         : faiblesse
Gravité        : haute
Preuve         : source/owl/private/physics/PhysicCommand.cpp (aucun `b2DestroyBody` : un corps n'est libéré
                 qu'avec le monde, l.214) ; l.68-103 (les corps ne sont créés qu'à `init()`) ; l.234-235, 276,
                 310, 339, 356, 368 (`m_impl->bodies[bodyId]` avec `operator[]`) ; l.212-218 (`destroy()` sans
                 garde sur `m_impl`).
Constat        : une entité physique détruite en Play (par Lua, par exemple) laisse un collider fantôme dans le
                 monde Box2D. Un `PhysicBody` ajouté après le démarrage garde `bodyId == 0` : `operator[]` insère
                 alors un `b2BodyId` nul, passé à `b2Body_GetPosition` (assert en debug, lecture hors bornes en
                 release).
Comparaison    : les intégrations EnTT usuelles branchent `on_construct` / `on_destroy` du composant physique
                 pour créer et détruire le corps.
Recommandation : corriger, effort S/M (hooks EnTT `on_construct`/`on_destroy` sur `PhysicBody`, `find()` au lieu
                 de `operator[]`).
Statut         : confirmé (fuite) ; plausible (chemin bodyId nul)
Vérification   : aucun `b2DestroyBody` (grep) et `nextId` part de 1 (PhysicCommand.cpp:44) : un `bodyId` resté à 0
                 n'a pas d'entrée, `operator[]` insère bien un `b2BodyId` nul ; ajout de `PhysicBody` en Play non
                 exposé à Lua, donc ce second chemin passe par l'éditeur.
```

## Constats — gravité moyenne

```text
ID             : D-05
Axe            : Sous-systèmes (physique)
Nature         : faiblesse
Gravité        : moyenne
Preuve         : source/owl/private/physics/PhysicCommand.cpp:228 (`b2World_Step(dt_frame, 4)`) ;
                 source/owl/public/core/Timestep.h:50-57 (delta brut, aucun plafond) ;
                 source/owl/private/scene/Scene.cpp:621.
Constat        : la physique avance au pas de la frame, sans accumulateur ni borne : le résultat dépend du
                 framerate, n'est pas déterministe, et une frame longue (chargement synchrone, compilation Slang,
                 fenêtre déplacée) produit un pas énorme (tunneling, explosions de contacts).
Comparaison    : la documentation Box2D recommande un pas fixe ; Godot (`physics_ticks_per_second`) et Unity
                 (`FixedUpdate`) découplent physique et rendu.
Recommandation : corriger, effort S (pas fixe 60 Hz + accumulateur + plafond + interpolation des transforms).
Statut         : confirmé
Vérification   : `b2World_Step(worldId, iTimestep.getSeconds(), 4)` (PhysicCommand.cpp:228) et `m_stepper.update()`
                 brut (Application.cpp:338), aucun plafond ni accumulateur trouvé.
```

```text
ID             : D-06
Axe            : Sous-systèmes (script)
Nature         : risque
Gravité        : moyenne
Preuve         : source/owl/private/script/LuaEngine.cpp:17-59 (`luaL_newstate`, allocateur par défaut, ni
                 `lua_sethook` ni limite mémoire : grep vide) ; l.80 et 96 (`luaL_dofile` / `luaL_loadbuffer` en
                 mode "bt") ; doc/pages/scripting.md:15-16 et 469-482 (« sandboxed … for security »).
                 Mesuré (sonde `probe sandbox`) : `load`, `string.dump`, `collectgarbage`, `setmetatable`,
                 `rawset` présents ; `load(string.dump(f))` exécuté avec succès (bytecode accepté) ;
                 `require`, `package` et `debug` absents.
Constat        : le sandbox retire bien l'accès au système, mais accepte le bytecode (le manuel Lua prévient que
                 du bytecode forgé peut faire planter l'interpréteur) et n'a aucune limite : `while true do end`
                 gèle le jeu ou l'éditeur (`extractProperties` exécute le script), `string.rep` peut épuiser la
                 mémoire. Sans gravité tant que les scripts sont écrits par l'auteur du jeu, bloquant pour le
                 modding.
Comparaison    : Luau (Roblox) est conçu pour du code non fiable (interruptions, pas de bytecode côté
                 utilisateur) ; en Lua stock, on passe le mode "t", un allocateur à quota et un hook d'instructions.
Recommandation : corriger, effort S (mode "t", retirer `string.dump`, quota mémoire via `lua_newstate`, hook
                 d'instructions) ; corriger la doc.
Statut         : confirmé
Vérification   : `luaL_newstate`, ni `lua_sethook` ni allocateur (grep vide) ; seuls `dofile`/`loadfile` retirés
                 (LuaEngine.cpp:51-55), `load` et `string.dump` restent.
```

```text
ID             : D-07
Axe            : Sous-systèmes (script, son, physique)
Nature         : étrangeté
Gravité        : moyenne
Preuve         : `on_collision` : doc/pages/scripting.md:12,106,118,528 ; source/owl/private/script/ScriptInstance.cpp:93-97
                 n'a aucun appelant (grep) et Box2D n'émet aucun événement (pas de `b2World_GetContactEvents`).
                 `other_id` des triggers : scripting.md:365-380 contre SceneTrigger.cpp:20-26 (`iArg` ignoré,
                 `callFunction` sans argument). Volumes par catégorie : CLAUDE.md (« volume_music, volume_sfx —
                 auto-applied ») et gui/component/render.cpp:558 (« per-category volume mixing ») contre
                 SettingsManager.cpp:193-195 (seul `volume_master` est appliqué), `SceneSound::category` n'étant
                 jamais lu à l'exécution. Joints : architecture.md:214 (« use Box2D joints ») alors qu'aucune API
                 n'expose de joint (grep `Joint` vide).
Constat        : quatre fonctions documentées, ou présentées dans l'inspecteur, n'existent pas. Un concepteur qui
                 suit la doc écrit des callbacks jamais appelés ou reçoit `nil`.
Comparaison    : —
Recommandation : corriger, effort S pour la doc ; M pour implémenter (événements de contact Box2D v3 →
                 `on_collision`, bus de volume par catégorie).
Statut         : confirmé
Vérification   : `ScriptInstance::onCollision` sans appelant, aucun `ContactEvents` (grep) ; `dispatchLuaCallback`
                 ignore `iArg` ; `applyBuiltins` n'applique que `volume_master` (SettingsManager.cpp:193-195) ; aucun
                 `Joint` dans `source/`.
```

```text
ID             : D-08
Axe            : Sous-systèmes (voxel)
Nature         : faiblesse
Gravité        : moyenne
Preuve         : source/owl/private/renderer/RendererVoxel.cpp:161-171 (tous les chunks sales sont maillés et
                 envoyés au GPU dans la frame, sur le thread principal, sans budget) ; Scene.cpp:1189-1200 (un
                 chunk streamé n'invalide que lui-même, alors que `VoxelWorld::markNeighborChunksDirty` existe,
                 VoxelWorld.cpp:50) ; Chunk.cpp:76-78 (`isEmpty` parcourt les 4 096 blocs) appelé deux fois par
                 chunk et par frame (RendererVoxel.cpp:163,203) ; Scene.cpp:1240 et RendererVoxel.cpp:161,201
                 (`chunkCoordinates()` alloue un vecteur à chaque appel, plusieurs fois par frame).
                 Mesuré : `meshByKind` = 350–560 µs par chunk (moyenne sur 75 chunks dont 50 non vides) ;
                 `isEmpty` sur un chunk d'air = 1,75–2,1 µs.
Constat        : pendant le streaming, jusqu'à 16 chunks par frame (Scene.cpp:1202) peuvent arriver et être
                 maillés dans la frame, soit environ 5 à 9 ms de pic (estimation à partir des mesures). Les faces
                 et l'AO des voisins déjà maillés ne sont pas recalculés (faces cachées en trop, coutures d'AO).
                 Le seul test `isEmpty` coûte jusqu'à ~9 ms par frame dans l'éditeur (2 601 chunks à r=8, h=4,
                 pire cas tout en air) : estimation à mesurer (M3).
Comparaison    : les moteurs voxel (Minecraft, Vintage Story, les démos Godot voxel) maillent sur des workers
                 avec une file priorisée par distance et un budget d'upload par frame.
Recommandation : corriger, effort M (maillage en tâche Taskflow : il est pur, seul l'upload reste sur le thread
                 principal ; budget par frame ; invalider les voisins ; drapeau `empty` tenu à jour par
                 `setBlock` / `fill`).
Statut         : confirmé (code) ; pics de frame estimés, à mesurer
Vérification   : maillage synchrone de tous les chunks sales dans `prepareWorld` (RendererVoxel.cpp:160-170), voisins
                 non invalidés à l'installation (Scene.cpp:1189-1200) ; le budget de 16 porte sur la mise en file de
                 génération, pas sur le maillage, donc le pic peut dépasser 16 chunks si plusieurs frames de
                 résultats s'accumulent.
```

```text
ID             : D-09
Axe            : Sous-systèmes (tâches / .owlpack)
Nature         : risque
Gravité        : basse
Preuve         : source/owlnest/runner/RunnerLayer.cpp:406-418 (le worker de téléportation appelle
                 `app.loadFromPack`) ; source/owl/private/app/Application.cpp:531-535 (aucun verrou) ;
                 source/owl/public/data/assets/pack/PackReader.h:145-146 (`mutable std::ifstream`, seek + read dans
                 une méthode const).
Constat        : la règle « Scheduler main-thread-only » est respectée, mais le `PackReader`, partagé et non
                 thread-safe, est lu depuis un worker : toute lecture concurrente depuis le thread principal
                 pendant la transition (texture, script, settings) est une data race sur le flux.
Comparaison    : lecture positionnelle (`pread`) ou mmap, sans curseur partagé.
Recommandation : corriger, effort S (mutex dans `readEntry` ou lecture positionnelle) ; ajouter un run TSan sur
                 la transition.
Statut         : plausible
Vérification   : le worker ne fait que lire et parser (RunnerLayer.cpp:441-452) ; textures et polices sont chargées
                 par `applyParsed` sur le thread principal après `ready`, et la scène précédente est arrêtée : aucune
                 lecture concurrente identifiée dans la fenêtre. Course théorique, gravité abaissée de moyenne à
                 basse.
```

```text
ID             : D-10
Axe            : Sous-systèmes (.owlpack)
Nature         : étrangeté
Gravité        : moyenne
Preuve         : source/owl/private/data/assets/pack/PackFormat.cpp:39-44 (XOR, clé `(0xA7 ^ index) + i*37` sur
                 8 bits) ; Application.cpp:68-91 (au démarrage, tout le pack est extrait en clair dans
                 `<cwd>/assets`, et une entrée n'est réécrite que si sa taille change) ; Texture.cpp:189-195 et
                 FontLibrary.cpp:61-68 (seconde extraction dans `/tmp/owl_pack_cache/<nom de fichier>`, dossier
                 partagé entre utilisateurs, jamais nettoyé, noms en collision entre sous-dossiers ;
                 `create_directories` lève une exception si un autre utilisateur possède le dossier).
Constat        : la doc parle honnêtement d'« obfuscation » et non de chiffrement (architecture.md:305), mais
                 l'extraction intégrale en clair au démarrage la rend nulle. Les chargeurs « pack-aware » font
                 doublon avec cette extraction. Un pack modifié sans changement de taille laisse des assets
                 périmés sur disque.
Comparaison    : les paks (UE, Godot `.pck`) sont lus en place (mmap) ; la protection réelle passe par signature
                 et chiffrement, pas par un XOR.
Recommandation : remplacer, effort M (lecture en mémoire ou mmap pour tous les chargeurs, plus d'extraction ;
                 somme de contrôle par entrée, par exemple xxhash) ; garder le XOR comme simple anti-grep.
Statut         : confirmé
Vérification   : XOR 8 bits et extraction intégrale au démarrage vérifiés (PackFormat.cpp:39-44,
                 Application.cpp:63-91). Correction de détail : `create_directories` ne lève pas sur un dossier
                 existant appartenant à un autre utilisateur ; c'est l'écriture `ofstream` qui échoue en silence
                 (Texture.cpp:190-194), avec chargement possible d'un fichier périmé.
```

```text
ID             : D-11
Axe            : Sous-systèmes (mémoire)
Nature         : faiblesse
Gravité        : moyenne
Preuve         : source/owl/private/debug/Tracker.cpp:28,122-138 (mutex global à chaque new/delete) ;
                 l.288-296 (un nœud `std::list` et une entrée `unordered_map` par allocation, en double : état
                 courant et global) ; Application.cpp:374-391 (l'état courant n'est permuté que si
                 `OWL_TRACKER_VERBOSITY >= 3`, sinon il grossit indéfiniment) ; l.65-76 (`std::stack` de suivi
                 global, modifiée sans verrou depuis n'importe quel thread) ; EditorLayer.cpp:938-949 (lit
                 `globals()` sans verrou pendant que les workers allouent) ; l.93-97 (`operator new` renvoie
                 `nullptr` au lieu de lever `bad_alloc`) ; Tracker.h:31-33 (toujours actif en Debug).
Constat        : chaque allocation coûte un verrou global et quatre allocations internes, plus deux nœuds
                 conservés par allocation vivante. Le tracker contient des data races, viole le contrat
                 d'`operator new`, et fausse toutes les mesures faites en Debug. Il ne donne que des totaux.
Comparaison    : heaptrack et Tracy (mode mémoire) offrent piles d'appel et timeline à coût bien moindre, sans
                 remplacer `new`/`delete` dans le binaire livré.
Recommandation : remplacer, effort S/M (désactivé par défaut en Debug ; à terme Tracy ou heaptrack).
Statut         : confirmé (code) ; coût à mesurer (M6)
Vérification   : mutex global, `std::list` + `unordered_map` en double, `globals()` lu sans verrou par l'éditeur,
                 `malloc` nul renvoyé par `operator new` : vérifiés. Exagération : l'état courant ne « grossit » pas
                 indéfiniment, `freeMemory` retire les blocs libérés (Tracker.cpp:298-309), il double seulement
                 l'ensemble vivant.
```

```text
ID             : D-12
Axe            : Sous-systèmes (profilage)
Nature         : faiblesse
Gravité        : moyenne
Preuve         : source/owl/private/debug/Profiler.cpp:46-65 (une `std::stringstream` par scope, écriture puis
                 `flush()` sous mutex) ; l.37 (message « Instrumentor could not open results file », hérité tel
                 quel de l'Instrumentor de Hazel) ; EntryPoint.h:27-36 (session « Runtime » ouverte sur toute la
                 durée, JSON non borné) ; 164 `OWL_PROFILE_*` dans le moteur, dont des chemins par entité et par
                 frame (`LuaEngine::callFunction`, LuaEngine.cpp:120).
Constat        : profiler au format Chrome tracing, désactivé par défaut (`OWL_ENABLE_PROFILING=OFF`). Activé,
                 son coût (formatage et flush système à chaque scope) domine les zones courtes qu'il mesure.
                 L'axe performance de l'audit n'a donc aujourd'hui aucun profiler de frame exploitable.
Comparaison    : Tracy (zones à quelques ns, timeline, mémoire, GPU, verrous) est le standard de fait ; ses
                 macros se substituent directement à `OWL_PROFILE_SCOPE` / `OWL_PROFILE_FUNCTION`.
Recommandation : remplacer, effort S (backend Tracy derrière les macros existantes ; ajouter Tracy à l'image,
                 01-environnement.md §3).
Statut         : confirmé
Vérification   : `stringstream` + `flush()` sous mutex à chaque scope (Profiler.cpp:46-65), message « Instrumentor »
                 l.37 ; 180 occurrences `OWL_PROFILE_` (définitions comprises), même ordre de grandeur que les 164
                 cités.
```

```text
ID             : D-13
Axe            : Sous-systèmes (son)
Nature         : faiblesse
Gravité        : moyenne
Preuve         : source/owl/private/sound/openal/SoundAPI.cpp:110-115 (un `alGenSources` par lecture, sans pool) ;
                 openal/SoundData.cpp:204-216 (fichier entier décodé en mémoire, pas de streaming, musique
                 comprise) ; LuaBindings.cpp:203-205 (`sound.play` charge et décode depuis le disque, sur le
                 thread principal, en pleine frame) ; AssetLibrary.h:221-268 (un asset absent relance un
                 `recursive_directory_iterator` à chaque appel, sans cache négatif) ; SoundAPI.cpp:34-77 (pas de
                 reprise sur perte de périphérique).
Constat        : la base OpenAL Soft + libsndfile tient la route (formats ADPCM gérés, backend null pour les
                 tests), mais le pipeline est naïf : saccade au premier `play`, scan disque répété pour un son
                 manquant, pas de streaming, pas de bus (D-07), limite implicite des sources OpenAL.
Comparaison    : miniaudio (graphe de nœuds, streaming, spatialisation, mono-fichier) ou FMOD / Wwise
                 (propriétaires) couvrent bus, streaming et mixage nativement.
Recommandation : corriger, effort M (préchargement au `onStartRuntime`, pool de sources, streaming de la musique,
                 bus par catégorie) ; évaluer miniaudio au chantier « Audio » de la roadmap.
Statut         : confirmé
Vérification   : `alGenSources` par lecture (SoundAPI.cpp:111) avec recyclage des sources arrêtées dans `frame`
                 (l.212-219), décodage intégral (SoundData.cpp:204-216), `library.load` paresseux dans `sound.play`
                 et `recursive_directory_iterator` sans cache négatif (AssetLibrary.h:250).
```

```text
ID             : D-14
Axe            : Sous-systèmes (physique)
Nature         : faiblesse
Gravité        : moyenne
Preuve         : source/owl/private/physics/PhysicCommand.cpp:96-102 (boîtes uniquement) ; l.139-155 (une boîte
                 par cellule de tilemap collidable, pas de chaîne) ; l.61-63 (`b2DefaultWorldDef`, gravité codée
                 en dur, sans `workerCount` ni `enqueueTask`) ; Scene.cpp:654-686 (triggers en AABB maison, testés
                 contre le seul joueur principal, pas de capteurs Box2D) ; aucun filtrage de collision
                 (catégories, masques).
Constat        : Box2D v3 est utilisé comme un solveur de boîtes. Ses atouts (chaînes contre les collisions
                 fantômes aux jonctions de tuiles, capteurs, événements, solveur multi-thread) sont ignorés. Le
                 joueur voxel a sa propre collision AABB (VoxelCollision.cpp), sans lien avec Box2D.
Comparaison    : Box2D v3 fournit `b2CreateChainShape`, `enableSensorEvents` et un solveur parallèle branché sur
                 un job system via `enqueueTask`.
Recommandation : corriger, effort M (chaînes pour les tilemaps, capteurs à la place des AABB, `enqueueTask` vers
                 Taskflow, gravité par scène).
Statut         : confirmé
Vérification   : seules des `b2MakeBox` / `b2MakeOffsetBox` (grep), une boîte par cellule de tilemap (l.150-152), ni
                 capteur, ni filtre, ni `workerCount` ; gravité codée l.62.
```

```text
ID             : D-15
Axe            : Sous-systèmes (couplage)
Nature         : étrangeté
Gravité        : moyenne
Preuve         : PhysicCommand.cpp:47-48 (`static m_impl`, `static m_scene`) ; ScriptEngine.cpp:26 et
                 LuaBindings.cpp:30 (toutes les liaisons passent par `ScriptEngine::getActiveScene()`) ;
                 RendererVoxel.cpp:43 (`g_Data` global, clé `int` d'entité) ; SoundAPI.cpp:31 (`g_Device`) ;
                 Input glfw/Input.cpp:16-31 (`Application::get()` à chaque requête).
Constat        : chaque sous-système est un singleton statique lié à « la » scène active. Une seule scène peut
                 être simulée par processus (pas de prévisualisation de prefab en Play, pas de tests parallèles,
                 pas de serveur multi-instances), et les dépendances réelles sont invisibles dans les signatures.
Comparaison    : Bevy et Godot (`World3D` / `PhysicsServer` par espace) rattachent le monde physique et les
                 ressources au monde ECS.
Recommandation : surveiller, effort L (contexte par scène : `PhysicsWorld`, `ScriptContext` portés par `Scene`) ;
                 à traiter avec l'axe A.
Statut         : confirmé
Vérification   : états statiques vérifiés : `PhysicCommand::m_impl`/`m_scene`, `ScriptEngine::s_impl`, `g_Data`
                 (RendererVoxel.cpp:43), `g_Device` (SoundAPI.cpp:31).
```

```text
ID             : D-16
Axe            : Sous-systèmes (script)
Nature         : risque
Gravité        : moyenne
Preuve         : mesuré : `nm liblua.a` (paquet DepManager lua 5.5.0) référence `_setjmp` / `_longjmp` et aucun
                 symbole C++ : Lua est compilé en C ; source/owl/private/core/external/lua.h:14-18 (`extern "C"`) ;
                 liaisons C++ qui appellent `luaL_check*` (LuaBindings.cpp, par exemple l.72-74, 294-298) ou des
                 fonctions pouvant lever (`SettingsManager::saveUserSettings`, YAML, l.925).
Constat        : une erreur Lua levée dans une liaison fait un `longjmp` à travers des frames C++ (destructeurs
                 sautés, comportement indéfini dès qu'un objet non trivial est vivant) ; une exception C++ levée
                 dans une liaison traverse les frames C de Lua, et `lua_pcall` ne l'attrape pas (crash probable).
                 Les liaisons actuelles sont prudentes (surtout des types triviaux), mais rien ne protège les
                 suivantes.
Comparaison    : compiler Lua en C++ (`LUAI_THROW` par exceptions) ou utiliser sol2, qui protège chaque liaison
                 avec try/catch et convertit les exceptions en erreurs Lua.
Recommandation : corriger, effort S (recette DepManager de Lua compilée en C++, ou trampoline try/catch générique
                 autour des liaisons).
Statut         : confirmé (risque latent, aucun crash observé)
Vérification   : `nm` de `liblua.a` refait dans Docker : `_setjmp`/`_longjmp` seulement, aucun symbole C++ ; les
                 liaisons appellent `luaL_check*` et des fonctions C++ qui peuvent lever, sans try/catch (grep). Le
                 fait structurel est établi, la conséquence reste latente.
```

```text
ID             : D-17
Axe            : Sous-systèmes (raycast)
Nature         : faiblesse
Gravité        : moyenne
Preuve         : source/owl/private/renderer/RendererRaycast.cpp:261-268 (`getData` synchrone des buffers
                 hit-count et z-buffer à chaque frame, juste après le dispatch DDA) ; l.269-276 (relecture
                 utilisée surtout pour des statistiques).
Constat        : la passe DDA sur GPU (bon choix, voir D-29) est suivie d'une lecture GPU → CPU bloquante à chaque
                 frame : point de synchronisation qui sérialise CPU et GPU. Le z-buffer sert aux sprites ; les
                 statistiques pourraient venir d'un compteur atomique lu une frame plus tard.
Comparaison    : lecture différée (ring de buffers de readback sur N frames) ou garder le test de profondeur des
                 sprites sur le GPU.
Recommandation : corriger, effort M ; mesurer d'abord (M9). À croiser avec l'axe B.
Statut         : plausible (coût non mesuré)
Vérification   : deux `getData` synchrones par frame juste après le dispatch (RendererRaycast.cpp:260-267), le
                 hit-count ne servant qu'aux statistiques ; le coût dépend de l'implémentation de `getData` par
                 backend, non mesuré ici.
```

```text
ID             : D-18
Axe            : Sous-systèmes (polices)
Nature         : faiblesse
Gravité        : moyenne
Preuve         : source/owl/private/data/fonts/Font.cpp:71 (jeu de caractères 0x20–0xFF, Latin-1 seul) ;
                 l.27-48 (`createAndCacheAtlas` ne met rien en cache, paramètres `[[maybe_unused]]`, nom "Test"
                 l.123) ; l.37 (`setThreadCount(8)` en dur, hors Taskflow) ; l.103 (`if constexpr ((false))`) et
                 bloc `#if 0` (code mort hérité de Hazel).
Constat        : « œ », « Œ », « € », les guillemets typographiques et tout ce qui sort du Latin-1 ne s'affichent
                 pas, ce qui est gênant pour un auteur francophone. L'atlas MSDF est régénéré à chaque lancement,
                 sur 8 threads qui ne passent pas par le pool du moteur.
Comparaison    : atlas dynamiques (glyphes ajoutés à la demande) ou charset configurable avec atlas précalculé
                 au cook (msdf-atlas-gen sait l'écrire sur disque).
Recommandation : corriger, effort S/M (charset configurable, cache disque de l'atlas, suppression du code mort).
Statut         : confirmé
Vérification   : charset `{0x20, 0xFF}` (Font.cpp:71), paramètres `[[maybe_unused]]` sans cache, `setThreadCount(8)`,
                 `if constexpr ((false))` et `#if 0` l.124 vérifiés. Nuance : les accents français courants (é, è, à,
                 ç) sont dans Latin-1 ; seuls œ, Œ, € et la typographie manquent.
```

```text
ID             : D-19
Axe            : Sous-systèmes (physique)
Nature         : force
Gravité        : moyenne
Preuve         : depmanager.yml:7-9 (box2d 3.1.1) ; PhysicCommand.cpp:29-46 (pimpl, `b2WorldId` / `b2BodyId`
                 manipulés par valeur) ; public/physics/PhysicCommand.h:11-12 (aucun en-tête Box2D exposé) ;
                 PhysicCommand.cpp:231-258 (resynchronisation monde → local pour les enfants de la hiérarchie) ;
                 test/physics_tests (11 tests, dont `FrameUpdatesChildBodiesInLocalSpace`, `SnapshotRoundTrip`).
Constat        : bon choix de version (v3, API C à handles générationnels) et usage idiomatique de cette API. La
                 frontière publique est propre, et la conversion vers l'espace local avec la hiérarchie est
                 traitée et testée. Les faiblesses (D-04, D-05, D-14) viennent de l'intégration, pas du choix de
                 Box2D.
Comparaison    : Box2D v3 reste la référence 2D ; Jolt serait le pendant 3D (chantier axe K).
Recommandation : garder
Statut         : confirmé
```

```text
ID             : D-20
Axe            : Sous-systèmes (voxel)
Nature         : force
Gravité        : moyenne
Preuve         : source/owl/private/data/voxel/ChunkMesher.cpp:36-43 (masque greedy incluant bloc, orientation et
                 AO 4 coins) ; l.92-114 (AO à 3 voisins par coin) ; l.188-190 (bascule de la diagonale quand l'AO est
                 asymétrique) ; l.257-262 (passes opaque et transparente) ; TerrainGenerator.cpp:15-80 (génération
                 pure, déterministe par graine, bruit fBm maison) ; VoxelRaycast.cpp (DDA Amanatides & Woo) ;
                 test/voxel_tests (85 tests). Mesuré : génération 63–107 µs par chunk 16³, maillage 350–560 µs
                 par chunk.
Constat        : les algorithmes sont les bons et sont proprement implémentés : greedy meshing qui préserve l'AO,
                 correction de l'anisotropie de l'AO, tri arrière → avant du transparent, culling par frustum
                 (RendererVoxel.cpp:214), génération sans état partagé. Le choix voxel est cohérent avec le
                 reste du moteur ; le défaut est l'ordonnancement (D-08), pas l'algorithmique.
Comparaison    : méthode de référence (Lysenko 2012 pour le greedy meshing, AO « 0fps ») ; les moteurs plus
                 poussés ajoutent compression par palette, vertex compactés et LOD.
Recommandation : garder
Statut         : confirmé
```

```text
ID             : D-21
Axe            : Sous-systèmes (tâches)
Nature         : force
Gravité        : moyenne
Preuve         : source/owl/private/renderer/gpu/Texture.cpp:242-291 (octets lus sur le thread principal,
                 décodage sur un worker, upload dans le callback de terminaison sur le thread principal,
                 `weak_ptr` contre la libération en vol) ; Scene.cpp:1183-1236 (génération voxel en tâche,
                 paramètres copiés, résultat poussé sous mutex et drainé sur le thread principal) ;
                 SchedulerImpl.h:19-42 (Taskflow caché derrière un pimpl, aucun type `tf::` dans l'API publique).
Constat        : le contrat « Scheduler main-thread-only, workers sans état partagé » est respecté dans les deux
                 usages moteur : la propriété des données est explicite. C'est le bon socle pour déplacer le
                 maillage voxel et le décodage audio sur des workers.
Comparaison    : même découpe que le job system de Godot 4 (`WorkerThreadPool`) ou les `AsyncComputeTaskPool`
                 de Bevy.
Recommandation : garder
Statut         : confirmé
```

```text
ID             : D-22
Axe            : Sous-systèmes (script)
Nature         : force
Gravité        : moyenne
Preuve         : LuaEngine.cpp:96-107,129-133 (chargement et appels tous protégés par `lua_pcall`, pile nettoyée
                 après erreur) ; ScriptInstance.cpp:18-25 (une `lua_State` par instance) ; LuaBindings.cpp:280-291
                 et 573-594 (chargement de scène et sauvegarde différés par drapeau, sûrs en plein script).
                 Mesuré (1 000 instances, 200 frames, 3 répétitions) : création 26–32 µs et 24,4–25,2 KiB par
                 instance ; `on_update` vide 80–111 ns, avec `transform.get_position` + `set_position`
                 264–317 ns par appel.
Constat        : intégration Lua sobre et robuste aux erreurs de script. Le coût mesuré est acceptable :
                 1 000 entités scriptées coûtent environ 0,1 à 0,3 ms par frame. L'isolation par état est
                 réelle. Le patron « requête différée » existe déjà : il manque seulement pour `destroy_entity`
                 (D-01).
Comparaison    : sol2 apporterait la sûreté des types et des exceptions ; LuaJIT, un JIT mais avec la sémantique
                 de Lua 5.1 ; Luau, sandbox et typage.
Recommandation : garder (corriger D-01, D-06, D-16)
Statut         : confirmé
```

## Constats — gravité basse

```text
ID             : D-23
Axe            : Sous-systèmes (logs)
Nature         : faiblesse
Gravité        : basse
Preuve         : source/owl/public/core/Log.h:127 (le `logClient` templaté appelle `logCore` : le logger « APP »
                 ne reçoit jamais de message formaté) ; Log.h:101-106 (`std::format` évalué avant le filtrage
                 par niveau : un `OWL_CORE_TRACE` désactivé paie quand même le formatage) ; Log.cpp:128,132
                 (`flush_on(niveau courant)` : chaque message affiché déclenche un flush disque synchrone) ;
                 Log.cpp:104-106 (`Owl.log` dans le répertoire courant, tronqué au démarrage ;
                 test/script_tests/Owl.log en est un reliquat, ignoré par git).
Constat        : petits défauts cumulés : séparation moteur / jeu cassée, coût caché des traces désactivées
                 dans les boucles chaudes, I/O synchrones à chaque `log.info` d'un script.
Comparaison    : les macros spdlog (`SPDLOG_TRACE`) filtrent avant de formater ; le sink asynchrone de spdlog
                 existe.
Recommandation : corriger, effort S.
Statut         : confirmé
```

```text
ID             : D-24
Axe            : Sous-systèmes (tâches)
Nature         : faiblesse
Gravité        : basse
Preuve         : private/core/task/ParallelUtils.h (aucun appelant : grep `parallelFor` vide hors de ce fichier) ;
                 Scheduler.cpp:97-100 (si l'action lève, la promesse détruite donne `broken_promise`, et
                 Task.cpp:31-33 exécute la terminaison comme un succès) ; Scheduler.cpp:42-56 (attente active
                 par `yield`) ; Scheduler.cpp:92-104 (lancement seulement dans `Scheduler::frame`, en fin de
                 frame, terminaison au poll suivant : au moins une frame de latence) ; Scene.cpp:1218-1246 (ni
                 priorité ni annulation : des chunks sortis du rayon sont générés puis installés et déchargés).
Constat        : Taskflow est sous-exploité, réduit à une file FIFO de lambdas. Les erreurs des tâches sont
                 avalées, et les utilitaires parallèles, documentés dans CLAUDE.md, sont du code mort.
Comparaison    : enkiTS (priorités, faible surcoût) ou les graphes de dépendances natifs de Taskflow.
Recommandation : corriger, effort S (propager les exceptions vers la terminaison, priorité et annulation) ;
                 garder Taskflow.
Statut         : confirmé
```

```text
ID             : D-25
Axe            : Sous-systèmes (entrées)
Nature         : faiblesse
Gravité        : basse
Preuve         : source/owl/public/input/Input.h:62-70 (`isKeyPressed` et `isMouseButtonPressed` seulement) ;
                 Scene.cpp:1280-1299 et SceneTrigger.cpp:73-79 (détection de front recodée à la main) ;
                 .claude/rules/script.md (« keycodes are raw GLFW integers ») ; aucune manette ni action.
Constat        : entrées en polling brut : pas de « vient d'être pressé », pas d'actions remappables, pas de
                 manette, codes GLFW exposés tels quels aux scripts. Chaque gameplay réinvente la détection de
                 front.
Comparaison    : Godot (InputMap, `is_action_just_pressed`), Unity Input System ; GLFW fournit déjà l'API
                 gamepad.
Recommandation : corriger, effort M (états de front par frame, table d'actions sérialisée, manettes).
Statut         : confirmé
```

```text
ID             : D-26
Axe            : Sous-systèmes (script)
Nature         : étrangeté
Gravité        : basse
Preuve         : ScriptEngine.cpp:18-24,38 (une `LuaEngine` « partagée », avec liaisons, que personne n'utilise :
                 chaque instance a la sienne) ; l.63-121 et 123-182 (deux fonctions de 60 lignes identiques au
                 chargement près) ; gui/component/render.cpp:586-589 (l'inspecteur exécute le script au niveau
                 global, sans liaisons ni limite de temps) ; LuaBindings.cpp:240-256 (`scene.find_entity` parcourt
                 toutes les entités en comparant des chaînes, appelé dans les `on_update` du sample) ;
                 LuaEngine.cpp:129-133 (un `on_update` en erreur relogue à chaque frame, sans désactivation).
Constat        : dette de l'éditeur de scripts : état mort, duplication, exécution non bornée dans l'éditeur,
                 recherche en O(n) offerte aux scripts sans avertissement.
Comparaison    : —
Recommandation : corriger, effort S.
Statut         : confirmé
```

```text
ID             : D-27
Axe            : Sous-systèmes (voxel)
Nature         : faiblesse
Gravité        : basse
Preuve         : Chunk.cpp:27-30 (16 KiB denses par chunk, blocs + méta, même tout en air) ; ChunkMesher.h:28-39
                 et Renderer3D.h:32-42 (vertex CPU de 40 octets, vertex GPU de 56 octets) ; RendererVoxel.cpp:77-86
                 (copie et conversion de chaque vertex avant upload) ; Scene.cpp:1198 (`*chunk = *finished.chunk`,
                 copie au lieu d'un move). Mesuré : 417 vertex par chunk en moyenne (terrain par défaut), soit
                 environ 26 KiB GPU par chunk.
Constat        : empreinte mémoire non optimisée : un monde éditeur à r=8, h=4 (2 601 chunks) pèse environ
                 42 MiB CPU, même en grande partie vide. C'est suffisant aujourd'hui, mais c'est un frein au
                 streaming lointain.
Comparaison    : compression par palette ou RLE en mémoire, vertex compactés en 8 octets (position 5 bits par
                 axe, normale 3 bits, AO 2 bits, index de texture).
Recommandation : surveiller, effort M.
Statut         : confirmé
```

```text
ID             : D-28
Axe            : Sous-systèmes (.owlpack)
Nature         : force
Gravité        : basse
Preuve         : PackReader.cpp:17-92 (contrôle de magic et de version, nombre d'entrées vérifié, erreurs typées
                 `owl::expected<void, PackOpenError>`) ; PackFormat.cpp:95-139 (désérialisation du TOC bornée à
                 chaque champ) ; PackReader.cpp:160-170 (garde contre les collisions de hash) ;
                 test/io_tests (72 tests, dont `tryOpen_invalid_magic`).
Constat        : le format est simple, versionné, compressé par zstd, et son parseur de TOC est défensif. Les
                 trous de D-02 (tailles et chemins) sont localisés et se corrigent sans changer de format.
Comparaison    : —
Recommandation : garder le format, corriger D-02.
Statut         : confirmé
```

```text
ID             : D-29
Axe            : Sous-systèmes (raycast)
Nature         : force
Gravité        : basse
Preuve         : source/owl/private/renderer/utils/RaycastDDAPass.cpp:31-126 (DDA en compute shader Slang,
                 buffers redimensionnés seulement à la croissance) ; RendererRaycast.cpp:238-246 (repli CPU) ;
                 l.615-641 (grille et métadonnées remontées seulement en cas de changement).
Constat        : le raycaster façon Wolfenstein lance ses rayons sur le GPU, garde un repli CPU et ne réenvoie
                 la grille qu'en cas de changement : architecture saine pour un mode de rendu de niche (seul
                 défaut : la relecture synchrone, D-17).
Comparaison    : —
Recommandation : garder
Statut         : confirmé
```

## Points de mesure proposés

Pour l'agent benchmark (`bench/`, Google Benchmark + mode « frame bench »). Les valeurs déjà relevées
par la sonde sont des points de départ, pas des résultats publiables.

| ID  | Sujet                         | Protocole proposé                                                                                                                      | Valeur indicative (sonde)                  |
|-----|-------------------------------|----------------------------------------------------------------------------------------------------------------------------------------|--------------------------------------------|
| M1  | Lua, coût par entité          | 100 / 1k / 10k `ScriptInstance` ; `on_update` vide, avec liaisons transform, avec `scene.find_entity` (O(n)) ; création ; heap         | 80–317 ns par appel ; 24–25 KiB ; 26–32 µs |
| M2  | Physique, pas Box2D           | 100 → 10k boîtes dynamiques ; durée de `b2World_Step` et de la boucle de synchro `PhysicCommand::frame` (avec et sans hiérarchie)      | —                                          |
| M3  | Physique, tilemap             | tilemap N×N collidable : temps d'`init()`, nombre de formes, coût de pas ; comparer à une version en chaînes                           | —                                          |
| M4  | Voxel, génération et maillage | `generateChunk`, `meshByKind` par chunk (vide, surface, grotte) ; avec et sans AO ; dispersion sur 30 répétitions                      | gen 63–107 µs ; maillage 350–560 µs        |
| M5  | Voxel, coût par frame         | `prepareWorld` + `drawVoxelWorld` CPU selon le rayon (r=4/h=2 en Play, r=8/h=4 en éditeur) ; part d'`isEmpty`                          | `isEmpty` 1,75–2,1 µs par chunk d'air      |
| M6  | Voxel, pics de streaming      | frame bench : caméra à vitesse constante (superSpeed) ; distribution des temps de frame (p50, p99, max) ; mémoire CPU et GPU par chunk | ~16 KiB CPU, ~26 KiB GPU par chunk         |
| M7  | Son                           | latence du premier `sound.play` non préchargé (disque + décodage) ; coût d'un son absent (scan récursif) ; saturation des sources      | —                                          |
| M8  | `.owlpack`                    | ouverture + extraction au démarrage (Application.cpp:63-91), premier et second lancement ; débit de `readEntry` ; fuzzing libFuzzer    | `tocSize` 4 GiB : 4 GiB alloués            |
| M9  | Raycast                       | temps CPU par frame avec et sans relecture synchrone (RendererRaycast.cpp:261-268) ; timestamps GPU                                    | —                                          |
| M10 | Tracker mémoire               | ns par `new`/`delete` : Release, Release + `OWL_ENABLE_MEMORY_TRACKER`, Debug ; contention avec N workers ; surcoût mémoire vivant     | —                                          |
| M11 | Profiler                      | ns par `OWL_PROFILE_SCOPE` avec `OWL_ENABLE_PROFILING=ON` ; taille du JSON par minute ; même scène avec Tracy                          | —                                          |
| M12 | Logs                          | coût d'un `OWL_CORE_TRACE` désactivé dans une boucle chaude ; coût d'un `log.info` Lua par frame (flush)                               | —                                          |
| M13 | Tâches                        | surcoût d'une tâche vide (push → terminaison) ; latence en frames ; débit à 1k tâches                                                  | —                                          |
| M14 | Polices                       | temps de chargement d'une police (génération MSDF) au démarrage, par taille de charset                                                 | —                                          |
| M15 | Non-régression D-01           | scénario `destroy_entity(self)` sous valgrind ou ASan dans la suite de tests, une fois D-01 corrigé                                    | 1 896 erreurs valgrind                     |

## Pistes pour l'avenir

- **Physique** : pas fixe + interpolation (prérequis du déterminisme et du rollback réseau) ; cycle de vie des
  corps par hooks EnTT ; événements de contact et capteurs Box2D v3 → `on_collision` / triggers ; chaînes
  pour les tilemaps ; `enqueueTask` branché sur Taskflow. Jolt pour la 3D le jour où le voxel ou les static
  meshes en auront besoin ; il remplacerait aussi la collision AABB maison de `VoxelPlayer`.
- **Script** : destruction et création d'entités différées (command buffer) ; handles d'entité en userdata à
  la place des UUID cherchés à chaque appel ; sandbox réel (mode "t", quotas, hook) ou passage à Luau si le
  modding devient un objectif ; sol2 ou Lua compilé en C++ pour la sûreté des exceptions ; hot reload et
  débogueur (DAP) pour l'outillage. Un seul `lua_State` avec un `_ENV` par script diviserait la mémoire par
  instance, au prix de l'isolation : à arbitrer.
- **Voxel** : maillage sur workers avec file par distance, budget d'upload et annulation ; invalidation des
  voisins ; palette en mémoire ; vertex compactés ; LOD lointain. Corriger D-03 avant tout.
- **Audio** : préchargement, pool de sources, streaming, bus par catégorie et effets. miniaudio est le
  candidat naturel si l'on quitte OpenAL ; cela correspond au chantier « Audio mixer » de la roadmap.
- **Outillage** : Tracy derrière `OWL_PROFILE_*` (zones CPU, GPU, mémoire) en remplacement du profiler et du
  tracker maison ; tracker désactivé par défaut en Debug.
- **Assets** : `.owlpack` lu en place (mmap), sans extraction, avec validation des chemins et des tailles et
  une somme de contrôle par entrée ; fuzzing en CI ; signature si une protection est réellement voulue.
  À intégrer au futur pipeline de cook (références par GUID).
- **Entrées et polices** : table d'actions, états de front, manettes ; charset configurable et atlas
  précalculé au cook (indispensable pour localiser en français).
- **Architecture** : sortir les sous-systèmes de l'état statique global pour les rattacher à la scène
  (avec l'axe A), condition pour des tests parallèles, des prévisualisations en Play et un serveur dédié.
