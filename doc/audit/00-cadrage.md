# Audit du dépôt Owl — cadrage

> **Statut : cadrage validé le 2026-10-05, revu le 2026-10-20 sur `main` 4e1a6a10.** Décisions complémentaires au §10.
> **Lecteurs visés** : agents IA (Claude Code et sous-agents) qui exécutent l'audit, et le mainteneur.
> Chaque section est écrite pour être découpée en tâches indépendantes.
> Environnement et configuration IA : §11 et §12.

## 1. Objectif

Dresser un bilan **honnête et argumenté** du dépôt Owl (moteur de jeu C++23, éditeur Owl Nest, CI
Python, pipelines TeamCity) : ce qui est solide, ce qui est faible, ce qui est étrange, ce qui mérite
d'être conservé. Rien n'est sacré : choix techniques, conventions, outillage, processus, configuration
IA du dépôt.

Quatre résultats sont attendus :

1. **Un bilan du dépôt** : forces, faiblesses, bizarreries, risques, chacun avec une preuve.
2. **Une confrontation à l'état de l'art** : chaque choix structurant comparé à ce que font les autres
   moteurs, avec des **mesures de performance du moteur** (priorité sur la chaîne de build).
3. **Une vision de l'avenir du moteur** : trajectoire à long terme, y compris des chantiers longs ou
   complexes qui attendent d'autres briques, avec leurs dépendances (§5).
4. **Une configuration IA plus efficace** : déjà optimisée en première passe (§12),
   à réévaluer à la fin de l'audit à la lumière de ce que les agents auront réellement utilisé.

## 2. Décisions de cadrage (2026-10-05)

| Sujet       | Décision                                                                            |
|-------------|-------------------------------------------------------------------------------------|
| Build       | Docker uniquement, via `./docker_run.sh` (miroir de la toolchain CLion *Docker Owl*). |
| Mesures     | Performance du **moteur** en priorité ; la chaîne de build est secondaire.          |
| Couverture  | Tous les axes, aussi profond que possible.                                          |
| Langue      | Français pour les livrables de l'audit.                                             |
| Emplacement | `doc/audit/`, avec un point d'entrée `AUDIT.md` à la racine.                        |
| Horizon     | Large : avenir du moteur, y compris ce qui ne peut pas être fait tout de suite.     |
| Config IA   | Optimisée tout de suite (fait), réévaluée en fin d'audit.                           |

## 3. Ce que ce n'est pas

- Pas une session de correction : l'audit **ne modifie pas** `source/`, `test/`, `ci/`, `cmake/`. Les
  correctifs viendront ensuite, dans des PR séparées, une fois le bilan validé. Seule exception : le
  harnais de benchmark `test/bench/` (§10.1).
- Pas une liste de lint : un problème de style n'entre dans le bilan que s'il révèle un problème de fond.
- Pas un avis non sourcé : toute affirmation renvoie à `chemin:ligne`, une commande reproductible, une
  mesure avec son protocole, ou une référence externe datée.

## 4. Périmètre et axes

Taille : ~217 kLOC C++ (`source/` + `test/`), ~660 fichiers moteur, ~195 fichiers éditeur (dont
`EditorLayer.cpp` à ~1 300 lignes), 20 catégories de tests, ~40 dépendances Conan, CI Python +
TeamCity Kotlin, 420 commits. Build complet en Docker : ~60 s sur 32 cœurs (avec ccache chaud).

| Axe | Contenu                                                           | Questions centrales                                                                              |
|-----|-------------------------------------------------------------------|--------------------------------------------------------------------------------------------------|
| A   | Architecture moteur (`source/owl/`)                               | Découpage en modules, dépendances cycliques, frontière public/privé, couplage par singletons.    |
| B   | Rendu (OpenGL 4.5, Vulkan 1.4, Null, Slang)                       | Abstraction GPU saine ou plus petit dénominateur commun ? Coût par draw, batching, UBO partagés. |
| C   | ECS, scène, hiérarchie, prefab, sauvegarde                        | EnTT bien exploité ? Coût des transforms monde, copies par aller-retour YAML.                    |
| D   | Sous-systèmes : physique, son, script Lua, tâches, voxel, raycast | Choix pertinents ? Intégration propre ou couplage caché ? Coût par frame.                        |
| E   | Éditeur Owl Nest                                                  | Dette d'`EditorLayer`, undo/redo par snapshot YAML : exactitude et coût.                         |
| F   | Qualité : tests, couverture, sanitizers, clang-tidy, CodeStyle    | Que testent vraiment les tests ? Trous sur les chemins à risque (GPU, Vulkan, éditeur).          |
| G   | Build, dépendances, packaging                                     | Conan 2 (`conanfile.py`, `conan.lock`, `conan/recipes/`) : coût, verrouillage, recettes maison.                       |
| H   | CI (`ci/`, `.teamcity/`)                                          | Robustesse, redondances, signal utile par minute de CI.                                          |
| I   | Documentation (`doc/pages`, Doxygen, README, CHANGELOG, roadmap)  | À jour par rapport au code ? Exigences Doxygen proportionnées ?                                  |
| J   | Configuration IA                                                  | Ce que les agents ont réellement utilisé, ce qui manquait, ce qui était faux.                    |
| K   | Avenir du moteur (§5)                                             | Où va Owl, dans quel ordre, avec quelles briques préalables ?                                    |

## 5. Axe K : l'avenir du moteur

Le but n'est pas une roadmap de plus, mais une **carte des chantiers** : pour chacun, l'intérêt, la
difficulté, les briques préalables manquantes, le risque et un ordre de grandeur d'effort. La sortie
attendue est un graphe de dépendances (mermaid) et une trajectoire proposée en horizons (court : v0.5.0,
moyen : v0.6–v0.8, long : v1.0.0 et au-delà). La roadmap (`doc/pages/roadmap.md`, jusqu'à v1.0.0) est
un point de départ, pas une contrainte.

Pistes à instruire au minimum (liste ouverte, chaque agent peut en ajouter) :

- **Rendu 3D moderne** : static meshes, matériaux PBR, éclairage et ombres, post-process ; render graph ;
  rendu piloté par GPU (indirect draw, culling GPU, bindless) ; abandon ou maintien d'OpenGL.
- **Animation** : squelettale, blend trees, IK ; animation 2D (sprites, Spine-like).
- **Physique 3D** (Jolt ou équivalent) à côté de Box2D, et déterminisme.
- **Pipeline d'assets** : import / cook hors-ligne, formats binaires, hot reload, cache dérivé,
  références d'assets par GUID plutôt que par chemin.
- **Script** : hot reload, débogueur, typage, performance (LuaJIT ? Luau ?), API stable pour les jeux.
- **Multithreading** : job system au niveau frame, rendu sur thread séparé, ECS parallèle.
- **Réseau / multijoueur** : réplication, rollback, serveur dédié.
- **Audio** : mixage, bus, spatialisation 3D, streaming.
- **UI de jeu** : système d'UI runtime dédié (le HUD actuel passe par Renderer2D).
- **Plateformes** : Web (WebGPU / wasm), Android, macOS (MoltenVK), console (hypothétique).
- **Outils** : profiler intégré type Tracy, capture GPU (RenderDoc), éditeur visuel de graphes, tests
  de rendu par comparaison d'images.
- **Écosystème** : packaging du moteur pour des projets tiers (OwlDrone), stabilité d'API, versionnage
  sémantique réel, modding.

## 6. Comparaison à l'état de l'art

Pour chaque choix structurant, une fiche courte : **choix Owl / alternatives / critères / mesures /
verdict** (garder, surveiller, remplacer).

Choix à confronter, au minimum :

- **Langage** : C++23 (modules C++20 non utilisés, `std::expected`, `std::print`), double support GCC/Clang/MinGW.
- **ECS** : EnTT face à flecs, à un ECS maison, aux archétypes de Bevy.
- **Rendu** : double backend GL + Vulkan face à une RHI (bgfx, Diligent, NVRHI, sokol, SDL_GPU, WebGPU / Dawn).
- **Shaders** : Slang compilé au runtime face à une compilation hors-ligne (glslang, DXC) ; coût du premier lancement.
- **Script** : Lua 5.5 + bindings manuels face à sol2, LuaJIT, Luau, Wren, AngelScript, C#.
- **Sérialisation** : YAML (yaml-cpp) face à JSON, binaire (cereal, flatbuffers) ; impact sur undo, prefab, sauvegarde.
- **Physique** : Box2D (quelle version ?) face à Box2D v3 et à Jolt pour la 3D.
- **Tâches** : Taskflow face à enkiTS, à un job system à fibres.
- **Dépendances** : Conan 2 face à vcpkg, CPM.cmake.
- **CI** : TeamCity + wrapper Python + plugin GitHub maison face à GitHub Actions.
- **Moteurs de référence** : Hazel (ADN commun probable), Godot, Bevy, O3DE, Wicked Engine, raylib, Defold.

### Mesures de performance du moteur (priorité)

Pas d'optimisation à l'aveugle, pas de chiffre inventé. Chaque chiffre porte son protocole : machine,
GPU, backend, preset, scène, nombre de répétitions, dispersion.

- **Frame** : temps CPU et GPU par frame sur les scènes de `engine_demo/` (2D, tilemap, raycast,
  voxel) et sur des scènes synthétiques à charge croissante ; mêmes scènes en OpenGL et en Vulkan.
- **Rendu 2D** : quads / s, draw calls et changements d'état par frame, coût de `Renderer2D::flush`.
- **Voxel** : temps de génération et de maillage par chunk, streaming, mémoire par chunk.
- **Scène** : `getWorldTransform` sur hiérarchies profondes et larges, itération des vues EnTT, copie de
  scène (Play), coût d'un snapshot undo et d'une instanciation de prefab selon la taille de la scène.
- **Script** : coût d'un `on_update` Lua par entité, aller-retour C++ ↔ Lua.
- **Physique** : pas Box2D selon le nombre de corps, synchronisation avec les transforms.
- **Démarrage et chargement** : premier lancement (compilation Slang : ~165 ms à froid en Release, shaders précompilés au build par `OwlShaderBake`), cache SPIR-V, chargement de
  scène, ouverture de `.owlpack`.
- **Mémoire** : pic et allocations par frame (`TrackerAPI`, heaptrack si ajouté à l'image).
- **Comparaison externe** : chiffres publiés par les autres moteurs, sourcés et datés (pas de build de
  moteurs tiers).

Machine de mesure : 32 threads CPU, NVIDIA RTX 5000 Ada (laptop), Intel UHD (RPL-S), llvmpipe ; toutes
trois visibles depuis le conteneur avec `./docker_run.sh --gui`. Le profilage `perf` passe par
`./docker_run.sh --perf`.

La chaîne de build est mesurée en second rang : build à froid, build incrémental après modification d'un
header central, temps de la CI par action.

## 7. Méthode

1. **Cartographie** (lecture seule) : graphe des modules et des includes, tailles, points chauds de churn
   (`git log --stat`), dépendances réelles par rapport à `conanfile.py`.
2. **Revue par axe** (§4) : un agent par axe ou groupe d'axes, en parallèle, chacun rendant des constats
   au format du §9.
3. **Vérification croisée** : chaque constat « faiblesse », « étrangeté » ou « risque » est relu par un
   second agent chargé de le réfuter. Non réfuté : confirmé ; contesté : plausible.
4. **Mesures** (§6) : harnais de bench, puis campagnes de mesure dans Docker.
5. **Comparaison externe** : recherche web sourcée et datée.
6. **Avenir** (§5) : synthèse des constats en carte des chantiers.
7. **Synthèse** : rapport final hiérarchisé, puis révision de la configuration IA.

## 8. Règles d'engagement pour les agents

- **Lecture seule** sur le code. Écritures autorisées : `doc/audit/`, `AUDIT.md`, le scratchpad de
  session, et le harnais `test/bench/` (§10.1).
- **Builds, tests et mesures uniquement via `./docker_run.sh`**, jamais en natif, jamais d'installation
  sur l'hôte. Un outil manquant se consigne au §11.3.
- **Pas de push** ; commits locaux sur la branche de l'audit uniquement à la demande.
- Distinguer clairement « constaté », « mesuré » et « opinion ».
- La mémoire et les consignes IA sont des **objets d'étude** : une consigne fausse se signale, elle ne se
  suit pas aveuglément.
- Factuel et sans complaisance : une force se justifie autant qu'une faiblesse.

## 9. Format des livrables

**Constat** (un par entrée) :

```text
ID             : B-01
Axe            : Rendu
Nature         : force | faiblesse | étrangeté | risque
Gravité        : haute | moyenne | basse
Preuve         : source/owl/private/renderer/...:123, commande, mesure
Constat        : une ou deux phrases
Comparaison    : ce que font les autres (si pertinent)
Recommandation : garder | surveiller | corriger | remplacer, effort S / M / L
Statut         : confirmé | plausible
```

**Fichiers de l'audit** (`doc/audit/`) :

| Fichier                | Contenu                                                                   |
|------------------------|---------------------------------------------------------------------------|
| `00-cadrage.md`        | Ce document : cadrage, environnement (§11), configuration IA (§12).       |
| `10-constats-<axe>.md` | Constats par axe (A à J).                                                 |
| `20-mesures.md`        | Protocoles et résultats de mesure.                                        |
| `30-etat-de-l-art.md`  | Fiches de comparaison.                                                    |
| `40-avenir.md`         | Carte des chantiers, trajectoire (axe K), correspondance avec la roadmap. |
| `90-synthese.md`       | Synthèse : 5 forces, 5 faiblesses, 5 bizarreries, plan d'action priorisé. |

## 10. Décisions complémentaires (2026-10-05)

1. **Harnais de benchmark** : accepté. Dossier `test/bench/` (harnais maison `owl_bench`, baseline dans
   `test/bench/baseline/`, plus un mode « frame bench » du runner sur des scènes fixes), derrière une option
   CMake `OWL_BENCHMARK` désactivée par défaut. Seule entorse autorisée à la règle « pas de modification
   hors `doc/audit/` ».
2. **Comparaisons externes** : chiffres publiés uniquement, sourcés et datés ; pas de build de moteurs tiers.
3. **Versionnage** : l'audit est commité sur la branche `Feature/Bench` ; `doc/audit/` reste exclu de
   Doxygen et de codespell.

## 11. Environnement

Toute commande passe par `./docker_run.sh`, qui reproduit la toolchain CLion *Docker Owl*.

### 11.1 Lanceur

| Élément     | Valeur                                                                                       |
|-------------|----------------------------------------------------------------------------------------------|
| Image       | `registry.argawaen.net/builder/devel-ubuntu2604:latest` (surchargeable : `OWL_DOCKER_IMAGE`) |
| Utilisateur | UID/GID de l'utilisateur courant                                                             |
| `$HOME`     | `/fhome` ← `fake_home` (virtualenv Poetry, cache Conan, ccache)                              |
| Montage     | dépôt monté à son chemin hôte                                                                |

`--gui` ajoute GPU NVIDIA, `/dev/dri`, affichage X11/Wayland et audio ; `--perf` ajoute ptrace, perf events et
lève seccomp. Pas de `--privileged` par défaut. Validé : `poetry sync --no-root`, configure Conan 2, build
(~1 min sur 32 cœurs, ccache chaud), `ctest -j8`, `CodeStyle`, et `vulkaninfo --gui` (RTX 5000 Ada, Intel UHD,
llvmpipe).

### 11.2 Outillage présent

Vérifié le 2026-10-20 dans le conteneur.

| Catégorie       | Outils                                                                          |
|-----------------|---------------------------------------------------------------------------------|
| Compilateurs    | clang 22.1.2, gcc 15.2, mold, ccache 4.12                                       |
| Build           | cmake 4.4.4, ninja 1.13                                                         |
| Python          | Python 3.14.4, Poetry 2.5.1 ; via Poetry : gcovr, codespell                     |
| Qualité         | clang-tidy et clang-format 22.1.2, cppcheck 2.19, llvm-cov, lcov                |
| Débogage        | gdb 17.1, lldb 22.1, valgrind 3.26                                              |
| Docs            | doxygen 1.15, graphviz (`dot`)                                                  |
| GPU / affichage | Mesa 26.0 (lavapipe), `vulkan-tools` et `vulkan-validationlayers` 1.4.341, Xvfb |

### 11.3 Ce qui manque à l'image

Absent le 2026-10-20, classé selon la priorité de l'audit (performance du moteur).

| Priorité | Ajout                                                            | Pourquoi                                                          |
|----------|------------------------------------------------------------------|-------------------------------------------------------------------|
| Haute    | `perf` exécutable                                                | Aucun binaire : profilage CPU impossible malgré `--perf`.         |
| Haute    | `heaptrack` (+ `heaptrack_print`)                                | Allocations par frame et pics mémoire, plus rapide que massif.   |
| Haute    | `renderdoccmd` (RenderDoc)                                       | Capture GPU en ligne de commande, OpenGL et Vulkan.               |
| Moyenne  | `hyperfine`                                                      | Temps de démarrage, de chargement et benchmarks CLI répétés.      |
| Moyenne  | `hotspot`                                                        | Lecture graphique des enregistrements perf (avec `--gui`).        |
| Moyenne  | `spirv-tools` (`spirv-val`, `spirv-dis`, `spirv-opt`), `glslang` | Inspecter et valider le SPIR-V produit par Slang.                 |
| Basse    | `include-what-you-use`, ClangBuildAnalyzer                       | Coût des headers et temps de build.                               |
| Basse    | `bloaty`                                                         | Taille des binaires et sections, par symbole.                     |

Tracy n'est pas un outil de l'image : c'est un backend du profileur du moteur (`OWL_PROFILER=none|tracy|chrome`),
fourni par Conan.

### 11.4 Effet de `doc/audit/` sur la CI

`doc/` est scanné par codespell (`CodeStyle`) et ingéré par Doxygen (`Documentation`, `WARN_AS_ERROR=YES`) ; des
pages en français feraient échouer les deux gates. Exclusions : `--skip=<doc>/audit` dans `ci/actions/code_style.py`,
et `@PROJECT_SOURCE_DIR@/doc/audit` dans `DoxyfileTemplate` (le `Doxyfile` racine en est généré, ignoré par git,
`.gitignore:53`). `CodeStyle` scanne `source/owl/public`, `source/owl/private`, `source/owlnest/sources`,
`source/owlnest/runner`, `test/unit` et `test/bench`.

## 12. Configuration IA

Première passe appliquée le 2026-10-05 ; réévaluation prévue en fin d'audit (axe J).

### 12.1 Problèmes relevés

| Source                        | Problèmes                                                                                                                                                   |
|-------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `CLAUDE.md` racine            | 466 lignes (~33 Ko) : commandes `cmake` nues (absent du PATH hôte), version périmée, sous-systèmes chargés même pour une tâche CI ou doc.                   |
| `.claude/rules/`              | 3 règles sans `paths:` chargées toujours ; liste de catégories de tests fausse ; changelog « sous-listes autorisées » contraire à la règle globale.         |
| `.claude/skills/`             | Natives et sur `linux-gcc-*`, contre la préférence Clang et la règle Docker.                                                                                |
| Mémoire auto                  | 39 fichiers, index de 117 lignes contenant du contenu ; doublons du dépôt, notes contradictoires, faits périmés.                                            |
| `.claude/settings.local.json` | 144 autorisations ponctuelles, chemins inexistants, worktrees supprimés ; `Bash(git:*)` autorisait `git push`.                                              |

Contradictions bloquantes : build (global « Docker uniquement » contre mémoire « natif »), commits (global « commit
local » contre mémoire « jamais de commit »), changelog (global « une ligne » contre `documentation.md` « sous-items »).
Contexte chargé à chaque session : ~55 Ko.

### 12.2 Configuration en place

| Élément                           | Contenu                                                                                                                                                                 |
|-----------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `docker_run.sh`                   | Lanceur unique, options `--gui` et `--perf`.                                                                                                                            |
| `CLAUDE.md`                       | 121 lignes : build Docker, presets Clang, actions CI, carte du dépôt, workflow, pièges.                                                                                 |
| Règles ciblées par `paths:`       | `renderer`, `scene`, `editor`, `script`, `core-task`, `data-pack`, `dependencies`, `slang-shaders`, `python-ci`, `ongoing-quality`, `documentation`, `module-layout`, `cmake`, `cpp-style` : chargées quand on touche aux fichiers concernés. |
| Skills                            | `build`, `test`, `coverage`, `add-test`, `add-dependency` en Docker et Clang ; `check` (gate avant de rendre la main).                                                  |
| `.claude/settings.json` (versionné) | Autorisations partagées (`docker_run.sh`, git en lecture, commit local) ; `ask` sur `git push`, `gh pr create`, `git reset --hard`, `git clean` ; `deny` sur `pip install`, `sudo apt`. |
| `.claude/settings.local.json`     | Courte liste générique (lecture, domaines web, MCP).                                                                                                                    |
| Mémoire                           | 22 fichiers, index de 25 lignes ; les leçons techniques vivent dans les règles du dépôt, la mémoire ne garde que profil, retours de méthode, décisions de fond, références externes. |
| CI                                | `doc/audit/` exclu de codespell et de Doxygen (§11.4).                                                                                                                  |

Sauvegarde de l'ancienne mémoire et de l'ancien `settings.local.json` :
`~/.claude/projects/-data-sources-personnel-stack-owl-Owl/memory-backup-2026-10-05/`.

Décisions prises au nom du mainteneur (à valider) :

- **Commits** : règle globale (commit local court, jamais de push ni de PR) ; la mémoire « jamais de commit » (avril
  2026) est supprimée. `git push` et `gh pr create` demandent confirmation.
- **Roadmap** : `doc/pages/roadmap.md` (publiée par Doxygen) fait foi ; `ROADMAP.md` et `CHANGELOG.md` racine changent
  à la release.
- **Docker sans `--privileged`** : `docker_run.sh` ajoute les capacités au cas par cas (`--gui`, `--perf`).

### 12.3 Points ouverts

1. **Hook `PostToolUse`** lançant `clang-format` en Docker sur chaque fichier C++ édité : évite les allers-retours avec
   `CodeStyle`, coûte ~1 s de démarrage de conteneur par édition ; à tester.
2. **MCP CLion** (`get_file_problems`, `search_symbol`) : diagnostics et navigation sémantique sans build ; à évaluer
   face à grep + clang-tidy.
3. **Règle globale** : citer `docker_run.sh` comme convention de lanceur pour tous les projets qui en ont un.
4. **Mesure** : relever les règles réellement chargées par les agents et élaguer celles qui n'ont servi à rien.

### 12.4 Retours de la première vague d'agents (2026-10-05)

Huit sous-agents ont instruit les axes A à I, chacun en contexte neuf, avec une consigne autonome renvoyant au cadrage.

| Retour                                                                              | Suite donnée                                                          |
|-------------------------------------------------------------------------------------|-----------------------------------------------------------------------|
| `docker_run.sh` ne transmettait pas stdin hors terminal ni ne montait le scratchpad | `-i` si stdin est un pipe, variable `OWL_DOCKER_MOUNTS`.              |
| « Compilation Slang ~50 s » faux en Release                                         | `CLAUDE.md` donne ~165 ms à froid et 21 à 35 ms par shader.           |
| `CodeStyle` ne scannait pas le harnais de bench                                     | `test/bench` fait partie des racines scannées.                        |
| Un agent a lu l'ancien index mémoire (liens cassés)                                 | Les liens de l'index résolvent ; l'index est un instantané de début de session. |
| Comparaisons à Hazel / Godot / Bevy tirées de la mémoire des modèles                | Sourcées par `30-etat-de-l-art.md` ; les agents marquent « non revérifié ». |
| Un script de texte lancé une fois en natif                                          | Sans conséquence ; règle Docker rappelée dans chaque consigne.        |

Coût observé : 130 k à 360 k tokens et 7 à 60 min par agent d'axe ; l'orchestrateur ne reçoit qu'un résumé de
20 lignes par agent, les fichiers de constats faisant foi.
