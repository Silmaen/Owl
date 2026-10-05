# Audit du dépôt Owl

Audit en profondeur du dépôt lancé le 2026-10-05 : bilan forces / faiblesses / bizarreries, mesures de
performance du moteur, confrontation à l'état de l'art, avenir du moteur, configuration IA. Les livrables
sont en français et vivent dans [`doc/audit/`](doc/audit/), exclu de Doxygen et de codespell.

**Par où commencer** : [`90-synthese.md`](doc/audit/90-synthese.md) (verdict, forces, faiblesses, défauts à corriger,
plan d'action en PR, décisions ouvertes), puis [`40-avenir.md`](doc/audit/40-avenir.md) pour la trajectoire.

| Document                                                                       | Contenu                                                                     | Statut                     |
|--------------------------------------------------------------------------------|-----------------------------------------------------------------------------|----------------------------|
| [`00-cadrage.md`](doc/audit/00-cadrage.md)                                     | Objectifs, axes, méthode, règles pour les agents, format.                   | v3, validé                 |
| [`01-environnement.md`](doc/audit/01-environnement.md)                         | Toolchain Docker, outillage, ce qui manque à l'image.                       | Validé                     |
| [`02-config-ia.md`](doc/audit/02-config-ia.md)                                 | Configuration IA : avant, changements faits, suite.                         | Appliqué, suite ouverte    |
| [`10-constats-A-architecture.md`](doc/audit/10-constats-A-architecture.md)     | Axe A : modules, dépendances, singletons, ABI (21 constats).                | Contre-vérifié             |
| [`10-constats-B-rendu.md`](doc/audit/10-constats-B-rendu.md)                   | Axe B : backends GPU, synchronisation Vulkan, Renderer2D, Slang (25).       | Contre-vérifié             |
| [`10-constats-CE-scene-editeur.md`](doc/audit/10-constats-CE-scene-editeur.md) | Axes C et E : scène, ECS, prefab, sauvegarde, éditeur, undo (30).           | Contre-vérifié             |
| [`10-constats-D-sous-systemes.md`](doc/audit/10-constats-D-sous-systemes.md)   | Axe D : physique, script, son, tâches, voxel, pack, outillage (29).         | Contre-vérifié             |
| [`10-constats-FH-qualite-ci.md`](doc/audit/10-constats-FH-qualite-ci.md)       | Axes F et H : tests, couverture, sanitizers, clang-tidy, CI (25).           | Contre-vérifié             |
| [`10-constats-GI-build-docs.md`](doc/audit/10-constats-GI-build-docs.md)       | Axes G et I : build, dépendances, packaging, documentation (29).            | Contre-vérifié             |
| [`20-mesures.md`](doc/audit/20-mesures.md)                                     | Protocole et résultats du harnais `bench/` (14 constats P).                 | Première campagne, vérifié |
| [`30-etat-de-l-art.md`](doc/audit/30-etat-de-l-art.md)                         | Douze fiches de comparaison sourcées et datées.                             | v1                         |
| [`40-avenir.md`](doc/audit/40-avenir.md)                                       | Axe K : carte des chantiers, dépendances, horizons, écart à la roadmap.     | v1, à arbitrer             |
| [`90-synthese.md`](doc/audit/90-synthese.md)                                   | Synthèse : verdict, comptes, plan d'action en PR, décisions, usage de l'IA. | v1, à arbitrer             |

Pour un agent qui reprend l'audit : lire `00-cadrage.md` en entier (§8 règles d'engagement, §9 format),
puis lancer toute commande via `docker/run.sh`.

L'audit a été mené sur `main` plus le commit « Upgrade clang-tidy on diff » de `Feature/TidyDiff`, retiré depuis
de cette branche : H-03 (action `ClangTidy` mono-processus) et F-06 (test de version figé sur 0.2.2) portent sur
`Feature/TidyDiff`, pas sur `main`.
