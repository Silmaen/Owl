# Audit Owl — constats des axes G (build, dépendances, packaging) et I (documentation)

> Périmètre : `CMakeLists.txt`, `cmake/`, presets, `conanfile.py`, `conan.lock`, `conan/recipes/`, `CPackConfig.cmake`,
> `doc/pages/*.md`, `README.md`, `CHANGELOG.md`, `DoxyfileTemplate`, `tools/doc/`. Revu le 2026-11-12 sur
> `Feature/sample-games-new` b4281d72, lecture seule, outils via `./docker_run.sh`. **mesuré** = commande reproductible ;
> **constaté** = lu dans le code ; **opinion** = jugement ; **confirmé** = vérifié. Dépendances : Conan 2
> (`conanfile.py`, `conan.lock`).

## Résumé

- CMake moderne (cibles, `FILE_SET`, presets en couches pilotant la CI), `-Weverything -Werror` tenu à zéro, Conan 2
  verrouillé et modulaire (G-10, G-11, G-23).
- Défauts mineurs : DLL Windows copiées à la main, deux bibliothèques YAML, rendu mermaid maison qui peut casser le
  configure (G-16, G-20, G-21).
- Documentation abondante (77 pages + 34 pages de conception), référence Lua générée et gardée par un test, aide
  contextuelle dans l'éditeur (I-10) ; `@brief` de remplissage (I-06), contradiction UUID / handle (I-11).
- 12 constats : 0 haut, 4 moyens, 8 bas ; 3 forces, 2 faiblesses, 0 risque, 7 étrangetés.

## Constats — gravité moyenne

```text
ID             : G-10 — force, moyenne, confirmé (build)
Constat        : structure CMake moderne et cohérente : propriétés par cibles, `FILE_SET HEADERS`, ccache câblé, presets
                 (5 fichiers en couches, bloc `vendor.silmaen` lu par `BuildAndTest`) comme source unique de la CI. Les
                 commentaires expliquent les vrais pièges (MinGW `-mbig-obj`, `MAP_IMPORTED_CONFIG_DEBUG`). Build complet
                 en 60 s, configure en 5,3 s (`00-cadrage.md`, non remesuré).
Preuve         : source/owl/CMakeLists.txt:77, 243 ; cmake/BaseConfig.cmake:208 ; cmake/CompilerCache.cmake ;
                 CMakeLists.txt:84 ; cmake/CMakePresets*.json.
Recommandation : garder
```

```text
ID             : G-11 — étrangeté, moyenne, confirmé (build)
Constat        : zéro warning sous `-Weverything -Werror` (force), mais chaque nouveau Clang casse le build tant qu'une
                 exclusion n'est pas ajoutée (paliers 18, 20, puis les `-Wlifetime-safety-*`) ; les headers tiers exigent
                 des wrappers de suppression ; GCC et Clang ne voient pas le même jeu de warnings. `-Werror` n'est pas
                 débrayable pour un développeur.
Preuve         : cmake/BaseConfig.cmake:83, 102-139 ; core/external/*.h (12 wrappers `OWL_DIAG_DISABLE_CLANG`).
Recommandation : surveiller, effort S (garder `-Weverything`, `-Werror` en CI seulement via une option).
```

```text
ID             : G-23 — force, moyenne, confirmé (dépendances)
Constat        : Conan 2 seul, `conan.lock` à jour (luau, joltphysics, plus de Box2D ni de Lua), dépendances
                 conditionnées par module (23 paquets au lieu de 47 en `linux-clang-minimal`), paquet testé
                 (`test/package`). Contrepartie : 16 recettes maison dans `conan/recipes/` à suivre.
Preuve         : conanfile.py:125-180 ; conan.lock ; conan/recipes/ ; doc/pages/building.md:443 ; test/package.
Recommandation : garder (surveiller la charge des recettes maison).
```

```text
ID             : I-10 — force, moyenne, confirmé (documentation)
Constat        : documentation abondante pour un projet personnel : 77 pages et 34 pages de conception (~21 400 lignes),
                 un niveau de lecture par page, mermaid + doxygen-awesome, lisible sur GitHub comme en HTML, embarquée
                 dans l'aide de l'éditeur (F1 contextuel), référence Lua générée du registre et gardée par un test qui
                 échoue à la moindre dérive.
Preuve         : doc/pages ; doc/pages/lua-api.md:7 ; script_tests/LuaBindingRegistry_test.cpp ; cmake/HelpAssets.cmake ;
                 source/owlnest/sources/EditorLayer.cpp:695.
Recommandation : garder
```

## Constats — gravité basse

```text
ID             : G-16 — étrangeté, basse, plausible (build, Windows non exécuté)
Constat        : Windows Clang est construit en CI (force), mais les DLL de Vulkan, Slang et OpenAL sont copiées à la main à
                 côté de `$<TARGET_RUNTIME_DLLS>` ; le suffixe Debug `d` d'OpenAL contredit le mappage Debug → Release ;
                 une `libpng16-16.dll` du toolchain s'ajoute à la libpng de Conan.
Preuve         : source/owl/CMakeLists.txt:196-224 ; cmake/BaseConfig.cmake:185-199 ; CMakeLists.txt:84.
Recommandation : surveiller, effort S (`$<TARGET_RUNTIME_DLLS>` seul, retirer `libpng16-16.dll`).
```

```text
ID             : G-18 — étrangeté, basse, confirmé (build)
Constat        : `GLOB_RECURSE CONFIGURE_DEPENDS` supprime la tenue des listes de fichiers (gain réel en solo) mais re-glob
                 à chaque build, compile tout `.cpp` déposé sous `source/` et va contre la doc CMake (opinion).
Preuve         : source/owl/CMakeLists.txt:6-13 ; source/owlnest/CMakeLists.txt:6-81 ; test/unit/CMakeLists.txt:65 ;
                 .claude/rules/cmake.md.
Recommandation : garder, à surveiller si des contributeurs arrivent.
```

```text
ID             : G-20 — étrangeté, basse, confirmé (dépendances)
Constat        : deux bibliothèques YAML : rapidyaml lit scènes, prefabs et snapshots, yaml-cpp fait tout le reste (150
                 fichiers) ; deux parseurs dont les tolérances peuvent diverger sur un même fichier (plausible).
Preuve         : conanfile.py:137, 141 ; source/owl/private/core/YamlNode.cpp ; doc/pages/design/dependencies.md:209.
Recommandation : surveiller, effort M (converger vers une seule, cf. fiche 5).
```

```text
ID             : G-21 — étrangeté, basse, confirmé (build, documentation)
Constat        : l'aide de l'éditeur rend les 76 diagrammes mermaid par un moteur maison de 1 317 lignes lancé au
                 configure ; un diagramme qu'il ne sait pas dessiner fait échouer le configure.
Preuve         : cmake/HelpAssets.cmake:131-150 ; tools/doc/mermaid_svg.py.
Recommandation : surveiller, effort S (dégrader en image d'erreur plutôt qu'en échec, contrôle dans CodeStyle).
```

```text
ID             : I-01 — faiblesse, basse, plausible (documentation)
Constat        : CodeStyle vérifie désormais les identifiants cités en `code` ; restent sans contrôle la prose (menus,
                 ruban), les pages de conception, le changelog et la roadmap, exclus du contrôle.
Preuve         : ci/actions/code_style.py:37-38 ; ci/utils/doc_identifiers.py ; doc/pages/getting_started.md.
Recommandation : surveiller, effort S (captures de l'aide vérifiées, revue des pages au fil des PR).
```

```text
ID             : I-06 — faiblesse, basse, confirmé (documentation)
Constat        : l'exigence « tout documenter, erreur au moindre manque » produit des `@brief` de remplissage (425
                 « Default constructor. », « Destructor. »… dans `source/owl/public`) : poids de lecture, de revue et de
                 contexte IA. La règle du dépôt l'exige explicitement.
Preuve         : DoxyfileTemplate:522, 528, 881, 905 ; source/owl/public ; .claude/rules/ongoing-quality.md.
Recommandation : corriger, effort S (exempter membres spéciaux `=default` / `=delete` et surcharges documentées ;
                 garder `WARN_AS_ERROR` ; opinion, décision du mainteneur).
```

```text
ID             : I-08 — étrangeté, basse, confirmé (documentation)
Constat        : Keep a Changelog est respecté ; la section 0.1.1 garde 133 sous-puces ; `[Unreleased]` compte déjà 179
                 entrées neuf jours après la 0.6.0, dont des réglages de jeux d'exemple (« Wolfenstein mini: food heals »)
                 mêlés aux changements du moteur.
Preuve         : doc/pages/changelog.md:10-200 ; .claude/rules/documentation.md.
Recommandation : surveiller, effort S (sous-section « Samples » ou une ligne par jeu ; 0.1.1 tel quel).
```

```text
ID             : I-11 — étrangeté, basse, confirmé (documentation)
Constat        : la référence générée dit qu'une `entity` est « l'UUID entier de l'entité », la page de scripting qu'un
                 script « ne voit jamais l'UUID, seulement son handle » ; le code pousse bien un handle.
Preuve         : doc/pages/lua-api.md:9 (généré par script/LuaBindings.cpp:5546) ; doc/pages/scripting.md:200 ;
                 script/LuaBindings.cpp:1133.
Recommandation : corriger, effort S (texte du générateur, puis régénérer).
```
