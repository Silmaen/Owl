# Audit Owl — synthèse

> **Revu le 2026-10-20 sur `main` 4e1a6a10.** Sources : [`00-cadrage.md`](00-cadrage.md) (environnement, configuration IA),
> les six `10-constats-*.md`, [`20-mesures.md`](20-mesures.md), [`30-etat-de-l-art.md`](30-etat-de-l-art.md) et
> [`40-avenir.md`](40-avenir.md). Gravités et statuts : après contre-vérification. Aucun constat nouveau ; chaque
> affirmation renvoie à un ID. Le plan de versions fait foi dans [`doc/pages/roadmap.md`](../pages/roadmap.md).
> Effort, mainteneur seul : **S** jours, **M** semaines, **L** mois. **[O]** : opinion de l'auditeur.

[TOC]

## 1. En une page

### Verdict

Owl est un moteur solo d'une ampleur rare (2D, tilemap, raycast, voxel, isométrique, éditeur avec undo, prefabs et
documents), aux choix tiers bons et à jour, avec une frontière publique/privée et un outillage plus rigoureux que son
ancêtre Hazel (A-06, A-21, H-11). Deux chantiers de fond restent : `Scene` porte du gameplay et les sous-systèmes sont
liés à une scène unique (A-02, A-08, D-15) ; le backend Vulkan garde des attentes synchrones et des synchronisations
fragiles (B-01, B-04, B-19). Les filets de rendu dépendent de lavapipe (B-06, F-01). Tenir ces filets, puis bâtir la 3D,
le réseau et le modding sur des fondations ouvertes (`40-avenir.md`).

### Les 5 forces

1. **Frontière publique/privée tenue** : Box2D, Lua, Taskflow, spdlog, Vulkan, GL, GLFW et OpenAL restent hors des
   headers publics (A-06) ; la convention dossier == namespace est respectée (A-14), l'ordre de démarrage et d'arrêt
   est explicite (A-20).
2. **Cœur ECS et rendu 2D efficaces** : `Scene::copy` à 3 ms pour 10 000 entités (P-08, C-16), Renderer2D instancié par
   SSBO, un draw call jusqu'à 20 000 instances (B-08, P-05), pile de rendu pilotée par les données (A-15, B-22).
3. **Choix tiers solides, bien intégrés côté algorithmes** : Box2D v3, Lua, Slang en source unique pour deux backends,
   greedy meshing voxel avec AO, contrat du `Scheduler` respecté (D-19, D-22, B-17, D-20, D-21, D-28, D-29).
4. **Discipline d'outillage rare pour un projet solo** : logique pure testée avec assertions de comportement, ciblage
   clang-tidy correct par construction, CI reproductible en local par un point d'entrée unique, CMake moderne, aucun
   secret dans le dépôt (F-13, H-11, H-10, G-10, H-12).
5. **Éditeur au modèle sain** : chaque propriété des 31 composants est inspectable, éditable et annulable, avec un
   modèle de commandes homogène entre documents ; documentation abondante et accessible depuis l'éditeur (E-10, E-09,
   I-10).

### Les 5 faiblesses

1. **`Scene` et singletons hérités de Hazel** : `Scene` cumule ECS, ordonnancement et gameplay (A-02, C-14), ~25
   singletons au cycle de vie dispersé (A-08), physique et VM Lua liées à « la » scène active (D-15), `Application`
   comme localisateur de services (A-07), composants du moteur déclarés à la main (A-05).
2. **Socle Vulkan à consolider** : attentes synchrones des opérations one-shot (B-01), SSBO écrit hors
   enregistrement (B-04), image de swapchain écrite avant son acquisition dans le runner (B-19), swapchain recréée
   sans l'ancienne (B-25) ; l'abstraction GPU fuit hors de `renderer/gpu` (A-09, B-07).
3. **Filets de rendu et de couverture partiels** : les tests d'image sont ignorés sans lavapipe (B-06), la couverture
   officielle exclut les backends (F-01, F-04), aucun check n'est requis pour fusionner (H-06).
4. **Éditeur incomplet au regard de ses propres règles** : pas de sélection multiple ni de copier/coller (E-06),
   snapshots d'entité entiers (E-05), logique de transition de niveau en double (E-12).
5. **Écart entre ce qui est annoncé et ce qui existe** : volumes par catégorie et joints documentés mais absents
   (D-07, I-02), OpenGL « 4.5 » dont le draw indirect exige 4.6 (B-16), README plus strict que le code (I-03), pages
   susceptibles de dériver (I-01).

### Les 5 bizarreries

1. **Du code écrit pour un futur qui n'est pas venu** : culling GPU, tri bitonique et draw indirect branchés aux deux
   backends sans appelant (B-20), `parallelForEach` et Taskflow réduits à une file FIFO (D-24), recherche en O(n)
   offerte aux scripts (D-26).
2. **Le `.owlpack` est obfusqué puis extrait intégralement en clair au démarrage** ; un pack modifié sans changement
   de taille laisse des assets périmés (D-10) ; lu depuis un worker, il n'est pas thread-safe (D-09).
3. **Une rigueur de warnings qui suit chaque Clang** : zéro warning sous `-Weverything`, mais chaque nouvelle version
   de Clang casse le build tant qu'une exclusion n'est pas ajoutée (G-11).
4. **Deux images de build** : la CI et le poste local n'utilisent pas la même, donc « vert en local » ne garantit pas
   « vert en CI » (H-09).
5. **Un moteur dérivé de Hazel** : forces et limites héritées, écarts dans les deux sens (A-21).

### Défauts à corriger en priorité

Ordre : exposition × gravité, puis coût ; constats de gravité moyenne ou plus qui appellent une correction.

| Rang | Constats | Symptôme                                                                    | Exposition                      | Effort |
|------|----------|-----------------------------------------------------------------------------|---------------------------------|--------|
| 1    | B-19     | Image de swapchain écrite avant son sémaphore d'acquisition (runner Vulkan) | chaque frame du runner Vulkan   | S      |
| 2    | D-09     | `PackReader` partagé lu depuis un worker : course sur le flux               | transition de niveau            | S      |
| 3    | D-16     | `longjmp` de `luaL_check*` à travers des frames C++                         | erreur Lua dans une liaison     | S      |
| 4    | E-12     | Transition de niveau en double : éditeur et jeu livré peuvent diverger      | téléportation entre niveaux     | S      |
| 5    | C-12     | Lien d'entité retombant sur un nom non unique : cassé en silence            | renommage ou duplication        | S      |
| 6    | D-10     | Pack extrait en clair, assets périmés si la taille ne change pas            | tout jeu exporté                | M      |
| 7    | D-07     | Volumes `music` / `sfx` annoncés sans effet, joints non exposés             | tout projet audio ou physique   | S      |
| 8    | B-01     | Attente fence à chaque opération one-shot                                   | transitions et copies Vulkan    | M      |
| 9    | E-06     | Pas de sélection multiple ni de copier/coller                               | tout travail d'édition de scène | M      |

## 2. Comptes par axe

L'axe P regroupe les constats de `20-mesures.md` ; l'axe J (configuration IA) est au §12 de `00-cadrage.md`, sans ID.

| Axe                | Constats | Forces | Faiblesses | Risques | Étrangetés | Haute | Moyenne | Basse  |
|--------------------|----------|--------|------------|---------|------------|-------|---------|--------|
| A Architecture     | 13       | 5      | 7          | 0       | 1          | 2     | 8       | 3      |
| B Rendu            | 14       | 4      | 6          | 2       | 2          | 0     | 4       | 10     |
| C Scène, ECS       | 8        | 2      | 4          | 0       | 2          | 0     | 3       | 5      |
| D Sous-systèmes    | 19       | 6      | 7          | 2       | 4          | 0     | 8       | 11     |
| E Éditeur          | 6        | 2      | 4          | 0       | 0          | 0     | 3       | 3      |
| F Qualité          | 8        | 1      | 5          | 0       | 2          | 0     | 3       | 5      |
| G Build, packaging | 7        | 1      | 2          | 1       | 4          | 0     | 2       | 6      |
| H CI               | 8        | 3      | 2          | 1       | 2          | 0     | 3       | 5      |
| I Documentation    | 6        | 1      | 4          | 0       | 1          | 0     | 1       | 5      |
| P Mesures          | 7        | 3      | 4          | 0       | 0          | 0     | 0       | 7      |
| **Total**          | **97**   | **28** | **45**     | **6**   | **18**     | **2** | **35**  | **60** |

Lecture :

- Les deux constats de gravité haute sont A-02 (faiblesse) et A-06 (force).
- Les constats dont le statut n'est pas « confirmé » : A-21, D-09, D-16, F-02, F-09, H-04, H-05, G-16, G-17,
  I-01, I-02 (plausibles) et H-06 (non vérifiable depuis le dépôt).
- **Doublons à traiter ensemble** : A-02 ≈ C-14 ; A-08 ≈ D-15 ; A-09 ≈ B-07 ; D-07 ≈ I-02 ; C-10 ≈ P-07. Une fois
  fusionnés, il reste environ 92 constats distincts.

## 3. Plan d'action

Chantiers de long terme : [`40-avenir.md`](40-avenir.md) ; correspondance avec les versions : §5 du même fichier et
`doc/pages/roadmap.md`. Chaque ligne renvoie à une version de la roadmap ou à une fondation à glisser dans les versions
en cours.

| Version / horizon | Chantiers (`40-avenir.md`)         | Constats couverts                  | Contenu                                                                | Effort |
|-------------------|------------------------------------|------------------------------------|------------------------------------------------------------------------|--------|
| Fondations        | K-02, K-04, K-05                   | F-02, D-11, D-12, F-04, H-05, H-06 | Tests de l'éditeur étendus, outils de mesure dans l'image, CI qui mord | S      |
| Fondations        | K-07, K-08, K-09, K-10             | A-02, A-05, A-07, A-08, D-15, C-14 | Cycle de vie des entités, monde par scène, systèmes à phases, registre | M-L    |
| Fondations        | K-15, K-16, K-18, K-28, K-38, K-39 | E-05, E-06, B-04, D-27, H-09, I-02 | Undo par composant, prefabs imbriqués, reliquat Vulkan, doc vérifiée   | M      |
| v0.5.0            | K-21, K-22, K-23, K-33             | B-22, B-07, B-14, B-20, D-24       | Render graph, rendu 3D, rendu piloté par GPU, rendu multithread        | XL     |
| v0.6.0            | K-12, K-13, K-14, K-27, K-31       | A-07, C-10, P-07, D-10, D-16, D-26 | GUID, format binaire cuit, pack parallèle, script, squelette           | L      |
| v0.7.0            | K-25, K-26, K-29                   | D-14, D-07, D-13                   | Physique 2D complète, Jolt, audio                                      | M      |
| v0.8.0            | K-34, K-35, K-37                   | B-16, A-05                         | Réseau, export WebGPU, modding                                         | XL     |
| v1.0.0            | K-35, K-36                         | A-12, B-16                         | Web en 3e plateforme, paquet Conan consommable                         | M      |

Défauts sans chantier dédié, à traiter au fil des PR : B-19, B-01, B-25, D-09, E-12, C-12, C-11, C-13, F-01, F-07, G-09,
G-17, I-01, I-03, I-06.

## 4. Décisions encore ouvertes

| # | Question                                                                                      | Options                                                                                       | Constats                  |
|---|-----------------------------------------------------------------------------------------------|-----------------------------------------------------------------------------------------------|---------------------------|
| 1 | Que faire d'OpenGL face à Vulkan et au rendu 3D ?                                             | exiger 4.6 ; repli 4.5 sans draw indirect ; geler OpenGL en repli                             | B-16, B-01, B-07, fiche 2 |
| 2 | Le gameplay (Victory/Death, `VoxelPlayer`) a-t-il sa place dans le moteur ?                   | sortir vers le sample ou des modules ; garder                                                 | A-02, C-14                |
| 3 | Contributions externes visées ?                                                               | si oui : chaîne de dépendances publique, GitHub Actions minimal ; sinon DepManager + lockfile | H-09, fiches 8 et 9       |
| 4 | Le modding vise-t-il du code non fiable ?                                                     | Lua 5.5 durci ; Luau ; mods de données seulement                                              | D-16, D-10, fiche 4       |
| 5 | Modèle Lua : un `lua_State` par entité ou une VM partagée avec `_ENV` par script ?            | isolation actuelle (~9 Ko, 28 µs par instance) ; VM partagée                                  | P-11, D-22                |
| 6 | YAML comme format interne (undo, prefab, copie) ?                                             | rapidyaml ; binaire cuit pour runtime et undo ; statu quo                                     | P-07, C-10, fiche 5       |
| 7 | Exigence Doxygen : documenter les membres triviaux sous `WARN_AS_ERROR` ?                     | alléger (API publique seulement) ; garder                                                     | I-06                      |
| 8 | Protection de branche : quels checks requis pour fusionner sur `main` ? arm64 sur chaque PR ? | Build Linux Clang + CodeStyle + ASan requis ; arm64 en nightly                                | H-06, H-05                |
| 9 | `-Werror` sous `-Weverything` pour les développeurs ou seulement en CI ?                      | option `OWL_WARNINGS_AS_ERRORS` en CI ; garder                                                | G-11                      |

## 5. L'IA dans ce dépôt

### État

Configuration décrite au §12 de [`00-cadrage.md`](00-cadrage.md) : `CLAUDE.md` court, règles ciblées par chemin,
`docker_run.sh`, skills `build`, `test`, `coverage`, `check`, mémoire réduite à un index de pointeurs. La vague d'agents
d'axe a coûté 130 k à 360 k tokens et 7 à 60 min par agent ; l'orchestrateur ne reçoit qu'un résumé (§12.4).

### Ce que l'audit révèle sur la configuration

- **Une consigne fausse oriente le travail** : un chiffre de compilation Slang erroné a classé un constat en gravité
  haute ; seule la mesure (`20-mesures.md`) tranche.
- **Les agents se relisent** : la contre-vérification baisse des gravités et rejette des constats ; les comparaisons aux
  autres moteurs viennent de la mémoire des modèles tant que `30-etat-de-l-art.md` ne les source pas.
- **Le harnais injecte l'instantané de début de session** : une consigne modifiée en cours de session ne prend effet
  qu'à la suivante.

### Ce qui reste à faire

| Action                                                                                      | Source              | Effort |
|---------------------------------------------------------------------------------------------|---------------------|--------|
| Tester un hook `PostToolUse` lançant `clang-format` en Docker sur chaque fichier C++ édité  | 00-cadrage §12.3.1  | S      |
| Évaluer le MCP CLion (`get_file_problems`, `search_symbol`) face à grep + clang-tidy        | 00-cadrage §12.3.2  | S      |
| Citer `docker_run.sh` comme convention de lanceur dans la règle globale                     | 00-cadrage §12.3.3  | S      |
| Relever les règles réellement chargées par les agents et élaguer celles qui n'ont pas servi | 00-cadrage §12.3.4  | S      |
| Ajouter à l'image les outils manquants : `perf`, `heaptrack`, `renderdoccmd`, `hyperfine`   | 00-cadrage §11.3    | S      |
| Aligner l'image locale et l'image CI (ou un tag commun)                                     | H-09                | S      |
| Reformater la section 0.1.1 de la page changelog en une ligne par changement                | I-08                | S      |
| Faire de la contre-vérification adverse une étape standard de toute campagne d'agents       | cadrage §7          | S      |
| Exiger la mention « non revérifié » sur toute comparaison externe non sourcée               | 00-cadrage §12.4    | S      |
