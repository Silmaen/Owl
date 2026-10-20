# Audit du dépôt Owl

Audit en profondeur du dépôt lancé le 2026-10-05 : bilan forces / faiblesses / bizarreries, mesures de
performance du moteur, confrontation à l'état de l'art, avenir du moteur, configuration IA. Les livrables
sont en français et vivent dans [`doc/audit/`](doc/audit/), exclu de Doxygen et de codespell.

**Statut** : audit revu le 2026-10-20 sur `main` 4e1a6a10 ; les pages se limitent à ce qui reste vrai.

**Par où commencer** : [`90-synthese.md`](doc/audit/90-synthese.md) (verdict, forces, faiblesses, défauts à corriger,
plan d'action par version, décisions ouvertes), puis [`40-avenir.md`](doc/audit/40-avenir.md) pour la trajectoire.

| Document                                                                       | Contenu                                                                             | Statut          |
|--------------------------------------------------------------------------------|-------------------------------------------------------------------------------------|-----------------|
| [`00-cadrage.md`](doc/audit/00-cadrage.md)                                     | Objectifs, axes, méthode, règles, format, environnement Docker, configuration IA.   | Revu 2026-10-20 |
| [`10-constats-A-architecture.md`](doc/audit/10-constats-A-architecture.md)     | Axe A : modules, dépendances, singletons, ABI (13 constats).                        | Revu 2026-10-20 |
| [`10-constats-B-rendu.md`](doc/audit/10-constats-B-rendu.md)                   | Axe B : backends GPU, synchronisation Vulkan, Renderer2D, Slang (14).               | Revu 2026-10-20 |
| [`10-constats-CE-scene-editeur.md`](doc/audit/10-constats-CE-scene-editeur.md) | Axes C et E : scène, ECS, prefab, sauvegarde, éditeur, undo (14).                   | Revu 2026-10-20 |
| [`10-constats-D-sous-systemes.md`](doc/audit/10-constats-D-sous-systemes.md)   | Axe D : physique, script, son, tâches, voxel, pack, outillage (19).                 | Revu 2026-10-20 |
| [`10-constats-FH-qualite-ci.md`](doc/audit/10-constats-FH-qualite-ci.md)       | Axes F et H : tests, couverture, sanitizers, clang-tidy, CI (16).                   | Revu 2026-10-20 |
| [`10-constats-GI-build-docs.md`](doc/audit/10-constats-GI-build-docs.md)       | Axes G et I : build, dépendances, packaging, documentation (13).                    | Revu 2026-10-20 |
| [`20-mesures.md`](doc/audit/20-mesures.md)                                     | Protocole et résultats du harnais `bench/` (7 constats P).                          | Revu 2026-10-20 |
| [`30-etat-de-l-art.md`](doc/audit/30-etat-de-l-art.md)                         | Douze fiches de comparaison sourcées et datées.                                     | Revu 2026-10-20 |
| [`40-avenir.md`](doc/audit/40-avenir.md)                                       | Axe K : carte des chantiers, dépendances, horizons, correspondance avec la roadmap. | Revu 2026-10-20 |
| [`90-synthese.md`](doc/audit/90-synthese.md)                                   | Synthèse : verdict, comptes, plan d'action par version, décisions, usage de l'IA.   | Revu 2026-10-20 |

Pour un agent qui reprend l'audit : lire `00-cadrage.md` en entier (§8 règles d'engagement, §9 format),
puis lancer toute commande via `./docker_run.sh`.
