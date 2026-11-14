# Audit Owl — synthèse

> **Revu le 2026-11-12 sur `Feature/sample-games-new` b4281d72.** Sources : [`00-cadrage.md`](00-cadrage.md)
> (environnement, configuration IA), les six `10-constats-*.md`, [`20-mesures.md`](20-mesures.md),
> [`30-etat-de-l-art.md`](30-etat-de-l-art.md) et [`40-avenir.md`](40-avenir.md). Chaque affirmation renvoie à un ID. Le
> plan de versions fait foi dans [`doc/pages/roadmap.md`](../pages/roadmap.md). Effort, mainteneur seul : **S** jours,
> **M** semaines, **L** mois. **[O]** : opinion de l'auditeur.

[TOC]

## 1. En une page

### Verdict

Owl a livré en un mois le rendu 3D sur frame graph, le GPU-driven, le cook et les GUID, le visual scripting sur Luau,
Jolt en 2D et 3D, une simulation bit-exacte avec replay et l'IA, avec dix jeux d'exemple joués par des scénarios
(B-22, B-28, D-30, D-32, D-33). Les fondations ont moins avancé que les fonctionnalités : `Scene` a doublé et remplit le
frame graph (A-02, A-16, B-26), le jeu de composants reste fermé (A-05), et deux coûts nouveaux sont mesurés (état Luau
de 126 Kio par entité, P-11 ; pas 2D sur Jolt 3 à 6 fois Box2D, P-12). Les filets laissent l'éditeur hors couverture
et la nocturne rouge sans propriétaire (F-02, H-13). Avant le modding (v0.8.0) et le réseau (v0.9.0) : registre de
types et monde par scène (K-10, K-08).

### Les 5 forces

1. **Frontière publique/privée tenue** : ni Jolt, ni Luau, ni Vulkan, ni yaml-cpp dans les headers publics (A-06) ;
   convention dossier == namespace (A-14), démarrage et arrêt explicites (A-20).
2. **Simulation déterministe et rendu vérifiés** : flux aléatoires par système, maths bit-exactes, Jolt figé, hash de
   monde et replay (D-30) ; 40 scènes de référence en tests d'image sur lavapipe / llvmpipe (B-28).
3. **Cœur ECS et rendu efficaces** : `Scene::copy` à 3,1 ms pour 10 000 entités (P-08), un draw call jusqu'à 20 000
   instances (P-05), frame graph et culling GPU en production (B-22), timestamps GPU sur les deux backends (B-27).
4. **Discipline d'outillage rare pour un projet solo** : CodeStyle à gardes fortes (F-14), clang-tidy ciblé par
   construction (H-11), logique pure testée (F-13), Conan 2 verrouillé (G-23), fuzz et include-check.
5. **Éditeur au modèle sain** : 79 des 83 composants inspectables (E-10), ~81 commandes nommées partagées par console,
   macros, journal et MCP (E-13), visual scripts compilés en Luau et vérifiés par types (D-32).

### Les 5 faiblesses

1. **`Scene` concentre tout** : 4 723 + 3 127 lignes, gameplay et remplissage du frame graph (A-02, B-26), agrégateurs
   doublés (A-16), composants déclarés à la main (A-05), `Application` localisateur de services (A-07, A-08).
2. **Deux coûts nouveaux, mesurés** : 126 Kio et 46 µs par `ScriptInstance` Luau (P-11) ; pas 2D Jolt 3 à 6 fois
   Box2D sur un thread, 5 000 corps sous 1,5 ms seulement à 4 workers avec un pas de collision (P-12).
3. **Filets incomplets** : éditeur hors couverture (F-02), nocturne rouge 4 fois sur 6 (H-13), ASan seulement sur
   `main` (H-06).
4. **Éditeur en deçà de ses propres règles** : ni sélection multiple ni copier/coller d'entités (E-06), gestes hors
   `CommandRegistry` (E-14), `EditorLayer` et `Viewport` à plus de 3 000 lignes chacun (E-07).
5. **Reliquats de sous-systèmes** : barrières globales autour de chaque transfert Vulkan (B-01), un `alGenSources` par
   lecture et pas de streaming (D-13), écriture yaml-cpp à 20-24 µs par entité (C-10, P-07).

### Les 5 bizarreries

1. **Deux bibliothèques YAML** : rapidyaml lit, yaml-cpp écrit et fusionne (A-22, G-20).
2. **Jolt fait la 2D** avec des corps 3D verrouillés dans un plan, et deux mécanismes de déclencheurs coexistent (P-12,
   D-14).
3. **Le `.owlpack` est lu en mémoire mais seulement XORé**, sans somme de contrôle par entrée (D-10).
4. **Deux images de build**, sans tag ni digest côté CI : « vert en local » ne garantit pas « vert en CI » (H-09).
5. **Zéro warning sous `-Weverything -Werror`**, mais chaque nouveau Clang casse le build (G-11) ; un moteur mermaid
   maison de 1 317 lignes tourne au configure (G-21).

### Défauts à corriger en priorité

Ordre : exposition × gravité, puis coût ; constats de gravité moyenne qui appellent une correction.

| Rang | Constats | Symptôme                                                         | Exposition                       | Effort |
|------|----------|------------------------------------------------------------------|----------------------------------|--------|
| 1    | H-06     | ASan ne tourne que sur `main`                                    | chaque fusion                    | S      |
| 2    | H-13     | Nocturne rouge 4 fois sur 6, échecs cumulés                      | arm64, TSan, bench               | S      |
| 3    | P-12     | Pas 2D Jolt 3 à 6 fois Box2D sur un thread                       | jeux 2D physiques denses         | S-M    |
| 4    | P-13     | Hash du monde en YAML : 28 µs par corps, frame enregistrée +37 % | tout Play dans l'éditeur, replay | M      |
| 5    | P-14     | Volée de 1 000 agents : 9,8 ms par frame pour 2 ms               | IA de groupe dense               | S      |
| 6    | F-02     | Éditeur (58 600 lignes) hors couverture                          | toute régression de l'éditeur    | S      |
| 7    | E-06     | Pas de sélection multiple ni de copier/coller d'entités          | tout travail d'édition de scène  | M      |
| 8    | B-01     | Deux barrières globales autour de chaque transfert Vulkan        | uploads et copies par frame      | M      |

## 2. Comptes par axe

98 constats ouverts : 16 fermés depuis le 2026-10-20 (B-04, B-06, B-19, B-20, D-07, D-09, D-25, D-26, F-09, H-08, G-09,
G-15, G-17, G-19, I-02, I-03), 19 nouveaux (A-22, B-26 à B-28, E-13, E-14, D-30 à D-33, F-14, H-13, G-20, G-21, G-23,
I-11, P-12 à P-14), les autres revérifiés et mis à jour. L'axe P regroupe `20-mesures.md` ; l'axe J (configuration IA) est au
§12 de `00-cadrage.md`, sans ID.

| Axe                | Constats | Forces | Faiblesses | Risques | Étrangetés | Haute | Moyenne | Basse  |
|--------------------|----------|--------|------------|---------|------------|-------|---------|--------|
| A Architecture     | 14       | 4      | 8          | 0       | 2          | 2     | 7       | 5      |
| B Rendu            | 13       | 6      | 5          | 0       | 2          | 0     | 6       | 7      |
| C Scène, ECS       | 8        | 2      | 4          | 0       | 2          | 0     | 3       | 5      |
| D Sous-systèmes    | 18       | 9      | 6          | 0       | 3          | 0     | 7       | 11     |
| E Éditeur          | 8        | 3      | 5          | 0       | 0          | 0     | 5       | 3      |
| F Qualité          | 7        | 2      | 4          | 0       | 1          | 0     | 4       | 3      |
| G Build, packaging | 7        | 2      | 0          | 0       | 5          | 0     | 3       | 4      |
| H CI               | 8        | 3      | 4          | 0       | 1          | 0     | 4       | 4      |
| I Documentation    | 5        | 1      | 2          | 0       | 2          | 0     | 1       | 4      |
| P Mesures          | 10       | 2      | 8          | 0       | 0          | 0     | 4       | 6      |
| **Total**          | **98**   | **34** | **46**     | **0**   | **18**     | **2** | **44**  | **52** |

Lecture :

- Les deux constats de gravité haute sont A-02 (faiblesse) et A-06 (force).
- **Doublons à traiter ensemble** : A-02 ≈ C-14 ≈ B-26 ; A-08 ≈ D-15 ; A-22 ≈ G-20 ; C-10 ≈ P-07 ; D-22 ≈ P-11.

## 3. Plan d'action

Chantiers : [`40-avenir.md`](40-avenir.md) ; versions : `doc/pages/roadmap.md`.

| Version / horizon | Chantiers                    | Constats couverts                                     | Contenu                                                                             | Effort |
|-------------------|------------------------------|-------------------------------------------------------|-------------------------------------------------------------------------------------|--------|
| v0.8.0 (reliquat) | K-01, K-02, K-05, K-38, K-40 | C-12, E-12, F-02, H-06, H-13, H-09, P-12 à P-14, D-14 | Défauts reproduits, éditeur couvert, CI, image, coût du pas, du hash et de la volée | S-M    |
| Avant v0.8.0      | K-10, K-18, K-27             | A-05, B-01, B-25, P-11                                | Registre de types, Vulkan, mémoire Luau                                             | M-L    |
| v0.8.0            | K-35, K-37                   | B-16, D-10                                            | Android, Web (WebGPU), modding                                                      | XL     |
| Avant v0.9.0      | K-07, K-08, K-09             | C-09, A-07, A-08, A-02, B-26                          | Cycle de vie, monde par scène, `Scene` découpée                                     | L      |
| v0.9.0            | K-34, K-36                   | A-05, A-12                                            | Réseau, SDK                                                                         | XL     |
| Au fil des PR     | K-15, K-16, K-28, K-29, K-39 | E-05, E-06, E-14, C-11, D-13, I-06                    | Undo, prefabs, voxel, audio, doc                                                    | M      |

Le calendrier ambitieux est gardé, les versions sont pilotées par leurs fonctionnalités (décision du 2026-11-12). [O]
K-08 et K-10 conditionnent pourtant le modding et le réseau : à glisser dans v0.8.0 et v0.9.0 (`40-avenir.md` §5).

## 4. Décisions

Prises par le mainteneur le 2026-11-12 :

| # | Question                        | Décision                                                                 | Constats      |
|---|---------------------------------|--------------------------------------------------------------------------|---------------|
| 1 | Modèle Luau                     | un `lua_State` par entité, gardé pour l'isolation                        | P-11, D-22    |
| 2 | Erreurs Luau                    | exceptions C++ au lieu de `longjmp`                                      | K-27          |
| 3 | Physique 2D                     | Jolt gardé ; régler le pas 2D et réviser la cible                        | P-12, fiche 6 |
| 4 | Règle de fusion                 | pipeline de MR optimisé, relancé au passage en prêt sur les builds verts | H-06          |
| 5 | Presets annoncés non construits | MinGW GCC, `linux-clang-minimal`, `linux-gcc-release` en jobs manuels    | K-05          |
| 6 | Calendrier v0.8.0 à v1.0.0      | gardé ; versions pilotées par leurs fonctionnalités                      | 40-avenir §5  |
| 7 | Origine flottante en 3D         | décalage 3D et positions d'IA portés par l'origine flottante             | K-01          |

Encore ouvertes :

| #  | Question                                                     | Options                                                                | Constats         |
|----|--------------------------------------------------------------|------------------------------------------------------------------------|------------------|
| 8  | Le gameplay et le frame graph restent-ils dans `Scene` ?     | sortir vers des systèmes et les couches de la pile ; garder            | A-02, B-26, C-14 |
| 9  | Visibilité de la CI pour un contributeur GitHub              | statuts GitLab publiés sur GitHub ; GitHub Actions minimal ; statu quo | fiche 9, K-38    |
| 10 | YAML comme format interne (undo, prefab, copie)              | émetteur rapidyaml ; binaire pour l'undo ; statu quo                   | A-22, C-10, P-07 |
| 11 | Exigence Doxygen sur les membres triviaux                    | alléger (API publique seulement) ; garder                              | I-06             |
| 12 | `-Werror` sous `-Weverything` pour tous ou seulement en CI ? | option `OWL_WARNINGS_AS_ERRORS` en CI ; garder                         | G-11             |

## 5. L'IA dans ce dépôt

### État

Configuration décrite au §12 de [`00-cadrage.md`](00-cadrage.md) : `CLAUDE.md` de 127 lignes, quinze règles ciblées par
chemin, `docker_run.sh`, skills `build`, `test`, `coverage`, `check`, `add-test`, `add-dependency`, mémoire réduite à un
index de pointeurs. La revue du 2026-11-12 a confié la vérification des constats à deux sous-agents (axes A à C/E, D à
G/I) et la documentation à un troisième ; l'orchestrateur a refait les mesures et la synthèse.

### Ce que l'audit révèle sur la configuration

- **Une intuition fausse oriente le travail** : seule la mesure tranche (P-11 et P-12 contredisent « Luau plus rapide »
  et « Jolt aussi rapide que Box2D en 2D ») ; le mainteneur a gardé les deux choix en connaissance du coût.
- **Les mesures de la CI ne remplacent pas le protocole** : la baseline du runner de bench corrobore, elle ne se
  compare pas aux chiffres de `20-mesures.md` (autre machine).
- **Le harnais injecte l'instantané de début de session** : une consigne modifiée en cours de session ne prend effet
  qu'à la suivante.

### Ce qui reste à faire

| Action                                                                                       | Source             | Effort |
|----------------------------------------------------------------------------------------------|--------------------|--------|
| Tester un hook `PostToolUse` lançant `clang-format` en Docker sur chaque fichier C++ édité   | 00-cadrage §12.3.1 | S      |
| Évaluer le MCP CLion (`get_file_problems`, `search_symbol`) face à grep + clang-tidy         | 00-cadrage §12.3.2 | S      |
| Relever les règles réellement chargées par les agents et élaguer celles qui n'ont pas servi  | 00-cadrage §12.3.4 | S      |
| Ajouter à l'image `heaptrack`, `renderdoccmd`, `hyperfine`                                   | 00-cadrage §11.3   | S      |
| Aligner l'image locale et l'image CI (un tag ou un digest commun)                            | H-09               | S      |
| Séparer au changelog les réglages des jeux d'exemple des changements du moteur               | I-08               | S      |
| Remplacer `box2d.org` par `luau.org` dans les domaines `WebFetch` de `.claude/settings.json` | revue 2026-11-12   | S      |
