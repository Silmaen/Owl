# Constats — axes F (qualité) et H (CI)

> Statut : revu le 2026-10-20 sur `main` 4e1a6a10, commandes via `./docker_run.sh`. Les mesures de couverture
> datent du 2026-10-05 (clang 22.1.8, `linux-clang-debug`, gcovr 8.x) et n'ont pas été refaites.
> **constaté** = lu dans le dépôt ; **mesuré** = commande exécutée ; **opinion** = jugement.

## Résumé

- Tests de logique réels (~2 392 `TEST`/`TEST_F`), tests d'image GPU et tests de l'éditeur présents (F-13, F-01, F-02).
- Couverture officielle 70,9 % contre 57,6 % brute : `gcovr.cfg:41` exclut vulkan, opengl, openal, glfw, io (F-01, F-04).
- clang-tidy propre mais sans `clang-analyzer-*`, complexité à 75, tests non analysés (F-07).
- CI reproductible en local, ciblage clang-tidy sûr, aucun secret dans le dépôt (H-10, H-11, H-12).
- Ruleset GitHub de `main` non vérifiable ; deux images distinctes CI / local (H-06, H-09).
- 15 constats : 0 haut, 6 moyens, 9 bas ; 4 forces.

## Mesures portant un constat

Couverture par module, lignes officielles / brutes (2026-10-05) : les écarts viennent des exclusions.

| Module         | Officielles | Brutes                 |
|----------------|-------------|------------------------|
| `owl/renderer` | 76,3 %      | 39,7 % (2 972 / 7 491) |
| `owl/sound`    | 81,8 %      | 34,0 % (162 / 476)     |
| `owl/window`   | 83,0 %      | 19,6 % (44 / 225)      |
| `owl/io`       | exclu       | 0,0 % (0 / 412)        |
| `owl/gui`      | 38,7 %      | 38,7 % (1 198 / 3 099) |
| **Moteur**     | **70,9 %**  | **57,6 %**             |

Autres : 25 tests sans assertion (2026-10-05, non recomptés) ; 4 tests à `GTEST_SKIP` conditionnel ;
`CodeStyle` complet 4,0 s ; ctest ASan 4 s sans rapport ; ctest TSan 10 s, 0 *data race* ; durées CI TeamCity
non mesurables depuis le dépôt.

## Constats — gravité moyenne

```text
ID             : F-01 — faiblesse, moyenne, confirmé (qualité)
Constat        : des tests d'image exécutent les backends GPU (Vulkan sous lavapipe, OpenGL sous Xvfb), mais `gcovr.cfg`
                 exclut toujours le code vulkan / opengl : la couverture officielle ne mesure pas le rendu GPU.
Preuve         : test/unit/render_tests/RenderImage_test.cpp, ImageCompare.h ; gcovr.cfg:41.
Recommandation : corriger, effort S (publier la couverture des backends GPU à côté de l'officielle).
```

```text
ID             : F-07 — faiblesse, moyenne, confirmé (qualité)
Constat        : configuration sobre et état propre (262 unités, 0 constat), mais `clang-analyzer-*` est éteint, les tests
                 ne sont jamais analysés (`OWL_TESTING=OFF` dans le preset), seuil de complexité cognitive à 75, et une
                 partie des 102 `NOLINT` cite des checks inactifs.
Preuve         : .clang-tidy ; CMakePresetsCI.json:21 ; CMakeLists.txt (test/ ajouté seulement si clang-tidy est OFF).
Recommandation : corriger, effort S/M (activer `clang-analyzer-core/cplusplus/deadcode`, seuil 25–40 avec dérogations,
                 purger les NOLINT morts, analyser `test/` avec un jeu réduit).
```

```text
ID             : H-06 — risque, moyenne, non vérifiable depuis le dépôt (CI)
Constat        : le ruleset GitHub de `main` est hors dépôt ; si aucun check TeamCity n'est requis, la CI informe sans
                 bloquer.
Preuve         : aucune dans le dépôt.
Recommandation : corriger, effort S (exiger « Build / Linux x64 / Clang », « Quality / Code Style »,
                 « Quality / Sanitizer Address »).
```

## Constats — gravité basse

```text
ID             : F-02 — faiblesse, basse, plausible (qualité)
Constat        : l'éditeur et l'undo/redo (fusion à 1 s, dirty flag, profondeur 100, snapshots YAML) sont testés ;
                 leur couverture n'a pas été remesurée.
Preuve         : test/unit/owlnest_tests (54 fichiers).
Recommandation : surveiller, effort S (mesurer `owlnest/sources`, viser undo∘redo = identité et la fusion).
```

```text
ID             : F-04 — faiblesse, basse, confirmé (qualité)
Constat        : le gate `COVERAGE_DROP` vérifie la différence, mais le chiffre publié exclut backends GPU, son, fenêtre et
                 io, et surestime la couverture réelle.
Preuve         : gcovr.cfg:40-41 ; .teamcity/build/Linux.kt:22.
Recommandation : corriger, effort S (publier aussi le chiffre brut).
```

```text
ID             : F-09 — faiblesse, basse, plausible (qualité)
Constat        : une minorité de tests (25 mesurés) ne vérifie que « ça ne plante pas » ; les suites « *Coverage »
                 affirment en majorité de vrais comportements.
Preuve         : mesure du 2026-10-05, recomptage non fait.
Recommandation : surveiller, effort S (recompter, exiger une assertion dans CodeStyle, renommer par comportement).
```

```text
ID             : F-12 — étrangeté, basse, confirmé (qualité)
Constat        : `OWL_SANITIZER_CUSTOM_ALLOCATOR` est définie par la configuration des sanitizers mais n'est lue que par
                 trois fichiers de test.
Preuve         : cmake/Sanitizers.cmake:45,57,92.
Recommandation : corriger, effort S (documenter ou supprimer).
```

```text
ID             : H-04 — faiblesse, basse, plausible (CI)
Constat        : le wrapper CI a 21 fichiers de tests ; la couverture de ses parties subtiles (sélection du diff
                 clang-tidy, 1 230 lignes de regex CodeStyle) n'est pas revérifiée.
Preuve         : ci/tests/ ; code_style.py.
Recommandation : surveiller, effort S (mesurer la couverture de `ci/`, compléter `_select`, `_resolve_base`, regex).
```

```text
ID             : H-05 — faiblesse, basse, plausible (CI)
Constat        : arm64 est hors des PR ; le coût de la matrice par PR n'est pas recompté et le dépôt ne contient aucune
                 durée de CI, d'où l'impossibilité de mesurer le signal par minute.
Preuve         : .teamcity/build/LinuxArm64.kt ; .teamcity/quality/.
Recommandation : surveiller, effort S (exporter les durées TeamCity dans `20-mesures.md`).
```

```text
ID             : H-08 — étrangeté, basse, confirmé (CI)
Constat        : un commentaire (« python3 -u » cassé sous poetry run 2.x) et une commande (« poetry run python3 -u »)
                 divergent.
Preuve         : ci_action.py:6-12 ; .teamcity/common/Templates.kt:265.
Recommandation : corriger au fil de l'eau, effort S.
```

```text
ID             : H-09 — étrangeté, basse, confirmé (CI)
Constat        : la CI (`builder-ubuntu2604`) et le poste local (`devel-ubuntu2604`) utilisent deux images : « vert en
                 local » ne garantit pas « vert en CI ». Du code contourne un `$HOME` d'agent partagé entre conteneurs
                 amd64 et arm64.
Preuve         : cmake/CMakePresetsLinux.json:22 ; docker_run.sh:25 ; ci/utils/venv.py ; ci_action.py:58-95 ;
                 cmake/Poetry.cmake.
Recommandation : surveiller, effort S (image de référence commune, un venv par architecture).
```

## Forces

```text
ID             : F-13 — force, moyenne, confirmé (qualité)
Constat        : la logique pure (math, hiérarchie, sérialisation, prefab, sauvegarde, bindings Lua) est testée par
                 comportement, pas seulement en fumée : math 95 %, debug 96 %, script 84 %, data 82 %.
Preuve         : ~2 392 TEST/TEST_F dans test/unit.
Recommandation : garder
```

```text
ID             : H-10 — force, moyenne, confirmé (CI)
Constat        : une seule source de vérité (les presets, `vendor.silmaen`) et un point d'entrée unique
                 (`ci_action.py <Action> <preset>`) pour le local et TeamCity ; actions auto-découvertes ; matrice Kotlin
                 factorisée.
Preuve         : ci/actions/__init__.py ; ci/actions/base/action.py ; .teamcity/common/Helpers.kt.
Recommandation : garder
```

```text
ID             : H-11 — force, moyenne, confirmé (CI)
Constat        : le ciblage clang-tidy du diff (`.cpp` touchés + fermeture d'includes via `ninja -t deps`) retombe sur le
                 périmètre complet au moindre doute ; les diagnostics sont annotés sur la ligne de la PR.
Preuve         : ci/actions/clang_tidy.py ; code_style.py ; BridgeHelpers.kt.
Recommandation : garder (bus factor : le plugin GitHub est maison).
```

```text
ID             : H-12 — force, basse, confirmé (CI)
Constat        : aucun secret dans les fichiers suivis ni l'historique ; `.env` ignoré, vide et jamais commité ;
                 identifiants via paramètres TeamCity.
Preuve         : scan (mot de passe, token, clé privée, ghp_, AKIA) ; .gitignore ; Github.kt.
Recommandation : garder
```
