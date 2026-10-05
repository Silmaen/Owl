# Constats — axes F (qualité) et H (CI)

> Statut : première passe, 2026-10-05. Tout a été lancé via `docker/run.sh` (image
> `registry.argawaen.net/builder/devel-ubuntu2404:latest`, clang 22.1.8, 32 threads). Builds dans
> `output/build/linux-clang-debug` et dans des dossiers dédiés `output/build/audit-*` (ASan, UBSan,
> TSan, clang-tidy). `linux-clang-release` et `linux-clang-tidy` n'ont pas été touchés. Les scripts
> d'analyse jetables sont dans `output/build/linux-clang-debug/audit_cov/`.
> Légende : **constaté** = lu dans le dépôt ; **mesuré** = commande exécutée ; **opinion** = jugement.

## Résumé

1. La couverture **officielle** (`gcovr.cfg`) est de **70,9 % des lignes** et 40,5 % des branches, mais
   elle exclut les backends GPU, son, fenêtre et io (74 fichiers moteur, 4 568 lignes, 2 couvertes).
2. Sans ces exclusions : **57,6 %** sur le moteur, **42,3 %** sur tout `source/`. L'éditeur est à **2,5 %**
   (224 lignes sur 8 937) et l'undo/redo à **0 %**.
3. **Aucun test n'exécute Vulkan ou OpenGL**. Il n'y a pas de test de rendu par comparaison d'images, et
   6 des 13 shaders livrés ne sont jamais compilés par un test.
4. Les tests de logique sont réels : 1 127 tests, 4 056 assertions, moins de 2,5 % de tests sans
   assertion. La suite tourne en 3,4 s.
5. Les tests dépendent de leur ordre : avec `--gtest_shuffle`, `PhysicCommand` plante (assert EnTT en
   debug, *stack-use-after-return* sous ASan), parce qu'un `Scene*` statique survit à sa scène.
6. Le job **UBSan ne peut pas échouer** : `-fsanitize-recover=undefined` sans `halt_on_error`, donc
   code de sortie 0 sur UB (mesuré). Il dure en plus 29 min, contre 4 s pour ASan.
7. clang-tidy est propre (0 constat sur 262 unités). En revanche `clang-analyzer-*` est désactivé, les
   tests ne sont pas analysés, et 47 % des noms de checks cités par les `NOLINT` désignent des checks
   inactifs.
8. Le wrapper CI Python est bien découpé (`BaseAction`, découverte automatique), mais il n'a **aucun
   test**. Un bug fait tourner clang-tidy sur **1 job** au lieu de 32 (~12 min au lieu de 67 s).
9. Les mots de passe de publication passent en argument de ligne de commande et sont **écrits dans le
   log** par le code CI. La publication télécharge puis exécute un script Python distant, sans contrôle
   d'intégrité.
10. La CI TeamCity est sophistiquée (filtres de chemins, drafts, annotations, diff clang-tidy), mais
    chère : 8 configurations par PR, dont arm64 émulé, clean complet à chaque build. Elle ne bloque
    probablement pas la fusion.

## Mesures

### Couverture par module (tests `linux-clang-debug`, `llvm-cov gcov`, gcovr 8.x)

« Officielle » = `gcovr.cfg` tel quel (ce que publie la CI). « Brute » = `--config /dev/null`, filtre
`source/`, mêmes exclusions de branches, rien d'autre d'exclu.

| Module             | Fichiers (brut) | Lignes officielles | Lignes brutes                | Branches brutes |
|--------------------|-----------------|--------------------|------------------------------|-----------------|
| `owl/math`         | 16              | 95,3 %             | 95,3 % (1 138 / 1 194)       | 71,4 %          |
| `owl/debug`        | 6               | 96,3 %             | 96,3 % (261 / 271)           | 55,6 %          |
| `owl/event`        | 5               | 90,0 %             | 90,0 % (135 / 150)           | 47,1 %          |
| `owl/script`       | 5               | 83,9 %             | 83,9 % (891 / 1 062)         | 49,9 %          |
| `owl/data`         | 58              | 81,6 %             | 81,6 % (2 273 / 2 785)       | 46,3 %          |
| `owl/input`        | 6               | 97,3 %             | 78,5 % (73 / 93)             | 75,9 %          |
| `owl/core`         | 24              | 77,5 %             | 77,5 % (583 / 752)           | 28,3 %          |
| `owl/scene`        | 104             | 69,7 %             | 69,7 % (3 766 / 5 403)       | 41,7 %          |
| `owl/physics`      | 2               | 65,9 %             | 65,9 % (191 / 290)           | 60,8 %          |
| `owl/app`          | 6               | 63,3 %             | 63,3 % (274 / 433)           | 34,3 %          |
| `owl/renderer`     | 142             | 76,3 %             | 39,7 % (2 972 / 7 491)       | 23,4 %          |
| `owl/gui`          | 29              | 38,7 %             | 38,7 % (1 198 / 3 099)       | 24,3 %          |
| `owl/sound`        | 18              | 81,8 %             | 34,0 % (162 / 476)           | 14,4 %          |
| `owl/window`       | 6               | 83,0 %             | 19,6 % (44 / 225)            | 2,7 %           |
| `owl/platform`     | 3               | 37,5 %             | 9,7 % (12 / 124)             | 6,8 %           |
| `owl/io`           | 10              | exclu              | 0,0 % (0 / 412)              | 0,0 %           |
| `owlnest/sources`  | 75              | hors filtre        | 2,5 % (224 / 8 937)          | 0,6 %           |
| `owlnest/runner`   | 3               | hors filtre        | 0,0 % (0 / 396)              | 0,0 %           |
| **Moteur**         | 440             | **70,9 %**         | **57,6 %** (13 973 / 24 260) | —               |
| **Tout `source/`** | 518             | —                  | **42,3 %** (14 197 / 33 593) | 20,2 %          |

Plus gros blocs non couverts (lignes) : `EditorLayer.cpp` 2 180 (0 %), `Scene.cpp` 1 136 (39 %),
`SceneFlowDocument.cpp` 740, `gui/component/render.cpp` 640, `Viewport.cpp` 560,
`vulkan/Framebuffer.cpp` 412, `VulkanHandler.cpp` 378, `RunnerLayer.cpp` 336. Au total, 62 fichiers de
plus de 50 lignes (12 987 lignes) sont à 0 %. Dans `Scene.cpp`, 58 fonctions sur 199 ne sont jamais
appelées : tout le rendu (`render`, `renderWithStack`, `renderTilemaps`, `renderHud`, `renderUI`,
`renderRaycast*`), le streaming voxel, le joueur voxel et `onUpdateEditor`.

### Tests et durées (mesuré, ccache chaud)

| Mesure                                              | Valeur                                                                     |
|-----------------------------------------------------|----------------------------------------------------------------------------|
| Fichiers de test / lignes                           | 133 / 21 530                                                               |
| `TEST*` / assertions `EXPECT_*`+`ASSERT_*`          | 1 127 / 4 056 (3,6 par test ; 34 % sont des `_TRUE`/`_FALSE`)              |
| Tests sans aucune assertion                         | 25 (2,2 %)                                                                 |
| Tests à `GTEST_SKIP` conditionnel                   | 4 (CPU < 6 threads, réseau, assets absents)                                |
| ctest debug + couverture (16 suites, `-j8`)         | 3,4 s ; 15/16 OK (`Core_Version.base` échoue)                              |
| ctest ×10, ordre de suites aléatoire, `-j32`        | 28,6 s ; 15/15 OK (core exclu)                                             |
| `--gtest_shuffle --gtest_repeat=5` par binaire      | physics : abort (6 graines sur 6) ; core : 2 tests échouent ; 14 autres OK |
| gcovr (rapport brut, JSON)                          | 24 s                                                                       |
| Build ASan / UBSan / TSan / clang-tidy              | 46 s / 51 s / 52 s / 72 s                                                  |
| ctest ASan (ordre par défaut)                       | 4 s, aucun rapport ASan/LSan                                               |
| ctest TSan (`docker/run.sh --perf`)                 | 10 s, 0 *data race*                                                        |
| ctest UBSan                                         | **1 735 s** (scene 1 207 s, renderer 1 425 s), rc 0 hors version           |
| clang-tidy, 262 unités, 32 processus                | 67 s, 0 constat                                                            |
| clang-tidy, 16 unités en série                      | 42,7 s (2,7 s par unité, soit ~12 min pour 262 en série)                   |
| `CodeStyle` complet                                 | 4,0 s (clang-format 2,2 s, codespell 0,8 s, audits regex 1,5 s)            |
| 9 tests `SlangCompute.*` (compilation Slang réelle) | 0,95 s                                                                     |
| Durées CI TeamCity                                  | non mesurables depuis le dépôt (pas d'accès au serveur)                    |

## Constats

Classés par gravité, F puis H à gravité égale.

### Gravité haute

```text
ID             : F-01
Axe            : Qualité
Nature         : faiblesse
Gravité        : haute
Preuve         : grep "Type::(Vulkan|OpenGL|Null)" test → 88 Null, 1 OpenGL (sérialisation de
                 AppParams, test/core_tests/application_test.cpp:82), 0 Vulkan ; gcovr.cfg:41 exclut
                 .*vulkan.*|.*opengl.*|.*openal.*|.*glfw.* ; couverture brute : vulkan/opengl 0 %,
                 renderer 39,7 % ; grep des noms quad/circle/line/text/background/tilemap_instanced
                 dans test/ → aucun appel à compileSlangToSpirv sur ces 6 shaders.
Constat        : Aucun test n'exécute un backend GPU réel, et l'exclusion dans gcovr.cfg masque ce trou.
                 Les régressions de rendu connues (UBO partagé, Renderer2D invisible sous OpenGL, voir
                 la mémoire du projet) n'ont aucun filet de sécurité. L'image contient pourtant Xvfb,
                 lavapipe et llvmpipe.
Comparaison    : wgpu, Filament, bgfx et Bevy font tourner des tests « golden image » sur un rasteriseur
                 logiciel (lavapipe / SwiftShader / WARP) en CI (opinion sur la pratique courante).
Recommandation : corriger — un test de fumée Vulkan et OpenGL sous lavapipe/llvmpipe + Xvfb
                 (init, une frame, readback, comparaison d'image avec tolérance), puis compiler en test
                 les 13 shaders livrés. Effort M.
Statut         : confirmé
Vérification   : grep re-fait : 88 Type::Null, 1 Type::OpenGL (sérialisation), 0 Vulkan ; quad, line, tilemap_instanced, text, circle,
                 background absents de test/ (6/13) ; Xvfb et lvp_icd.json présents dans l'image locale (pas vérifié dans l'image CI, cf. H-09).
```

```text
ID             : F-02
Axe            : Qualité
Nature         : faiblesse
Gravité        : haute
Preuve         : couverture brute : owlnest/sources 2,5 % (224/8 937), seul
                 document/codeEditor/MarkdownDocument.cpp couvert (87,7 %) ; UndoManager.h (120 l.),
                 UndoCommand.h, EntitySnapshot.cpp, commands/*.cpp : 0 % ; runner 0 % ;
                 test/CMakeLists.txt:30-35 (seul scene_tests voit les sources de l'éditeur).
Constat        : L'éditeur n'a quasiment aucun test, et le système undo/redo (fusion à 1 s, dirty flag,
                 profondeur 100, snapshots YAML) n'est jamais exécuté. C'est pourtant la règle
                 « Editor Coverage » du projet (ongoing-quality.md) qui en fait une exigence de base.
Comparaison    : UndoManager et les commandes ne dépendent pas d'ImGui : ils se testent comme de la
                 logique pure.
Recommandation : corriger — extraire undo/commands/snapshot dans une petite bibliothèque testable
                 (ou les compiler dans un owl_editor_tests), viser les invariants undo∘redo = identité
                 et la fusion. Effort M.
Statut         : confirmé
Vérification   : test/CMakeLists.txt:31-36 : seul scene_tests voit source/owlnest/sources, et seul MarkdownPreview_test.cpp
                 l'utilise ; grep UndoManager|EntitySnapshot dans test/ vide. Doublon de E-03 (axe E), à fusionner en synthèse.
```

```text
ID             : F-03
Axe            : Qualité
Nature         : risque
Gravité        : moyenne
Preuve         : cmake/Sanitizers.cmake:66 (-fsanitize=undefined -fsanitize-recover=undefined) ; aucun
                 UBSAN_OPTIONS dans le dépôt (grep SAN_OPTIONS|halt_on_error → rien) ; sonde mesurée :
                 dépassement d'int signé → rapport « runtime error » puis rc=0, rc=1 seulement avec
                 UBSAN_OPTIONS=halt_on_error=1 ; ctest UBSan = 1 735 s ; ctest --output-on-failure
                 n'affiche pas la sortie d'un test qui passe.
Constat        : Le job « Sanitizer undefined behavior » réussit même quand il détecte un UB : le
                 rapport reste dans la sortie d'un test vert, qui n'est même pas imprimée. Il coûte en
                 plus ~29 min de tests, 400 fois plus qu'ASan. Bonne nouvelle mesurée : 0 « runtime
                 error » sur les 16 binaires relancés à la main.
Comparaison    : la documentation Clang recommande -fno-sanitize-recover=undefined (ou halt_on_error=1)
                 pour qu'un UB fasse échouer la CI.
Recommandation : corriger — -fno-sanitize-recover=all ou UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
                 via ENVIRONMENT dans test/CMakeLists.txt ; fusionner UBSan dans le job ASan
                 (combinaison supportée) ; trouver d'où vient la lenteur (hypothèse : tracker mémoire
                 + OWL_ENABLE_STACKTRACE=ON). Effort S.
Statut         : confirmé
Vérification   : sonde refaite (clang++ -fsanitize=undefined -fsanitize-recover=undefined) : runtime error puis rc=0, rc=1 avec
                 halt_on_error=1. Gravité ramenée à moyenne : le job UBSan est main-only (skipAutoPRs, Build.kt:233-238) et 0 UB
                 n'existe aujourd'hui ; c'est un filet inopérant, pas un défaut actif.
```

### Gravité moyenne

```text
ID             : F-04
Axe            : Qualité
Nature         : faiblesse
Gravité        : moyenne
Preuve         : gcovr.cfg:40-41 ; mesures ci-dessus (70,9 % officiel, 57,6 % moteur brut, 42,3 %
                 source/) ; aucune failureCondition sur la couverture dans .teamcity/ (seules
                 TEST_COUNT et ARTIFACT_SIZE, Build.kt:141-163) ; .claude/rules/ongoing-quality.md
                 (« Coverage trend must go up »).
Constat        : Le chiffre publié surestime de 13 points la couverture réelle du moteur. La règle « la
                 couverture ne baisse jamais » n'est vérifiée par rien : ni seuil, ni comparaison au
                 build précédent.
Comparaison    : Codecov/Coveralls ou un failOnMetricChange TeamCity sur la couverture (TeamCity sait
                 lire les statistiques de couverture) : gate sur la différence, pas sur l'absolu.
Recommandation : corriger — publier les deux chiffres (moteur hors backends, et brut), ajouter un
                 failOnMetricChange sur la couverture des lignes. Effort S.
Statut         : confirmé
Vérification   : gcovr.cfg:41 exclut bien vulkan/opengl/openal/glfw/io ; ci/actions/coverage.py lance gcovr -r <root> (qui lit
                 gcovr.cfg) sans seuil ; Build.kt:140-157 : seules TEST_COUNT et ARTIFACT_SIZE ont un failOnMetricChange.
```

```text
ID             : F-05
Axe            : Qualité
Nature         : risque
Gravité        : moyenne
Preuve         : bin/owl_physics_tests_unit_test --gtest_shuffle --gtest_random_seed=7919 → assert EnTT
                 « Value must be a power of two » (debug), puis ASan : stack-use-after-return dans
                 PhysicCommand::frame (source/owl/private/physics/PhysicCommand.cpp:231) appelé depuis
                 PhysicCommand_tests.cpp:41 ; paire minimale badImpulse → Creation (rc 134) ;
                 badImpulse (PhysicCommand_tests.cpp:60-94) fait init(&scene) sur une scène locale,
                 sans destroy() ; core : Environement.variables et CoreCoverage.FactoryCreateProduct
                 échouent en répétition ou en ordre aléatoire.
Constat        : Les tests passent uniquement dans l'ordre de déclaration. Le cas physique révèle un
                 vrai défaut d'API : PhysicCommand (état statique) garde un Scene* brut au-delà de la
                 vie de la scène. Le même scénario est possible dans le moteur (scène détruite pendant
                 que la physique est initialisée).
Comparaison    : il est courant de lancer gtest avec --gtest_shuffle en CI pour débusquer ce couplage.
Recommandation : corriger — fixture qui appelle PhysicCommand::destroy() en TearDown ; côté moteur,
                 liaison de vie explicite (la scène possède son monde physique, ou destroy() dans
                 ~Scene) ; ajouter --gtest_shuffle à un job (ASan). Effort S (tests) / M (API).
Statut         : confirmé
Vérification   : reproduit dans linux-clang-debug (seed 7919 : isInitialized() vrai au début de Creation puis assert EnTT) ;
                 log ASan de l'axe confirme stack-use-after-return ; badImpulse n'appelle pas destroy() ; ~Scene = default
                 (Scene.cpp:273), destroy() seulement dans onEndRuntime (Scene.cpp:506).
```

```text
ID             : F-06
Axe            : Qualité
Nature         : faiblesse
Gravité        : basse
Preuve         : test/core_tests/version_test.cpp:8-9 (littéraux "0.2.1" et 0x00020100) ;
                 CMakeLists.txt:8 (VERSION 0.2.2) ; le passage à 0.2.2 vient de 1991cc25 « Upgrade
                 clang-tidy on diff » (branches Feature/TidyDiff et Feature/Bench) ; ctest : seul échec.
Constat        : Le test recopie la version au lieu de la comparer à la source de vérité. Il casse donc
                 à chaque montée de version. La montée a ici voyagé dans une PR sans rapport (outillage
                 clang-tidy), contrairement à la règle « kickoff PR = bump seul ».
Recommandation : corriger — comparer à OWL_VERSION_* généré par CMake (ou au moins vérifier la
                 cohérence chaîne / code). Effort S.
Statut         : confirmé
Vérification   : version_test.cpp:8-9 et CMakeLists.txt:8 conformes ; mais 1991cc25 n'est pas dans main (main reste en
                 0.2.1 et le test y passe) : l'échec n'existe que sur la branche non fusionnée. Gravité ramenée à basse
                 (test fragile, cassure visible immédiatement).
```

```text
ID             : F-07
Axe            : Qualité
Nature         : faiblesse
Gravité        : moyenne
Preuve         : .clang-tidy (liste blanche, aucun clang-analyzer-*, -bugprone-unchecked-optional-access,
                 readability-function-cognitive-complexity.Threshold = 75, HeaderFilterRegex limité à
                 source/owl et source/owlnest) ; CMakePresetsCI.json:21 (linux-clang-tidy :
                 OWL_TESTING=OFF, donc test/ absent de compile_commands) ; 138 lignes NOLINT, 158 noms
                 de checks dont 75 (47 %) désignent un check inactif (pro-type-reinterpret-cast ×13,
                 magic-numbers ×24, hicpp-explicit-conversions ×8, concurrency-mt-unsafe ×6…) ;
                 analyse complète mesurée : 262 unités, 0 constat.
Constat        : La configuration est sobre et l'état est propre, ce qui est rare. Mais l'analyseur
                 statique de Clang (le plus utile contre les nullptr et use-after-free) est éteint, les
                 21 kLOC de tests ne sont jamais analysés, et presque la moitié des NOLINT sont du bruit
                 hérité d'une ancienne configuration.
Recommandation : corriger — activer clang-analyzer-core.*, cplusplus.*, deadcode.* (mesurer le surcoût) ;
                 seuil de complexité ramené vers 25–40 avec dérogations ; purger les NOLINT morts
                 (clang-tidy --list-checks + script) ; analyser test/ avec un jeu de checks réduit.
                 Effort S/M.
Statut         : confirmé
Vérification   : .clang-tidy : '-*' puis liste blanche, 0 occurrence de clang-analyzer, Threshold 75 (ligne 104) ;
                 CMakePresetsCI.json:21 OWL_TESTING=OFF, et CMakeLists.txt n'ajoute test/ que si clang-tidy est OFF.
                 Le décompte 47 % des NOLINT n'a pas été recompté (les checks cités sont bien absents de la liste).
```

```text
ID             : H-01
Axe            : CI
Nature         : risque
Gravité        : moyenne
Preuve         : ci/actions/publish_package.py:110 et publish_doc.py:68 (log.info du dict contenant
                 "passwd") ; ci/utils/run.py:97 (log de la commande complète, donc de --passwd) ;
                 ci/utils/publish.py:177-179 (mot de passe en argument CLI) ; ci/utils/remote.py:75-78 et 98-99
                 (masque son propre message, puis run_command réimprime --passwd en clair) ;
                 GlobalBuild.kt:85, 151, 160 (--remote_passwd / --password en ligne de commande).
Constat        : Le code CI écrit lui-même les identifiants de publication et du remote DepManager dans
                 le log. Seul le masquage des paramètres « password » de TeamCity les protège, et leur
                 type n'est pas déclaré dans le DSL : il est réglé à la main sur le serveur, donc
                 invérifiable ici. Les arguments sont aussi visibles dans `ps` sur l'agent.
Comparaison    : GitHub Actions et TeamCity masquent les secrets typés ; la pratique est de les passer
                 en variable d'environnement et de ne jamais les journaliser.
Recommandation : corriger — lire les secrets dans l'environnement, rédiger systématiquement dans
                 run_command (liste de drapeaux sensibles), déclarer password("deploy_passwd", …) dans
                 le DSL. Effort S.
Statut         : confirmé
Vérification   : publish_package.py:106-110 et publish_doc.py:64-68 journalisent le dict avec passwd ; run.py:97 journalise
                 la commande ; remote.py masque son message puis passe --passwd à run_command. Seul garde-fou : le
                 masquage TeamCity des paramètres de type password, invérifiable ici.
```

```text
ID             : H-02
Axe            : CI
Nature         : risque
Gravité        : moyenne
Preuve         : ci/utils/publish.py:13-38 (download_api_script : GET <url>/static/scripts/api.py,
                 « https:// » ajouté seulement si aucun schéma) ; publish_package.py:122-128 et
                 publish_doc.py:85-91 (écrit dans ci/api.py puis l'exécute avec les identifiants) ;
                 ci/api.py est ignoré par .gitignore mais présent localement (315 lignes, version
                 téléchargée).
Constat        : À chaque publication, la CI exécute un code Python arrivé du serveur cible, sans
                 épinglage ni contrôle d'intégrité, en lui passant les identifiants. Une compromission
                 du serveur de déploiement devient une exécution de code sur l'agent.
Recommandation : corriger — versionner api.py dans le dépôt (ou l'épingler par empreinte SHA-256),
                 imposer https. Effort S.
Statut         : confirmé
Vérification   : publish.py:13-38 télécharge <url>/static/scripts/api.py sans empreinte ; run_api_push (publish.py:168-181)
                 l'exécute avec --passwd. Exposition limitée à main (étapes conditionnées par is_default, GlobalBuild.kt:148-165).
```

```text
ID             : H-03
Axe            : CI
Nature         : faiblesse
Gravité        : moyenne
Preuve         : ci/actions/clang_tidy.py:528-533 : jobs = max(1, int(parsed.get("jobs", "0"))) vaut 1,
                 donc la branche « if jobs == 0: jobs = cpu_count » n'est jamais prise ; Build.kt:204-208
                 ne passe pas --jobs ; mesuré : 262 unités en 67 s avec 32 processus, 2,7 s par unité en
                 série (~12 min).
Constat        : Par défaut, l'action ClangTidy tourne sur un seul processus, contrairement à sa
                 docstring (« default: CPU count »). Sur un `main` (périmètre complet), cela coûte
                 environ dix fois le temps nécessaire.
Recommandation : corriger — int(parsed.get("jobs", "0")) puis « or os.cpu_count() ». Effort S.
Statut         : confirmé
Vérification   : clang_tidy.py:528-533 : max(1, int("0")) vaut 1, la branche jobs == 0 n'est atteinte que sur ValueError ;
                 l'étape TeamCity (Build.kt:200-206) ne passe pas --jobs. Impact surtout sur main (périmètre complet).
```

```text
ID             : H-04
Axe            : CI
Nature         : faiblesse
Gravité        : moyenne
Preuve         : aucun fichier test_*.py / conftest.py dans le dépôt ; ~3,8 kLOC Python suivis dans
                 ci/ + ci_action.py ; code_style.py : 1 230 lignes d'heuristiques regex sur du C++
                 (_FUNCTION_DECL_RE, _RETURN_LEADING_RE…) ; pyproject.toml déclare black mais
                 « black --check ci ci_action.py » → 21 fichiers sur 28 non conformes ; aucun mypy/ruff ;
                 requests et requests-toolbelt utilisés mais non déclarés (présents seulement en
                 transitif dans poetry.lock).
Constat        : La logique de gate la plus subtile (sélection du diff clang-tidy, audits CodeStyle) n'a
                 aucun test. Une régression silencieuse, du type F-03 ou H-03, n'est donc détectable
                 qu'en production. black est une dépendance morte.
Recommandation : corriger — pytest sur _select / _resolve_base / parse_extra_args / les regex
                 CodeStyle (fixtures C++ minimales), ruff + mypy dans CodeStyle, déclarer requests.
                 Effort M.
Statut         : confirmé
Vérification   : aucun test_*.py/conftest.py suivi ; pyproject.toml déclare black (groupe build) sans aucun appel dans ci/,
                 .teamcity ni cmake ; requests importé par ci/utils/publish.py sans déclaration. black --check non relancé.
```

```text
ID             : H-05
Axe            : CI
Nature         : faiblesse
Gravité        : moyenne
Preuve         : Build.kt (matrice), GlobalBuild.kt:90-145 (Clean = rmtree complet puis Build debug,
                 Test, Coverage, Build release, Test release, Documentation) ; define_variables.py
                 (release_preset) ; Build_LinuxArm64 : Clang non exclu des PR, exécuté sous émulation
                 Docker (Build.kt:110-119) ; dépendance snapshot sur CodeStyle avec FAIL_TO_START
                 (GlobalBuild.kt:217-219) ; LSan séparé d’ASan (Build.kt:232).
Constat        : Une PR non draft qui touche du C++ lance 8 configurations : Linux x64 Clang, Linux
                 arm64 Clang émulé, Windows Clang, CodeStyle, ASan, TSan, LSan, clang-tidy. Chacune
                 refait un build complet après un rmtree, et la plupart compilent debug puis release.
                 LSan est déjà inclus dans ASan sous Linux, donc ce job est redondant. arm64 émulé sur
                 chaque PR coûte cher pour un signal rare (opinion). Le dépôt ne contient aucune durée
                 de CI, ce qui empêche de mesurer le « signal par minute ».
Comparaison    : GitHub Actions et matrices courantes : builds incrémentaux avec cache (ccache,
                 actions/cache), sanitizers groupés (ASan+UBSan), plateformes émulées en nightly.
Recommandation : corriger — supprimer LSan, fusionner UBSan dans ASan, passer arm64 en main-only ou
                 nightly, garder le build dir plutôt que rmtree (ccache est déjà là). Exporter les
                 durées TeamCity dans 20-mesures.md. Effort S.
Statut         : confirmé
Vérification   : 8 BT par PR confirmés (3 plates-formes Clang, CodeStyle, ASan, TSan, LSan, clang-tidy ; UBSan exclu). Correction :
                 seuls les 3 BT de plate-forme ont un release_preset (CMakePresetsLinux/MinGW) ; sanitizers et clang-tidy
                 ont release_preset vide (CMakePresetsCI.json) et ne compilent qu'une fois, CodeStyle ne compile pas.
```

```text
ID             : H-06
Axe            : CI
Nature         : risque
Gravité        : moyenne
Preuve         : .teamcity/_Self/Project.kt:45-55 (« the repository's "main merging" ruleset requires
                 no status check by name »).
Constat        : D'après le DSL lui-même, aucun check TeamCity n'est requis pour fusionner sur main. La
                 CI informe mais ne bloque pas. Combiné à F-03, une PR peut fusionner avec un UB ou un
                 test rouge.
Recommandation : corriger — exiger au moins « Build / Linux x64 / Clang », « Quality / Code Style »
                 et « Quality / Sanitizer Address » dans le ruleset GitHub (noms déjà sans préfixe).
                 Effort S.
Statut         : plausible
Vérification   : seule preuve : le commentaire du DSL (Project.kt:50-53), écrit par l'auteur, donc crédible mais non vérifiable.
                 La combinaison avec F-03 ne tient pas : UBSan ne tourne pas sur les PR (skipAutoPRs).
```

### Gravité basse

```text
ID             : F-08
Axe            : Qualité
Nature         : faiblesse
Gravité        : basse
Preuve         : ci/actions/code_style.py:58-63 (SOURCE_ROOTS : ni test/ ni source/owlnest/runner,
                 source/owlrunner/sources n'existe pas) ; mesuré : clang-format --dry-run échoue sur
                 test/renderer_tests/SlangCompute_test.cpp et BitonicSortPass_test.cpp ; codespell
                 trouve 8 fautes dans test/ + runner.
Constat        : Le gate de style ignore 22 kLOC (tests et runner). La racine fantôme est neutralisée
                 silencieusement par _iter_sources (« if not r.exists(): continue »).
Recommandation : corriger — ajouter test/ et source/owlnest/runner, faire échouer une racine absente.
                 Effort S.
Statut         : confirmé
```

```text
ID             : F-09
Axe            : Qualité
Nature         : faiblesse
Gravité        : basse
Preuve         : 25 tests sans assertion (script audit_cov/tq.py), par exemple
                 SceneTriggerTest.TimerFiresAfterDuration (test/scene_tests/SceneTrigger_test.cpp:99-119),
                 Renderer.fakeScene, SoundSystem.listenerControl ; suites nommées *Coverage* : 213 tests.
Constat        : Une minorité de tests ne fait que vérifier « ça ne plante pas » alors que leur nom
                 promet un comportement (le timer « fires » n'est jamais observé). Les suites
                 « *Coverage » ont été écrites pour la couverture, mais elles affirment en majorité de
                 vrais comportements (ex. SceneCoverage.CopyCreatesIndependentEntities).
Recommandation : surveiller — exiger au moins une assertion (script dans CodeStyle), renommer
                 « *Coverage » par comportement. Effort S.
Statut         : confirmé
```

```text
ID             : F-10
Axe            : Qualité
Nature         : étrangeté
Gravité        : basse
Preuve         : test/CMakeLists.txt:36-45 (un ctest par catégorie, TIMEOUT 3600, RESOURCE_LOCK sur 6/16
                 suites « to avoid sporadic SEGFAULTs ») ; test/core_tests/application_test.cpp:83
                 (fichier temporaire au nom fixe param.yml) ; test/CMakeLists.txt:76
                 (COVERAGE_GCOV "llvm gcov" : il manque le tiret de llvm-cov).
Constat        : Les suites sont des processus distincts : un segfault « croisé » entre elles ne
                 s'explique que par un état partagé sur disque (cache de shaders, noms temporaires fixes),
                 que le verrou masque au lieu de le corriger. Avec un seul ctest par catégorie, on ne voit
                 pas quel test échoue, on ne parallélise pas dans une catégorie, et le timeout d'une heure
                 laisse un blocage consommer un agent. La cible All_Tests + couverture CMake est cassée
                 sous Clang.
Recommandation : corriger — gtest_discover_tests (timeout par test ~60 s), répertoires temporaires
                 uniques par processus, retirer le RESOURCE_LOCK une fois la cause trouvée, corriger
                 « llvm-cov gcov ». Effort S.
Statut         : plausible (cause des segfaults non reproduite)
```

```text
ID             : F-11
Axe            : Qualité
Nature         : étrangeté
Gravité        : basse
Preuve         : .claude/rules/testing.md (« Slang tests excepted, ~50 s cold »), mémoire projet (« First
                 Slang compilation per session takes ~50s ») ; mesuré : 9 tests SlangCompute.* en 0,95 s,
                 première compilation 148 ms.
Constat        : La consigne IA sur le coût de Slang est périmée. Elle pousse à des fixtures
                 SetUpTestSuite inutiles et fausse les estimations de durée.
Recommandation : corriger (axe J) — mettre à jour testing.md et la mémoire. Effort S.
Statut         : confirmé
```

```text
ID             : F-12
Axe            : Qualité
Nature         : étrangeté
Gravité        : basse
Preuve         : cmake/Sanitizers.cmake:46,58,81 définissent OWL_SANITIZER_CUSTOM_ALLOCATOR, que grep ne
                 trouve nulle part dans source/ ; -fsanitize-recover=leak (ligne 77) sans effet ; un seul
                 sanitizer autorisé à la fois (ligne 86), ce qui interdit ASan+UBSan.
Constat        : Des réglages morts ou sans effet traînent dans la configuration des sanitizers, et le
                 FATAL_ERROR empêche la combinaison la plus rentable.
Recommandation : corriger — supprimer la macro morte, autoriser ASan+UBSan. Effort S.
Statut         : confirmé
```

```text
ID             : H-07
Axe            : CI
Nature         : faiblesse
Gravité        : basse
Preuve         : ci/utils/run.py:52-56 (MODE_FOR_NINJA : toute ligne hors progression passe en ERROR) ;
                 ci/utils/logging.py:15-17 (ERROR devient un service message status='ERROR').
Constat        : Pendant un build Ninja, chaque warning ou ligne d'information du compilateur apparaît
                 en rouge « erreur » dans TeamCity. Le vrai signal se noie dans ce bruit.
Recommandation : corriger — ne classer ERROR que les lignes « : error: » et le résumé ninja. Effort S.
Statut         : confirmé
```

```text
ID             : H-08
Axe            : CI
Nature         : étrangeté
Gravité        : basse
Preuve         : ci_action.py:6-12 (« python3 -u » cassé sous poetry run 2.x) contre
                 CodeStylingCheck.kt:45 (« poetry run python3 -u », et mesuré OK avec Poetry 2.5.1) ;
                 GlobalBuild.kt:101/119 (id Build_Release affiché « Build », id Build_Debug affiché
                 « Build Release ») ; Build.kt:16 (« 11 buildTypes », il y en a 12) ; preset.py:187 (repli
                 root/"build" alors que get_build_dir, ligne 17, renvoie root/"output"/"build") ;
                 GlobalBuild.kt:76-80 (étape native en python3 de l'hôte, qui doit être ≥ 3.12 à cause de
                 la f-string PEP 701 de preset.py:149).
Constat        : Commentaires et noms ont divergé du comportement. Isolément, chaque point est mineur ;
                 ensemble, ils rendent le diagnostic d'un échec CI plus lent.
Recommandation : corriger au fil de l'eau. Effort S.
Statut         : confirmé
```

```text
ID             : H-09
Axe            : CI
Nature         : étrangeté
Gravité        : basse
Preuve         : ci/utils/venv.py (132 lignes), ci_action.py:58-95, cmake/Poetry.cmake : détection et
                 réparation d'un venv Poetry « cross-arch » partagé ; image CI
                 builder-clang-llvm22-ubuntu2404 (CMakePresetsLinux.json:22) ≠ image locale
                 devel-ubuntu2404 (docker/run.sh:18).
Constat        : Environ 200 lignes servent à contourner un $HOME d'agent partagé entre conteneurs amd64
                 et arm64 émulés : complexité accidentelle. De plus, la CI et le poste local n'utilisent
                 pas la même image, si bien que « vert en local » ne garantit pas « vert en CI ».
Recommandation : surveiller — un venv par architecture (POETRY_VIRTUALENVS_PATH suffixé par l'arch), et
                 une seule image de référence (ou un tag commun). Effort S.
Statut         : confirmé
```

### Forces

```text
ID             : F-13
Axe            : Qualité
Nature         : force
Gravité        : moyenne
Preuve         : 1 127 tests, 4 056 assertions, 3,6 par test ; math 95 %, debug 96 %, script 84 %, data 82 % ;
                 PrefabSerializer.cpp 92 %, SceneSerializer.cpp 91 %, SaveManager.cpp 84 %,
                 LuaBindings.cpp 83 % ; tests nommés par comportement (SceneHierarchy.CircularReferencePrevention,
                 CascadeDelete…) ; suite complète en 3,4 s ; ASan et TSan propres dans l'ordre par défaut,
                 0 UB sur les 16 binaires relancés.
Constat        : La logique pure (math, hiérarchie, sérialisation, prefab, sauvegarde, bindings Lua) est
                 testée sérieusement, avec des assertions de comportement et pas seulement des tests de
                 fumée. La suite est assez rapide pour tourner à chaque sauvegarde.
Recommandation : garder.
Statut         : confirmé
```

```text
ID             : H-10
Axe            : CI
Nature         : force
Gravité        : moyenne
Preuve         : ci/actions/__init__.py (découverte par pkgutil + issubclass(BaseAction)) ;
                 ci/actions/base/action.py (43 lignes) ; la plupart des actions font moins de 100 lignes ;
                 la même commande `poetry run python ci_action.py <Action> <preset>` sert en local
                 (docker/run.sh) et dans TeamCity (ciAction(), GlobalBuild.kt:22-37) ; le preset CMake
                 porte la config CI (vendor.silmaen : image, release_preset, publish_doc).
Constat        : Une seule source de vérité (les presets) et un seul point d'entrée rendent la CI
                 reproductible en local. Ajouter une action ne demande aucun enregistrement. Le DSL Kotlin
                 factorise proprement la matrice (stdPlatform, Sanitizer, bridgeOverride).
Recommandation : garder.
Statut         : confirmé
```

```text
ID             : H-11
Axe            : CI
Nature         : force
Gravité        : moyenne
Preuve         : ci/actions/clang_tidy.py:1-28, 266-401 (périmètre du diff = .cpp touchés + fermeture
                 d'includes issue de `ninja -t deps`, retour au périmètre complet sur tout doute : pas de
                 merge base, diff vide, CMake / .clang-tidy / depmanager.yml modifiés) ; diagnostics au
                 format GNU (code_style.py:92-122) annotés sur le diff par teamcity-github-bridge ; CodeStyle
                 en 4 s ; drafts, filtres de chemins et skipIfCommitPassed (BridgeHelpers.kt).
Constat        : Le ciblage clang-tidy est correct par construction : il échoue toujours vers « plus
                 d'analyse », jamais vers moins. Il est rare de le voir aussi rigoureux dans un projet de
                 cette taille. Les constats de style arrivent directement sur la ligne de la PR.
Comparaison    : équivalent de clang-tidy-diff ou de reviewdog sur GitHub Actions, avec une fermeture
                 d'includes plus juste que la plupart des scripts maison.
Recommandation : garder (et tester, voir H-04). Attention au bus factor : le plugin GitHub est maison.
Statut         : confirmé
```

```text
ID             : H-12
Axe            : CI
Nature         : force
Gravité        : basse
Preuve         : scan des fichiers suivis et de l'historique (motifs mot de passe / token / clé privée /
                 ghp_ / AKIA) : aucun résultat ; .gitignore contient .env ; .env fait 0 octet et n'a
                 jamais été commité (git log --all -- '*.env' vide) ; identifiants fournis par des
                 paramètres TeamCity (%deploy_passwd%, %remote_passwd%) ; GITHUB_CONNECTION_ID
                 (Github.kt) est un identifiant de connexion, pas un secret.
Constat        : Aucun secret en clair dans le dépôt ni dans son historique. La faiblesse se situe dans
                 la manipulation des secrets à l'exécution (H-01, H-02), pas dans le dépôt.
Recommandation : garder ; ajouter gitleaks ou detect-secrets à CodeStyle pour le verrouiller. Effort S.
Statut         : confirmé
```

## Comparaison brève avec les pratiques courantes (opinion, sauf mention)

| Pratique                  | Courant (GitHub Actions, projets C++ moteurs)        | Owl                                                       |
|---------------------------|------------------------------------------------------|-----------------------------------------------------------|
| Matrice OS × compilateur  | `strategy.matrix`, fail-fast désactivable            | DSL Kotlin factorisé ; 8 BT par PR, 12 sur `main`         |
| Cache de build            | `actions/cache` + ccache/sccache, build dir conservé | ccache présent, mais `rmtree` du build dir à chaque run   |
| Sanitizers                | ASan+UBSan combinés, `halt_on_error=1`, TSan à part  | 4 jobs séparés, UBSan non bloquant (F-03), LSan redondant |
| Analyse statique sur diff | clang-tidy-diff, reviewdog, CodeQL                   | Ciblage par fermeture d'includes, plus fin (H-11)         |
| Couverture                | Codecov/Coveralls, gate sur la différence de PR      | Rapport HTML en artefact, aucun gate (F-04)               |
| Tests de rendu            | images de référence sous lavapipe/SwiftShader/WARP   | absents (F-01)                                            |
| Ordre des tests           | `--gtest_shuffle` en CI                              | jamais ; dépendances d'ordre réelles (F-05)               |
| Secrets                   | variables d'environnement masquées, jamais en argv   | argv + journalisés (H-01)                                 |
| Merge gate                | checks requis dans la protection de branche          | aucun check requis (H-06, plausible)                      |
| Tests de l'outillage CI   | pytest sur les scripts maison                        | aucun (H-04)                                              |

## Pistes pour l'avenir

1. **Filet de rendu** (prérequis à tout chantier rendu de l'axe K) : un binaire `owl_render_tests` lancé
   sous Xvfb + lavapipe/llvmpipe, qui rend des scènes fixes de `engine_assets/` sur les deux backends,
   relit le framebuffer et compare à des images de référence avec tolérance. Le futur harnais `bench/`
   (« frame bench ») peut partager les mêmes scènes.
2. **Éditeur testable** : sortir undo/commands/snapshot et `ActionRegistry` de l'exécutable OwlNest vers
   une bibliothèque statique `OwlNestCore`, testée sans ImGui ; plus tard, des tests ImGui pilotés
   (`imgui_test_engine`) pour les panneaux.
3. **Sanitizers qui mordent** : un seul job ASan+UBSan non récupérable, avec `--gtest_shuffle` et
   `ASAN_OPTIONS=detect_stack_use_after_return=1`, en remplacement de trois jobs ; TSan conservé, avec
   un test de stress du `Scheduler`.
4. **Gate de couverture différentielle** sur le moteur hors backends, avec la couverture brute publiée
   à côté pour garder les trous visibles.
5. **Tests du CI** : pytest sur `ci/`, plus ruff et mypy dans CodeStyle ; un faux dépôt git + un faux
   `ninja -t deps` suffisent pour couvrir `_select`.
6. **Fuzzing** des désérialiseurs (YAML de scène, `.owlpack`, `.owlprefab`, sauvegardes) avec libFuzzer :
   ce sont les entrées non fiables du moteur (mods, sauvegardes partagées).
7. **Mesurer la CI** : exporter les durées par BT et par étape (API REST TeamCity) dans `20-mesures.md`,
   puis élaguer la matrice au signal par minute (arm64 en nightly, release build seulement sur `main`).
8. **Sécuriser la publication** : `api.py` versionné et épinglé, secrets en variables d'environnement,
   et `gitleaks` en pré-gate.
