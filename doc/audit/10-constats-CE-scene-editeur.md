# Audit Owl — constats des axes C (scène, ECS) et E (éditeur Owl Nest)

> Périmètre : `source/owl/{public,private}/scene/**`, `source/owlnest/` (dont `EditorLayer.cpp`, 1 321 lignes).
> Revu le 2026-10-20 sur `main` 4e1a6a10. Lecture seule. « confirmé » = vérifié dans le code ou par une sonde ;
> « mesuré » = 1 exécution sur la machine d'audit (32 threads), ordre de grandeur sans dispersion. Comparaisons
> externes de mémoire, non revérifiées (`30-etat-de-l-art.md`). Recoupements axe A : A-02, A-05. Contexte :
> `00-cadrage.md`.

## Résumé

- Le YAML est la monnaie universelle (scène, prefab, undo, inspecteur) ; chargement coûteux : 1,49 s pour 10 000
  entités (C-10).
- Scène : `Scene::copy` et index UUID rapides (C-16), cache de transforms monde bien conçu (C-15), hiérarchie par UUID
  haché (C-09) ; `Scene` porte encore Victory/Death (C-14).
- Dettes locales : `Version` de prefab figée (C-11), repli par nom des liens (C-12), re-parse YAML des sauvegardes (C-13).
- Éditeur : inspection exhaustive des 31 composants (E-10) et undo générique (E-09), mais pas de sélection multiple ni
  de copier/coller (E-06) ; `EditorLayer.cpp` agrégateur (E-07), transition de niveau dupliquée (E-12), snapshots
  d'undo coûteux en mémoire (E-05).

## Constats

14 constats : 8 faiblesses, 2 étrangetés (C-09, C-14), 4 forces (C-15, C-16, E-09, E-10) ; 6 moyennes, 8 basses.

### Gravité moyenne

**C-10** · faiblesse · moyenne · confirmé (mesuré)
Le YAML est lisible et diffable, mais ~150 µs par entité simple au chargement plafonnent un niveau à quelques
milliers d'entités pour une transition rapide.
Preuve : entités Tag+Transform+Visibility+Hierarchy+SpriteRenderer, 1 essai, non remesuré : N = 1 000 : sérialisation
30 ms (277 Kio), chargement 126 ms ; N = 10 000 : 282 ms (2,7 Mio), 1 492 ms.
Reco : surveiller (format binaire cuit pour le runtime et les `.owlpack`, M, avec l'axe K).

**E-06** · faiblesse · moyenne · confirmé
Contre la règle *Editor Coverage* (`.claude/rules/ongoing-quality.md`) : sélection unique, donc ni déplacement ni
suppression de groupe, et pas de copier/coller d'entités (la duplication existe).
Preuve : `SceneHierarchy.h` (`scene::Entity m_selection;`), aucun copy/paste/clipboard dans `source/owlnest/sources`.
Reco : corriger, M (sélection en `std::vector<UUID>`, commande composite : `CompositeCommand` existe dans
SceneFlowCommands ; copier/coller par commande).

**C-15** · force · moyenne · confirmé (mesuré)
`getWorldTransform` recalcule la chaîne hors frame (éditeur, gizmo, tests) mais, pendant le rendu, un seul parcours
préfixe alimente un cache dont les fenêtres de validité sont documentées. Owl recalcule tout à chaque frame, sans
dirty flag incrémental.
Preuve : Scene.cpp (`prepareWorldTransforms`, `getWorldTransform`), Scene.h (`m_inUpdatePass`,
`m_worldTransformCacheActive`) ; sans cache, profondeur ≤ 16 : 1 000 entités 1,0 ms, 10 000 9,2 ms (≈ 0,9 µs/appel) ;
`isEffectivelyVisible` 10 000 : 2,1 ms.
Reco : garder ; surveiller le parcours complet à grande échelle.

**C-16** · force · moyenne · confirmé (mesuré)
Le passage au Play copie par storage sans YAML (~0,4 µs/entité) ; l'index UUID est O(1) et auto-réparant ; la
séparation parse/apply prépare un chargement asynchrone.
Preuve : Scene.cpp:275-298, 2093-2108 ; `SceneSerializer.cpp:113-180` ; `SceneDocument.cpp:169-175` ;
`LuaScript.h:28-50` ; `Scene::copy` 1 000 : 0,40 ms, 10 000 : 4,1 ms.
Reco : garder.

**E-09** · force · moyenne · confirmé
`UndoManager<Target>` générique réutilisé par les documents NodeGraph, Tilemap, Tileset, Animation ; undo refusé hors
mode Edit ; sélection restaurée par UUID ; fusion des glisser continus ; delta pour le voxel. Le défaut résiduel (E-05)
tient au snapshot, pas au modèle.
Preuve : `UndoManager.h`, `SceneDocument.cpp:242-260`, `UndoCommand.h`, `ComponentCommands.cpp:72-88`, `VoxelCommands`.
Reco : garder ; ajouter un « begin/end group » pour les opérations multiples (E-06).

**E-10** · force · moyenne · confirmé
Chaque propriété authorable des 31 composants est inspectable, éditable et annulable ; seuls des états runtime ne
sont pas exposés. Tilemap, tileset, animation, graphe de nœuds et voxel ont des documents dédiés.
Preuve : `public/gui/component/render.h:240-249`, `render.cpp:173-1133` ; comparaison scriptée champs publics /
`renderProps` ; undo `SceneHierarchy.cpp:644-657`, gizmo `Viewport.cpp:641-690`, picking `Viewport.cpp:283`.
Reco : garder ; combler les manques structurels de E-06.

### Gravité basse

**C-09** · étrangeté · basse · confirmé
EnTT 3.15.0 sert de magasin interrogé par vues (1 seul `registry.group`, 8 signaux) ; `entt::registry registry` est
public et accédé directement ; la hiérarchie (`parentId` + `vector<UUID>`) passe par des UUID hachés, ce qui simplifie
sérialisation et undo.
Preuve : `Hierarchy.h`, Scene.h, PhysicCommand, LuaBindings, SceneTrigger, render.cpp.
Reco : surveiller, M (UUID en sérialisation + cache `entt::entity` du parent résolu au chargement et à `setParent`).

**C-11** · faiblesse · basse · confirmé
Le champ `Version` d'un prefab vaut toujours 1 : `syncedVersion` n'a aucun effet de versionnage.
Preuve : `PrefabSerializer.cpp:151, 250, 460`.
Reco : surveiller, S (incrémenter à la modification ou retirer le champ).

**C-12** · faiblesse · basse · confirmé
Un lien dont l'UUID ne se résout pas retombe sur un nom (Tag) non unique : renommer ou dupliquer la cible casse le
lien en silence.
Preuve : `EntityLink.h` (`linkedEntityName`), Scene.cpp:1505.
Reco : corriger, S (supprimer le repli ou le limiter à un avertissement unique).

**C-13** · faiblesse · basse · confirmé (code)
Une sauvegarde copie toute la scène, plus GameState et vitesses ; chaque composant subit un parse YAML
supplémentaire ; l'état Lua local n'est pas sauvé (cohérent avec la doc).
Preuve : `SaveManager.cpp:122, 129`, `SaveManager.h:117`, `SettingsManager.h:201-205`.
Reco : corriger, S (parse unique) ; surveiller, M (sauvegarde différentielle).

**C-14** · étrangeté · basse · confirmé
Les systèmes sont enregistrés par phase, mais les règles d'un jeu de démonstration (statuts Victory/Death et leurs
textes) restent dans le moteur ; voir A-02.
Preuve : Scene.cpp:557-585, `SystemSchedule.h`.
Reco : surveiller, M (déplacer Victory/Death dans le jeu ou le runner).

**E-05** · faiblesse · basse · confirmé (code)
Le snapshot YAML d'entité (avant/après par commande, profondeur 100) fait croître l'historique avec la taille des
entités : un monde voxel édité est stocké 2 fois par commande (hors `VoxelEditCommand`, en delta).
Preuve : `UndoManager.h`, `ComponentCommands.h` (`EntitySnapshot`).
Reco : corriger, M (snapshot par composant, la clé YAML suffit).

**E-07** · faiblesse · basse · confirmé
`EditorLayer` (1 321 lignes, 55 méthodes) agrège ce qui n'a pas de panneau ; la règle `.claude/rules/editor.md`
limite sa croissance.
Preuve : `source/owlnest/sources/EditorLayer.cpp`.
Reco : corriger, M (extraire les blocs sans dépendance croisée forte).

**E-12** · faiblesse · basse · confirmé (code)
La résolution de niveau et la téléportation existent en double (éditeur et jeu livré) : divergence possible.
Preuve : `SceneDocument.cpp`, `source/owlnest/runner/RunnerLayer.cpp:732-753`.
Reco : corriger, S (service de chargement de niveau dans le moteur).

## Points de mesure (agent benchmark)

Fonctions appelables sans GPU (sauf `prepareWorldTransforms` et le rendu) ; un test se compile contre
`libOwlEngine.so` avec les drapeaux de `owl_scene_tests_unit_test`. Tailles suggérées : N = 1k, 10k, 100k, profondeur
1/16/64, arbre large. Chiffres de l'audit (1 essai) :
`Scene::getWorldTransform` 10k : 9,2 ms ; `isEffectivelyVisible` 10k : 2,1 ms ; `Scene::copy` 10k : 4,1 ms ;
`SceneSerializer::serializeToString` 10k : 282 ms ; chargement (`parseBuffer` puis `applyParsed`) 10k : 1 492 ms ;
`serializeEntityToString` : 0,06-0,07 ms. Non mesurés : `prepareWorldTransforms` (requiert un renderer),
restauration undo (`SubtreeSnapshot::restore`), `PrefabSerializer::instantiate/applyToInstance`, `findEntityByUUID`
(indexé, échec, après destroy), `onUpdateRuntime(ts, false)`, `updateEntityLinks`, `SaveManager::save/load`,
`onStartRuntime` (déjà journalisé par phase).

## Pistes pour l'avenir

1. **Références et assets par GUID** : `EntityRef` (UUID) à la place des noms, prefabs par GUID d'asset (C-12).
2. **Gameplay hors de `Scene`** : Victory/Death en systèmes enregistrés par phase (C-14, A-02, A-05).
3. **Transforms incrémentaux** : dirty flag par sous-arbre ou tri topologique ; à mesurer d'abord (C-15).
4. **Undo par composant** et regroupement de commandes : base de la sélection multiple (E-05, E-06).
5. **Forme binaire cuite** (ou `entt::snapshot`) pour le runtime et les `.owlpack`, YAML restant la source (C-10).
6. **Prefabs de seconde génération** : imbrication, variants, document isolé ; L, après 1 et 4 (C-11).
7. **Découpage d'`EditorLayer`** dans une PR dédiée (E-07).
