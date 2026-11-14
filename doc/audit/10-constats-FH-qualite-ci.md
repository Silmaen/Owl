# Constats — axes F (qualité) et H (CI)

> Statut : revu le 2026-11-12 sur `Feature/sample-games-new` b4281d72, commandes via `./docker_run.sh`. La couverture
> par module date du 2026-10-05 (clang 22.1.8, `linux-clang-debug`, gcovr 8.x) et n'a pas été refaite ; les chiffres CI
> viennent de l'API publique GitLab (lecture seule, 2026-11-12). **constaté** = lu dans le dépôt ; **mesuré** =
> commande exécutée ou relevé CI ; **opinion** = jugement.

## Résumé

- Tests de logique réels (~3 580 `TEST`/`TEST_F`/`TEST_P`, 223 scénarios), gardes CodeStyle (assertions, NOLINT
  morts, identifiants de doc), fuzz et include-check nocturnes (F-13, F-14).
- Couverture officielle 82–86 % mais sans backends GPU / son / fenêtre ni éditeur (`source/owlnest` hors filtre)
  (F-01, F-02, F-04).
- CI GitLab mince au-dessus de `ci_action.py`, réutilisation des verdicts à arbre identique, ciblage clang-tidy sûr,
  secrets en variables masquées (H-10, H-11, H-12).
- Le code fusionné casse `main` (4 pipelines rouges sur 14) et la nocturne est rouge 4 fois sur 6 (H-06, H-13).
- 15 constats : 0 haut, 8 moyens, 7 bas ; 5 forces, 8 faiblesses, 0 risque, 2 étrangetés.

## Mesures portant un constat

Couverture par module, lignes officielles / brutes (2026-10-05, non remesurée) : les écarts viennent des exclusions.

| Module         | Officielles | Brutes                 |
|----------------|-------------|------------------------|
| `owl/renderer` | 76,3 %      | 39,7 % (2 972 / 7 491) |
| `owl/sound`    | 81,8 %      | 34,0 % (162 / 476)     |
| `owl/window`   | 83,0 %      | 19,6 % (44 / 225)      |
| `owl/io`       | exclu       | 0,0 % (0 / 412)        |
| `owl/gui`      | 38,7 %      | 38,7 % (1 198 / 3 099) |
| **Moteur**     | **70,9 %**  | **57,6 %**             |

CI GitLab (API publique, 2026-11-12) :

| Relevé                                         | Valeur                                                        |
|------------------------------------------------|---------------------------------------------------------------|
| Couverture officielle `linux-gcc-debug` (MR)   | 82,34 %                                                       |
| Couverture officielle `linux-clang-debug` (nuit) | 86,19 %                                                     |
| Pipeline de MR prête (65, 61)                  | 84–88 min ; chemin critique `linux-gcc-debug` 41–61 min       |
| `code-style` / `linux-clang-release` / Windows | 2–3 min / 10–35 min / 8–37 min                                |
| `sanitizer-address` (`main` seulement)         | 35 min                                                        |
| Nocturne (69)                                  | arm64 59 min, static-analyzer 79 min, include-check 57 min    |
| Pipelines `push` de `main` (2026-11-06 → 11)   | 10 verts, 4 rouges (code-style, ASan, Windows ×2, Clang)       |
| Nocturnes planifiées (2026-11-07 → 12)         | 1 verte, 4 rouges, 1 annulée                                  |

Autres : 23 `GTEST_SKIP` ; `CodeStyle` complet 4,0 s et ctest TSan 10 s (2026-10-05, non refaits).

## Constats — gravité moyenne

```text
ID             : F-01 — faiblesse, moyenne, confirmé (qualité)
Constat        : des tests d'image exécutent les backends GPU (Vulkan sous lavapipe, OpenGL sous Xvfb), mais `gcovr.cfg`
                 exclut toujours vulkan / opengl / openal / glfw : la couverture officielle ne mesure ni le rendu GPU
                 ni le son.
Preuve         : test/unit/render_tests/RenderImage_test.cpp, ImageCompare.h ; gcovr.cfg:41-42.
Recommandation : corriger, effort S (publier la couverture des backends à côté de l'officielle).
```

```text
ID             : F-02 — faiblesse, moyenne, confirmé (qualité)
Constat        : l'éditeur a 84 fichiers de tests, mais le filtre gcovr ne retient que `source/owl/` : les ~58 600 lignes de
                 `source/owlnest` (un tiers du C++) ne comptent dans aucune couverture ni dans la porte de baisse.
Preuve         : gcovr.cfg:41 (`filter = .*source/owl/.*`) ; test/unit/owlnest_tests ; `wc -l` des `.cpp` (owlnest 58 643,
                 owl 111 897).
Recommandation : corriger, effort S (publier la couverture de `source/owlnest` à part, sans l'inclure dans la porte au
                 début).
```

```text
ID             : H-06 — faiblesse, moyenne, confirmé (CI ; réglage serveur non vérifiable)
Constat        : le pipeline de MR est relancé au passage de brouillon à prêt en réutilisant les builds verts, mais
                 ASan ne tourne que sur `main` (et à la main sur une MR) : une erreur mémoire n'apparaît qu'après la
                 fusion. Effet mesuré : 4 pipelines `push` de `main` rouges sur 14 (code-style, ASan, Windows).
Preuve         : .gitlab-ci.yml:156-162, 206-210 ; API GitLab, pipelines 14, 34, 51, 64.
Recommandation : surveiller, effort S (ASan sur les MR prêtes si le coût de 25 minutes de runner le permet).
```

```text
ID             : H-13 — faiblesse, moyenne, confirmé (CI)
Constat        : la nocturne a été rouge 4 fois sur 6 depuis la migration (arm64 sous QEMU, TSan, include-check,
                 static-analyzer, bench) ; rien ne bloque sur elle, les échecs se cumulent sans propriétaire.
Preuve         : API GitLab, pipelines 18, 23, 43, 62 (enfants 24, 44, 63) ; ci/gitlab/nightly.yml.
Recommandation : corriger, effort S (trier chaque échec le matin, marquer les jobs instables, objectif nocturne verte).
```

```text
ID             : F-13 — force, moyenne, confirmé (qualité)
Constat        : la logique pure (math, hiérarchie, sérialisation, prefab, sauvegarde, physique déterministe, bindings Lua,
                 scripts visuels) est testée par comportement, plus 223 scénarios `.owltest` joués de bout en bout.
Preuve         : ~3 580 TEST/TEST_F/TEST_P dans test/unit ; test/unit/scenarios.
Recommandation : garder
```

```text
ID             : F-14 — force, moyenne, confirmé (qualité)
Constat        : CodeStyle porte des gardes rares : tout test affirme quelque chose, aucun `NOLINT` ne cite un check
                 inactif, chaque identifiant cité en `code` dans `doc/pages` existe, dépendances de modules acycliques,
                 secrets ; la nocturne ajoute libFuzzer (pack, tilemap) et un include-check sans PCH.
Preuve         : ci/actions/code_style.py:28-45 ; ci/utils/test_assertions.py, nolint.py, doc_identifiers.py,
                 module_deps.py ; test/fuzz ; ci/gitlab/nightly.yml:71-93.
Recommandation : garder (étendre le fuzz aux chargeurs YAML de scène et de prefab).
```

```text
ID             : H-10 — force, moyenne, confirmé (CI)
Constat        : une seule source de vérité (les presets et leur bloc `vendor.silmaen`) et un point d'entrée
                 (`ci_action.py <Action> <preset>`) pour le local et GitLab ; le YAML dit seulement où et quand. Un job de
                 push reprend le verdict d'un succès sur un arbre identique, la nocturne ne tourne que si `main` a bougé.
Preuve         : ci/gitlab/templates.yml:3-5 ; ci/actions/build_and_test.py:15-50 ; ci/utils/gitlab.py:139-179 ;
                 ci/actions/nightly_gate.py.
Recommandation : garder
```

```text
ID             : H-11 — force, moyenne, confirmé (CI)
Constat        : le ciblage clang-tidy du diff (`.cpp` touchés + fermeture d'includes via `ninja -t deps` ou
                 `clang-scan-deps`) retombe sur le périmètre complet au moindre doute ; les diagnostics remontent dans la
                 MR en rapports Code Quality.
Preuve         : ci/actions/clang_tidy.py:8-27, 264-273 ; ci/gitlab/templates.yml:93-99.
Recommandation : garder
```

## Constats — gravité basse

```text
ID             : F-04 — faiblesse, basse, confirmé (qualité)
Constat        : la porte de baisse (> 1 point sous le dernier succès de `main`) compare un chiffre qui exclut backends,
                 fenêtre, io et éditeur, et se contente d'un avertissement sans référence ni jeton d'API.
Preuve         : gcovr.cfg:41-42 ; ci/actions/coverage.py:66-83.
Recommandation : corriger, effort S (publier aussi le chiffre brut ; échouer si le jeton manque en CI).
```

```text
ID             : F-07 — faiblesse, basse, confirmé (qualité)
Constat        : clang-tidy analyse désormais les tests (jeu allégé) et l'analyseur statique tourne la nuit, mais le seuil
                 de complexité cognitive reste à 75 et l'analyseur ne voit pas les MR.
Preuve         : .clang-tidy:103-104 ; cmake/CMakePresetsCI.json:15-27 (`OWL_TESTING=ON`) ; test/unit/.clang-tidy ;
                 ci/gitlab/nightly.yml:60-64.
Recommandation : corriger, effort S (seuil 25–40 avec dérogations).
```

```text
ID             : F-12 — étrangeté, basse, confirmé (qualité)
Constat        : `OWL_SANITIZER_CUSTOM_ALLOCATOR` est défini par ASan et TSan mais n'est lu que par deux fichiers de test
                 (comptage d'allocations), sans documentation.
Preuve         : cmake/Sanitizers.cmake:44, 56 ; core_tests/CoreCoverage_test.cpp:223 ;
                 renderer_tests/Renderer2DAllocation_test.cpp:24.
Recommandation : corriger, effort S (commenter le sens ou renommer en `OWL_TEST_NO_ALLOC_COUNT`).
```

```text
ID             : H-04 — faiblesse, basse, plausible (CI)
Constat        : le wrapper CI (~7 100 lignes, 28 fichiers de tests pytest lancés par CodeStyle) n'a pas de couverture
                 mesurée ; ses parties subtiles (sélection du diff clang-tidy, réutilisation des verdicts, CodeStyle de
                 1 560 lignes) restent non revérifiées.
Preuve         : ci/tests/ ; ci/actions/code_style.py ; ci/utils/gitlab.py:151-179.
Recommandation : surveiller, effort S (mesurer la couverture de `ci/`).
```

```text
ID             : H-05 — faiblesse, basse, confirmé (CI)
Constat        : une MR prête attend 84–88 min, dont 41–61 min pour `linux-gcc-debug` (couverture) ; arm64 (59 min sous
                 QEMU) et Windows GCC sont hors MR. Le signal rapide (`code-style` + Clang release) reste à 12–38 min.
Preuve         : API GitLab, pipelines 61, 65, 69 ; .gitlab-ci.yml:149-173.
Recommandation : surveiller, effort S (paralléliser ou échantillonner la couverture ; suivre ces durées dans
                 `20-mesures.md`).
```

```text
ID             : H-09 — étrangeté, basse, confirmé (CI)
Constat        : la CI (`builder-ubuntu2604`, sans tag ni digest) et le poste local (`devel-ubuntu2604:latest`) utilisent
                 deux images ; le commentaire de `docker_run.sh` annonce pourtant l'image CI par défaut. Un verdict
                 réutilisé à arbre identique ignore un changement d'image. Le contournement du venv partagé amd64 / arm64
                 survit alors qu'aucun runner arm64 ne tourne.
Preuve         : ci/gitlab/templates.yml:16 ; docker_run.sh:19, 26 ; ci/actions/build_and_test.py:44 ; ci/utils/venv.py:1-16.
Recommandation : surveiller, effort S (épingler l'image par digest, l'inclure dans la clé de réutilisation, corriger le
                 commentaire).
```

```text
ID             : H-12 — force, basse, confirmé (CI)
Constat        : aucun secret dans les fichiers suivis ; identifiants en variables CI/CD masquées, listées en tête des
                 templates ; CodeStyle scanne chaque fichier suivi et refuse un `.env` suivi.
Preuve         : ci/gitlab/templates.yml:7-11 ; ci/actions/code_style.py:42-45 ; ci/utils/secrets.py ; .gitignore.
Recommandation : garder
```
