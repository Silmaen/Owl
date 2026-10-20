# Audit Owl — constats des axes G (build, dépendances, packaging) et I (documentation)

> Périmètre : `CMakeLists.txt`, `cmake/`, presets, `conanfile.py`, `conan.lock`, `CPackConfig.cmake`, `doc/pages/*.md`,
> `README.md`, `CHANGELOG.md`, `CONTRIBUTING.md`, `SECURITY.md`, `DoxyfileTemplate`. Revu le 2026-10-20 sur `main`
> 4e1a6a10, lecture seule, outils via `./docker_run.sh`. **mesuré** = commande reproductible ; **constaté** = lu dans le
> code ; **opinion** = jugement ; **confirmé** = vérifié. Dépendances : Conan 2 (`conanfile.py`, `conan.lock`).

## Résumé

- CMake moderne (cibles, `FILE_SET`, presets en couches pilotant la CI) et `-Weverything -Werror` tenu à zéro (G-10, G-11).
- Défauts mineurs de build : configure dépendant du réseau, `-rpath` dupliqué, DLL Windows codées en dur, headers
  libpng / zlib système (G-09, G-15, G-16, G-17).
- Documentation abondante et accessible depuis l'éditeur (I-10) ; dérives possibles de la référence Lua et des pages
  (I-01, I-02) ; `@brief` de remplissage sur l'API publique (I-06).
- 14 constats : 0 haut, 3 moyens, 11 bas ; 2 forces.

## Constats — gravité moyenne

```text
ID             : G-10 — force, moyenne, confirmé (build)
Constat        : structure CMake moderne et cohérente : propriétés par cibles, ccache câblé, presets (5 fichiers en
                 couches, métadonnées `vendor.silmaen` qui pilotent l'image Docker de la CI) comme source unique de la CI.
                 Les commentaires expliquent les vrais pièges (MinGW `-mbig-obj`, `MAP_IMPORTED_CONFIG_DEBUG`, venv
                 multi-architecture). Build complet en 60 s, configure en 5,3 s (`00-cadrage.md`).
Preuve         : source/owl/CMakeLists.txt:44-59 ; cmake/BaseConfig.cmake:17-18 ; cmake/CompilerCache.cmake ;
                 cmake/CMakePresets*.json.
Recommandation : garder
```

```text
ID             : G-11 — étrangeté, moyenne, confirmé (build)
Constat        : zéro warning sous `-Weverything -Werror` (force), mais chaque nouveau Clang casse le build tant qu'une
                 exclusion n'est pas ajoutée (paliers 18, 20, 21) ; les headers tiers exigent des wrappers de
                 suppression ; GCC et Clang ne voient pas le même jeu de warnings. Marquer les dépendances SYSTEM éviterait
                 les wrappers.
Preuve         : cmake/BaseConfig.cmake:97-123 ; core/external/*.h (`OWL_DIAG_DISABLE_CLANG`).
Recommandation : surveiller, effort S (garder `-Weverything`, `-Werror` en CI seulement via `OWL_WARNINGS_AS_ERRORS`).
```

## Constats — gravité basse

```text
ID             : G-09 — faiblesse, basse, confirmé (build)
Constat        : le configure télécharge des badges d'aide (non bloquant, timeout 15 s par badge) : dépendance au réseau
                 au premier configure. Le `.env` n'affiche que ses clés, aucune valeur.
Preuve         : cmake/HelpAssets.cmake:43-46 ; cmake/Environment.cmake:21-22.
Recommandation : corriger, effort S (versionner les badges).
```

```text
ID             : G-15 — faiblesse, basse, confirmé (build)
Constat        : sous Linux, `CMAKE_EXE_LINKER_FLAGS` écrase toute valeur utilisateur et contient deux fois
                 `-rpath='$ORIGIN'` : inoffensif aujourd'hui, piège pour un futur drapeau propre aux exécutables.
Preuve         : cmake/BaseConfig.cmake:186-187.
Recommandation : corriger, effort S (`BUILD_RPATH` / `INSTALL_RPATH` ou `add_link_options` sur Base).
```

```text
ID             : G-16 — étrangeté, basse, plausible (build, Windows non exécuté)
Constat        : MinGW est construit en CI (force), mais le chemin statique copie des DLL à la main ; le suffixe Debug `d`
                 d'OpenAL contredit le mappage Debug → Release ; une libpng du toolchain s'ajoute à celle de Conan.
Preuve         : source/owl/CMakeLists.txt:154-173 ; cmake/BaseConfig.cmake:163, 172-179 ; CMakeLists.txt:37-39.
Recommandation : surveiller, effort S (`$<TARGET_RUNTIME_DLLS>`, retirer `libpng16-16.dll`).
```

```text
ID             : G-17 — risque, basse, plausible (build)
Constat        : les headers libpng et zlib résolus sont ceux du système de l'image (`/usr/include`, mesuré dans le
                 CMakeCache) et non ceux du cache de dépendances ; écart de version possible avec la bibliothèque.
Preuve         : output/build/linux-clang-release/CMakeCache.txt (`PNG_PNG_INCLUDE_DIR`, `ZLIB_INCLUDE_DIR`).
Recommandation : surveiller, effort S (`CMAKE_FIND_PACKAGE_PREFER_CONFIG` ou vider ces variables).
```

```text
ID             : G-18 — étrangeté, basse, confirmé (build)
Constat        : `GLOB_RECURSE CONFIGURE_DEPENDS` supprime la tenue des listes de fichiers (gain réel en solo) mais re-glob
                 à chaque build, compile tout `.cpp` déposé sous `source/owl` et va contre la doc CMake (opinion).
Preuve         : source/owl/CMakeLists.txt:6-8, 31-34 ; test/CMakeLists.txt:2,13 ; .claude/rules/cmake.md.
Recommandation : garder, à surveiller si des contributeurs arrivent.
```

```text
ID             : G-19 — étrangeté, basse, confirmé (build)
Constat        : `Owl.log` est écrit dans le répertoire courant (ignoré par `.gitignore`), sans impact sur le dépôt.
Preuve         : racine du dépôt.
Recommandation : surveiller, effort S (écrire le log hors de la racine).
```

```text
ID             : I-01 — faiblesse, basse, plausible (documentation)
Constat        : des pages peuvent dériver après des refontes (ruban de l'éditeur, triggers, API Lua) malgré la règle de
                 mise à jour dans la même PR ; exactitude page par page non recomptée, pages récentes (CI, node graph, son,
                 voxel) fidèles à l'échantillon.
Preuve         : doc/pages/architecture.md, getting_started.md (menus, ruban non revérifié) ; doc/pages/renderer.md.
Recommandation : surveiller, effort S (contrôle automatique des identifiants et chemins cités en `code`).
```

```text
ID             : I-02 — faiblesse, basse, plausible (documentation)
Constat        : la référence de l'API Lua omettrait une partie des bindings : 13 tables documentées contre 15
                 enregistrées, `has_component` 6 noms contre 14 acceptés (comptes non revérifiés).
Preuve         : doc/pages/scripting.md:49,191-205 ; LuaBindings.cpp:378-404, 987-989.
Recommandation : corriger, effort S (page) ; M (référence générée depuis `LuaBindings.cpp`).
```

```text
ID             : I-03 — faiblesse, basse, confirmé (documentation)
Constat        : README et `building.md` annoncent les versions testées (GCC 14+, Clang 22+) ; le code impose des minima plus
                 bas (GCC 13, Clang 18), `building.md` précise que CMake avertit en dessous.
Preuve         : README.md:94-95 ; building.md:28 ; cmake/BaseConfig.cmake:68-69.
Recommandation : surveiller, effort S (distinguer « testé » et « minimum » dans le README).
```

```text
ID             : I-06 — faiblesse, basse, confirmé (documentation)
Constat        : l'exigence « tout documenter, erreur au moindre manque » produit des `@brief` de remplissage
                 (« Default constructor. », plusieurs dizaines) sur les membres spéciaux triviaux : poids de lecture, de
                 revue et de contexte IA.
Preuve         : DoxyfileTemplate (`EXTRACT_PRIVATE=NO`, `WARN_NO_PARAMDOC=YES`, `WARN_AS_ERROR=YES`) ; source/owl/public.
Recommandation : corriger, effort S (exempter constructeurs, destructeurs, `=default` / `=delete`, surcharges déjà
                 documentées ; garder `WARN_AS_ERROR` ; opinion).
```

```text
ID             : I-08 — étrangeté, basse, confirmé (documentation)
Constat        : Keep a Changelog est respecté ; la section 0.1.1 de la page changelog (133 sous-puces) est un rapport de
                 chantier hors de la règle « une ligne par changement ». Le `CHANGELOG.md` racine tient en une ligne par
                 release.
Preuve         : doc/pages/changelog.md ; .claude/rules/documentation.md.
Recommandation : garder la règle ; 0.1.1 reste tel quel (effort nul).
```

```text
ID             : I-10 — force, moyenne, confirmé (documentation)
Constat        : documentation abondante pour un projet personnel : 16 pages structurées par sous-système, mermaid +
                 doxygen-awesome, lisible sur GitHub comme en HTML, embarquée dans l'aide de l'éditeur (F1 contextuel). Les
                 pages écrites avec la fonctionnalité (CI, node graph, voxel) sont précises.
Preuve         : doc/pages (7 602 lignes avec README, CHANGELOG, CONTRIBUTING, SECURITY) ; doc/fix_md_links.py ;
                 cmake/HelpAssets.cmake ; EditorLayer.cpp:624.
Recommandation : garder (protéger par le contrôle de I-01).
```
