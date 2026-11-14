# Audit Owl — confrontation à l'état de l'art

> **Revu le 2026-11-12 sur `Feature/sample-games-new` b4281d72.** Livrable du §6 de [`00-cadrage.md`](00-cadrage.md).
> **Règle** : chiffres externes **publiés uniquement**, avec source et date ; aucun moteur tiers construit.
> Métadonnées de dépôts (version, licence, activité) : API GitHub du 2026-10-05 (`gh api repos/<owner>/<repo>`).
> **Légende** : **[F]** fait sourcé, **[C]** constaté dans le dépôt Owl (`chemin`), **[O]** opinion de l'auditeur.
> Les chiffres externes ne se comparent pas aux mesures de [`20-mesures.md`](20-mesures.md) (machines, scènes et
> protocoles différents) : ils situent des ordres de grandeur.

[TOC]

## Fiche 1 — ECS : EnTT

**Choix Owl** [C] : EnTT 4.0.0 (`conanfile.py`), registre unique par `Scene`, vues à la volée ; un seul
usage de `group` dans `source/`, 89 appels `view<`/`group<` dans les `.cpp`.

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

**Verdict : GARDER.** Exploiter
davantage les `group` sur les chemins chauds (rendu, transforms) avant toute idée de changement d'ECS.

Sources : <https://github.com/abeimler/ecs_benchmark> ; <https://github.com/skypjack/entt/releases/tag/v4.0.0> ;
<https://github.com/SanderMertens/flecs/releases>.

## Fiche 2 — RHI : double backend maison OpenGL 4.5 + Vulkan 1.4

**Choix Owl** [C] : abstraction maison `source/owl/private/renderer/gpu/{opengl,vulkan,null}`, `vulkan-headers` / `vulkan-loader` 1.4.357.0 (Conan, instance créée en API 1.3).
OpenGL est gelé, Vulkan est le backend de référence.

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

**Verdict : SURVEILLER.** Valeur pédagogique et maîtrise réelles, mais OpenGL ne mène nulle part au-delà de
Linux/Windows et double le coût de chaque fonctionnalité. [O] Garder OpenGL en « backend de repli » (aucune
fonctionnalité exclusive), concentrer l'effort sur Vulkan, réévaluer une RHI tierce ou une cible WebGPU au rendu 3D
moderne (v0.5.0) si le Web ou macOS devient un objectif. Migration complète : **L**.

Sources : <https://www.osnews.com/story/30449/apple-deprecates-opengl-opencl-in-macos-mojave/> (2018-06) ;
<https://wiki.libsdl.org/SDL3/SDL_GPUShaderFormat> ; <https://www.phoronix.com/news/SDL3-Official-Release> (2025-01) ;
<https://floooh.github.io/> (billet Vulkan sokol-gfx, 2025-12) ; <https://github.com/NVIDIA-RTX/NVRHI> ;
<https://github.com/DiligentGraphics/DiligentEngine> ; <https://github.com/webgpu-native/webgpu-headers>.

## Fiche 3 — Shaders : Slang compilé au runtime

**Choix Owl** [C] : `compileSlangToSpirv()` (`source/owl/private/renderer/utils/shaderFileUtils.cpp`),
Slang 2026.19 fourni par Conan (`cmake/Vulkan.cmake`, recette `conan/recipes/slang`), cache `.spv` validé par
hash, réflexion spirv-cross. Les shaders du moteur sont précompilés au build par `OwlShaderBake` ; Slang ne
compile à l'exécution que les shaders modifiés ou de jeu. Première compilation mesurée : ~165 ms
(`20-mesures.md`, §3.9).

**Alternatives** : compilation hors-ligne seule (glslang 16.6.0 du 2026-09-11, DXC v1.9.2609 du 2026-09-29 ou `slangc`),
`.spv` livrés avec les assets ; SDL_shadercross pour une chaîne multi-cible.

**Faits** [F] :

- Slang est hébergé par Khronos depuis le **2024-11-21** (Slang Initiative, gouvernance multi-entreprises),
  cibles D3D12, Vulkan, Metal, D3D11, OpenGL, CUDA, CPU.
- Rythme de release élevé : v2026.19 le 2026-09-29 (versions calendaires, plusieurs par mois).
- Licence Apache-2.0 avec exception LLVM.
- Aucune mesure publiée et sourçable du temps de création de session Slang n'a été trouvée (recherche web et
  issues GitHub du 2026-10-05) : pas de chiffre externe à opposer aux mesures Owl.

**Critères** [O] : le langage est le bon choix (portabilité Vulkan/GL/Metal, modules, standard Khronos). La pratique
dominante (Unreal, Unity, Godot pour ses variantes) est de compiler hors-ligne et de ne charger que des binaires au
runtime ; Owl fait de même pour ses shaders et garde le compilateur pour l'éditeur (hot reload) et les shaders de jeu.

**Verdict : GARDER.** Slang comme langage, précompilation au build, compilation au besoin.

Sources : <https://www.khronos.org/news/press/khronos-group-launches-slang-initiative-hosting-open-source-compiler-contributed-by-nvidia> (2024-11-21) ;
<https://github.com/shader-slang/slang/releases> ; <https://moonside.games/posts/introducing-sdl-shadercross/>.

## Fiche 4 — Script : Luau avec bindings manuels

**Choix Owl** [C] : Luau 0.746 (`conanfile.py`, recette locale `conan/recipes/luau`), API C directe
(`source/owl/private/script/LuaBindings.cpp`, 5 615 lignes, pas de sol2), un `lua_State` par `ScriptInstance`
(`ScriptInstance.cpp:46-48`), bac à sable et quotas ; types vérifiés dans l'éditeur (`ScriptTypeChecker.cpp`) à partir
de déclarations générées du registre de liaisons. Les visual scripts (`.owlvs`) sont compilés en Luau.

**Faits** [F] (relevés le 2026-10-05, non revérifiés) :

- Luau (Roblox) : MIT, typage graduel, bac à sable natif ; se dit « noticeably faster than Lua 5.x (including Lua 5.4) »
  et capable d'égaler l'interpréteur LuaJIT sur certaines charges, JIT optionnel x64/arm64 ; **aucun benchmark
  tête-à-tête publié** sur la page performance.
- Lua 5.5.0 publiée le 2025-12-22. LuaJIT : rolling release, compatible **Lua 5.1** seulement.
- sol2 : dernière release v3.3.0 (2022-06-25). C# : Coral (hôte .NET de StudioCherno, MIT), sans release taguée.

**Mesure** [C] : sur le runner de bench GitLab (2026-11-09, `test/bench/thresholds.json`), un `on_update` vide coûte
120 ns par appel et 300 µs pour 1 000 instances sous Luau, contre 88 ns et 188 µs sous Lua 5.5 : l'interpréteur n'est
pas plus rapide sur ce micro-cas, le gain vient du typage et du bac à sable.

**Verdict : GARDER Luau.** Le binding manuel évite sol2. Le modèle « un `lua_State` par entité » est gardé (décision du
2026-11-12, P-11) malgré ses 126 Kio par instance. Ne pas viser LuaJIT (5.1 figé).

Sources : <https://luau.org/performance> ; <https://www.lua.org/work/> ; <https://luajit.org/status.html> ;
<https://github.com/ThePhD/sol2> ; <https://github.com/StudioCherno/Coral>.

## Fiche 5 — Sérialisation : yaml-cpp

**Choix Owl** [C] : yaml-cpp **0.9.0** pour l'écriture des scènes, prefabs, sauvegardes, réglages, replays **et**
snapshots undo (`EntitySnapshot` via `serializeEntityToString`) ; rapidyaml 0.15.2 pour leur lecture
(`core::YamlNode`, `core/external/ryml.h`) ; 120 fichiers `.cpp` de `source/owl/private` utilisent `YAML::`. Le cook
convertit le YAML en binaire (`core::YamlDocument::toBinary`, `serialize/scene_from_binary`) pour le `.owlpack`.

**Faits** [F] :

- yaml-cpp 0.9.0 est sortie le 2026-02-04.
- rapidyaml (doc « Is it rapid? ») : « generally 30x (parse) / 150x (emit) faster than yaml-cpp » ;
  ~150 Mo/s en YAML complet ; sur `appveyor.yml` en Release (VS2017) 101,5 Mo/s contre 5,3 Mo/s ; en JSON
  sous Linux (clang, Release) 453,5 Mo/s contre 15,1 Mo/s. rapidyaml v0.16.0 (2026-07-22), MIT.
- JSON : simdjson v5.0.2 (2026-10-04, Apache-2.0, lecture seule) ; glaze v9.0.0 (2026-09-24, MIT,
  réflexion à la compilation, JSON + BEVE binaire).
- Binaire : cereal v1.3.2 (2022-02-28, peu actif), FlatBuffers (2026-02, Apache-2.0, zéro-copie, schéma),
  bitsery (MIT, dernier push 2025-10).

**Critères** [O] : YAML est un bon format **source** (lisible, diffable, éditable à la main) et un mauvais format
**interne** : l'undo par aller-retour YAML paie parsing et allocations à chaque opération. La lecture par rapidyaml
coûte 7,3 µs par entité (`20-mesures.md`, §9.7) ; l'écriture, en yaml-cpp, 23 µs ; la lecture du binaire cuit environ
deux fois moins que celle du texte (`20-mesures.md`, §3.4).

**Verdict : SURVEILLER.** Écriture : migrer vers rapidyaml ou un émetteur maison si le coût des snapshots d'undo devient
gênant. Effort **M**. Le binaire cuit du `.owlpack` est en place ; YAML reste le format d'édition.

Sources : <https://rapidyaml.readthedocs.io/latest/sphinx_is_it_rapid.html> ;
<https://github.com/jbeder/yaml-cpp/releases> ; <https://github.com/simdjson/simdjson> ;
<https://github.com/stephenberry/glaze> ; <https://github.com/google/flatbuffers>.

## Fiche 6 — Physique : Jolt en 2D et en 3D

**Choix Owl** [C] : Jolt Physics **5.6.0** (recette locale `conan/recipes/joltphysics`), construit
`CROSS_PLATFORM_DETERMINISTIC` (`conanfile.py` de la recette, l. 57 et 97), seul moteur physique : la 2D est faite de
corps verrouillés dans un plan sur l'interface physique commune (`source/owl/private/physics/JoltWorld.cpp`, 1 111
lignes). Les tâches Jolt passent par Taskflow (`TaskflowJobSystem`, `JoltWorld.cpp:158`).

**Faits** [F] (relevés le 2026-10-05, non revérifiés) :

| Moteur | Version             | Licence | Références publiques                                              |
|--------|---------------------|---------|-------------------------------------------------------------------|
| Jolt   | v5.6.0 (2026-07-11) | MIT     | Horizon Forbidden West, Death Stranding 2 ; Godot 4.4+ en option  |
| Box2D  | v3.1.1 (2025-06-04) | MIT     | 5 050 corps en 0,90 à 1,91 ms par pas selon le SIMD (Catto, 2024) |
| Box3D  | v0.1.0 (2026-06-30) | MIT     | annoncé 2026-07 par Erin Catto ; C17, API proche de Box2D         |

[O] Un seul moteur pour la 2D et la 3D simplifie l'interface et le déterminisme (une seule bibliothèque à figer), au
prix d'un solveur 3D pour des scènes 2D : mesuré, un pas de 1 000 boîtes 2D qui tombent coûte 1,28 ms sous Jolt contre
228 µs sous Box2D 3.1.1 sur le même cas (`20-mesures.md` §3.7, P-12). La recette plafonne le SIMD à SSE4.2 (AVX, AVX2, AVX-512,
FMA coupés, recette l. 58-61) : choix assumé pour la simulation bit-exacte, sans variante AVX2 optionnelle.

**Verdict : GARDER Jolt en 2D et en 3D** (décision du 2026-11-12) ; profiler et régler le pas 2D, réviser la cible
(P-12). Box3D à revoir à sa 1.0.

Sources : <https://github.com/jrouwe/JoltPhysics/blob/master/Docs/PerformanceTest.md> ;
<https://box2d.org/posts/2024/08/simd-matters/> ; <https://github.com/erincatto/box3d>.

## Fiche 7 — Tâches : Taskflow

**Choix Owl** [C] : Taskflow 4.1.0, privé derrière `Scheduler` (pimpl), pont `std::promise`/`std::future`,
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

**Critères** [O] : Taskflow est solide et bien maintenu ; son point faible pour un moteur est l'absence de priorités et
d'affinité de thread (rendu, I/O), que fournit enkiTS. Le pont `std::future` ajoute une allocation partagée par tâche.
`std::execution` n'est pas livré complet par libstdc++/libc++ : prématuré.

**Verdict : GARDER.** Réévaluer (enkiTS ou fibres) au démarrage du chantier « job system au niveau frame / rendu sur
thread séparé » du §5 ; la façade `Scheduler` rend le changement local (effort **M**).

Sources : <https://arxiv.org/abs/2004.10908> ; <https://github.com/taskflow/taskflow/releases> ;
<https://github.com/dougbinks/enkiTS> ; <https://www.gdcvault.com/play/1022186/Parallelizing-the-Naughty-Dog-Engine> (GDC 2015) ;
<https://www.think-cell.com/en/career/devblog/trip-report-summer-iso-cpp-meeting-in-st-louis-usa> (2024-06).

## Fiche 8 — Dépendances : Conan 2

**Choix Owl** [C] : Conan 2 seul fournisseur (`conanfile.py`, `conan.lock`), ~40 `requires`, ConanCenter
d'abord, recettes locales dans `conan/recipes/` (entt, glfw, imgui-color-text-edit, imguizmo, joltphysics, ktx,
libmp3lame, luau, msdf-atlas-gen, msdfgen, nativefiledialog-extended, openal-soft, slang, taskflow, tinyobjloader,
ufbx).

| Outil     | Version (2026-10-05) | Licence | Modèle                                              | Étoiles |
|-----------|----------------------|---------|-----------------------------------------------------|---------|
| vcpkg     | 2026.07.29           | MIT     | manifest `vcpkg.json`, build source + cache binaire | 27,5 k  |
| Conan 2   | 2.33.0 (2026-09-29)  | MIT     | recettes Python, binaires, ConanCenter              | 9,5 k   |
| CPM.cmake | v0.43.2 (2026-09-24) | MIT     | `FetchContent` amélioré, sources                    | 4,1 k   |

**Critères** [O] :
- Portabilité : Linux x64/arm64 et MinGW, moins bien servi par ConanCenter, d'où des recettes locales.
- Reproductibilité : `conan.lock` fige les versions ; tout tiers passe par Conan, jamais vendoré.
- Coût : seize recettes locales à maintenir (Slang, Jolt, Luau, OpenAL Soft…).

**Verdict : GARDER.** Un contributeur externe construit Owl avec ConanCenter et les recettes du dépôt.

Sources : <https://github.com/microsoft/vcpkg> ; <https://github.com/conan-io/conan> ;
<https://github.com/cpm-cmake/CPM.cmake>.

## Fiche 9 — CI : GitLab CI auto-hébergé + wrapper Python

**Choix Owl** [C] : GitLab CI sur l'instance du mainteneur (`.gitlab-ci.yml`, `ci/gitlab/templates.yml`,
`ci/gitlab/nightly.yml`), runners du mainteneur (GPU, Windows, bench), wrapper `ci_action.py` (`BuildAndTest`,
`NightlyGate`) ; la fusion attend le dernier pipeline (`.gitlab-ci.yml:16-17`). Le dépôt GitHub public reste la
vitrine ; aucun `.github/workflows`.

**Faits** [F] (relevés le 2026-10-05, non revérifiés) : GitHub Actions offre des runners standards gratuits et
illimités aux dépôts publics.

| Critère                  | GitLab CI auto-hébergé                         | GitHub Actions                                   |
|--------------------------|------------------------------------------------|--------------------------------------------------|
| Coût                     | gratuit, instance et runners à maintenir       | gratuit (dépôt public)                           |
| Visibilité contributeurs | instance du mainteneur, hors du dépôt GitHub   | native dans la PR                                |
| GPU / matériel réel      | oui (runners du mainteneur)                    | non sur runners standards (self-hosted possible) |
| Puissance                | YAML, pipelines enfants, règles, cache local   | YAML, matrice, cache distant limité              |
| Bus factor               | instance et runners maison, sans plugin maison | aucun service à maintenir                        |

**Verdict : GARDER.** [O] La migration supprime le plugin GitHub maison et le DSL Kotlin ; le wrapper Python reste
portable. La visibilité pour un contributeur externe tient à la publication des statuts côté GitHub (décision
ouverte, synthèse §4).

## Fiche 10 — Audio : OpenAL Soft

**Choix Owl** [C] : OpenAL Soft 1.25.2 en **partagé** (recette locale `conan/recipes/openal-soft`), libsndfile pour le décodage.

| Option      | Licence                                                               | Version (2026-10-05) | Remarques                                               |
|-------------|-----------------------------------------------------------------------|----------------------|---------------------------------------------------------|
| OpenAL Soft | **LGPL-2.0**                                                          | 1.25.2 (2026-05-12)  | spatialisation 3D, HRTF, EFX ; relink exigé (dynamique) |
| miniaudio   | domaine public ou MIT-0                                               | 0.11.25 (2026-03-03) | un seul fichier, décodeurs intégrés, graphe de nœuds    |
| SoLoud      | zlib                                                                  | release 2020-02-07   | dernier push 2024-08 : peu maintenu                     |
| FMOD        | propriétaire, gratuit < 200 k$ de revenu annuel et < 500 k$ de budget | —                    | outil auteur, standard industriel                       |
| Wwise       | propriétaire, gratuit < 250 k$ de budget                              | —                    | outil auteur, pas de source en Indie                    |

**Critères** [O] : OpenAL Soft est mature et à jour, mais la LGPL contraint la distribution (bibliothèque remplaçable
par l'utilisateur : lien dynamique) et il n'apporte ni décodage ni mixage par bus. miniaudio couvre lecture, décodage
(remplaçant possible de libsndfile), bus et spatialisation simple, sans contrainte de licence. FMOD et Wwise sont hors
sujet pour un moteur open source (non redistribuables).

**Verdict : SURVEILLER**, miniaudio étant le remplaçant naturel au chantier « audio : mixage, bus, streaming » du §5
(supprime aussi libsndfile). Effort **S–M** (backend `sound` isolé + Null).

Sources : <https://github.com/kcat/openal-soft> (fichier COPYING) ; <https://github.com/mackron/miniaudio> ;
<https://github.com/jarikomppa/soloud> ; <https://www.gamedeveloper.com/audio/small-developers-and-creators-can-now-use-fmod-studio-for-free> ;
<https://www.audiokinetic.com/pricing/for-games>.

## Fiche 11 — Profilage : macros `OWL_PROFILE_*` et Tracy

**Choix Owl** [C] : `OWL_PROFILE_SCOPE/FUNCTION` (`source/owl/public/debug/Profiler.h`) devant un backend choisi
par `OWL_PROFILER=none|tracy|chrome` : Tracy 0.13.1 (option Conan `tracy`) ou un JSON « Chrome tracing » ;
profiler in-editor ; `TrackerAPI` pour la mémoire.

| Outil        | Licence                                 | État (2026-10-05)                | Points forts                                                               |
|--------------|-----------------------------------------|----------------------------------|----------------------------------------------------------------------------|
| Tracy        | BSD-3                                   | v0.14.1 (2026-08-22), très actif | temps réel, ~15 ns par zone (manuel), GPU Vulkan/GL, mémoire, verrous, Lua |
| Optick       | MIT                                     | release 2022-05, push 2024-05    | quasi abandonné                                                            |
| Superluminal | propriétaire, 59 € perso / 289 €/an pro | actif                            | échantillonnage sans instrumentation                                       |
| Chrome JSON  | —                                       | —                                | zéro dépendance, mais JSON écrit à chaud, pas de GPU, pas de temps réel    |

**Verdict : GARDER** la façade `OWL_PROFILE_*` avec Tracy comme backend de mesure.

Sources : <https://github.com/wolfpld/tracy> (manuel `tracy.pdf`, section « Features / overhead ») ;
<https://github.com/bombomby/optick> ; <https://superluminal.eu/pricing/>.

## Fiche 12 — Moteurs de référence

[F] sauf mention ; métadonnées GitHub au 2026-10-05.

| Moteur        | Langage | Licence          | Dernière version                        | Taille d'équipe publiée                          | Architecture notable                              |
|---------------|---------|------------------|-----------------------------------------|--------------------------------------------------|---------------------------------------------------|
| Owl           | C++23   | MIT              | 0.7.0 (2026-11-13)                      | 1 auteur (`git log`)                             | EnTT, GL+Vulkan maison, Slang, Luau, Jolt         |
| Hazel         | C++     | Apache-2.0       | public figé (dernier commit 2023-10-27) | Cherno + équipe Patreon (version privée)         | EnTT, Vulkan, C# ; ADN visible dans Owl           |
| Godot 4       | C++     | MIT              | 4.7.2 (2026-08-18)                      | « well over 300 contributors », > 1 600 PR (4.7) | nœuds/scène, RD (Vulkan/D3D12/Metal), GDScript/C# |
| Bevy          | Rust    | MIT/Apache-2.0   | 0.19.1 (2026-08-13)                     | 261 contributeurs, 1 185 PR (0.19, 2026-06-19)   | ECS archétypes, wgpu, pas d'éditeur complet       |
| raylib        | C       | zlib             | 6.0 (2026-04-23)                        | 1 mainteneur principal + communauté              | bibliothèque, pas d'éditeur, GL                   |
| Wicked Engine | C++     | MIT              | v0.72.120 (2026-10-02)                  | 1 auteur principal                               | ECS maison, D3D12/Vulkan, rendu GPU-driven        |
| O3DE          | C++     | Apache-2.0 / MIT | 2605.0 (2026-05-27)                     | fondation Linux, multi-entreprises               | Atom (render graph), Gems modulaires              |
| Defold        | C/Lua   | Defold License   | 1.13.2 (2026-09-29)                     | Defold Foundation, équipe salariée               | Lua (LuaJIT), focalisé 2D/mobile/Web              |

**Ce que les autres font mieux** [O] :
- Plateformes : Godot, Bevy, Defold, raylib ciblent le Web ; Owl ne cible ni Web ni macOS (Web et Android prévus en v0.8.0).
- Rendu : Wicked Engine (solo aussi) montre qu'un auteur seul peut porter un rendu GPU-driven moderne en
  abandonnant OpenGL et en n'ayant qu'une abstraction mince sur D3D12/Vulkan.
- Assets : Godot/O3DE importent et « cookent » hors-ligne, références par UID/GUID ; Owl a ses GUID (`.owlmeta`) et
  son cook depuis la v0.6.0.
- Outils : les moteurs établis s'appuient sur des profileurs externes temps réel (Tracy, Superluminal) ; Owl dispose du backend Tracy.

**Ce qu'Owl fait différemment (et défendable)** [O] :
- Éditeur avec undo/redo, prefabs, voxel, raycast et tilemap dans un seul moteur solo — rare hors Godot.
- Discipline qualité (sanitizers, clang-tidy sur diff, CodeStyle, Doxygen obligatoire) supérieure à
  Hazel public, raylib ou Wicked Engine.
- Slang comme langage unique : plus moderne que GLSL (Godot) ou WGSL (Bevy).

Sources : <https://godotengine.org/releases/4.7/> ; <https://bevy.org/news/bevy-0-19/> (2026-06-19) ;
<https://github.com/TheCherno/Hazel> ; <https://github.com/turanszkij/WickedEngine> ;
<https://github.com/o3de/o3de> ; <https://github.com/defold/defold> ; <https://github.com/raysan5/raylib>.

## Synthèse

| Fiche             | Verdict                                                | Coût estimé si action  |
|-------------------|--------------------------------------------------------|------------------------|
| 1 ECS EnTT        | Garder                                                 | —                      |
| 2 RHI GL+Vulkan   | Surveiller (OpenGL gelé, Vulkan prioritaire)           | L pour une RHI tierce  |
| 3 Shaders Slang   | Garder (précompilé au build, compilé au besoin)        | —                      |
| 4 Script Luau     | Garder, un `lua_State` par entité                      | —                      |
| 5 Sérialisation   | Surveiller (écriture yaml-cpp ; binaire cuit en place) | M                      |
| 6 Jolt (2D et 3D) | Garder ; régler le pas 2D (P-12)                       | S–M (profil, réglages) |
| 7 Taskflow        | Garder                                                 | M si job system frame  |
| 8 Conan 2         | Garder                                                 | —                      |
| 9 GitLab CI       | Garder ; statuts visibles côté GitHub à décider        | S                      |
| 10 OpenAL Soft    | Surveiller ; miniaudio au chantier audio               | S–M                    |
| 11 Profileur      | Garder (façade `OWL_PROFILE_*`, backend Tracy)         | —                      |

À garder sans hésiter : EnTT, Slang (le langage), Conan 2.
