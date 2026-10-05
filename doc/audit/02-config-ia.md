# Configuration IA : état initial, changements, suite

> Statut : première passe appliquée le 2026-10-05. Réévaluation prévue en fin d'audit (axe J).

## 1. État initial

| Source                        | Portée                  | Taille                           | Problèmes relevés                                                                                                                                                                                                                                                                                        |
|-------------------------------|-------------------------|----------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `~/.claude/CLAUDE.md`         | globale                 | ~40 lignes                       | Saine ; contredite par la mémoire du projet (build, commits).                                                                                                                                                                                                                                            |
| `CLAUDE.md` racine            | projet, chargé toujours | 466 lignes, ~33 Ko               | Commandes `cmake` nues (cmake absent du PATH hôte), version périmée (0.2.1), descriptions de sous-systèmes chargées même pour une tâche CI ou doc.                                                                                                                                                       |
| `.claude/rules/` (8 fichiers) | projet                  | ~930 lignes                      | 3 règles sans `paths:` chargées toujours ; liste de catégories de tests fausse (`physic`, manque io / script / voxel / gui) ; changelog « sous-listes autorisées » contraire à la règle globale.                                                                                                         |
| `.claude/skills/` (5 skills)  | projet                  | ~130 lignes                      | Toutes en natif et sur `linux-gcc-*`, contre la préférence Clang et la règle Docker.                                                                                                                                                                                                                     |
| Mémoire auto                  | locale                  | 39 fichiers, index de 117 lignes | Index qui contenait du contenu (et non des pointeurs) ; doublons du dépôt ; notes contradictoires (closeout v0.2.1 « non corrigé » contre findings Vulkan « corrigé ») ; faits périmés (pack dans `io/`, wrapper `spirv_cross.h` inexistant, PNG d'icônes par tailles, PR1 isométrique « uncommitted »). |
| `.claude/settings.local.json` | locale                  | 144 autorisations                | Accumulation de commandes ponctuelles, chemins `/source/personnel/…` qui n'existent pas, worktrees supprimés ; `Bash(git:*)` autorisait `git push`.                                                                                                                                                      |

Contradictions bloquantes :

- **Build** : global « Docker uniquement » contre mémoire « pas de Docker, natif via cmake CLion ».
- **Commits** : global « commit local court sur une branche » contre mémoire « jamais de `git commit` ».
- **Changelog** : global « une ligne, pas de sous-liste » contre `documentation.md` « sous-items si besoin ».

Contexte chargé à chaque session (CLAUDE.md + règles sans `paths:` + index mémoire) : **~55 Ko**.

## 2. Changements appliqués

| Élément                                      | Changement                                                                                                                                                                                                                                                                                     |
|----------------------------------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `docker/run.sh` (nouveau)                    | Lanceur unique, miroir de *Docker Owl* ; options `--gui` et `--perf`. Validé (configure, build, tests, CodeStyle, Documentation).                                                                                                                                                              |
| `CLAUDE.md`                                  | 466 → 104 lignes : build Docker, presets Clang, tableau des actions CI, carte du dépôt, workflow, pièges connus.                                                                                                                                                                               |
| Nouvelles règles ciblées                     | `renderer.md`, `scene.md`, `editor.md`, `script.md`, `core-task.md`, `data-pack.md`, `dependencies.md` : détail par sous-système, chargé seulement quand on touche aux fichiers concernés.                                                                                                     |
| Règles existantes                            | `documentation.md` et `module-layout.md` ciblées par chemin ; catégories de tests corrigées ; changelog aligné sur la règle globale ; commandes passées par `docker/run.sh` ; options CMake déplacées dans `cmake.md` ; « type usage » et habitudes clang-tidy dans `cpp-style.md`.            |
| Skills                                       | `build`, `test`, `coverage`, `add-test`, `add-dependency` réécrites (Docker, Clang) ; nouvelle skill `check` (gate avant de rendre la main).                                                                                                                                                   |
| `.claude/settings.json` (nouveau, versionné) | Autorisations partagées propres (`docker/run.sh`, git en lecture, commit local) ; **`ask`** sur `git push`, `gh pr create`, `git reset --hard`, `git clean` ; **`deny`** sur `pip install` et `sudo apt`.                                                                                      |
| `.claude/settings.local.json`                | Réduit de 144 à 14 entrées génériques (lecture, domaines web, MCP).                                                                                                                                                                                                                            |
| Mémoire                                      | 39 → 8 fichiers. Les leçons techniques (Vulkan, OpenGL, voxel, atlas, clang-tidy) ont été distillées dans les règles du dépôt, où elles profitent à tous ; restent les faits que le dépôt ne peut pas porter (profil, décision Docker, audit en cours, plans de release, références externes). |
| CI                                           | `doc/audit/` exclu de codespell et de Doxygen (voir `01-environnement.md`, §4).                                                                                                                                                                                                                |

Sauvegarde intégrale de l'ancienne mémoire et de l'ancien `settings.local.json` :
`~/.claude/projects/-data-sources-personnel-stack-owl-Owl/memory-backup-2026-10-05/`.

Contexte chargé à chaque session après changement : **~14,5 Ko** (−74 %). Le détail reste accessible,
mais n'arrive que lorsque les fichiers concernés sont lus ou modifiés.

### Décisions prises en ton nom (à valider)

- **Commits** : j'ai retenu ta règle globale (commit local court, jamais de push ni de PR) et supprimé la
  mémoire « jamais de commit », plus ancienne (avril 2026). `git push` et `gh pr create` demandent
  désormais une confirmation, même s'ils restent bloqués par la règle.
- **Roadmap** : statu quo, `doc/pages/roadmap.md` (publiée par Doxygen) au lieu d'un `ROADMAP.md` racine ;
  la règle le dit explicitement et renvoie à cet audit pour la décision.
- **Docker « sans `--privileged` »** : `docker/run.sh` ajoute les capacités nécessaires au cas par cas
  (`--gui`, `--perf`) au lieu de tout ouvrir comme le fait la toolchain CLion.

## 3. Points ouverts pour la suite

1. **Hooks** : un hook `PostToolUse` lançant `clang-format` (en Docker) sur chaque fichier C++ édité
   éviterait les allers-retours avec `CodeStyle`. Coût : environ 1 s de démarrage de conteneur par
   édition. À tester avant de l'activer.
2. **MCP CLion** : les outils `get_file_problems` et `search_symbol` donnent des diagnostics et une
   navigation sémantique sans build. À évaluer face à grep + clang-tidy pendant l'audit.
3. **`CHANGELOG.md`** : les entrées `[Unreleased]` actuelles sont des paragraphes en gras (« **Diff-scoped
   clang-tidy** — … »), loin de la règle « une ligne, une phrase ». À reformater lors d'une PR de doc.
4. **Documentation utilisateur** : les sections déplacées de `CLAUDE.md` (undo/redo, sauvegarde,
   réglages) ont peu d'équivalent dans `doc/pages/` (`SaveManager` n'apparaît que dans la roadmap).
   Elles devraient y vivre en premier lieu, les règles `.claude/` se contentant d'y renvoyer.
5. **Règle globale** : elle pourrait citer `docker/run.sh` comme convention de lanceur pour tous les projets
   qui en ont un, afin que l'agent ne reconstitue plus la commande `docker run` à partir de l'IDE.
6. **Mesure** : à la fin de l'audit, relever quelles règles les agents ont effectivement chargées et
   utilisées, et élaguer celles qui n'ont servi à rien.

## 4. Retours de la première vague d'agents (2026-10-05)

Huit sous-agents ont instruit les axes A à I, chacun dans un contexte neuf, avec une consigne autonome
renvoyant à `00-cadrage.md`. Ce qu'ils ont révélé sur la configuration :

| Retour                                                                      | Suite donnée                                                                  |
|-----------------------------------------------------------------------------|-------------------------------------------------------------------------------|
| `docker/run.sh` ne transmettait pas stdin hors terminal ni ne montait le scratchpad | Corrigé : `-i` si stdin est un pipe, variable `OWL_DOCKER_MOUNTS`.       |
| « Compilation Slang ~50 s » faux en Release (74 ms à froid, mesuré deux fois) | Corrigé dans `CLAUDE.md`, `testing.md`, `slang-shaders.md`.                  |
| `CodeStyle` ne scannait pas `bench/`                                        | `bench` ajouté aux `SOURCE_ROOTS`.                                            |
| Un agent a lu l'ancien index mémoire (liens cassés)                         | Index actuel vérifié : tous les liens résolvent. Instantané de début de session. |
| La règle « une caméra 2D par frame » décrit un défaut (UBO Vulkan non versionné, B-03), pas un choix | À reformuler dans `renderer.md` quand B-03 sera traité.        |
| Les comparaisons à Hazel / Godot / Bevy viennent de la mémoire des modèles  | Sourcées par `30-etat-de-l-art.md` ; les agents doivent marquer « non revérifié ». |
| Un agent a lancé une fois un script de texte en natif                       | Sans conséquence ; la règle Docker est rappelée dans chaque consigne d'agent. |

Coût observé : 130 k à 360 k tokens par agent d'axe, 7 à 60 min chacun ; le contexte de l'orchestrateur ne
reçoit qu'un résumé de 20 lignes par agent, les fichiers de constats faisant foi.
