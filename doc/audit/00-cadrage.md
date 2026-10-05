# Audit du dépôt Owl — cadrage

> **Statut : v3, cadrage validé le 2026-10-05.** Décisions complémentaires au §10.
> **Lecteurs visés** : agents IA (Claude Code et sous-agents) qui exécutent l'audit, et le mainteneur.
> Chaque section est écrite pour être découpée en tâches indépendantes.
> Fichiers liés : [`01-environnement.md`](01-environnement.md) (Docker, outillage, manques),
> [`02-config-ia.md`](02-config-ia.md) (configuration IA : état initial, changements faits, suite).

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
4. **Une configuration IA plus efficace** : déjà optimisée en première passe (voir `02-config-ia.md`),
   à réévaluer à la fin de l'audit à la lumière de ce que les agents auront réellement utilisé.

## 2. Décisions de cadrage (2026-10-05)

| Sujet       | Décision                                                                            |
|-------------|-------------------------------------------------------------------------------------|
| Build       | Docker uniquement, via `docker/run.sh` (miroir de la toolchain CLion *Docker Owl*). |
| Mesures     | Performance du **moteur** en priorité ; la chaîne de build est secondaire.          |
| Couverture  | Tous les axes, aussi profond que possible.                                          |
| Langue      | Français pour les livrables de l'audit.                                             |
| Emplacement | `doc/audit/`, avec un point d'entrée `AUDIT.md` à la racine.                        |
| Horizon     | Large : avenir du moteur, y compris ce qui ne peut pas être fait tout de suite.     |
| Config IA   | Optimisée tout de suite (fait), réévaluée en fin d'audit.                           |

## 3. Ce que ce n'est pas

- Pas une session de correction : l'audit **ne modifie pas** `source/`, `test/`, `ci/`, `cmake/`. Les
  correctifs viendront ensuite, dans des PR séparées, une fois le bilan validé. Seule exception : le
  harnais de benchmark `bench/` (§10.1).
- Pas une liste de lint : un problème de style n'entre dans le bilan que s'il révèle un problème de fond.
- Pas un avis non sourcé : toute affirmation renvoie à `chemin:ligne`, une commande reproductible, une
  mesure avec son protocole, ou une référence externe datée.

## 4. Périmètre et axes

Taille : ~114 kLOC C++ (`source/` + `test/`), ~490 fichiers moteur, ~85 fichiers éditeur (dont
`EditorLayer.cpp` à ~2 900 lignes), 16 catégories de tests, ~36 dépendances DepManager, CI Python +
TeamCity Kotlin, 243 commits. Build complet en Docker : ~60 s sur 32 cœurs (avec ccache chaud).

| Axe | Contenu                                                           | Questions centrales                                                                              |
|-----|-------------------------------------------------------------------|--------------------------------------------------------------------------------------------------|
| A   | Architecture moteur (`source/owl/`)                               | Découpage en modules, dépendances cycliques, frontière public/privé, couplage par singletons.    |
| B   | Rendu (OpenGL 4.5, Vulkan 1.4, Null, Slang)                       | Abstraction GPU saine ou plus petit dénominateur commun ? Coût par draw, batching, UBO partagés. |
| C   | ECS, scène, hiérarchie, prefab, sauvegarde                        | EnTT bien exploité ? Coût des transforms monde, copies par aller-retour YAML.                    |
| D   | Sous-systèmes : physique, son, script Lua, tâches, voxel, raycast | Choix pertinents ? Intégration propre ou couplage caché ? Coût par frame.                        |
| E   | Éditeur Owl Nest                                                  | Dette d'`EditorLayer`, undo/redo par snapshot YAML : exactitude et coût.                         |
| F   | Qualité : tests, couverture, sanitizers, clang-tidy, CodeStyle    | Que testent vraiment les tests ? Trous sur les chemins à risque (GPU, Vulkan, éditeur).          |
| G   | Build, dépendances, packaging                                     | DepManager maison face à vcpkg / Conan 2 / CPM : coût, risque, bus factor.                       |
| H   | CI (`ci/`, `.teamcity/`)                                          | Robustesse, redondances, signal utile par minute de CI.                                          |
| I   | Documentation (`doc/pages`, Doxygen, README, CHANGELOG, roadmap)  | À jour par rapport au code ? Exigences Doxygen proportionnées ?                                  |
| J   | Configuration IA                                                  | Ce que les agents ont réellement utilisé, ce qui manquait, ce qui était faux.                    |
| K   | Avenir du moteur (§5)                                             | Où va Owl, dans quel ordre, avec quelles briques préalables ?                                    |

## 5. Axe K : l'avenir du moteur

Le but n'est pas une roadmap de plus, mais une **carte des chantiers** : pour chacun, l'intérêt, la
difficulté, les briques préalables manquantes, le risque et un ordre de grandeur d'effort. La sortie
attendue est un graphe de dépendances (mermaid) et une trajectoire proposée en horizons (court : v0.2.x,
moyen : v0.3–v0.5, long : au-delà). La roadmap existante (`doc/pages/roadmap.md`, jusqu'à v0.5.0) est
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
- **Dépendances** : DepManager face à vcpkg, Conan 2, CPM.cmake.
- **CI** : TeamCity + wrapper Python + plugin GitHub maison face à GitHub Actions.
- **Moteurs de référence** : Hazel (ADN commun probable), Godot, Bevy, O3DE, Wicked Engine, raylib, Defold.

### Mesures de performance du moteur (priorité)

Pas d'optimisation à l'aveugle, pas de chiffre inventé. Chaque chiffre porte son protocole : machine,
GPU, backend, preset, scène, nombre de répétitions, dispersion.

- **Frame** : temps CPU et GPU par frame sur les scènes de `sample_project/` (2D, tilemap, raycast,
  voxel) et sur des scènes synthétiques à charge croissante ; mêmes scènes en OpenGL et en Vulkan.
- **Rendu 2D** : quads / s, draw calls et changements d'état par frame, coût de `Renderer2D::flush`.
- **Voxel** : temps de génération et de maillage par chunk, streaming, mémoire par chunk.
- **Scène** : `getWorldTransform` sur hiérarchies profondes et larges, itération des vues EnTT, copie de
  scène (Play), coût d'un snapshot undo et d'une instanciation de prefab selon la taille de la scène.
- **Script** : coût d'un `on_update` Lua par entité, aller-retour C++ ↔ Lua.
- **Physique** : pas Box2D selon le nombre de corps, synchronisation avec les transforms.
- **Démarrage et chargement** : premier lancement (compilation Slang : ~50 s annoncées, 74 ms mesurées en Release, voir `20-mesures.md`), cache SPIR-V, chargement de
  scène, ouverture de `.owlpack`.
- **Mémoire** : pic et allocations par frame (`TrackerAPI`, heaptrack si ajouté à l'image).
- **Comparaison externe** : chiffres publiés par les autres moteurs, sourcés et datés (pas de build de
  moteurs tiers).

Machine de mesure : 32 threads CPU, NVIDIA RTX 5000 Ada (laptop), Intel UHD (RPL-S), llvmpipe ; toutes
trois visibles depuis le conteneur avec `docker/run.sh --gui`. Le profilage `perf` exige d'abord un ajout
à l'image (voir `01-environnement.md`).

La chaîne de build est mesurée en second rang : build à froid, build incrémental après modification d'un
header central, temps de la CI par action.

## 7. Méthode

1. **Cartographie** (lecture seule) : graphe des modules et des includes, tailles, points chauds de churn
   (`git log --stat`), dépendances réelles par rapport à `depmanager.yml`.
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
  session, et le harnais `bench/` (§10.1).
- **Builds, tests et mesures uniquement via `docker/run.sh`**, jamais en natif, jamais d'installation
  sur l'hôte. Un outil manquant se consigne dans `01-environnement.md`.
- **Pas de push** ; commits locaux sur la branche de l'audit uniquement à la demande.
- Distinguer clairement « constaté », « mesuré » et « opinion ».
- La mémoire et les consignes IA sont des **objets d'étude** : une consigne fausse se signale, elle ne se
  suit pas aveuglément.
- Factuel et sans complaisance : une force se justifie autant qu'une faiblesse.

## 9. Format des livrables

**Constat** (un par entrée) :

```text
ID             : B-03
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
| `00-cadrage.md`        | Ce document.                                                              |
| `01-environnement.md`  | Environnement Docker, outillage disponible et manquant.                   |
| `02-config-ia.md`      | Configuration IA : état initial, changements faits, points ouverts.       |
| `10-constats-<axe>.md` | Constats par axe (A à J).                                                 |
| `20-mesures.md`        | Protocoles et résultats de mesure.                                        |
| `30-etat-de-l-art.md`  | Fiches de comparaison.                                                    |
| `40-avenir.md`         | Carte des chantiers et trajectoire (axe K).                               |
| `90-synthese.md`       | Synthèse : 5 forces, 5 faiblesses, 5 bizarreries, plan d'action priorisé. |

## 10. Décisions complémentaires (2026-10-05)

1. **Harnais de benchmark** : accepté. Dossier `bench/` à la racine (Google Benchmark via DepManager pour
   les micro-benchs, plus un mode « frame bench » du runner sur des scènes fixes), derrière une option
   CMake `OWL_BENCHMARK` désactivée par défaut. Seule entorse autorisée à la règle « pas de modification
   hors `doc/audit/` ».
2. **Comparaisons externes** : chiffres publiés uniquement, sourcés et datés ; pas de build de moteurs tiers.
3. **Versionnage** : l'audit est commité sur la branche `Feature/Bench` ; `doc/audit/` reste exclu de
   Doxygen et de codespell.
