# Audit Owl — constats des axes G (build, dépendances, packaging) et I (documentation)

> Périmètre : `CMakeLists.txt`, `cmake/`, presets, `depmanager.yml`, `owl_engine.py`, `CPackConfig.cmake`,
> paquet publié `OwlEngine-0.2.0-bc94f34-linux-glibc_2.39-x64.tar.gz`, `doc/pages/*.md`, `README.md`,
> `CHANGELOG.md`, `CONTRIBUTING.md`, `SECURITY.md`, `DoxyfileTemplate`. Branche `Feature/Bench`, commit
> `45e27892`, 2026-10-05. Lecture seule. Les outils ont tourné via `docker/run.sh` (`depmanager` 0.5.5,
> `clang++` 22), sans aucun build. Les scripts et extraits sont dans le scratchpad `axeGI/`.
>
> « Mesuré » : obtenu par une commande reproductible donnée dans la preuve. « Constaté » : lu dans le code.
> « Opinion » : jugement de l'auteur, signalé comme tel. Le statut « confirmé » veut dire vérifié par l'auteur.
> La contre-vérification par un second agent (§7.3 du cadrage) a été faite le 2026-10-05 : voir les lignes
> « Vérification ». Gravités ajustées : G-04, G-09, I-05 et I-07 baissent, G-06 monte à haute.
>
> Versions amont : la référence est **ce que publie le serveur DepManager** (`pack ls --default`, mesuré le
> 2026-10-05) et les tags des sous-modules d'`OwlDependencies`. Ma connaissance propre de l'amont s'arrête vers
> la mi-2026. Quand elle est utilisée, c'est indiqué « à vérifier ».

## Résumé

1. **Le paquet moteur livré aux tiers est inutilisable en l'état (mesuré).** Les headers sont installés sous
   `include/public/` alors que la cible exporte `include/`, donc `#include <owl.h>` échoue. La cible exporte en plus
   `Owl::Owl_Base`, qui impose `-Werror -Weverything` (option propre à Clang) à tout consommateur.
2. Ce défaut n'a pas été vu parce que **personne ne consomme le paquet** : OwlDrone est resté sur `owlengine` 0.0.3,
   et aucun test de consommation n'existe en CI.
3. DepManager, OwlDependencies et le serveur `package.argawaen.net` reposent sur **une seule personne** (bus factor 1).
   Les binaires ne sont identifiés que par nom, version, ABI et glibc : pas de hash, pas de lockfile, et `pull-newer: true`.
4. **Dix des 34 dépendances** sont en retard sur des versions **déjà publiées sur le serveur du projet** : EnTT 3.16,
   FreeType 2.14.1, libpng 1.6.53, OpenAL 1.25, spdlog 1.17, ufbx 0.21.2, entre autres. `tinyxml2` est déclarée sans être utilisée.
5. Le configure n'est ni minimal ni hermétique : Doxygen y est obligatoire (`OWL_ENABLE_DOCUMENTATION` n'est jamais lu),
   il télécharge des badges, écrit dans l'arbre source et ignore le `CMAKE_INSTALL_PREFIX` des presets.
6. Côté forces, le CMake est **moderne** (cibles, `FILE_SET`, expressions génératrices, Base INTERFACE, presets en couches
   qui pilotent la CI), et la discipline de warnings est réelle (`-Weverything -Werror` tenu à zéro).
7. Documentation : sur **97 affirmations contrôlées** dans 16 pages, 64 sont exactes (66 %), 20 inexactes et 12 obsolètes.
   Les pages CI, node graph, son et voxel sont fidèles. `renderer.md` et `scripting.md` ont décroché.
8. Le point le plus trompeur pour un utilisateur : `on_collision` est documenté en Lua, mais aucun appelant ne le déclenche.
   Le README annonce par ailleurs une manette, qui n'existe pas.
9. Les exigences Doxygen sont **disproportionnées** (opinion appuyée sur une mesure) : 66 % des lignes des headers publics
   sont de la doc, et au moins 300 `@brief` sont du remplissage (« Default constructor. »), le tout avec `WARN_AS_ERROR`.
10. Méta-documentation : SECURITY promet un support 0.1.x sans branche de maintenance et sans adresse de contact.
    CONTRIBUTING donne une commande clang-tidy devenue sans effet. Une règle IA cite une CLI DepManager invalide.

## Tableau des dépendances

Colonnes : **épinglée** = `depmanager.yml` ; **serveur** = plus récente version publiée sur le remote par défaut
(`docker/run.sh poetry run depmanager pack ls --default -p <nom>`, mesuré le 2026-10-05, sortie dans
`axeGI/remote_latest.txt`) ; **usage** = fichiers de `source/` et `test/` qui incluent la bibliothèque
(`grep -rlE '#include *[<"]…'`) et le point de liaison CMake.

| Nom                   | Épinglée       | Serveur        | Usage réel                                                                    | Remarque                                                                                                 |
|-----------------------|----------------|----------------|-------------------------------------------------------------------------------|----------------------------------------------------------------------------------------------------------|
| box2d                 | 3.1.1          | 3.1.1          | 1 fichier (`physics/PhysicCommand.cpp:15`)                                    | À jour sur le serveur. Release amont plus récente à vérifier.                                            |
| cpptrace              | 1.0.4          | 1.0.4          | 1 fichier (`debug/Tracker.cpp:18`)                                            | Tire libdwarf.                                                                                           |
| debugbreak            | 1.0            | —              | `public/core/Assert.h:18` (gardé par `OWL_ENGINE_BUILD`)                      | Header public mais non exporté : sans effet hors build moteur.                                           |
| entt                  | 3.15.0         | **3.16.0**     | 4 fichiers, PUBLIC (`source/owl/CMakeLists.txt:137`)                          | En retard d'une mineure. Dépendance publique : tout bump touche l'ABI.                                   |
| freetype              | 2.13.3         | **2.14.1**     | 0 include direct                                                              | Transitive (msdf-atlas-gen). Paquet construit le 2025-02-13.                                             |
| glad                  | 2.0.4          | 2.0.4          | 1 wrapper (`core/external/opengl46.h`)                                        | Construit le 2024-05-22.                                                                                 |
| glfw                  | 3.4.0          | 3.4.0          | 1 wrapper (`core/external/glfw3.h`)                                           | À jour (3.4 est la dernière release que je connais).                                                     |
| googletest            | 1.17.0         | 1.17.0         | 6 fichiers de test + `test/CMakeLists.txt:6`                                  | Pas de `kind` alors que la bibliothèque n'est pas header-only (résolue en shared).                       |
| imgui                 | 1.92.7-docking | 1.92.7-docking | 36 fichiers, PUBLIC (`source/owl/CMakeLists.txt:140`)                         | Branche docking. Dépendance publique.                                                                    |
| imgui_color_text_edit | 1.92.7         | 1.92.7         | 1 fichier (Owl Nest)                                                          | Éditeur uniquement.                                                                                      |
| imguizmo              | 1.92.7         | 1.92.7         | 4 fichiers                                                                    | Bundle (GraphEditor, ImSequencer…).                                                                      |
| libdwarf              | 2.2.0          | 2.2.0          | 0 include direct                                                              | Transitive (cpptrace).                                                                                   |
| lua                   | 5.5.0          | 5.5.0          | 5 fichiers                                                                    | À jour.                                                                                                  |
| lunasvg               | 3.5.0          | 3.5.0          | 5 fichiers                                                                    | Liée deux fois (moteur et Nest), avec un `find_package(Threads)` de contournement.                       |
| libpng                | 1.6.50         | **1.6.53**     | 0 include direct                                                              | Transitive. Les versions 1.6.51 à 1.6.53 corrigent, à ma connaissance, des failles mémoire (à vérifier). |
| libsndfile            | 1.2.2          | 1.2.2          | 1 fichier (`sound/openal/SoundData.cpp:12`)                                   | Pas de `kind` (résolue en shared), contrairement à la règle.                                             |
| magic_enum            | 0.9.7          | 0.9.7          | 10 fichiers                                                                   | À jour.                                                                                                  |
| md4c                  | 0.5.2          | 0.5.2          | 2 fichiers (Nest + `scene_tests`)                                             | 0.5.3 est déjà dans la recette `OwlDependencies` mais n'est pas publiée.                                 |
| msdfgen               | 1.12.1         | **1.13**       | 0 include direct                                                              | Transitive (msdf-atlas-gen).                                                                             |
| msdf-atlas-gen        | 1.3            | 1.3            | 1 fichier (`data/fonts/Font.cpp:15`)                                          | À jour.                                                                                                  |
| nfd                   | 1.2.1          | **1.3.0**      | 1 fichier (`platform/FileDialog.cpp:14`)                                      | En retard d'une mineure.                                                                                 |
| openal                | 1.24.3         | **1.25.0**     | 1 wrapper (`core/external/openal.h`)                                          | En retard d'une mineure.                                                                                 |
| spdlog                | 1.16.0         | **1.17.0**     | 2 fichiers                                                                    | En retard d'une mineure.                                                                                 |
| stb_image             | 2.28           | 2.28           | 5 fichiers                                                                    | 2.30 existe en amont à ma connaissance (à vérifier). Construit le 2024-05-22.                            |
| taskflow              | 4.0.0          | 4.0.0          | 3 fichiers (privé)                                                            | À jour.                                                                                                  |
| tinygltf              | 2.9.6          | **2.9.7**      | 1 fichier (`data/geometry/MeshLoader.cpp:19`)                                 | Patch en retard.                                                                                         |
| tinyobjloader         | 2.0.0-rc13     | 2.0.0-rc13     | 1 fichier (`MeshLoader.cpp:23`)                                               | **Pré-release** en production.                                                                           |
| tinyxml2              | 11.0.0         | 11.0.0         | **0** : aucun include, aucune liaison, aucun `tinyxml2_DIR` dans `CMakeCache` | Dépendance morte.                                                                                        |
| ufbx                  | 0.20.1         | **0.21.2**     | 1 fichier (`MeshLoader.cpp:24`)                                               | En retard d'une mineure (avant 1.0, mineure = rupture possible).                                         |
| vulkan_sdk            | 1.4.341        | 1.4.341        | 25 (`vulkan/`), plus Slang (2) et spirv-cross (4)                             | Récent (construit le 2026-02-24). Fournit aussi Slang et spirv-cross.                                    |
| yaml-cpp              | 0.8.0          | 0.8.0          | 14 fichiers, dont 1 header public (`renderer/RenderLayer.h:16`)               | Release amont de 2023, paquet construit le 2024-05-22. Fuit dans l'API publique (voir G-07).             |
| zeus                  | 1.3.1          | 1.3.1          | `public/core/expected.h:27` (branche de repli)                                | Branche morte : C++23 est requis (`BaseConfig.cmake:14-15`) et `std::expected` est pris en premier.      |
| zlib                  | 1.3.1          | **1.3.1.2**    | 0 include direct                                                              | Transitive. `ZLIB_INCLUDE_DIR=/usr/include` dans `CMakeCache` (voir G-17).                               |
| zstd                  | 1.5.7          | 1.5.7          | 1 fichier (`data/assets/pack/PackFormat.cpp:16`)                              | À jour.                                                                                                  |

Bilan : 34 paquets déclarés, 33 réellement résolus. 6 sont transitifs sans include direct (freetype, libdwarf, libpng,
msdfgen, zlib, et debugbreak hors build moteur). 1 est mort (tinyxml2), 1 n'est plus qu'un repli mort (zeus). 10 sont en
retard sur le serveur du projet lui-même. Le `CLAUDE.md` sur disque annonce « ~36 dépendances » ; il y en a 34.

## Contrôle des pages de documentation

Pour chaque page, 5 à 7 affirmations vérifiables ont été contrôlées dans le code (noms, chemins, valeurs par défaut,
comptes). Le contrôle a été réparti entre trois sous-agents en lecture seule, puis les points saillants ont été
re-vérifiés par l'auteur : `on_collision`, sous-namespaces du renderer, `Mesh3DVertex`.

| Page                        | Contrôlées | Exactes | Inexactes | Obsolètes | Écart principal                                                                                                                                     |
|-----------------------------|------------|---------|-----------|-----------|-----------------------------------------------------------------------------------------------------------------------------------------------------|
| `architecture.md`           | 7          | 3       | 3         | 1         | Menus « Project > Import Scene » devenus un ruban ; « gamepad » ; scan de meshes par `AssetScanner` inexistant ; URL lunasvg erronée.               |
| `building.md`               | 6          | 3       | 2         | 1         | « 15 catégories … physic » au lieu de 16 et `physics` ; nom des binaires de test ; `OWL_ENABLE_MEMORY_TRACKER` absent ; deux pseudo-options.        |
| `getting_started.md`        | 5          | 4       | 1         | 0         | « File → Open Project » au lieu du bouton du ruban.                                                                                                 |
| `contributing.md`           | 6          | 5       | 1         | 0         | Affirme que les sanitizers tournent sur les PR, alors que UB et les configurations GCC sont en `skipAutoPRs()` (`.teamcity/Build/Build.kt:52,251`). |
| `continuous_integration.md` | 7          | 5       | 2         | 0         | « onze configurations » au lieu de 17 ; un nom d'affichage.                                                                                         |
| `event_input.md`            | 6          | 4       | 2         | 0         | La vitesse par défaut 5.0 de `CameraOrthoController` est écrasée par le zoom à la première frame (`CameraOrthoController.cpp:59`).                  |
| `editor.md`                 | 7          | 5       | 1         | 1         | `Project` décrit avec 3 champs, il en a 9 (`source/owlnest/sources/Project.h:21-50`).                                                               |
| `scene.md`                  | 6          | 5       | 1         | 0         | Le pipeline de rendu omet les passes voxel, tilemap et raycast (`Scene.cpp:944-1060`).                                                              |
| `renderer.md`               | 6          | 3       | 0         | 3         | Sous-namespaces `renderer::stack/renderer2d/rendererraycast` inexistants ; `Mesh3DVertex` de 36 o au lieu de 56 (`Renderer3D.cpp:41`).              |
| `node_graph.md`             | 5          | 5       | 0         | 0         | Aucun.                                                                                                                                              |
| `voxel.md`                  | 6          | 5       | 0         | 1         | L'intro annonce « à venir » ce que la page documente déjà.                                                                                          |
| `physics.md`                | 6          | 4       | 1         | 1         | Triggers `Timer`, `Interaction` et `LuaCallback` absents ; `setGravityScale` et les snapshots absents.                                              |
| `sound.md`                  | 5          | 5       | 0         | 0         | Aucun (les clés `volume_music/sfx` ne sont jamais appliquées, ce que la page dit bien).                                                             |
| `scripting.md`              | 6          | 2       | 2         | 2         | `on_collision` jamais appelé ; tables `door` et `pushwall` absentes ; 3 fonctions de `scene` et 8 composants manquants.                             |
| `raycaster_method.md`       | 6          | 3       | 2         | 1         | La limite « pas de collision » est périmée ; un TODO cité n'existe pas ; contradiction interne sur l'aspect.                                        |
| `README.md`                 | 6          | 2       | 2         | 1         | Manette inexistante ; minima de compilateur contredits par `BaseConfig.cmake:66-69` ; badge 0.2.1 (1 point invérifiable : arm64).                   |
| **Total**                   | **97**     | **64**  | **20**    | **12**    | 66 % d'exactitude, plus 1 point invérifiable.                                                                                                       |

## Constats

### Gravité haute

```text
ID             : G-01
Axe            : Build, dépendances, packaging
Nature         : faiblesse
Gravité        : haute
Preuve         : source/owl/CMakeLists.txt:58 (Owl_Base lié PUBLIC) et :182 (Owl_Base installé dans
                 l'export) ; cmake/BaseConfig.cmake:97-123 ; paquet publié OwlEngine-0.2.0 :
                 lib/cmake/OwlEngine/OwlEngineTargets.cmake:83-87 (mesuré, extrait dans axeGI/pkg/)
Constat        : la cible exportée Owl::OwlEngine entraîne Owl::Owl_Base, qui porte
                 INTERFACE_COMPILE_OPTIONS "-Werror;-Weverything;-pedantic;-Wno-…" et des définitions
                 internes (OWL_MAJOR, OWL_AUTHOR, OWL_PLATFORM_*). Un consommateur GCC échoue sur
                 -Weverything, option inconnue de GCC. Un consommateur Clang se voit imposer -Werror
                 -Weverything sur son propre code. Les drapeaux sont figés selon le compilateur qui
                 a construit le paquet.
Comparaison    : vcpkg, Conan et les guides CMake (« Professional CMake », docs CMake sur
                 l'export) réservent les warnings au build du projet : BUILD_INTERFACE, ou cible
                 non exportée liée en PRIVATE.
Recommandation : corriger, effort S. Lier Owl_Base en PRIVATE (ou $<BUILD_INTERFACE:…>) et ne plus
                 l'installer. Garder dans l'export seulement les définitions réellement nécessaires
                 aux headers publics.
Statut         : confirmé
Vérification   : OwlEngineTargets.cmake du paquet 0.2.0 : `INTERFACE_LINK_LIBRARIES "Owl::Owl_Base;…"` et options
                 `-Werror;-Weverything;…` ; CMakeLists.txt:58 et :182 inchangés. Nuance : OWL_PLATFORM_* et
                 OWL_BUILD_SHARED servent à `OWL_API` (Core.h:15-24) et doivent rester exportés, ce que prévoit la reco.
```

```text
ID             : G-02
Axe            : Build, dépendances, packaging
Nature         : faiblesse
Gravité        : haute
Preuve         : source/owl/CMakeLists.txt:47-48 (FILE_SET HEADERS BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR})
                 et :56 ($<INSTALL_INTERFACE:include>) ; paquet 0.2.0 : 213 headers sous include/public/,
                 INTERFACE_INCLUDE_DIRECTORIES "${_IMPORT_PREFIX}/include". Mesuré :
                 `docker/run.sh clang++ -std=c++23 -fsyntax-only -I <pkg>/include t.cpp`
                 donne « 'owl.h' file not found ». Avec <public/core/Log.h>, on obtient
                 « 'core/Core.h' file not found » (Log.h:11).
Constat        : le base dir du file set est source/owl et non source/owl/public. Les headers
                 s'installent donc sous include/public/ alors que l'include exporté pointe sur include/.
                 Aucun header du paquet n'est incluable sans ajouter -I …/include/public à la main.
Comparaison    : le motif standard est BASE_DIRS public, qui donne include/<module>/… sans
                 préfixe parasite.
Recommandation : corriger, effort S (BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/public), puis ajouter un
                 test de consommation (G-03).
Statut         : confirmé
Vérification   : `BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}` toujours à CMakeLists.txt:47-48 ; le paquet extrait n'a qu'un
                 dossier include/public/ et la cible exporte `${_IMPORT_PREFIX}/include` (find y compte 180 fichiers .h).
```

```text
ID             : G-03
Axe            : Build, dépendances, packaging
Nature         : risque
Gravité        : haute
Preuve         : ../OwlDrone/depmanager.yml (owlengine 0.0.3, plus yaml-cpp ajouté par le consommateur ;
                 dernier commit 2026-04-09) ; owl_engine.py:15 (name = "owl_engine", version = "0.2.1" en
                 dur alors que CMakeLists.txt:8 dit 0.2.2) ; cmake/config/OwlEngineConfig.cmake.in:4-5
                 (seuls EnTT et imgui sont en find_dependency) ; aucune étape de la CI ne consomme le paquet
                 (`grep -rn find_package .teamcity ci` ne renvoie rien qui concerne OwlEngine).
Constat        : le packaging pour des tiers, présenté comme un usage visé (CLAUDE.md, rule
                 dependencies.md), n'est exercé par personne depuis la 0.0.3. Deux chaînes coexistent :
                 CPack (presets package-engine-*) et la recette DepManager. Elles n'ont ni le même nom
                 de paquet (owl_engine contre owlengine côté consommateur) ni la même source de version.
                 C'est pour cela que G-01 et G-02 ont pu être publiés en 0.2.0.
Comparaison    : vcpkg et Conan valident une recette par un « test_package », un mini-consommateur
                 compilé à chaque build du paquet.
Recommandation : corriger, effort M. Ajouter un test_package (un main.cpp avec find_package(OwlEngine)
                 + Owl::OwlEngine) à l'action Package. Lire la version depuis CMakeLists.txt dans
                 owl_engine.py. Ne garder qu'un nom de paquet.
Statut         : confirmé
Vérification   : OwlDrone/depmanager.yml : `owlengine 0.0.3`, dernier commit 2026-04-09 ; owl_engine.py:14 (0.2.1) contre
                 CMakeLists.txt:8 (0.2.2) ; aucune mention d'OwlEngine dans .teamcity/ ni ci/. Aggravant : voir G-06.
```

```text
ID             : G-04
Axe            : Build, dépendances, packaging
Nature         : risque
Gravité        : moyenne (haute avant vérification)
Preuve         : DepManager : `git shortlog -sn` donne 102 commits sur 103 de Silmaen ; OwlDependencies :
                 198 Silmaen et 3 « Damien Lachouette » ; serveur unique srvs://package.argawaen.net
                 (doc/pages/building.md:33-39) ; métadonnées d'un paquet = name/version/os/arch/kind/abi/
                 glibc/build_date seulement (fake_home/.edm/data/*/info.yaml) ; depmanager.yml:4-5
                 (pull-newer: true) ; aucun lockfile ; 1 seul serveur, sans miroir documenté.
Constat        : toute la chaîne native (recettes, outil, binaires) dépend d'une personne et d'un
                 serveur auto-hébergé. Rien n'atteste quel binaire a servi à un build : pas de hash de
                 contenu ni de révision de recette, et le compilateur de construction n'est pas
                 enregistré. Avec pull-newer, une nouvelle publication sous le même numéro de version
                 peut remplacer silencieusement un binaire (plausible : le comportement exact de
                 pull-newer sur une même version n'a pas été testé). Les paquets sont tous « abi gnu,
                 glibc 2.35 », certains construits en 2024-05, et consommés par clang 22 sur Ubuntu 24.04.
                 Le toolset `llvm20` de la config locale (01-environnement.md §3) n'intervient que pour
                 `depmanager build`.
Comparaison    : Conan 2 identifie chaque binaire par une révision de recette (RREV) et une révision
                 de paquet (PREV), plus un lockfile. vcpkg fige les ports par un « builtin-baseline »
                 (commit) et vérifie les sources par SHA512. Le cache binaire de vcpkg est keyé par un
                 hash d'ABI qui inclut compilateur et triplet. CPM.cmake construit depuis les sources
                 avec un tag git, donc reproductible mais lent.
Recommandation : surveiller, effort M. À court terme : publier un lockfile (version + hash du
                 tarball) et passer pull-newer à false en CI. À moyen terme : évaluer une migration
                 vcpkg en mode manifest avec registre privé (voir « Pistes »).
Statut         : confirmé (la substitution silencieuse reste plausible)
Vérification   : shortlog : DepManager 103 commits, tous Silmaen ; OwlDependencies 198 + 3 du même auteur ; `pull-newer: true`
                 (depmanager.yml:5). Gravité ramenée à moyenne : ce bus factor 1 est celui du projet lui-même (même
                 mainteneur), sans risque propre ; reste l'absence de traçabilité binaire, sans incident constaté.
```

### Gravité moyenne

```text
ID             : G-05
Axe            : Build, dépendances, packaging
Nature         : faiblesse
Gravité        : moyenne
Preuve         : cmake/BaseConfig.cmake:231 (include(cmake/DocumentationConfig.cmake) inconditionnel) ;
                 cmake/DocumentationConfig.cmake:4-5 (find_package(Doxygen REQUIRED dot)) et :11
                 (configure_file vers ${CMAKE_SOURCE_DIR}/Doxyfile) ; OWL_ENABLE_DOCUMENTATION n'est lu
                 que par ci/utils/preset.py:125 et les presets (cmake/CMakePresetsMinGW.json:49,
                 CMakePresetsPackage.json:70), jamais par CMake ; building.md:11-18 ne liste pas Doxygen
                 dans les prérequis.
Constat        : tout configure, y compris celui de la recette du paquet moteur, exige Doxygen et
                 Graphviz. L'option documentée OWL_ENABLE_DOCUMENTATION (OFF par défaut) n'a aucun effet
                 côté CMake. Le Doxyfile est généré dans l'arbre source : deux presets configurés en
                 parallèle l'écrivent au même endroit. Le cadrage le juge « inutilisé, candidat à la
                 suppression » (01-environnement.md §4), à tort : c'est un artefact généré, ignoré par
                 .gitignore:52 et supprimé du suivi git en #82.
Recommandation : corriger, effort S. Écrire option(OWL_ENABLE_DOCUMENTATION) et conditionner
                 l'include ; Doxygen non REQUIRED ; Doxyfile dans ${CMAKE_BINARY_DIR}.
Statut         : confirmé
Vérification   : include inconditionnel à BaseConfig.cmake:229 (et non :231), `find_package(Doxygen REQUIRED dot)` en
                 DocumentationConfig.cmake:4-5, Doxyfile écrit dans `${CMAKE_SOURCE_DIR}` (:11) ; aucun CMake ne lit
                 OWL_ENABLE_DOCUMENTATION (grep).
```

```text
ID             : G-06
Axe            : Build, dépendances, packaging
Nature         : faiblesse
Gravité        : haute (moyenne avant vérification)
Preuve         : cmake/BaseConfig.cmake:152 (set(CMAKE_INSTALL_PREFIX ${PROJECT_SOURCE_DIR}/output/install),
                 variable normale qui masque le cache) ; cmake/CMakePresetsBase.json:14 (cache
                 output/install/${presetName}) ; mesuré : CMakeCache.txt:130 vaut
                 …/output/install/linux-clang-release, alors que cmake_install.cmake:5 vaut
                 …/output/install.
Constat        : le préfixe d'installation des presets, de -DCMAKE_INSTALL_PREFIX ou d'un gestionnaire
                 de paquets est ignoré : tous les presets installent au même endroit. building.md
                 (« Install : output/install/<preset>/ ») décrit un comportement qui n'existe pas.
                 Seul `cmake --install --prefix` y échappe.
Recommandation : corriger, effort S. Supprimer la ligne, ou la protéger par
                 if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT).
Statut         : confirmé (mécanisme) / plausible (paquet DepManager vide, non reproduit)
Vérification   : BaseConfig.cmake:152 et cmake_install.cmake:4-5 vérifiés. Aggravation : DepManager construit la recette
                 avec `-DCMAKE_INSTALL_PREFIX=<temp>/install` puis `--target install` (DepManager
                 recipe_builder.py:115,329), qui n'échappe pas au masquage : `depmanager build .` installe dans
                 Owl/output/install et importe un dossier vide (déduit du code). Gravité relevée à haute.
```

```text
ID             : G-07
Axe            : Build, dépendances, packaging
Nature         : risque
Gravité        : moyenne
Preuve         : source/owl/public/renderer/RenderLayer.h:16 (#include <yaml-cpp/yaml.h>) alors que yaml-cpp
                 est liée en privé (source/owl/CMakeLists.txt:123) et absente de
                 OwlEngineConfig.cmake.in ; OwlDrone déclare yaml-cpp lui-même ; l'axe A relève la même
                 fuite dans Scene.h (10-constats-A-architecture.md, résumé point 5).
Constat        : des headers publics exigent des dépendances privées que le paquet ne déclare pas.
                 Un consommateur doit deviner la version de yaml-cpp à fournir, avec un risque de
                 violation ODR si elle diffère de celle liée statiquement dans libOwlEngine.so.
Recommandation : corriger, effort M (sortir YAML des headers publics, ou déclarer yaml-cpp en
                 dépendance publique avec find_dependency).
Statut         : confirmé
Vérification   : passé de plausible à confirmé : RenderLayer.h:16 inclut `<yaml-cpp/yaml.h>` et rien n'en fournit les headers
                 au consommateur (OwlEngineConfig.cmake.in:4-5 : EnTT et imgui seulement), l'échec sans yaml-cpp est
                 certain. Le risque ODR reste conditionnel à une version divergente.
```

```text
ID             : G-08
Axe            : Build, dépendances, packaging
Nature         : faiblesse
Gravité        : moyenne
Preuve         : tableau des dépendances ci-dessus (mesuré, axeGI/remote_latest.txt) ; sous-modules
                 d'OwlDependencies (`git submodule status` : FreeType VER-2-14-1, libpng v1.6.53, entt
                 v3.16.0, openal-soft 1.25.0, ufbx v0.21.2, nfd v1.3.0, md4c release-0.5.3) ;
                 info.yaml : yaml-cpp et glad construits le 2024-05-22.
Constat        : dix dépendances ont une version plus récente déjà construite et publiée par le projet
                 lui-même, et que personne n'a basculée. Le point qui compte : libpng (1.6.50 contre
                 1.6.53), qui décode des images fournies par l'utilisateur. tinyobjloader reste en
                 pré-release (rc13).
Comparaison    : Dependabot et Renovate savent suivre vcpkg.json et conanfile ; DepManager n'a pas
                 d'équivalent, hormis le script check_updates.py d'OwlDependencies, qui ne regarde que
                 les recettes.
Recommandation : corriger, effort S pour le bump. Effort S aussi pour étendre check_updates.py à la
                 comparaison depmanager.yml / serveur et le brancher en CI.
Statut         : confirmé
Vérification   : remote_latest.txt recoupé : 10 écarts (entt, freetype, libpng, msdfgen, nfd, openal, spdlog, tinygltf,
                 ufbx, zlib). Nuance : 4 sont transitifs et liés statiquement par d'autres paquets (msdf-atlas-gen) ;
                 leur bump peut exiger de republier ces derniers, l'effort S est optimiste pour eux.
```

```text
ID             : G-09
Axe            : Build, dépendances, packaging
Nature         : faiblesse
Gravité        : basse (moyenne avant vérification)
Preuve         : cmake/HelpAssets.cmake:30-50 (file(DOWNLOAD) de badges https au configure) et en-tête
                 :7-20 (écriture dans engine_assets/help/) ; cmake/Poetry.cmake:41-47 (poetry sync à chaque
                 configure) ; cmake/Environment.cmake:21-22 (charge .env et affiche chaque clé=valeur en
                 message(STATUS)) ; cmake/DocumentationConfig.cmake:11.
Constat        : le configure dépend du réseau, de façon non bloquante mais avec un timeout de 15 s
                 par badge. Il écrit dans l'arbre source à deux endroits et modifie le virtualenv. Il
                 recopie aussi en clair dans les logs de CI toute variable d'un .env, secrets compris
                 s'il y en a un. Le .env actuel est vide (0 octet, ignoré par git).
Recommandation : corriger, effort S. Générer l'aide dans ${CMAKE_BINARY_DIR}, versionner les badges ou
                 les rendre localement, masquer les valeurs du .env, sortir poetry sync du configure
                 (c'est le rôle de ci_action.py).
Statut         : confirmé
Vérification   : HelpAssets.cmake:30-50, Poetry.cmake:41-47 et Environment.cmake:21-22 vérifiés. Gravité ramenée à basse :
                 les badges sont en cache (25 fichiers dans engine_assets/help/images/badges/, ignoré par .gitignore:57),
                 un échec réseau n'est pas bloquant, et la fuite de secrets suppose un .env non vide.
```

```text
ID             : G-10
Axe            : Build, dépendances, packaging
Nature         : force
Gravité        : moyenne
Preuve         : source/owl/CMakeLists.txt:44-59 (cibles, FILE_SET HEADERS, BUILD_/INSTALL_INTERFACE) ;
                 cmake/BaseConfig.cmake:17-18 (Base et BaseTest INTERFACE) ; cmake/CompilerCache.cmake ;
                 cmake/CMakePresets*.json (5 fichiers en couches : base → compilateur → OS, métadonnées
                 vendor « silmaen » qui pilotent l'image Docker de la CI) ; 01-environnement.md §1 (build
                 complet en 60 s, configure en 5,3 s).
Constat        : la structure CMake est moderne et cohérente : pas de variables globales de flags, des
                 propriétés propagées par cibles, un ccache câblé, des presets qui servent de source
                 unique à la CI. Les commentaires expliquent les pièges réels rencontrés (MinGW
                 -mbig-obj, MAP_IMPORTED_CONFIG_DEBUG, venv multi-architecture).
Recommandation : garder.
Statut         : confirmé
```

```text
ID             : G-11
Axe            : Build, dépendances, packaging
Nature         : étrangeté
Gravité        : moyenne
Preuve         : cmake/BaseConfig.cmake:97-123 (-Werror -Weverything, puis exclusions par version :
                 -Wno-switch-default et -Wno-unsafe-buffer-usage en ≥18, -Wno-unsafe-buffer-usage-in-libc-call
                 en ≥20, -Wno-nrvo en ≥21) ; GCC : -Wall -Wextra -pedantic seulement (:76-83) ;
                 core/external/*.h (wrappers OWL_DIAG_DISABLE_CLANG autour des headers tiers).
Constat        : la rigueur est réelle, avec zéro warning sous -Weverything (force). Mais chaque
                 nouvelle version de Clang introduit des warnings et casse le build tant qu'une exclusion
                 n'est pas ajoutée : le suivi est déjà de 4 versions. Les headers tiers exigent des
                 wrappers de suppression. GCC et Clang ne voient pas du tout le même jeu de warnings.
Comparaison    : la doc Clang déconseille -Weverything hors exploration. Godot, O3DE ou Bevy (en Rust,
                 via clippy) gèrent une liste explicite de warnings plus -Werror en CI seulement.
                 Marquer les dépendances SYSTEM éviterait les wrappers.
Recommandation : surveiller, effort S. Garder -Weverything, mais ne mettre -Werror qu'en CI (option
                 OWL_WARNINGS_AS_ERRORS) pour qu'un clang neuf ne bloque pas un développeur ; découpler
                 du paquet (G-01).
Statut         : confirmé (comparaison : opinion)
Vérification   : BaseConfig.cmake:97-123 vérifié. Nuance : 3 paliers de version (18, 20, 21), pas 4 ; GCC a lui aussi
                 `-Werror` (:78).
```

```text
ID             : G-12
Axe            : Build, dépendances, packaging
Nature         : risque
Gravité        : moyenne
Preuve         : output/teamcity-argawaen.2026-05-21.private-key.pem et output/git_app_creds présents dans
                 le dossier output/ du dépôt (ignorés par .gitignore:37 « output* ») ; docker/run.sh monte
                 tout le dépôt dans chaque conteneur ; contenu non lu.
Constat        : une clé privée TeamCity et des identifiants d'application GitHub sont stockés dans le
                 dossier des artefacts de build. Ce dossier est partagé avec chaque conteneur et chaque
                 agent IA, et c'est celui qu'on vide ou archive en premier. Le risque est faible mais
                 évitable.
Recommandation : corriger, effort S. Les déplacer hors du dépôt (~/.config, gestionnaire de secrets).
Statut         : confirmé
Vérification   : fichiers absents de output/ aujourd'hui et `git log --all` vide pour ces chemins ; constat juste à la
                 date de rédaction, résolu depuis.
Suivi          : résolu le 2026-10-05, les deux fichiers ont été retirés de `output/` par le mainteneur
                 (vérifié : absents, et jamais présents dans l'historique git).
```

### Gravité basse

```text
ID             : G-13
Axe            : Build, dépendances, packaging
Nature         : étrangeté
Gravité        : basse
Preuve         : depmanager.yml:28-29 (tinyxml2 11.0.0) ; `grep -rn tinyxml source test cmake` ne
                 renvoie rien ; pas de tinyxml2_DIR dans CMakeCache.txt ; depmanager.yml (googletest et
                 libsndfile sans kind, malgré .claude/rules/dependencies.md « set kind for every non
                 header-only ») ; source/owl/public/core/expected.h:27-36 (repli zeus mort).
Constat        : une dépendance déclarée n'est jamais utilisée. Deux contreviennent à la règle sur
                 `kind`. Une autre n'existe plus que pour une branche de préprocesseur inatteignable
                 en C++23.
Recommandation : corriger, effort S (retirer tinyxml2 et zeus ; ajouter les kind).
Statut         : confirmé
```

```text
ID             : G-14
Axe            : Build, dépendances, packaging
Nature         : étrangeté
Gravité        : basse
Preuve         : cmake/OwlUtils.cmake:446,455 (ForceRelease positionné mais jamais lu) et :464-469
                 (commentaire qui reconnaît le no-op) ; .claude/rules/cmake.md (« FORCE_RELEASE — use release
                 build in debug mode ») ; source/owl/CMakeLists.txt:62-63 ; cmake/OwlUtils.cmake:17-384
                 (print_target_properties, 370 lignes de débogage jamais appelées) ; cmake/Python.cmake
                 (6 lignes qui commencent par un `endif ()` orphelin, incluses nulle part, mais toujours
                 citées comme module par l'ancien CLAUDE.md).
Constat        : du code CMake mort ou trompeur. Le drapeau FORCE_RELEASE est passé à toutes les
                 liaisons sans aucun effet, et la règle IA le présente comme fonctionnel.
Recommandation : corriger, effort S.
Statut         : confirmé
```

```text
ID             : G-15
Axe            : Build, dépendances, packaging
Nature         : faiblesse
Gravité        : basse
Preuve         : cmake/BaseConfig.cmake:186-187 (CMAKE_EXE_LINKER_FLAGS construit à partir de
                 CMAKE_SHARED_LINKER_FLAGS, qui contient déjà le rpath, puis rajout du même rpath).
Constat        : sous Linux, les drapeaux d'édition de liens des exécutables écrasent toute valeur
                 utilisateur et contiennent deux fois -rpath='$ORIGIN'. C'est inoffensif aujourd'hui,
                 mais c'est un piège pour qui ajoute un drapeau propre aux exécutables.
Recommandation : corriger, effort S (propriétés BUILD_RPATH/INSTALL_RPATH, ou add_link_options sur Base).
Statut         : confirmé
```

```text
ID             : G-16
Axe            : Build, dépendances, packaging
Nature         : étrangeté
Gravité        : basse
Preuve         : source/owl/CMakeLists.txt:154-173 (copies de DLL par chemins relatifs codés en dur :
                 vulkan-1.dll, slang.dll, OpenAL32$<$<CONFIG:Debug>:d>.dll) ; cmake/BaseConfig.cmake:163
                 (libpng16-16.dll prise dans le dossier du compilateur MinGW) ; :172-179 (-Wa,-mbig-obj
                 global) ; .teamcity/Build/Build.kt:122-135 (Windows x64 en CI, GCC et Clang).
Constat        : la cible MinGW est réellement construite en CI (force). Le chemin statique, lui,
                 copie des DLL à la main. Le suffixe Debug `d` d'OpenAL contredit le mappage Debug →
                 Release de CMakeLists.txt:37-39. Une libpng du toolchain s'ajoute à celle de DepManager.
Recommandation : surveiller, effort S ($<TARGET_RUNTIME_DLLS> partout, retirer libpng16-16.dll de la liste).
Statut         : plausible (Windows non exécuté dans cet audit)
```

```text
ID             : G-17
Axe            : Build, dépendances, packaging
Nature         : risque
Gravité        : basse
Preuve         : output/build/linux-clang-release/CMakeCache.txt : PNG_PNG_INCLUDE_DIR:PATH=/usr/include,
                 ZLIB_INCLUDE_DIR:PATH=/usr/include (mesuré), alors que PNG_DIR et ZLIB_DIR pointent vers
                 /fhome/.edm/data/….
Constat        : un module Find (sans doute via freetype ou msdf-atlas-gen) a résolu les headers
                 libpng et zlib du système de l'image, pas ceux de DepManager. Un écart de version entre
                 headers et bibliothèque est possible selon l'image.
Recommandation : surveiller, effort S (forcer CMAKE_FIND_PACKAGE_PREFER_CONFIG ou vider ces variables).
Statut         : plausible
```

```text
ID             : G-18
Axe            : Build, dépendances, packaging
Nature         : étrangeté
Gravité        : basse
Preuve         : source/owl/CMakeLists.txt:6-8 et :31-34, test/CMakeLists.txt:2,13 (GLOB_RECURSE
                 CONFIGURE_DEPENDS) ; .claude/rules/cmake.md (« Always use file(GLOB_RECURSE …) »).
Constat        : les globs avec CONFIGURE_DEPENDS fonctionnent avec Ninja et éliminent la tenue des
                 listes, un gain réel pour un développeur seul. Revers : un re-glob à chaque build, tout
                 fichier .cpp déposé sous source/owl compilé dans le moteur, et la doc CMake qui
                 déconseille officiellement la pratique.
Recommandation : garder, à surveiller si des contributeurs arrivent.
Statut         : confirmé (jugement : opinion)
```

```text
ID             : G-19
Axe            : Build, dépendances, packaging
Nature         : étrangeté
Gravité        : basse
Preuve         : racine : .env (0 octet), Owl.log (0 octet), CMakeUserPresets.json (sans preset),
                 Doxyfile (généré) ; tous ignorés par .gitignore:37-53 (`git check-ignore -v`) ;
                 CMakeLists.txt:8 (0.2.2) contre owl_engine.py:15 (0.2.1) et README.md:3 (badge 0.2.1) ; le
                 bump 0.2.2 vient du commit 1991cc25 « Upgrade clang-tidy on diff » (sur Feature/Bench, pas
                 sur main qui reste en 0.2.1).
Constat        : les fichiers parasites à la racine sont purement locaux, sans impact sur le dépôt.
                 Le numéro de version, en revanche, vit à quatre endroits (CMake, recette, badge, test
                 version_test.cpp) et a été changé au détour d'un commit CI, contre la règle « kickoff
                 PR first ».
Recommandation : corriger, effort S. Source unique de version : la recette et le test la lisent
                 depuis CMake.
Statut         : confirmé
```

### Axe I — Documentation

```text
ID             : I-01
Axe            : Documentation
Nature         : faiblesse
Gravité        : moyenne
Preuve         : tableau « Contrôle des pages » (97 affirmations, 64 exactes, 20 inexactes, 12 obsolètes) ;
                 doc/pages/renderer.md:22-38 (sous-namespaces renderer::stack/renderer2d/rendererraycast,
                 inexistants : tout est dans owl::renderer, RenderStack.h:15) ; renderer.md:438
                 (Mesh3DVertex 36 o contre static_assert 56 à Renderer3D.cpp:41) ; architecture.md:115,300
                 et getting_started.md:50 (menus devenus un ruban, EditorLayer.cpp:985-1211).
Constat        : un tiers des affirmations contrôlées est faux ou périmé. La dérive vient surtout des
                 refontes (ruban de l'éditeur, renderer 3D, triggers, API Lua) qui n'ont pas repris les
                 pages existantes, malgré la règle « update the relevant page in the same PR ». Les pages
                 les plus récentes ou les plus ciblées (CI, node graph, son, voxel) sont fidèles.
Comparaison    : Godot génère sa référence de classes depuis le code (doc/classes/*.xml) et la CI échoue
                 si une classe n'est pas à jour. La prose reste hors de ce contrôle chez lui aussi.
Recommandation : corriger, effort M. Corriger renderer.md, scripting.md et architecture.md en priorité,
                 puis ajouter un contrôle automatique des identifiants cités en `code` dans doc/pages
                 (grep des symboles et des chemins, effort S).
Statut         : confirmé
Vérification   : échantillon recontrôlé : renderer.md:22-24 cite des sous-namespaces que grep ne trouve nulle part ;
                 renderer.md:438 (36 octets) contre `static_assert` 56 (Renderer3D.cpp:41) ; getting_started.md:50
                 « File → Open Project ». Le total 97/64/20/12 n'a pas été recompté page par page.
```

```text
ID             : I-02
Axe            : Documentation
Nature         : faiblesse
Gravité        : moyenne
Preuve         : doc/pages/scripting.md:105-118 (on_collision(other_id) « appelé lors d'une collision ») ;
                 `grep -rn onCollision source` ne trouve que la déclaration (ScriptInstance.h:108) et la
                 définition (ScriptInstance.cpp:93), aucun appelant ; scripting.md:49,191-205 (13 tables
                 documentées contre 15 enregistrées, LuaBindings.cpp:987-989 ; has_component : 6 noms
                 documentés contre 14 acceptés, LuaBindings.cpp:378-404).
Constat        : la référence de l'API Lua, la seule surface de programmation d'un auteur de jeu, promet
                 un callback que le moteur ne déclenche jamais et omet un quart des bindings. Un
                 utilisateur qui écrit on_collision n'obtiendra aucune erreur et aucun effet.
Comparaison    : Defold et LÖVE génèrent leur référence Lua depuis les sources de bindings
                 (script_doc, love-api).
Recommandation : corriger, effort S pour la page (et câbler ou retirer onCollision, axe D). Effort M
                 pour générer la référence depuis LuaBindings.cpp.
Statut         : confirmé
Vérification   : grep sur tout le dépôt : `onCollision` n'est appelé que par les tests (test/script_tests/ScriptInstance_test.cpp,
                 LuaEngine_test.cpp), jamais par source/ ; LuaBindings.cpp:985-989 enregistre bien trigger, door et pushwall.
```

```text
ID             : I-03
Axe            : Documentation
Nature         : faiblesse
Gravité        : moyenne
Preuve         : README.md:28 (« gamepad ») contre `grep -rni "gamepad|joystick" source/owl` (une seule
                 ligne commentée, UiLayer.cpp:75) ; README.md:54-55 et building.md:15 (GCC 14+ / Clang 22+)
                 contre cmake/BaseConfig.cmake:66-69 (minimum 13 et 18, bloquant sous 11 et 15) ;
                 README.md:35-45 (fonctionnalités sans voxel, raycast ni tilemap) ; README.md:72-82 (Quick
                 Start sans l'étape `depmanager remote add` indispensable, building.md:29-46, sans Doxygen
                 ni Docker).
Constat        : la vitrine du dépôt promet une fonction absente, donne trois minima de compilateur
                 différents selon le fichier (README, building.md, BaseConfig) et omet ce qui distingue
                 aujourd'hui le moteur. Son Quick Start échoue sur une machine neuve faute de remote
                 DepManager configuré.
Recommandation : corriger, effort S.
Statut         : confirmé
Vérification   : README.md:28, :54-55, :72-82, building.md:15 et BaseConfig.cmake:66-69 vérifiés ; depmanager.yml ne déclare
                 aucun serveur. Nuance : README et building.md concordent (GCC 14 / Clang 22), il y a donc deux minima
                 (doc contre code), pas trois ; « gamepad » figure dans la ligne de lien vers event_input.md.
```

```text
ID             : I-04
Axe            : Documentation
Nature         : faiblesse
Gravité        : moyenne
Preuve         : CONTRIBUTING.md:60 (« Run clang-tidy: cmake --preset linux-clang-tidy && cmake --build … »)
                 contre cmake/Sanitizers.cmake:22-27 (CMAKE_CXX_CLANG_TIDY volontairement non branché ;
                 l'analyse passe par `ci_action.py ClangTidy`) ; .claude/rules/dependencies.md:27 (`depmanager
                 pack ls <remote>`). Mesuré : `docker/run.sh poetry run depmanager pack ls default` donne
                 « error: unrecognized arguments: default » ; la forme valide est `--default` ou `--name`.
                 CONTRIBUTING.md (84 lignes) et doc/pages/contributing.md (91 lignes) se recouvrent.
Constat        : le guide du contributeur prescrit une vérification devenue sans effet : le build passe,
                 mais clang-tidy n'est pas lancé. Une règle IA prescrit une commande invalide. Deux
                 guides de contribution coexistent et divergent.
Recommandation : corriger, effort S (renvoyer à `ci_action.py ClangTidy`, corriger la CLI, fusionner les
                 deux guides : la page doc/ suffit, la racine y renvoie).
Statut         : confirmé
Vérification   : la consigne est à CONTRIBUTING.md:47 (et non :60) ; Sanitizers.cmake:22-27 confirme le hook débranché.
                 DepManager command/pack.py:200-235 n'accepte aucun remote positionnel (`-n`/`-d` seulement) ; le
                 CLAUDE.md du projet porte la même forme invalide.
```

```text
ID             : I-05
Axe            : Documentation
Nature         : faiblesse
Gravité        : basse (moyenne avant vérification)
Preuve         : SECURITY.md:5-9 (0.1.x « Yes ») ; `git branch -a` : seulement main et Feature/*, aucune
                 branche release/0.1 ; tags 0.1.0 et 0.1.1 seulement ; SECURITY.md:15 (« Send an email to
                 the project maintainer », sans adresse) ; SECURITY.md:18 (accusé de réception promis
                 sous 48 h).
Constat        : la politique annonce le support de deux lignes de version, alors qu'aucun correctif
                 n'a jamais été rétroporté et qu'aucune infrastructure ne le permet. Elle ne donne aucun
                 canal de contact, et sa promesse de délai est irréaliste pour un projet personnel.
Comparaison    : GitHub propose le « private vulnerability reporting ». Les petits projets (raylib,
                 par exemple) ne déclarent supportée que la dernière version.
Recommandation : corriger, effort S (« dernière version mineure seulement », activer le signalement
                 privé GitHub, retirer les 48 h).
Statut         : confirmé
Vérification   : SECURITY.md:5-9, :15 et :18 et `git branch -a` vérifiés ; nuance : les tags vont de 0.0.1 à 0.2.1, pas
                 seulement 0.1.x. Gravité ramenée à basse : moteur à usage local, sans utilisateur externe connu ; le
                 défaut est rédactionnel.
```

```text
ID             : I-06
Axe            : Documentation
Nature         : faiblesse
Gravité        : moyenne
Preuve         : mesuré par script (axeGI) sur source/owl/public : 196 headers, 16 579 lignes de
                 commentaire Doxygen contre 8 642 lignes de code, soit 66 % de doc ; 2 513 @brief, dont les
                 plus fréquents : « Default constructor. » (60), « Destructor. » (51), « Constructor. » (34),
                 « Default destructor. » (31), « Write this component to a YAML context. » (32), « Get the
                 YAML key for this component. » (27) ; DoxyfileTemplate : EXTRACT_PRIVATE=YES,
                 WARN_NO_PARAMDOC=YES, WARN_IF_UNDOC_ENUM_VAL=YES, WARN_AS_ERROR=YES ; CodeStyle audite en
                 plus le /// des membres privés m_*.
Constat        : l'exigence « tout documenter, y compris le privé, avec erreur au moindre manque »
                 produit au moins 300 commentaires de remplissage de 4 à 5 lignes. Les headers sont deux
                 fois plus longs que le code qu'ils déclarent, ce qui pèse sur la lecture, la revue et le
                 contexte des agents IA. Ce volume n'a pas empêché I-01 et I-02 : la doc qui manque est
                 celle de l'usage, pas celle des destructeurs.
Comparaison    : Bevy (Rust) active `missing_docs` sur l'API publique seulement. Godot exige une
                 description par méthode exposée, mais pas pour les membres internes. La plupart des
                 projets C++ (LLVM, Abseil) ne documentent pas les membres spéciaux triviaux.
Recommandation : corriger, effort S. Exempter constructeurs, destructeurs, opérateurs
                 =default/=delete et les surcharges virtuelles déjà documentées dans la base
                 (@copydoc implicite) ; EXTRACT_PRIVATE=NO ; garder WARN_AS_ERROR sur l'API publique.
                 (Opinion : le gain de lisibilité dépasse la perte de complétude.)
Statut         : confirmé (mesure) ; recommandation : opinion
Vérification   : recompte : 2 514 `@brief`, dont « Default constructor. » 60, « Destructor. » 51, « Constructor. » 34,
                 « Default destructor. » 31 ; DoxyfileTemplate:528, :881, :889 et :905 à YES. Ratio 66 % non recompté.
```

```text
ID             : I-07
Axe            : Documentation
Nature         : étrangeté
Gravité        : basse (moyenne avant vérification)
Preuve         : doc/pages/roadmap.md:386-423 (v0.2.2 isométrique « Planned », cohérent avec la branche non
                 mergée Feature/KickoffIsometricRenderer, commit f39a44f1) ; roadmap.md:13-56 (efforts
                 « continus » marqués ![Planned] en permanence, plus deux items Profiling et Rendering
                 optimizations sans version) ; le ClangTidy par diff (CHANGELOG [Unreleased] Added) absent
                 de la roadmap ; ~/.claude/CLAUDE.md impose un ROADMAP.md à la racine, alors que le projet
                 utilise doc/pages/roadmap.md (point laissé ouvert par .claude/rules/documentation.md).
Constat        : l'état des versions est juste : rien de l'isométrique n'est marqué fait. Mais la
                 section « Ongoing » détourne le badge Planned pour des engagements permanents, et la
                 roadmap de 729 lignes sert à la fois de spec détaillée et de suivi. L'emplacement du
                 fichier contredit la consigne globale ; la règle projet le signale comme point ouvert.
Recommandation : corriger, effort S. Badge dédié « Ongoing » ; specs détaillées déplacées dans des pages
                 de design ; trancher l'emplacement (décision du mainteneur).
Statut         : confirmé
Vérification   : roadmap.md:12-30 (engagements permanents sous `![Planned]`), 729 lignes. Gravité ramenée à basse : défaut de
                 présentation sans effet sur l'exactitude, ce que le constat reconnaît lui-même.
```

```text
ID             : I-08
Axe            : Documentation
Nature         : étrangeté
Gravité        : basse
Preuve         : CHANGELOG.md:152-625 (section 0.1.1 : 474 lignes, 54 puces, 133 sous-puces, 276 lignes de
                 continuation, mesuré par script) ; sections 0.2.0 et 0.2.1 : 29 puces chacune, 43 et 3
                 lignes de continuation ; puces jusqu'à 233 caractères ([Unreleased]) ;
                 .claude/rules/documentation.md (règle désormais alignée sur ~/.claude/CLAUDE.md :
                 « One line per change, one sentence … No nested sub-lists » ; elle autorisait
                 auparavant les sous-puces, `git show main:.claude/rules/documentation.md`).
Constat        : le format Keep a Changelog est respecté (catégories, dates, [Unreleased]), et les
                 sections récentes se rapprochent de la règle « une ligne » (force). La section 0.1.1 est
                 un rapport de chantier, écrit quand la règle projet autorisait les sous-puces ; la
                 contradiction avec la règle globale vient d'être levée sur disque.
Recommandation : garder la règle actuelle ; 0.1.1 peut rester tel quel (historique). Effort nul.
Statut         : confirmé
```

```text
ID             : I-09
Axe            : Documentation
Nature         : faiblesse
Gravité        : basse
Preuve         : DoxyfileTemplate:947-951 (INPUT en ../../../, qui suppose binaryDir = output/build/<preset>) ;
                 DoxyfileTemplate:174 (INPUT_FILTER "python3 …", Python système, contre la règle « always
                 poetry run ») ; building.md:187-204 et .claude/rules/cmake.md présentent
                 OWL_ENABLE_DOCUMENTATION et OWL_PACKAGING comme des options, alors qu'aucun option() ne les
                 déclare ; OWL_USE_CCACHE (CMakeLists.txt:24) et OWL_ENABLE_MEMORY_TRACKER absents de
                 building.md.
Constat        : la génération Doxygen ne marche que pour une profondeur de dossier de build précise et
                 repose sur l'interpréteur système. Le tableau des options CMake, recopié dans trois
                 fichiers (building.md, CLAUDE.md, cmake.md), a divergé du code.
Recommandation : corriger, effort S (chemins @PROJECT_SOURCE_DIR@ dans le template, filtre via
                 ${Python3_EXECUTABLE}, un seul tableau d'options généré ou référencé).
Statut         : confirmé
```

```text
ID             : I-10
Axe            : Documentation
Nature         : force
Gravité        : moyenne
Preuve         : 16 pages doc/pages (7 602 lignes avec README, CHANGELOG, CONTRIBUTING et SECURITY) ;
                 mermaid + doxygen-awesome (doc/header.html, doc/css/) ; doc/fix_md_links.py (liens .md
                 convertis en @ref) ; cmake/HelpAssets.cmake (pages embarquées dans l'aide de l'éditeur,
                 F1 contextuel : EditorLayer.cpp:624) ; continuous_integration.md, node_graph.md, sound.md
                 exacts à 100 % ou presque.
Constat        : pour un projet personnel, la documentation est abondante, structurée par sous-système,
                 lisible sur GitHub comme en HTML, et accessible depuis l'éditeur. Les pages écrites avec
                 la fonctionnalité (CI, node graph, voxel) sont précises au niveau des noms et des valeurs.
Comparaison    : c'est plus que raylib (cheatsheet + exemples) ou Hazel (aucune doc utilisateur), et dans
                 l'esprit du manuel de Godot, à une autre échelle.
Recommandation : garder, et protéger par le contrôle automatique proposé en I-01.
Statut         : confirmé
```

## Comparaison brève des gestionnaires de dépendances

Opinion argumentée, appuyée sur ma connaissance des outils (jusqu'à la mi-2026, à revérifier avant toute décision).

| Critère                    | DepManager (actuel)                    | vcpkg (manifest)                                                   | Conan 2                                        | CPM.cmake                                   |
|----------------------------|----------------------------------------|--------------------------------------------------------------------|------------------------------------------------|---------------------------------------------|
| Catalogue                  | 46 recettes maison (OwlDependencies)   | plus de 2 000 ports publics + registres privés                     | ConanCenter (~1 500 recettes) + remotes privés | tout dépôt git ou archive                   |
| Reproductibilité           | nom + version + ABI + glibc, sans hash | baseline (commit) + SHA512 des sources + hash d'ABI                | révisions RREV/PREV + lockfile                 | tag ou commit git, compilé depuis sources   |
| Binaires précompilés       | oui, serveur privé                     | binary caching (fichiers, NuGet, HTTP)                             | oui, remotes Artifactory ou conan_server       | non (ccache seulement)                      |
| Intégration CMake          | dm_load_environment + wrapper maison   | toolchain file, find_package natif                                 | CMakeDeps/CMakeToolchain                       | add_subdirectory / FetchContent             |
| Bus factor                 | 1                                      | Microsoft + communauté                                             | JFrog + communauté                             | communauté, outil minimal                   |
| Coût de migration pour Owl | —                                      | M : la plupart des 34 libs existent en port (à vérifier une à une) | M : recettes similaires à celles de DepManager | S pour peu de libs, L pour Vulkan SDK/Slang |
| Point faible pour Owl      | reproductibilité, isolement            | Slang et imgui_color_text_edit sans doute en overlay               | courbe d'apprentissage, profils                | temps de build à froid                      |

Lecture (opinion) : DepManager rend un vrai service (paquets précompilés pour 3 cibles, intégration en une ligne) et
son code est modeste. Le danger n'est pas l'outil, c'est l'absence de traçabilité binaire et la dépendance à
une seule personne et à un seul serveur. vcpkg en manifest, avec un registre privé pour les quelques recettes
maison (Slang via le Vulkan SDK, imgui docking, zeus), est l'alternative la plus proche : binaires en cache,
CMake natif, `test_package` implicite par port.

## Pistes pour l'avenir

1. **Paquet moteur consommable** (S puis M) : corriger G-01 et G-02, puis ajouter un `test_package` à l'action
   `Package`, qui compile un mini-jeu avec `find_package(OwlEngine)`. Republier ensuite et migrer OwlDrone. C'est la
   condition préalable à tout « écosystème » (axe K).
2. **Traçabilité des dépendances** (M) : lockfile DepManager (version + hash du tarball + date de build) commité,
   `pull-newer: false` en CI, compilateur enregistré dans `info.yaml`. Ces ajouts sont à faire dans DepManager
   lui-même : ils servent aussi aux autres projets.
3. **Veille de versions automatisée** (S) : étendre `check_updates.py` à la comparaison `depmanager.yml` / serveur /
   amont, avec un rapport hebdomadaire en CI. libpng et les autres dépendances qui décodent des données externes
   sont prioritaires.
4. **Configure minimal et hermétique** (S) : options réelles (`OWL_ENABLE_DOCUMENTATION`, `OWL_WARNINGS_AS_ERRORS`),
   aucune écriture dans l'arbre source, aucun réseau, `CMAKE_INSTALL_PREFIX` respecté.
5. **Évaluer vcpkg manifest + registre privé** (L, en parallèle) : prototype sur une branche avec 5 dépendances
   (spdlog, yaml-cpp, glfw, box2d, Vulkan SDK). Mesurer le build à froid, le build à chaud avec binary cache et
   l'effort sur Slang. Décider sur chiffres.
6. **Documentation vérifiée par la CI** (S puis M) : script qui vérifie que chaque identifiant ou chemin cité en `code`
   dans `doc/pages` existe dans le code ; référence Lua générée depuis `LuaBindings.cpp` ; tableau des options CMake
   généré depuis les `option()`.
7. **Doxygen proportionné** (S) : exemptions pour les membres spéciaux triviaux et le privé. L'effort ainsi libéré
   va aux pages d'usage, celles qui ont dérivé.
8. **Une seule source de version** (S) : `project(VERSION)`, lue par la recette, le test, le badge (généré) et
   SECURITY.md (« dernière mineure »).
