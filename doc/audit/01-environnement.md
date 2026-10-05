# Environnement de build et de mesure

> Statut : validé le 2026-10-05. Toute commande de l'audit passe par `docker/run.sh`.

## 1. La toolchain *Docker Owl*

CLion définit une toolchain *Docker Owl* (`~/.config/JetBrains/CLion2026.2/options/linux/toolchains.xml`) :

| Élément     | Valeur                                                                   |
|-------------|--------------------------------------------------------------------------|
| Image       | `registry.argawaen.net/builder/devel-ubuntu2404:latest` (4 Go, 11 jours) |
| Utilisateur | `-u 1001:1001` (UID/GID de l'utilisateur)                                |
| `$HOME`     | `/fhome` ← `/data/sources/personnel/stack_owl/fake_home`                 |
| Montages    | `/data/sources`, `/tmp/.X11-unix`, `/dev/snd`, `/run/user/1001`          |
| Options     | `--privileged --device=/dev/dri --group-add audio --gpus all`            |

`fake_home` contient le virtualenv Poetry (`.cache/pypoetry`), le cache DepManager (`.edm`) et ccache.

**`docker/run.sh`** reproduit cette toolchain en ligne de commande, pour l'agent comme pour l'humain :
même image, même UID/GID, dépôt monté à son chemin hôte, même `$HOME`. Deux options l'étendent :
`--gui` (GPU NVIDIA via `--gpus all`, `/dev/dri`, affichage X11/Wayland, PulseAudio) et `--perf`
(`SYS_PTRACE`, `PERFMON`, `SYS_ADMIN`, seccomp levé). Il n'utilise pas `--privileged` par défaut.

### Validation (2026-10-05)

| Étape                                | Résultat                                                                  |
|--------------------------------------|---------------------------------------------------------------------------|
| `poetry sync --no-root`              | OK, rien à installer.                                                     |
| `cmake --preset linux-clang-release` | OK en 5,3 s, dépendances résolues depuis `/fhome/.edm`.                   |
| `cmake --build` (429 étapes)         | OK en 60 s.                                                               |
| `ctest -j8` (16 suites)              | 15/16 OK en 1,7 s. Échec : `Core_Version.base` (voir ci-dessous).         |
| `CodeStyle`                          | OK une fois `doc/audit/` exclu de codespell (§4).                         |
| `vulkaninfo` avec `--gui`            | Trois devices visibles : NVIDIA RTX 5000 Ada, Intel UHD (Mesa), llvmpipe. |

`Core_Version.base` échoue sur la branche courante : `CMakeLists.txt` annonce `0.2.2` (kickoff), le
test attend toujours `0.2.1` (`test/core_tests/version_test.cpp:9-10`). C'est un premier constat de
l'audit (le test fige la version, et le kickoff ne l'a pas mis à jour) ; il n'est pas corrigé ici.

### Côté CLion

Aucun profil CMake de `.idea/workspace.xml` ne déclare de `TOOLCHAIN_NAME` : ils utilisent la toolchain
par défaut (`Default`, hôte). Pour que CLion builde réellement dans *Docker Owl*, il faut, dans
*Settings → Build → CMake*, choisir la toolchain *Docker Owl* pour chaque profil actif
(`linux-clang-release-vk-vaidation`, `linux-clang-debug-vk-validation`, `linux-gcc-release`,
`linux-clang-tidy`), ou la placer en tête de liste dans *Toolchains* pour qu'elle devienne le défaut. Je
n'ai pas modifié `workspace.xml` : CLion le réécrit à la fermeture.

Les caches actuels sont mixtes. `output/build/linux-clang-{release,debug}` pointent déjà sur
`/usr/bin/ninja` et `/usr/bin/clang++` (chemins du conteneur), alors que `linux-gcc-release` et
`linux-clang-tidy` pointent sur le ninja de CLion Toolbox (build natif). Quand un profil passe sur
*Docker Owl*, il faut le reconfigurer une fois (*Reset Cache and Reload Project*).

Le nom `linux-clang-release-vk-vaidation` contient une coquille (*vaidation*).

## 2. Outillage présent dans l'image

| Catégorie       | Outils                                                                                 |
|-----------------|----------------------------------------------------------------------------------------|
| Compilateurs    | clang 22.1.8, gcc 14.2, lld, mold, ccache                                              |
| Build           | cmake 4.4.3, ninja                                                                     |
| Python          | Python 3.12.3, Poetry 2.5.1 ; via Poetry : depmanager 0.5.5, gcovr, codespell          |
| Qualité         | clang-tidy, clang-format, cppcheck, llvm-cov, lcov                                     |
| Débogage        | gdb, lldb, valgrind                                                                    |
| Docs            | doxygen, graphviz (`dot`)                                                              |
| GPU / affichage | Mesa 25.2.8 (dont lavapipe), `vulkan-tools` et `vulkan-validationlayers` 1.3.275, Xvfb |

## 3. Ce qu'il faudrait ajouter à l'image

Classé par importance pour l'audit, dont la priorité est la performance du moteur.

| Priorité | Ajout                                                            | Pourquoi                                                                                                                                                                                                                                                       |
|----------|------------------------------------------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Haute    | `perf` réellement exécutable                                     | Seul le wrapper `linux-tools-common` est installé, sans binaire. Installer un `linux-tools-<version>-generic` et exposer `/usr/lib/linux-tools-*/perf` sous `/usr/local/bin/perf`, ou builder perf depuis les sources du noyau. Profilage CPU impossible sans. |
| Haute    | Tracy (profiler + `tracy-capture`, `tracy-csvexport`)            | Profiler de frame de référence dans le monde du jeu vidéo ; permet de comparer les zones `OWL_PROFILE_SCOPE` sur la timeline. À coupler à un package DepManager pour la partie client.                                                                         |
| Haute    | `heaptrack` (+ `heaptrack_print`)                                | Allocations par frame et pics mémoire, plus rapide que valgrind/massif.                                                                                                                                                                                        |
| Haute    | `renderdoccmd` (RenderDoc)                                       | Capture GPU en ligne de commande pour mesurer et inspecter les passes, OpenGL et Vulkan.                                                                                                                                                                       |
| Moyenne  | `hyperfine`                                                      | Mesures répétées et statistiques des temps de démarrage, de chargement et des benchmarks CLI.                                                                                                                                                                  |
| Moyenne  | `hotspot`                                                        | Lecture graphique des enregistrements perf (avec `--gui`).                                                                                                                                                                                                     |
| Moyenne  | `spirv-tools` (`spirv-val`, `spirv-dis`, `spirv-opt`), `glslang` | Inspecter et valider le SPIR-V produit par Slang, comparer une compilation hors-ligne.                                                                                                                                                                         |
| Moyenne  | Validation layers Vulkan alignées sur 1.4                        | L'image fournit les layers 1.3.275 alors que le moteur vise Vulkan 1.4 (le `vulkan_sdk` 1.4.341 de DepManager les copie avec `OWL_ENABLE_VULKAN_LAYERS=ON`, à vérifier).                                                                                       |
| Moyenne  | `mesa-utils` (`glxinfo`, `eglinfo`)                              | Vérifier rapidement quel driver OpenGL est réellement utilisé dans le conteneur.                                                                                                                                                                               |
| Basse    | `include-what-you-use`, ClangBuildAnalyzer                       | Coût des headers et temps de build (axe secondaire).                                                                                                                                                                                                           |
| Basse    | `bloaty`                                                         | Taille des binaires et des sections, par symbole.                                                                                                                                                                                                              |
| Basse    | Google Benchmark                                                 | Plutôt en package DepManager qu'en paquet système, pour le harnais `bench/` (cadrage, §10.1).                                                                                                                                                                  |

Incohérence à signaler : la config DepManager de `fake_home` déclare une toolset `llvm20` avec
`compiler_path: clang++-20`, alors que l'image fournit clang 22. À vérifier lors de l'axe G (ABI des
paquets, reproductibilité).

## 4. Effet de `doc/audit/` sur la CI

`doc/` est scanné par codespell (`CodeStyle`) et ingéré par Doxygen (`Documentation`,
`WARN_AS_ERROR=YES`). Des pages en français y feraient échouer les deux gates. Deux exclusions ont été
ajoutées :

- `ci/actions/code_style.py` : `--skip=<repo>/doc/audit` sur la commande codespell ;
- `DoxyfileTemplate` : `EXCLUDE = ../../../doc/audit`.

Le `Doxyfile` racine n'est pas un fichier mort : il est généré dans l'arbre source à partir de
`DoxyfileTemplate` et ignoré par git (`.gitignore:52`). Correction apportée par l'axe I (voir
`10-constats-GI-build-docs.md`).

Constat annexe : `CodeStyle` ne scanne ni `test/` (le codespell direct y trouve 8 fautes, par exemple
`Environement` dans `test/core_tests/env_test.cpp:8`), et ses `SOURCE_ROOTS` citent
`source/owlrunner/sources`, qui n'existe pas (le runner est dans `source/owlnest/runner`).
