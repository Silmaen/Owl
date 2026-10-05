# Audit Owl — confrontation à l'état de l'art

> **Statut : v1, 2026-10-05.** Livrable du §6 de [`00-cadrage.md`](00-cadrage.md).
> **Règle** : chiffres externes **publiés uniquement**, chacun avec source et date ; aucun moteur tiers n'a
> été construit. Les métadonnées de dépôts (version, licence, dernière activité) proviennent de l'API
> GitHub interrogée le 2026-10-05 (`gh api repos/<owner>/<repo>` et `.../releases/latest`).
> **Légende** : **[F]** fait sourcé, **[C]** constaté dans le dépôt Owl (`chemin`), **[O]** opinion de l'auditeur.
> Les chiffres externes ne sont **pas** comparables aux mesures Owl de `20-mesures.md` (machines,
> scènes et protocoles différents) : ils situent des ordres de grandeur, rien de plus.

[TOC]

## Fiche 1 — ECS : EnTT

**Choix Owl** [C] : EnTT 3.15.0 (`depmanager.yml`), registre unique par `Scene`, vues à la volée ; un seul
usage de `group` dans `source/`, 89 appels `view<`/`group<` dans les `.cpp`.

**Alternatives** : flecs (C/C++, archétypes + relations), ECS de Bevy (Rust, archétypes + tables sparse),
ECS maison.

| Projet   | Dernière version    | Licence    | Modèle de stockage              | Étoiles GitHub  |
|----------|---------------------|------------|---------------------------------|-----------------|
| EnTT     | v4.0.0 (2026-07-23) | MIT        | sparse sets, groups optionnels  | 13,2 k          |
| flecs    | v4.1.6 (2026-06-29) | MIT        | archétypes, relations, requêtes | 8,7 k           |
| Bevy ECS | 0.19.1 (2026-08-13) | MIT/Apache | archétypes (tables) + sparse    | 48,6 k (moteur) |

**Chiffres publiés** [F] — `abeimler/ecs_benchmark` (README, EnTT 3.13.2, flecs 4.0.1, gcc 14.2.1, 12 cœurs,
Linux 6.10 ; relevé le 2026-10-05) :

| Scénario (~1 M entités)            | EnTT  | EnTT group | flecs  | pico_ecs | gaia-ecs |
|------------------------------------|-------|------------|--------|----------|----------|
| Création d'entités                 | 68 ms | —          | 409 ms | 48 ms    | 151 ms   |
| Mise à jour, 7 systèmes            | 40 ms | 35 ms      | 16 ms  | 19 ms    | 31 ms    |
| Ajout / retrait de composants      | 26 ms | —          | 250 ms | 8 ms     | 137 ms   |
| Mise à jour, 8 entités, 7 systèmes | —     | 199 ns     | 53 µs  | —        | —        |

Lecture [O] : flecs gagne en itération massive (archétypes contigus), EnTT gagne nettement en
création / changement de structure et sur les petits mondes. Les scènes Owl (2D, tilemap, quelques
centaines à milliers d'entités, hiérarchie et ajout/retrait fréquents en éditeur) sont dans la zone où
EnTT est favorable.

**Critères** [O] : perf adaptée au profil Owl ; maturité forte (Minecraft Bedrock, cité par le README
d'EnTT) ; mainteneur quasi unique (bus factor) ; MIT ; header-only, portable ; coût d'intégration déjà payé.

**Point d'attention** [F] : EnTT **v4.0.0** est sortie le 2026-07-23 avec des ruptures d'API (macros de
config renommées, `entt::identity`/`to_address` retirés, refonte de `meta`). Owl est sur 3.15.0.

**Verdict : GARDER**, et planifier la montée en v4 (effort S–M, surtout `meta` si utilisé). Exploiter
davantage les `group` sur les chemins chauds (rendu, transforms) avant toute idée de changement d'ECS.

Sources : <https://github.com/abeimler/ecs_benchmark> ; <https://github.com/skypjack/entt/releases/tag/v4.0.0> ;
<https://github.com/SanderMertens/flecs/releases>.

## Fiche 2 — RHI : double backend maison OpenGL 4.5 + Vulkan 1.4

**Choix Owl** [C] : abstraction maison `source/owl/private/renderer/gpu/{opengl,vulkan,null}`, Vulkan SDK
1.4.341. Limites connues consignées en mémoire IA : UBO partagé « last-write-wins » sur Vulkan, descriptor
set unique (voir constats axe B).

**Alternatives** [F] (métadonnées GitHub au 2026-10-05) :

| RHI          | Backends                                                                      | Licence                | État                                                           |
|--------------|-------------------------------------------------------------------------------|------------------------|----------------------------------------------------------------|
| bgfx         | D3D11/12, Metal, Vulkan, GL/GLES, WebGPU…                                     | BSD-2                  | actif (push 2026-10-05), pas de releases taguées               |
| Diligent     | D3D11/12, Vulkan, Metal, GL/GLES, WebGPU                                      | Apache-2.0             | actif, dernière release v2.5.6 (2024-09-02)                    |
| NVRHI        | D3D11, D3D12, Vulkan 1.2 (macOS via MoltenVK)                                 | MIT                    | actif (NVIDIA-RTX/NVRHI)                                       |
| sokol_gfx    | GL, GLES/WebGL, Metal, D3D11, WebGPU, Vulkan (expérimental depuis 2025-12-02) | zlib                   | actif                                                          |
| SDL3 GPU     | Vulkan, D3D12, Metal (SPIR-V / DXIL / MSL)                                    | zlib                   | stable depuis SDL 3.2.0 (janv. 2025), SDL 3.4.18 au 2026-10-02 |
| WebGPU natif | Dawn (Google), wgpu (Rust, v30.0.1 2026-08-22), en-tête commun `webgpu.h`     | BSD-3 / Apache-2.0-MIT | actif ; seule voie vers le Web                                 |

**État d'OpenGL en 2026** [F] : Apple a déprécié OpenGL à la WWDC 2018 (macOS 10.14) et l'a figé en 4.1 —
donc **OpenGL 4.5 n'existe pas sur macOS** ; Khronos ne fait plus évoluer OpenGL au-delà de 4.6 (2017).
[O] Sous Linux/Windows les pilotes GL restent maintenus (Mesa, NVIDIA), mais les innovations
(ray tracing, mesh shaders, bindless moderne) n'arrivent plus que côté Vulkan/D3D12/Metal.

**Critères** [O] :

| Critère     | Maison GL+Vulkan                                    | RHI tierce (ex. SDL3 GPU, NVRHI, Diligent)             |
|-------------|-----------------------------------------------------|--------------------------------------------------------|
| Perf        | dépend du code maison, défauts connus (UBO partagé) | éprouvée, mais plus petit dénominateur commun possible |
| Maturité    | faible, un seul mainteneur                          | élevée (production, plusieurs moteurs)                 |
| Maintenance | deux backends à porter pour chaque fonctionnalité   | portée par un tiers                                    |
| Licence     | —                                                   | toutes permissives                                     |
| Portabilité | Linux/Windows ; pas de macOS ni Web                 | macOS (Metal) et Web selon la RHI                      |
| Intégration | déjà faite                                          | L : réécriture du bas du renderer + shaders            |

**Verdict : SURVEILLER.** La valeur pédagogique et la maîtrise sont réelles, mais OpenGL ne mène nulle
part au-delà de Linux/Windows et double le coût de chaque fonctionnalité. Recommandation [O] : geler
OpenGL en « backend de repli » (pas de nouvelle fonctionnalité exclusive), concentrer l'effort sur Vulkan,
et réévaluer à l'arrivée du rendu 3D moderne (v0.3+) une RHI tierce ou une cible WebGPU si le Web/macOS
devient un objectif. Coût d'une migration complète : **L** (plusieurs mois homme).

Sources : <https://www.osnews.com/story/30449/apple-deprecates-opengl-opencl-in-macos-mojave/> (2018-06) ;
<https://wiki.libsdl.org/SDL3/SDL_GPUShaderFormat> ; <https://www.phoronix.com/news/SDL3-Official-Release> (2025-01) ;
<https://floooh.github.io/> (billet Vulkan sokol-gfx, 2025-12) ; <https://github.com/NVIDIA-RTX/NVRHI> ;
<https://github.com/DiligentGraphics/DiligentEngine> ; <https://github.com/webgpu-native/webgpu-headers>.

## Fiche 3 — Shaders : Slang compilé au runtime

**Choix Owl** [C] : `compileSlangToSpirv()` (`source/owl/private/renderer/utils/shaderFileUtils.cpp`),
Slang fourni par le Vulkan SDK 1.4.341 (`cmake/Vulkan.cmake:47`), cache `.spv` validé par hash,
réflexion spirv-cross. Premier lancement ~50 s (compilation du module cœur de Slang) — chiffre issu de la
mémoire IA du projet, **à confirmer par `20-mesures.md`**.

**Alternatives** : compilation hors-ligne en étape de build (glslang 16.6.0 du 2026-09-11, DXC
v1.9.2609 du 2026-09-29, ou `slangc` lui-même), `.spv` livrés avec les assets ; SDL_shadercross pour une
chaîne multi-cible.

**Faits** [F] :

- Slang est hébergé par Khronos depuis le **2024-11-21** (Slang Initiative, gouvernance multi-entreprises),
  cibles D3D12, Vulkan, Metal, D3D11, OpenGL, CUDA, CPU.
- Rythme de release élevé : v2026.19 le 2026-09-29 (versions calendaires, plusieurs par mois).
- Licence Apache-2.0 avec exception LLVM.
- Aucune mesure publiée et sourçable du temps de création de session Slang n'a été trouvée lors de cette
  passe (recherche web et issues GitHub du 2026-10-05) : pas de chiffre externe à opposer aux ~50 s.

**Critères** [O] : le langage est le bon choix (portabilité Vulkan/GL/Metal future, modules, standard
Khronos). Le problème est le *moment* de la compilation : un coût de démarrage de l'ordre de la minute est
inacceptable pour un joueur, et le runtime embarque un compilateur complet (taille binaire, surface de
crash). La pratique dominante (Unreal, Unity, Godot pour ses variantes) est de compiler hors-ligne et de
ne garder au runtime que le chargement de binaires.

**Verdict : GARDER Slang, REMPLACER la compilation au runtime dans le runner.** Compiler les `.slang` en
`.spv` au build (ou dans le « Pack Game ») et ne garder le compilateur que dans l'éditeur (hot reload).
Effort **M** (cible CMake + intégration au pack + chemin de repli).

Sources : <https://www.khronos.org/news/press/khronos-group-launches-slang-initiative-hosting-open-source-compiler-contributed-by-nvidia> (2024-11-21) ;
<https://github.com/shader-slang/slang/releases> ; <https://moonside.games/posts/introducing-sdl-shadercross/>.

## Fiche 4 — Script : Lua 5.5 avec bindings manuels

**Choix Owl** [C] : Lua 5.5.0 (`depmanager.yml`), API C directe (`source/owl/private/script/LuaBindings.cpp`,
992 lignes, pas de sol2), un `lua_State` par `ScriptInstance`, bac à sable.

**Faits** [F] :

- Lua 5.5.0 publiée le **2025-12-22** ; nouveautés : déclarations de globales, collectes majeures
  incrémentales, tableaux plus compacts (« large arrays use about 60% less memory »), variables de boucle
  `for` en lecture seule.
- LuaJIT : rolling release sans tags (version = horodatage du commit), dernier commit 2026-09-08 ; reste
  compatible **Lua 5.1** seulement.
- Luau (Roblox) : 0.741 le 2026-10-02, MIT, typage graduel, bac à sable natif ; se dit « noticeably faster
  than Lua 5.x (including Lua 5.4) » et capable d'égaler l'interpréteur LuaJIT sur certaines charges, JIT
  optionnel x64/arm64 ; **aucun benchmark tête-à-tête publié** sur la page performance.
- sol2 : dernière release v3.3.0 (2022-06-25), dernier push 2025-03-07 — support de Lua 5.5 non annoncé.
- Wren : dernière release 0.4.0 (2021-04-09). AngelScript : miroir GitHub archivé (développement sur SVN).
- C# : Coral (hôte .NET de StudioCherno, MIT) actif (push 2026-09-28), sans release taguée.

| Option             | Perf (publiée)                                      | Typage   | Maintenance              | Coût de migration depuis Lua 5.5 |
|--------------------|-----------------------------------------------------|----------|--------------------------|----------------------------------|
| Lua 5.5            | référence                                           | non      | PUC-Rio, lente mais sûre | —                                |
| LuaJIT             | JIT, très rapide (pas de chiffre récent sourcé ici) | non      | rolling, 5.1 figé        | M : perte des features 5.4/5.5   |
| Luau               | > Lua 5.4 (affirmation qualitative)                 | graduel  | Roblox, très actif       | M : API C proche, sandbox natif  |
| C# (.NET)          | JIT/AOT                                             | fort     | Microsoft + Coral        | L : nouvelle chaîne, hot reload  |
| Wren / AngelScript | —                                                   | — / fort | faible                   | L, non recommandé                |

**Verdict : GARDER Lua 5.5 ; SURVEILLER Luau.** Lua 5.5 est à jour et le binding manuel évite une
dépendance (sol2) dont la maintenance ralentit. Luau devient intéressant si l'axe « script typé +
débogueur + perf » du §5 est priorisé (effort M). Ne pas viser LuaJIT (5.1 figé) [O].

Sources : <https://www.lua.org/work/> et <https://www.lua.org/manual/5.5/readme.html> ;
<https://luajit.org/status.html> ; <https://luau.org/performance> ; <https://github.com/ThePhD/sol2> ;
<https://github.com/StudioCherno/Coral>.

## Fiche 5 — Sérialisation : yaml-cpp

**Choix Owl** [C] : yaml-cpp **0.8.0** pour scènes, prefabs, sauvegardes, réglages **et** snapshots undo
(`EntitySnapshot` via `serializeEntityToString`) ; 56 fichiers de `source/owl/private` utilisent `YAML::`.

**Faits** [F] :

- yaml-cpp 0.9.0 est sortie le 2026-02-04 ; Owl est en retard d'une version.
- rapidyaml (doc « Is it rapid? ») : « generally 30x (parse) / 150x (emit) faster than yaml-cpp » ;
  ~150 Mo/s en YAML complet ; sur `appveyor.yml` en Release (VS2017) 101,5 Mo/s contre 5,3 Mo/s ; en JSON
  sous Linux (clang, Release) 453,5 Mo/s contre 15,1 Mo/s. rapidyaml v0.16.0 (2026-07-22), MIT.
- JSON : simdjson v5.0.2 (2026-10-04, Apache-2.0, lecture seule) ; glaze v9.0.0 (2026-09-24, MIT,
  réflexion à la compilation, JSON + BEVE binaire).
- Binaire : cereal v1.3.2 (2022-02-28, peu actif), FlatBuffers (2026-02, Apache-2.0, zéro-copie, schéma),
  bitsery (MIT, dernier push 2025-10).

**Critères** [O] : YAML est le bon format **source** (lisible, diffable, éditable à la main). Il est un
mauvais format **interne** : l'undo par aller-retour YAML, la duplication et l'instanciation de prefab
paient parsing + allocations de `YAML::Node` à chaque opération. yaml-cpp est connu comme l'un des
parseurs YAML C++ les plus lents.

**Verdict : SURVEILLER (format) / REMPLACER (bibliothèque et usage interne).**
1. Court terme : mesurer (`20-mesures.md`) le coût d'un snapshot undo et d'une instanciation de prefab.
2. Moyen terme : migrer vers rapidyaml (même format de fichier, API différente : effort **M**, 56 fichiers).
3. Long terme : snapshot undo et copie de scène en binaire (glaze/BEVE ou bitsery), YAML réservé aux
   fichiers ; format « cooké » binaire dans `.owlpack`. Effort **M–L**.

Sources : <https://rapidyaml.readthedocs.io/latest/sphinx_is_it_rapid.html> ;
<https://github.com/jbeder/yaml-cpp/releases> ; <https://github.com/simdjson/simdjson> ;
<https://github.com/stephenberry/glaze> ; <https://github.com/google/flatbuffers>.

## Fiche 6 — Physique : Box2D v3 (et la 3D future)

**Choix Owl** [C] : Box2D **3.1.1** (`depmanager.yml`) — c'est la dernière release (2025-06-04) [F] ; Owl
est à jour.

**Chiffres publiés** [F] — Erin Catto, « SIMD Matters » (2024-08-19), scène *large pyramid* (5 050 corps,
14 950 paires de contact), 4 workers, AMD 7950X / Apple M2 :

| Jeu d'instructions | fps  | ms / pas |
|--------------------|------|----------|
| AVX2 (7950X)       | 1117 | 0,90     |
| SSE2 (7950X)       | 982  | 1,02     |
| Neon (M2)          | 1058 | 0,95     |
| Scalaire (7950X)   | 524  | 1,91     |
| Scalaire (M2)      | 679  | 1,47     |

Conséquence [O] : vérifier que le paquet DepManager de Box2D est compilé avec AVX2 (`BOX2D_AVX2`) sur
x64 ; sinon on laisse ~×1,1 à ×2 sur la table.

**3D future** [F] :

| Moteur | Version             | Licence | Références publiques                                              |
|--------|---------------------|---------|-------------------------------------------------------------------|
| Jolt   | v5.6.0 (2026-07-11) | MIT     | Horizon Forbidden West, Death Stranding 2 ; Godot 4.4+ en option  |
| Box3D  | v0.1.0 (2026-06-30) | MIT     | annoncé 2026-07 par Erin Catto ; s&box ; C17, API proche de Box2D |

[O] Box3D est séduisant pour Owl (même auteur, même API C, même modèle mental que Box2D v3), mais il est
en 0.1. Jolt est le choix sûr et documenté (`Docs/PerformanceTest.md` du dépôt Jolt publie des courbes de
passage à l'échelle multicœur).

**Verdict : GARDER Box2D v3 ; pour la 3D, SURVEILLER Box3D et partir sur Jolt si la 3D arrive avant
qu'il n'atteigne une 1.0.** Coût d'intégration 3D : **M** (nouveau module, synchronisation transforms).

Sources : <https://box2d.org/posts/2024/08/simd-matters/> ; <https://github.com/erincatto/box2d/releases> ;
<https://github.com/erincatto/box3d> ; <https://gamefromscratch.com/box2d-enters-the-3rd-dimension-box3d-released/> (2026-07-06) ;
<https://github.com/jrouwe/JoltPhysics/blob/master/Docs/PerformanceTest.md>.

## Fiche 7 — Tâches : Taskflow

**Choix Owl** [C] : Taskflow 4.0.0, privé derrière `Scheduler` (pimpl), pont `std::promise`/`std::future`,
`parallelForEach` interne. Taskflow 4.1.0 est sortie le 2026-06-20 [F].

**Alternatives** :

| Option                   | Licence | Dernière version                   | Profil                                                                       |
|--------------------------|---------|------------------------------------|------------------------------------------------------------------------------|
| Taskflow                 | MIT     | v4.1.0 (2026-06-20)                | graphes de tâches, work stealing, header-only                                |
| enkiTS                   | zlib    | v1.12 (2026-07-04)                 | ordonnanceur de jeu, zéro allocation à l'ordonnancement, priorités           |
| Fibres (Naughty Dog)     | —       | GDC 2015 (Gyrling)                 | job system à fibres, tout le moteur en jobs (The Last of Us Remastered, PS4) |
| `std::execution` (P2300) | std     | C++26 (adopté 2024-06, vote serré) | senders/receivers ; implémentation de référence stdexec                      |

**Chiffres publiés** [F] : article Taskflow (Huang et al., arXiv:2004.10908) — coût de création d'une
tâche 61 ns contre 99 ns (oneTBB) et 259 ns (StarPU). Aucun benchmark publié comparant directement
Taskflow et enkiTS n'a été trouvé.

**Critères** [O] : Taskflow est solide et bien maintenu ; son point faible pour un moteur est l'absence de
priorités et d'affinité de thread (thread de rendu, I/O), que enkiTS fournit. Le pont `std::future` ajoute
une allocation partagée par tâche. `std::execution` n'est pas encore livré par libstdc++/libc++ de façon
complète : prématuré.

**Verdict : GARDER**, monter en 4.1 (effort S). Réévaluer (enkiTS ou fibres) seulement quand le chantier
« job system au niveau frame / rendu sur thread séparé » du §5 démarre ; la façade `Scheduler` rend le
changement local (effort **M**).

Sources : <https://arxiv.org/abs/2004.10908> ; <https://github.com/taskflow/taskflow/releases> ;
<https://github.com/dougbinks/enkiTS> ; <https://www.gdcvault.com/play/1022186/Parallelizing-the-Naughty-Dog-Engine> (GDC 2015) ;
<https://www.think-cell.com/en/career/devblog/trip-report-summer-iso-cpp-meeting-in-st-louis-usa> (2024-06).

## Fiche 8 — Dépendances : DepManager maison

**Choix Owl** [C] : DepManager (`Silmaen/DepManager`, MIT, 0.5.5 du 2026-04-19, 0 étoile) — 34 paquets
précompilés servis par un serveur du mainteneur, recettes dans le projet OwlDependencies.

| Outil      | Version (2026-10-05) | Licence | Modèle                                              | Étoiles |
|------------|----------------------|---------|-----------------------------------------------------|---------|
| DepManager | 0.5.5 (2026-04-19)   | MIT     | binaires précompilés, serveur privé                 | 0       |
| vcpkg      | 2026.07.29           | MIT     | manifest `vcpkg.json`, build source + cache binaire | 27,5 k  |
| Conan 2    | 2.33.0 (2026-09-29)  | MIT     | recettes Python, binaires, ConanCenter              | 9,5 k   |
| CPM.cmake  | v0.43.2 (2026-09-24) | MIT     | `FetchContent` amélioré, sources                    | 4,1 k   |

**Critères** [O] :
- Perf : binaires précompilés = configure rapide (comparable à Conan avec cache, meilleur que vcpkg sans
  cache binaire).
- Maturité / bus factor : **le point faible majeur** — un seul utilisateur connu, un seul mainteneur, un
  serveur de paquets privé. Un contributeur externe ne peut pas construire Owl sans ce serveur.
- Portabilité : couvre Linux x64/arm64 et MinGW, ce que vcpkg/Conan font aussi (MinGW est moins bien
  servi par ConanCenter et vcpkg, triplets communautaires).
- Intégration : recettes au format proche de Conan (`owl_engine.py`), donc migration vers Conan 2
  plausible.

**Verdict : SURVEILLER.** Pour un projet solo qui contrôle aussi sa chaîne, DepManager est rationnel et
déjà payé. Mais pour un moteur open source, il bloque toute contribution extérieure. Mesure minimale [O] :
documenter un chemin de repli public (serveur de paquets accessible en lecture, ou manifest vcpkg/Conan
parallèle). Migration complète vers Conan 2 : **M–L** (34 recettes, dont Vulkan SDK et Slang).

Sources : <https://github.com/Silmaen/DepManager> ; <https://github.com/microsoft/vcpkg> ;
<https://github.com/conan-io/conan> ; <https://github.com/cpm-cmake/CPM.cmake>.

## Fiche 9 — CI : TeamCity + wrapper Python

**Choix Owl** [C] : TeamCity en Kotlin DSL (`.teamcity/`), wrapper `ci_action.py`, plugin GitHub maison ;
aucun `.github/workflows`. Le dépôt GitHub `Silmaen/Owl` est public (MIT).

**Faits** [F] :
- TeamCity Professional est gratuit (3 agents, 100 configurations) ; une licence Enterprise gratuite
  renouvelable annuellement existe pour les projets open source non commerciaux.
- GitHub Actions : runners standards gratuits et illimités pour les dépôts publics ; baisse des prix des
  runners hébergés au 2026-01-01 ; la taxe de 0,002 $/min sur les runners auto-hébergés annoncée en
  2025-12 a été reportée sine die.

| Critère                  | TeamCity auto-hébergé                      | GitHub Actions                                   |
|--------------------------|--------------------------------------------|--------------------------------------------------|
| Coût                     | gratuit, mais serveur + agents à maintenir | gratuit (dépôt public)                           |
| Visibilité contributeurs | faible (serveur privé, statut via plugin)  | native dans la PR                                |
| GPU / matériel réel      | oui (agents du mainteneur)                 | non sur runners standards (self-hosted possible) |
| Puissance                | DSL Kotlin, chaînes de builds, cache local | YAML, matrice, cache distant limité              |
| Bus factor               | serveur et plugin maison                   | aucun service à maintenir                        |

**Verdict : SURVEILLER.** [O] TeamCity se justifie par l'accès à du matériel réel (GPU, sanitizers lourds)
et par l'investissement existant. Le wrapper Python est un bon choix (portable vers n'importe quel CI).
Option à faible coût : un workflow GitHub Actions minimal (build + tests Null backend + CodeStyle) pour
la visibilité publique, TeamCity gardant les jobs lourds. Effort **S**.

Sources : <https://www.jetbrains.com/help/teamcity/licensing-policy.html> ;
<https://www.jetbrains.com/legal/docs/teamcity/license/> ;
<https://github.blog/changelog/2025-12-16-coming-soon-simpler-pricing-and-a-better-experience-for-github-actions/> ;
<https://resources.github.com/actions/2026-pricing-changes-for-github-actions/>.

## Fiche 10 — Audio : OpenAL Soft

**Choix Owl** [C] : OpenAL Soft 1.24.3 en **partagé** (`depmanager.yml`), libsndfile pour le décodage.

| Option      | Licence                                                               | Version (2026-10-05) | Remarques                                               |
|-------------|-----------------------------------------------------------------------|----------------------|---------------------------------------------------------|
| OpenAL Soft | **LGPL-2.0**                                                          | 1.25.2 (2026-05-12)  | spatialisation 3D, HRTF, EFX ; relink exigé (dynamique) |
| miniaudio   | domaine public ou MIT-0                                               | 0.11.25 (2026-03-03) | un seul fichier, décodeurs intégrés, graphe de nœuds    |
| SoLoud      | zlib                                                                  | release 2020-02-07   | dernier push 2024-08 : peu maintenu                     |
| FMOD        | propriétaire, gratuit < 200 k$ de revenu annuel et < 500 k$ de budget | —                    | outil auteur, standard industriel                       |
| Wwise       | propriétaire, gratuit < 250 k$ de budget                              | —                    | outil auteur, pas de source en Indie                    |

**Critères** [O] : OpenAL Soft est mature et à jour d'une version près, mais la LGPL contraint la
distribution (l'utilisateur doit pouvoir remplacer la bibliothèque : lien dynamique en pratique) et elle n'apporte ni décodage ni mixage par bus.
miniaudio couvre lecture, décodage (remplaçant possible de libsndfile), bus, spatialisation simple, sans
contrainte de licence. FMOD/Wwise sont hors sujet pour un moteur open source (non redistribuables).

**Verdict : SURVEILLER**, avec miniaudio comme remplaçant naturel quand le chantier « audio : mixage, bus,
streaming » du §5 démarre (supprime aussi libsndfile). Effort **S–M** (backend `sound` isolé + Null).

Sources : <https://github.com/kcat/openal-soft> (fichier COPYING) ; <https://github.com/mackron/miniaudio> ;
<https://github.com/jarikomppa/soloud> ; <https://www.gamedeveloper.com/audio/small-developers-and-creators-can-now-use-fmod-studio-for-free> ;
<https://www.audiokinetic.com/pricing/for-games>.

## Fiche 11 — Profilage : macros maison

**Choix Owl** [C] : `OWL_PROFILE_SCOPE/FUNCTION` (`source/owl/public/debug/Profiler.h`), écriture d'un JSON
« Chrome tracing » (`source/owl/private/debug/Profiler.cpp:69`) — même conception que Hazel ; profiler
in-editor ; `TrackerAPI` pour la mémoire.

| Outil        | Licence                                 | État (2026-10-05)                | Points forts                                                               |
|--------------|-----------------------------------------|----------------------------------|----------------------------------------------------------------------------|
| Tracy        | BSD-3                                   | v0.14.1 (2026-08-22), très actif | temps réel, ~15 ns par zone (manuel), GPU Vulkan/GL, mémoire, verrous, Lua |
| Optick       | MIT                                     | release 2022-05, push 2024-05    | quasi abandonné                                                            |
| Superluminal | propriétaire, 59 € perso / 289 €/an pro | actif                            | échantillonnage sans instrumentation                                       |
| Maison       | —                                       | —                                | zéro dépendance, mais JSON écrit à chaud, pas de GPU, pas de temps réel    |

**Verdict : REMPLACER par Tracy** (en gardant les macros `OWL_PROFILE_*` comme façade, qui deviennent des
`ZoneScoped`). Gain : profilage GPU Vulkan/GL, allocations, contention, zones Lua, sans écrire d'outil.
Effort **S** (paquet DepManager + macros + option CMake), compatible avec la règle « mesurer avant
d'optimiser » du projet.

Sources : <https://github.com/wolfpld/tracy> (manuel `tracy.pdf`, section « Features / overhead ») ;
<https://github.com/bombomby/optick> ; <https://superluminal.eu/pricing/>.

## Fiche 12 — Moteurs de référence

[F] sauf mention ; métadonnées GitHub au 2026-10-05.

| Moteur        | Langage | Licence          | Dernière version                        | Taille d'équipe publiée                          | Architecture notable                              |
|---------------|---------|------------------|-----------------------------------------|--------------------------------------------------|---------------------------------------------------|
| Owl           | C++23   | MIT              | 0.2.1                                   | 1 auteur (245 commits, `git log`)                | EnTT, GL+Vulkan maison, Slang, Lua 5.5            |
| Hazel         | C++     | Apache-2.0       | public figé (dernier commit 2023-10-27) | Cherno + équipe Patreon (version privée)         | EnTT, Vulkan, C# ; ADN visible dans Owl           |
| Godot 4       | C++     | MIT              | 4.7.2 (2026-08-18)                      | « well over 300 contributors », > 1 600 PR (4.7) | nœuds/scène, RD (Vulkan/D3D12/Metal), GDScript/C# |
| Bevy          | Rust    | MIT/Apache-2.0   | 0.19.1 (2026-08-13)                     | 261 contributeurs, 1 185 PR (0.19, 2026-06-19)   | ECS archétypes, wgpu, pas d'éditeur complet       |
| raylib        | C       | zlib             | 6.0 (2026-04-23)                        | 1 mainteneur principal + communauté              | bibliothèque, pas d'éditeur, GL                   |
| Wicked Engine | C++     | MIT              | v0.72.120 (2026-10-02)                  | 1 auteur principal                               | ECS maison, D3D12/Vulkan, rendu GPU-driven        |
| O3DE          | C++     | Apache-2.0 / MIT | 2605.0 (2026-05-27)                     | fondation Linux, multi-entreprises               | Atom (render graph), Gems modulaires              |
| Defold        | C/Lua   | Defold License   | 1.13.2 (2026-09-29)                     | Defold Foundation, équipe salariée               | Lua (LuaJIT), focalisé 2D/mobile/Web              |

**Ce que les autres font mieux** [O] :
- Plateformes : Godot, Bevy, Defold, raylib ciblent le Web ; Owl ne cible ni Web ni macOS.
- Rendu : Wicked Engine (solo aussi) montre qu'un auteur seul peut porter un rendu GPU-driven moderne en
  abandonnant OpenGL et en n'ayant qu'une abstraction mince sur D3D12/Vulkan.
- Assets : Godot/O3DE importent et « cookent » hors-ligne, références par UID/GUID ; Owl référence par
  chemin et compile les shaders au runtime.
- Outils : les moteurs établis s'appuient sur des profileurs externes temps réel (Tracy, Superluminal) ; Owl a un profiler JSON maison.

**Ce qu'Owl fait différemment (et défendable)** [O] :
- Éditeur avec undo/redo, prefabs, voxel, raycast et tilemap dans un seul moteur solo — rare hors Godot.
- Discipline qualité (sanitizers, clang-tidy sur diff, CodeStyle, Doxygen obligatoire) supérieure à
  Hazel public, raylib ou Wicked Engine.
- Slang comme langage unique : plus moderne que GLSL (Godot) ou WGSL (Bevy).

Sources : <https://godotengine.org/releases/4.7/> ; <https://bevy.org/news/bevy-0-19/> (2026-06-19) ;
<https://github.com/TheCherno/Hazel> ; <https://github.com/turanszkij/WickedEngine> ;
<https://github.com/o3de/o3de> ; <https://github.com/defold/defold> ; <https://github.com/raysan5/raylib>.

## Synthèse

| Fiche              | Verdict                                                   | Coût estimé si action     |
|--------------------|-----------------------------------------------------------|---------------------------|
| 1 ECS EnTT         | Garder (montée v4 à planifier)                            | S–M                       |
| 2 RHI GL+Vulkan    | Surveiller (geler OpenGL, Vulkan prioritaire)             | L pour une RHI tierce     |
| 3 Shaders Slang    | Garder Slang, remplacer la compilation runtime            | M                         |
| 4 Script Lua 5.5   | Garder ; surveiller Luau                                  | M si Luau                 |
| 5 yaml-cpp         | Remplacer la lib (rapidyaml) et l'usage interne (binaire) | M puis M–L                |
| 6 Box2D v3         | Garder ; Jolt/Box3D pour la 3D                            | M (3D)                    |
| 7 Taskflow         | Garder (monter en 4.1)                                    | S ; M si job system frame |
| 8 DepManager       | Surveiller (bus factor, contribution externe)             | M–L vers Conan 2          |
| 9 TeamCity         | Surveiller ; ajouter un GitHub Actions minimal            | S                         |
| 10 OpenAL Soft     | Surveiller ; miniaudio au chantier audio                  | S–M                       |
| 11 Profiler maison | Remplacer par Tracy (façade conservée)                    | S                         |

**À garder sans hésiter** : EnTT, Box2D v3, Slang (le langage).

**À surveiller** : le double backend OpenGL + Vulkan, DepManager, Lua (face à Luau) — ainsi qu'OpenAL Soft
et TeamCity à moindre enjeu.

**À remplacer** :
- Profiler maison → Tracy : **S**, gain immédiat pour toutes les campagnes de mesure de l'audit.
- Compilation Slang au runtime (runner) → hors-ligne au build/pack : **M**, supprime un démarrage de l'ordre
  de la minute.
- yaml-cpp → rapidyaml pour les fichiers, binaire pour undo/copie de scène : **M** puis **M–L**, à
  déclencher après mesure du coût des snapshots (`20-mesures.md`).
