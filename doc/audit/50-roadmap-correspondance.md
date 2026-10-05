# Audit Owl — correspondance entre l'ancienne et la nouvelle roadmap

> **Statut : brouillon, 2026-10-05.** Accompagne la réécriture de [`doc/pages/roadmap.md`](../pages/roadmap.md)
> (branche `Feature/Bench`). Sources : l'ancienne roadmap (`git show HEAD:doc/pages/roadmap.md`),
> [`90-synthese.md`](90-synthese.md), [`40-avenir.md`](40-avenir.md), [`20-mesures.md`](20-mesures.md),
> [`30-etat-de-l-art.md`](30-etat-de-l-art.md) et les décisions du mainteneur du 2026-10-05.

[TOC]

## 1. Nouvelle numérotation (révision 1, remplacée par la §6)

| Version | Date attendue | Thème                                  |
|---------|---------------|----------------------------------------|
| v0.3.0  | 2027-03-01    | Foundations (aucune fonction de jeu)   |
| v0.4.0  | 2027-05-01    | Isometric renderer + serveur MCP       |
| v0.5.0  | 2027-07-01    | 2D complete                            |
| v0.6.0  | 2027-10-01    | 3D core                                |
| v0.7.0  | 2028-01-01    | Content & visual scripting             |
| v0.8.0  | 2028-03-15    | Gameplay systems                       |
| v0.9.0  | 2028-05-15    | Opening & platforms, release candidate |
| v1.0.0  | 2028-07-01    | Stable                                 |

Cadence : v0.3.0 reçoit cinq mois (migration Conan, socle Vulkan, architecture) ; ensuite deux à trois mois par
mineure, proche du rythme observé (0.1.0 → 0.2.0 en sept semaines, 0.2.0 → 0.2.1 en quatre). Les versions publiées
(≤ v0.2.1) sont inchangées, octet pour octet.

## 2. Items de l'ancienne roadmap

Les 63 items planifiés ou non publiés de l'ancienne roadmap (section Ongoing et versions v0.2.2 à v0.5.0).

| Ancienne version | Item                                          | Nouvelle destination | Remarque                                                                                                           |
|------------------|-----------------------------------------------|----------------------|--------------------------------------------------------------------------------------------------------------------|
| Ongoing          | Code quality                                  | Ongoing              | Inchangé ; badge `Ongoing` au lieu de `Planned` (I-07)                                                             |
| Ongoing          | Test coverage                                 | Ongoing              | Ajout : chaque correctif avec son test de régression                                                               |
| Ongoing          | Performance                                   | Ongoing              | Outils cités : Tracy, frame bench, `bench/`                                                                        |
| Ongoing          | Documentation quality                         | Ongoing              | Inchangé                                                                                                           |
| Ongoing          | Editor coverage for every authored object     | Ongoing              | Inchangé                                                                                                           |
| Ongoing          | Profiling tools                               | v0.3.0 (phase B)     | Couvert par Tracy + timestamps GPU (PR-16, PR-17, K-04)                                                            |
| Ongoing          | Rendering optimizations                       | Éclaté               | Partitionnement spatial et préparation multi-thread → v0.6.0 ; atlas → cook v0.7.0 ; batching déjà livré en v0.2.0 |
| v0.5.0           | Session restore                               | v0.3.0 (phase D)     | Utilisabilité, en paire avec l'autosave et la récupération après crash                                             |
| v0.5.0           | Mod loading system                            | v0.9.0               | Après durcissement `.owlpack` (PR-07) et Lua (PR-14)                                                               |
| v0.5.0           | Lua mod API                                   | v0.9.0               | Sur le registre de composants ouvert (PR-37)                                                                       |
| v0.5.0           | In-game mod manager                           | v0.9.0               | —                                                                                                                  |
| v0.5.0           | Level streaming                               | v0.9.0               | —                                                                                                                  |
| v0.5.0           | Asset pipeline (cooking)                      | v0.7.0               | Avancé : la 3D et le contenu en ont besoin (40-avenir §6.2)                                                        |
| v0.5.0           | Web export                                    | v0.9.0               | Via un backend WebGPU d'Owl RHI (B-16 : GL 4.6 non portable)                                                       |
| v0.5.0           | Android support                               | v0.9.0 (To evaluate) | Candidat 3e plateforme ; OpenGL ES abandonné (OpenGL gelé)                                                         |
| v0.5.0           | Gamepad improvements                          | v0.9.0               | Le support de base arrive en v0.5.0 (K-30, nouveau)                                                                |
| v0.4.0           | Node-graph link waypoints                     | v0.7.0               | Utile au scripting visuel                                                                                          |
| v0.4.0           | Network transport layer                       | v0.8.0               | Après la simulation déterministe ; voir questions ouvertes                                                         |
| v0.4.0           | Entity replication                            | v0.8.0               | Idem                                                                                                               |
| v0.4.0           | RPC system                                    | v0.8.0               | Idem                                                                                                               |
| v0.4.0           | Lobby and session management                  | v0.8.0               | Idem                                                                                                               |
| v0.4.0           | Network debugging tools                       | v0.8.0               | Idem                                                                                                               |
| v0.4.0           | Pathfinding                                   | v0.8.0               | Module optionnel, hors `Scene` (A-02)                                                                              |
| v0.4.0           | Behaviour trees                               | v0.8.0               | Idem                                                                                                               |
| v0.4.0           | Steering behaviours                           | v0.8.0               | Idem                                                                                                               |
| v0.4.0           | Physics queries                               | v0.5.0               | Box2D : relève de la 2D complète                                                                                   |
| v0.4.0           | Joints and constraints                        | v0.5.0               | Box2D : relève de la 2D complète                                                                                   |
| v0.4.0           | Collision callbacks                           | v0.5.0               | `on_collision` documenté mais absent (D-07) : doc corrigée en v0.3.0 (PR-15)                                       |
| v0.4.0           | Audio mixer                                   | v0.8.0               | Doc des volumes par catégorie corrigée dès v0.3.0 (D-07)                                                           |
| v0.4.0           | Audio effects                                 | v0.8.0               | —                                                                                                                  |
| v0.4.0           | Dialogue system                               | v0.8.0               | Module optionnel                                                                                                   |
| v0.3.0           | 3D render pipeline                            | v0.6.0               | Sur le socle Vulkan réparé en v0.3.0                                                                               |
| v0.3.0           | Lighting system                               | v0.6.0               | Approximation de GI devenue objectif optionnel                                                                     |
| v0.3.0           | Material system                               | v0.6.0               | —                                                                                                                  |
| v0.3.0           | Mesh rendering                                | Éclaté               | Static mesh + instancing → v0.6.0 ; LOD → v0.7.0 (produit par le cook)                                             |
| v0.3.0           | Skeletal animation                            | v0.7.0               | Recentrage de la 3D core                                                                                           |
| v0.3.0           | Particle system                               | v0.7.0               | Recentrage de la 3D core                                                                                           |
| v0.3.0           | Post-processing pipeline                      | Éclaté               | Pile + bloom, vignette, LUT, tone mapping → v0.6.0 ; aberration, grain, flou, DoF → v0.7.0                         |
| v0.3.0           | Weather and environment effects               | Sample project       | Fonction de jeu (40-avenir §6.3) ; à bâtir sur particules et post-process                                          |
| v0.3.0           | Compute-driven culling                        | v0.6.0               | Après les tests d'image de v0.3.0 (B-20, F-01)                                                                     |
| v0.3.0           | GPU raycast sprite stripes + BitonicSortPass  | v0.6.0               | Même famille GPU-driven que le culling                                                                             |
| v0.3.0           | 3D physics                                    | v0.8.0               | Jolt (décision du mainteneur)                                                                                      |
| v0.3.0           | 3D scene editing in Owl Nest                  | v0.6.0               | Indispensable à la 3D core (règle d'édition)                                                                       |
| v0.3.0           | Script debugging aids                         | v0.7.0               | Partage l'UI de valeurs du débogueur visuel                                                                        |
| v0.3.0           | Cross-compile packaging from any host         | v0.9.0               | Plateformes                                                                                                        |
| v0.3.0           | Target platform selector in Pack Game         | v0.9.0               | Cible Web ajoutée                                                                                                  |
| v0.3.0           | Tileset editor: compose atlas                 | v0.5.0               | 2D complète ; spécification abrégée                                                                                |
| v0.3.0           | Mermaid rendering in help panel               | v0.5.0               | Outillage éditeur                                                                                                  |
| v0.3.0           | Help-panel rendering polish (V2)              | v0.5.0               | Outillage éditeur                                                                                                  |
| v0.3.0           | Per-batch descriptor sets (Done)              | Retiré (doublon)     | Déjà décrit dans l'entrée v0.2.1 et le CHANGELOG 0.2.1                                                             |
| v0.3.0           | `vkDestroyDevice` leak (Done)                 | Retiré (doublon)     | Idem                                                                                                               |
| v0.3.0           | Binary scene format                           | v0.7.0               | Format runtime cuit ; précédé de la version de format (PR-25, v0.3.0)                                              |
| v0.3.0           | Parallel pack `readEntry`                     | v0.7.0               | Pipeline d'assets                                                                                                  |
| v0.2.3           | 2D lighting system                            | v0.5.0               | —                                                                                                                  |
| v0.2.3           | Custom ImGui-based file picker                | v0.5.0               | À revoir si l'évaluation SDL3 retient ses dialogues                                                                |
| v0.2.3           | Editor camera controls overhaul (In Progress) | v0.5.0               | Reste In Progress ; sous-parties Done conservées                                                                   |
| v0.2.3           | Look through scene camera                     | v0.5.0               | —                                                                                                                  |
| v0.2.3           | Text rendering quality                        | v0.5.0               | Spécification abrégée                                                                                              |
| v0.2.3           | Dedicated HUD layer                           | v0.5.0               | Spécification abrégée                                                                                              |
| v0.2.3           | Inventory system                              | Sample project       | Démo Lua du sample (fonction de jeu, A-02, C-14), listée en v0.5.0                                                 |
| v0.2.3           | Enemies                                       | Sample project       | Démo Lua du sample, listée en v0.5.0 ; IA moteur en v0.8.0                                                         |
| v0.2.2           | `RendererIsometric` layer                     | v0.4.0               | Contenu repris ; reste Planned (`Feature/KickoffIsometricRenderer` non fusionnée)                                  |
| v0.2.2           | teamcity-github-bridge 1.10.0 (Done)          | v0.3.0 (Done)        | Sur `main` depuis 0.2.1 ; « Already landed »                                                                       |

Bilan par destination : Ongoing : 5, Retiré : 2, Sample project : 3, v0.3.0 : 3, v0.4.0 : 1, v0.5.0 : 12, v0.6.0 : 6, v0.7.0 : 7, v0.8.0 : 12, v0.9.0 : 9, Éclaté : 3.

Aucun item n'est abandonné sur le fond : les trois sorties du moteur vont au sample project (fonctions de jeu), les deux
retraits sont des doublons d'éléments déjà livrés en v0.2.1, et trois items sont éclatés entre plusieurs versions.

## 3. Items nouveaux

| Item                                                                     | Origine                                   | Version                  |
|--------------------------------------------------------------------------|-------------------------------------------|--------------------------|
| Positionnement « moteur qui mélange les styles de rendu » + item Ongoing | Décision du mainteneur                    | Tête de roadmap, Ongoing |
| Ongoing « Performance budgets enforced in CI »                           | Décision du mainteneur ; K-04             | Ongoing                  |
| Badges `Ongoing` et `To evaluate`                                        | I-07 ; décision du mainteneur             | Légende                  |
| Phase A : PR-01, 03, 04, 05, 06, 07, 10, 12, 13, 14, 25                  | 90-synthese lots 1 et 2                   | v0.3.0                   |
| OpenGL corrigé et testé comme backend de compatibilité                   | B-16, B-07, B-18 ; décision RHI           | v0.3.0 A / C             |
| Harnais `bench/` (`OWL_BENCHMARK`)                                       | Commit de `Feature/Bench` (In Progress)   | v0.3.0 B                 |
| PR-02, 11, 16, 17, 18, 19, 20, 34 ; bench en CI avec seuil               | 90-synthese lots 3 et 5 ; décision        | v0.3.0 B                 |
| Owl RHI nommée ; Vulkan référence ; OpenGL gelé ; Null tests             | Décision du mainteneur ; fiche 2, K-20    | v0.3.0 C                 |
| PR-27, 28, 29, 32, 33, 35, 36, 37                                        | 90-synthese lots 4 et 6                   | v0.3.0 C                 |
| Modules CMake optionnels (core, render, physics, audio, script, Gui)     | Décision du mainteneur ; A-10             | v0.3.0 C                 |
| Registre typé des bindings Lua                                           | Décision du mainteneur ; D-07, D-26       | v0.3.0 C                 |
| API de commandes de l'éditeur (mutations via `UndoManager`)              | Décision du mainteneur du 2026-10-05      | v0.3.0 C                 |
| SDL3 (fenêtrage, input, dialogues, audio, GPU)                           | Décision du mainteneur                    | v0.3.0 C (To evaluate)   |
| Migration Conan 2, ConanCenter d'abord, retrait de DepManager            | Décision du mainteneur ; fiche 8, G-04    | v0.3.0 D                 |
| Paquet Conan OwlEngine + `test_package` (PR-08, PR-09)                   | G-01, G-02, G-03, G-06                    | v0.3.0 D                 |
| Mises à jour de toutes les dépendances (EnTT 4, Taskflow 4.1…)           | G-08 ; fiches 1 et 7                      | v0.3.0 D                 |
| Réduction des dépendances publiques ; tinyxml2 et zeus retirés           | Décision du mainteneur ; A-03, A-10, G-07 | v0.3.0 D                 |
| Hot reload assets, shaders Slang, Lua (éditeur + runner de dev)          | Décision du mainteneur du 2026-10-05      | v0.3.0 D                 |
| Autosave + récupération après crash ; messages d'erreur ; templates      | Décision du mainteneur                    | v0.3.0 D                 |
| PR-15, 26, 38, 39                                                        | 90-synthese lots 4 et 7                   | v0.3.0 D                 |
| PR-21, 22, 23, 24, 30, 31 ; shaders précompilés au pack                  | 90-synthese lot 3 ; fiche 3               | v0.3.0 Performance work  |
| Tableau des cibles de performance et critères de sortie                  | Décision du mainteneur ; 20-mesures       | v0.3.0                   |
| Serveur MCP intégré à Owl Nest (HTTP `127.0.0.1`, opt-in)                | Décision du mainteneur du 2026-10-05      | v0.4.0                   |
| Actions d'entrée + support manette de base                               | K-30, D-25, I-03                          | v0.5.0                   |
| Première brique procédurale pour la tilemap                              | Décision du mainteneur du 2026-10-05      | v0.5.0                   |
| Render graph                                                             | K-21 ; décision du mainteneur             | v0.6.0                   |
| Scène de démo 3D mêlant les styles                                       | Positionnement                            | v0.6.0                   |
| Scripting visuel façon Blueprints MVP (compilé en Lua)                   | Décision du mainteneur                    | v0.7.0                   |
| Graphes PCG sur `NodeCanvas` (Planned)                                   | Décision du mainteneur du 2026-10-05      | v0.7.0                   |
| Hot reload d'un module C++ de jeu                                        | Décision du mainteneur du 2026-10-05      | v0.7.0 (To evaluate)     |
| GUID d'assets                                                            | K-12                                      | v0.7.0                   |
| Simulation déterministe, replay d'entrées, rewind en Play                | Décision du mainteneur ; D-05, K-34       | v0.8.0                   |
| Gel de l'API, release candidate                                          | Décision du mainteneur ; K-36             | v0.9.0                   |
| Autres backends Owl RHI (Metal, D3D12)                                   | Décision du mainteneur                    | v0.9.0 (To evaluate)     |
| Critères de la v1.0.0                                                    | Décision du mainteneur                    | v1.0.0                   |

Précision sur le serveur MCP (décision du mainteneur du 2026-10-05) : intégré directement dans Owl Nest (même
processus, pas d'exécutable pont), activé par une option des réglages, en écoute HTTP sur `127.0.0.1` uniquement
(transport MCP « streamable HTTP »), vivant et mourant avec l'éditeur ; les requêtes sont mises en file et exécutées sur
le thread principal pendant la frame. Ressources en lecture (arbre de scène, composants, assets, prefabs, docs, stats de
perf) ; outils d'action passant par l'API de commandes de v0.3.0, donc annulables (créer, modifier, supprimer des
entités, instancier un prefab, lancer le Play N frames et rendre capture et mesures).

## 4. Recettes Conan maison probablement nécessaires

Vérifié le 2026-10-05 sur `conan-io/conan-center-index` (dossiers `recipes/` et `config.yml`, branche `master`).

| Dépendance               | ConanCenter (2026-10-05)                  | Conséquence                                                        |
|--------------------------|-------------------------------------------|--------------------------------------------------------------------|
| Slang                    | absent                                    | Recette maison (ou binaires amont empaquetés)                      |
| ufbx                     | absent                                    | Recette maison (header + source unique, simple)                    |
| imgui_color_text_edit    | absent                                    | Recette maison                                                     |
| ImGuizmo (bundle 1.92.7) | `imguizmo` cci.20231114 et 1.83 seulement | Recette maison tant que le bundle suit imgui 1.92                  |
| nfd (extended, btzy)     | seulement `nativefiledialog` 116 (mlabbe) | Recette maison, ou disparaît avec le picker ImGui (v0.5.0)         |
| debugbreak               | absent                                    | À rendre privé ou remplacer par `std::breakpoint` quand disponible |
| zeus                     | absent                                    | Sans objet : remplacé par `std::expected`                          |
| tinyobjloader            | 2.0.0-rc10 (Owl : rc13)                   | Recul de version ou recette maison                                 |
| openal-soft              | 1.24.3 (Owl vise 1.25)                    | Bump bloqué tant que CCI n'a pas 1.25                              |
| msdfgen                  | 1.12 (Owl vise 1.13)                      | Idem                                                               |
| imgui                    | 1.92.9b et 1.92.9b-docking                | Disponible (montée de 1.92.7)                                      |

Disponibles sur ConanCenter dans une version égale ou supérieure : box2d 3.1.1, cpptrace 1.0.4, entt 3.16.0, freetype,
glad 2.0.8, glfw, gtest, libdwarf, libpng, libsndfile 1.2.2, lua 5.5.0, lunasvg 3.5.0, magic_enum 0.9.8, md4c 0.5.2,
msdf-atlas-gen 1.3, spdlog 1.17.0, spirv-cross et vulkan-headers 1.4.357.0, vulkan-loader, vulkan-validationlayers,
stb, taskflow 4.0.0, tinygltf 2.9.7, yaml-cpp 0.9.0, rapidyaml, zlib, zstd ; ainsi que tracy, miniaudio, volk,
vulkan-memory-allocator, joltphysics et sdl pour les chantiers à venir. EnTT 4.0.0 (sortie 2026-07-23) et Taskflow 4.1.0
ne sont pas encore sur ConanCenter : la montée exigera soit d'attendre, soit une contribution à ConanCenter.

## 5. Questions ouvertes

1. **v0.8.0 surchargée.** Elle cumule IA, Jolt, audio, narration, déterminisme, replay, rewind et tout le réseau en
   deux mois et demi. Proposition : sortir le réseau après la 1.0 (40-avenir le place au long terme), ou décaler la 1.0.
2. **Physique 2D en v0.5.0.** Requêtes, joints et callbacks de collision (ancienne v0.4.0) sont passés en « 2D
   complete » plutôt qu'en v0.8.0, car ils portent sur Box2D. À confirmer.
3. **`on_collision` et volumes par catégorie (D-07).** La roadmap corrige la doc en v0.3.0 et implémente en v0.5.0 /
   v0.8.0. Implémenter dès v0.3.0 (correctif d'une promesse documentée) serait-il préférable ?
4. **Contrainte « aucune fonction de jeu » de v0.3.0.** Hot reload, autosave, templates et API de commandes y sont
   classés outillage ; le support manette de base (K-30) a été mis en v0.5.0 pour la respecter.
5. **Troisième plateforme de la 1.0.** Web (Planned) est le candidat naturel ; Android et macOS restent « To evaluate ».
   Le Web suffit-il à remplir le critère ?
6. **Date de v0.3.0.** Cinq mois supposent la migration Conan sans recette maison lourde ; Slang et le bundle ImGuizmo
   peuvent la rallonger.
7. **Roadmap détaillée ou résumée.** Les spécifications longues (isométrique, tileset, HUD) ont été gardées ou abrégées
   en place ; 40-avenir §6.3 propose de les déplacer dans des pages de design (I-07).
8. **Emplacement** (`doc/pages/roadmap.md` contre `ROADMAP.md`, décision 11 de la synthèse) : non tranché, fichier
   laissé en place.
9. **`.claude/rules/documentation.md`** ne connaît que trois badges ; il faudra y ajouter `To evaluate` et `Ongoing`,
   et mettre à jour `.claude/rules/ongoing-quality.md` (miroir de la section Ongoing).

## 6. Révision 2 (décisions du mainteneur, 2026-10-05)

### 6.1 Nouvelle numérotation

| Version | Date attendue | Thème                                                |
|---------|---------------|------------------------------------------------------|
| v0.3.0  | 2027-01-15    | Foundations (aucune fonction de jeu)                 |
| v0.4.0  | 2027-02-28    | Isometric renderer, serveur MCP, manette de base     |
| v0.5.0  | 2027-04-15    | 2D complete + refonte visuelle d'Owl Nest            |
| v0.6.0  | 2027-06-15    | 3D core                                              |
| v0.7.0  | 2027-08-15    | Content & visual scripting                           |
| v0.8.0  | 2027-10-15    | Gameplay systems (IA, physique 3D, audio, narration) |
| v0.9.0  | 2027-12-15    | Simulation déterministe, replay, rewind, réseau      |
| v0.10.0 | 2028-02-15    | Opening & platforms, gel de l'API, release candidate |
| v1.0.0  | 2028-04-01    | Stable                                               |

Le développement est fortement assisté par IA (l'IA code, le mainteneur teste et oriente) : la cadence est resserrée
à environ deux mois par mineure, trois et demi pour v0.3.0. Les critères de sortie priment sur les dates, ce que la
roadmap affiche en tête.

### 6.2 Décisions et destinations

| Décision                                                                                   | Destination                                   |
|--------------------------------------------------------------------------------------------|-----------------------------------------------|
| Structure en trois niveaux : `ROADMAP.md`, `doc/pages/roadmap.md`, `doc/pages/design/*.md` | Fait (18 pages de design)                     |
| Changelog : `CHANGELOG.md` racine ultra-court, détail dans `doc/pages/changelog.md`        | Fait (contenu conservé tel quel)              |
| Sous-découpage autorisé jusqu'à v0.10.0                                                    | v0.8.0 / v0.9.0 / v0.10.0 séparées            |
| Réseau avant la 1.0, sur le déterminisme                                                   | v0.9.0                                        |
| Modding, backend Web, gel de l'API, release candidate                                      | v0.10.0                                       |
| Physique 2D complète, API ouverte à la 3D (interface de monde physique, Box2D puis Jolt)   | v0.5.0 ; page `physics-api.md`                |
| `on_collision` implémenté (bug D-07), pas seulement retiré de la doc                       | v0.3.0 phase A                                |
| Support manette de base                                                                    | v0.4.0 (était v0.5.0)                         |
| Web = 3e plateforme de la 1.0 ; macOS et Android après la 1.0                              | v0.10.0 Planned ; macOS / Android To evaluate |
| Cadence resserrée (développement assisté par IA), critères de sortie avant les dates       | 1.0 au 2028-04-01 (était 2028-07-01)          |
| Owl Nest UI : bases d'interaction (tooltips, menus contextuels, drag & drop, texte / DPI)  | v0.3.0 phase D ; page `nest-ui.md`            |
| Owl Nest UI : refonte visuelle (vignettes, thème, icônes, guide de style)                  | v0.5.0 ; page `nest-ui.md`                    |
| Export d'un jeu testé de bout en bout, sample exporté et lancé en headless en CI           | v0.3.0 phase A ; page `game-export.md`        |
| Wayland complet (icône, multi-fenêtres éditeur, X11 conservé), relié à l'évaluation SDL3   | v0.3.0 phase A ; page `windowing-input.md`    |

### 6.3 Items déplacés par rapport à la révision 1

| Item                                                      | Révision 1                                   | Révision 2                                    |
|-----------------------------------------------------------|----------------------------------------------|-----------------------------------------------|
| Simulation déterministe, replay d'entrées, rewind en Play | v0.8.0                                       | v0.9.0                                        |
| Réseau (transport, réplication, RPC, lobby, outils)       | v0.8.0                                       | v0.9.0                                        |
| Modding (chargement, API Lua, gestionnaire)               | v0.9.0                                       | v0.10.0                                       |
| Level streaming                                           | v0.9.0                                       | v0.10.0                                       |
| Web export (backend WebGPU)                               | v0.9.0                                       | v0.10.0                                       |
| Cross-compile packaging, sélecteur de cible               | v0.9.0                                       | v0.10.0                                       |
| Gamepad improvements                                      | v0.9.0                                       | v0.10.0                                       |
| Gel de l'API, release candidate                           | v0.9.0                                       | v0.10.0                                       |
| Autres backends RHI (Metal, D3D12), Android               | v0.9.0 (To evaluate)                         | Après 1.0 (To evaluate)                       |
| Input actions + manette de base                           | v0.5.0                                       | v0.4.0                                        |
| `on_collision` (D-07)                                     | doc corrigée en v0.3.0, implémenté en v0.5.0 | implémenté en v0.3.0 phase A                  |
| Section Ongoing détaillée                                 | dans la roadmap                              | page `ongoing-quality.md`, une ligne par item |

Les autres destinations de la §2 sont inchangées, au renumérotage près. Aucun item n'est perdu : chaque spécification
retirée de `doc/pages/roadmap.md` se trouve dans une page de design, et les sections publiées (≤ v0.2.1) restent telles
quelles.

### 6.4 Pages de design

| Page de design             | Contenu                                                                               |
|----------------------------|---------------------------------------------------------------------------------------|
| `foundations.md`           | v0.3.0 complète : phases A à D, travail de perf, cibles, critères de sortie           |
| `owl-rhi.md`               | Owl RHI, réparation Vulkan, OpenGL gelé, WebGPU, SDL GPU                              |
| `conan-migration.md`       | Conan 2, recettes maison probables, mises à jour, réduction des dépendances publiques |
| `windowing-input.md`       | Wayland, limites GLFW, évaluation SDL3, actions d'entrée, manette                     |
| `game-export.md`           | Export de bout en bout (v0.3.0), packaging multi-plateforme (v0.10.0)                 |
| `isometric.md`             | Spécification complète de `RendererIsometric` (ancienne v0.2.2)                       |
| `mcp-server.md`            | API de commandes éditeur (v0.3.0), serveur MCP intégré (v0.4.0)                       |
| `nest-ui.md`               | UI d'Owl Nest, caméra éditeur, look-through, file picker, panneau d'aide              |
| `2d-complete.md`           | Éclairage 2D, texte, HUD, éditeur de tileset, brique PCG, démos du sample             |
| `physics-api.md`           | Interface physique indépendante du backend, 2D complète, Jolt                         |
| `3d-core.md`               | Pipeline 3D, render graph, PBR, ombres, post-process, culling GPU, édition 3D         |
| `content-pipeline.md`      | Cook, format binaire, pack parallèle, hot reload C++, animation, effets               |
| `visual-scripting.md`      | Registre typé des bindings, scripting visuel, waypoints, débogage de script           |
| `pcg-graphs.md`            | Graphes de génération procédurale                                                     |
| `gameplay-systems.md`      | IA, audio, narration                                                                  |
| `simulation-networking.md` | Déterminisme, replay, rewind, réseau                                                  |
| `modding-platforms.md`     | Gel de l'API, modding, level streaming, Web, plateformes après 1.0                    |
| `stable-release.md`        | Critères de la 1.0.0                                                                  |
| `ongoing-quality.md`       | Détail de la section Ongoing                                                          |

Noms sans point (le bundle d'aide tire l'identifiant du nom jusqu'au premier point, d'où `foundations.md` plutôt que
`foundations-v0.3.0.md`) et distincts des pages de `doc/pages/` (le bundle les aplatit).

### 6.5 Effets de bord

- `doc/fix_md_links.py` résout désormais les liens relatifs par l'ancre `{#…}` du fichier cible (liens
  `design/x.md` et `../roadmap.md`).
- `DoxyfileTemplate` ajoute `ROADMAP.md` à `INPUT` ; les fichiers racine n'ont pas d'ancre, donc pas de doublon avec
  `page-roadmap` / `page-changelog`. Documentation passe sans avertissement.
- `cmake/HelpAssets.cmake` embarque `doc/pages/design/*.md` et n'embarque plus le `CHANGELOG.md` racine
  (`CHANGELOG.md` et `changelog.md` se chevaucheraient sur un système de fichiers insensible à la casse).
- Le contrôle codespell de CodeStyle lit aussi `ROADMAP.md`.

### 6.6 Questions ouvertes restantes

1. Les sections publiées de `doc/pages/roadmap.md` gardent leurs sous-listes : faut-il les réduire à une ligne par item,
   puisque le changelog détaillé porte déjà l'historique ?
2. Le bundle d'aide contient déjà `contributing.md` et `CONTRIBUTING.md` : même risque de collision sous Windows,
   antérieur à cette révision.
3. v0.3.0 en trois mois et demi reste le jalon le plus fragile (migration Conan, recettes Slang et ImGuizmo).

## 7. Révision 3 (décisions du mainteneur, 2026-10-05)

- **Versions publiées condensées** : dans `doc/pages/roadmap.md`, v0.0.1 à v0.2.1 passent à une ligne par item (badge
  Done + résumé), Goal en une ou deux lignes ; l'historique détaillé reste dans `doc/pages/changelog.md`, inchangé.
- **Risque d'abord** : une « Phase 0 — Risk first: dependencies & Conan » ouvre v0.3.0, avant la phase A : migration
  Conan 2 avec d'abord les recettes manquantes (Slang, ufbx, imgui_color_text_edit, ImGuizmo, nfd-extended,
  tinyobjloader) et les versions absentes de ConanCenter, puis les mises à jour cassantes (EnTT 4, Taskflow 4.1,
  yaml-cpp 0.9…), puis le paquet OwlEngine et le retrait de DepManager. La réduction des dépendances publiques reste
  en phase D (« Usability & dependency reduction »).
- **Socle Vulkan** : premier chantier de la phase C, lancé dès que le frame bench (PR-17) et les tests d'image
  (PR-18) de la phase B sont en place ; il ne précède pas les filets.
- **Bundle d'aide** : `cmake/HelpAssets.cmake` et `CONTRIBUTING.md` ne sont plus touchés (le mainteneur règle le
  conflit de casse) ; les modifications de la révision 2 sur ces deux fichiers restent en place, à garder ou annuler.
