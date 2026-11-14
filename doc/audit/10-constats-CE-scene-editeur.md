# Audit Owl — constats des axes C (scène, ECS) et E (éditeur Owl Nest)

> Périmètre : `source/owl/{public,private}/scene/**`, `source/owlnest/` (dont `EditorLayer.cpp`, 1 843 lignes).
> Revu le 2026-11-12 sur `Feature/sample-games-new` b4281d72. Lecture seule. « confirmé » = vérifié dans le code ;
> « mesuré » = baseline `test/bench/baseline/linux-bench.json` (15 échantillons, médiane), sauf mention. Comparaisons
> externes de mémoire, non revérifiées (`30-etat-de-l-art.md`). Recoupements axe A : A-02, A-05, A-22. Contexte :
> `00-cadrage.md`.

## Résumé

- Le YAML reste la monnaie universelle ; la lecture (rapidyaml) est passée à 12,6 µs/entité, l'écriture (yaml-cpp)
  est devenue le goulot : 54 µs/entité (C-10).
- Scène : cache de transforms monde persistant et parallèle (C-15), `Scene::copy` et index UUID rapides (C-16),
  hiérarchie par UUID haché (C-09) ; `Scene` porte encore Victory/Death (C-14).
- Dettes locales : `Version` de prefab figée (C-11), repli par nom des liens (C-12), re-parse YAML des sauvegardes (C-13).
- Éditeur : registre de ~81 commandes nommées (E-13), undo générique groupable dans 17 documents (E-09), inspecteur sur
  79 des 83 composants (E-10) ; mais toujours pas de sélection multiple ni de copier/coller d'entités (E-06),
  `EditorLayer` et `Viewport` grossissent (E-07), des gestes contournent encore le registre (E-14).

## Constats

16 constats : 9 faiblesses, 2 étrangetés (C-09, C-14), 5 forces (C-15, C-16, E-09, E-10, E-13) ;
8 moyennes, 8 basses.

### Gravité moyenne

**C-10** · faiblesse · moyenne · confirmé (mesuré)
Lecture rapide (rapidyaml) et forme binaire disponible, mais l'écriture yaml-cpp coûte 4 fois la lecture : elle pèse
sur la sauvegarde, l'export et les snapshots d'undo.
Preuve : bench `serialize/*` (scène sprites en forêt, 2,9 Mio à 10 000 entités) : `scene_to_string` 1 000 : 54,8 ms,
10 000 : 542,6 ms ; `scene_from_string` 10 000 : 125,8 ms (dont `scene_parse_only` 91,8 ms) ; `scene_from_binary`
10 000 : 62,2 ms (2,2 Mio) ; `test/bench/cases/SerializeBench.cpp:35-65`.
Reco : corriger, M (émetteur sur rapidyaml ou binaire, cf. A-22).

**C-15** · force · moyenne · confirmé (mesuré)
Les transforms monde vivent dans un cache persistant par nœud (tampon de version, parent résolu en `entt::entity`) :
chaque frame parcourt tout l'arbre mais ne recalcule que les nœuds dont le local ou le parent a changé, en parallèle
au-delà de 4 096 entités. Hors cache, une chaîne profonde reste en O(profondeur) par appel.
Preuve : Scene.cpp:3783-3860 (`getWorldTransform`, `refreshWorldNode`, `prepareWorldTransforms`), 3960-4030 ;
bench `scene/prepare_world_transforms/flat10000` 0,24 ms, `forest10000` 0,48 ms ; `scene/world_transform/chain1000_all`
13,6 ms ; `frame/editor_update/moving_forest50000` 7,4 ms.
Reco : garder ; surveiller les chaînes profondes.

**C-16** · force · moyenne · confirmé (mesuré)
Le passage au Play copie par storage sans YAML (~0,7 µs/entité) ; l'index UUID est O(1) et auto-réparant ; la
séparation parse/apply sert au chargement asynchrone des niveaux dans le runner.
Preuve : Scene.cpp:319, 3647-3657 ; `SceneSerializer.cpp:530-600` ; `RunnerLayer.cpp:809-833` ; bench `scene/copy/10000`
6,8 ms, `scene/find_entity_by_uuid/10000` 0,45 ms (10 000 recherches).
Reco : garder.

**E-06** · faiblesse · moyenne · confirmé
Contre la règle *Editor Coverage* (`.claude/rules/ongoing-quality.md`) : sélection d'entité unique, donc ni
déplacement ni suppression de groupe, et pas de copier/coller d'entités (la duplication existe). Les canevas de
graphes (matériaux, scripts visuels) ont, eux, sélection multiple et copier/coller.
Preuve : `panel/SceneHierarchy.h:96, 110, 215` (`scene::Entity m_selection;`) ; `SceneHierarchy.cpp:742-744`
(Duplicate) ; changelog 0.6 (canevas).
Reco : corriger, M (sélection en `std::vector<UUID>`, `beginGroup` existe ; copier/coller par commande du registre).

**E-07** · faiblesse · moyenne · confirmé
`EditorLayer` a grossi malgré la règle `.claude/rules/editor.md` (1 843 l. + header 1 438 l., 73 méthodes, plus 1 707 l.
dans 4 fichiers `EditorLayer*.cpp`) ; `Viewport.cpp` (3 447 l.) est devenu le plus gros fichier de l'éditeur.
Preuve : `source/owlnest/sources/EditorLayer.cpp`, `EditorLayer.h`, `EditorLayerProject.cpp` (1 138),
`panel/Viewport.cpp`.
Reco : corriger, M (extraire les blocs sans dépendance croisée ; outils du viewport par mode).

**E-09** · force · moyenne · confirmé
`UndoManager<Target>` générique réutilisé par 17 des 22 documents ; undo refusé hors mode Edit ; sélection restaurée par
UUID ; fusion des glisser continus ; groupes (`beginGroup` / `UndoGroupCommand`) ; delta pour le voxel. Le défaut
résiduel (E-05) tient au snapshot, pas au modèle.
Preuve : `UndoManager.h:28-94, 147, 293-316, 391`, `SceneDocument.cpp:419-423`, `commands/VoxelCommands.cpp`.
Reco : garder.

**E-10** · force · moyenne · confirmé (overloads), exhaustivité champ à champ non revérifiée
79 des 83 composants ont un panneau d'inspecteur (les 4 autres sont structurels : Tag, ID, Hierarchy, NativeScript) ;
tilemap, tileset, animation, matériau, script visuel, arbre de comportement, séquence, dialogue, son ont des documents
dédiés.
Preuve : comparaison `isComponent` (`components.h`) / `renderProps` (`public/gui/component/render.h`, 81 overloads) ;
`private/gui/component/render.cpp` (5 302 l.) ; `source/owlnest/sources/document/` (22 documents).
Reco : garder ; combler E-06.

**E-13** · force · moyenne · confirmé
Les gestes d'édition passent par un `CommandRegistry` de ~81 commandes nommées, validées et typées, partagé par la
console, la palette, le journal, les macros, le replay, les scénarios et le serveur MCP.
Preuve : `commands/CommandRegistry.cpp:1936-2101` (`add`, `validate`, `execute`, `parseArg`, `resolveEntity`) ;
`panel/ConsolePanel.cpp:36-38` (sources) ; `mcp/McpDispatcher.cpp`, `mcp/McpTools.cpp`, `commands/Macro.cpp`.
Reco : garder.

### Gravité basse

**C-09** · étrangeté · basse · confirmé
EnTT 4.0.0 sert de magasin interrogé par vues (1 seul `group`, ~79 connexions de signaux) ; `entt::registry registry`
est public et manipulé directement par 46 fichiers du moteur et 21 de l'éditeur ; la hiérarchie
(`parentId` + `vector<UUID>`) passe par des UUID hachés, le cache monde résout le parent en `entt::entity` (C-15).
Preuve : `component/Hierarchy.h:27-29`, Scene.h:1003, `conanfile.py:125`.
Reco : surveiller (accès au registre derrière `Scene` à mesure des découpages, A-02).

**C-11** · faiblesse · basse · confirmé
Le champ `Version` d'un prefab est toujours écrit à 1 : `syncedVersion` n'a aucun effet de versionnage, mais
l'inspecteur l'affiche.
Preuve : `PrefabSerializer.cpp:155, 239, 480` ; `component/PrefabLink.h:32` ; `gui/component/render.cpp:2978`.
Reco : surveiller, S (incrémenter à la modification ou retirer le champ).

**C-12** · faiblesse · basse · confirmé
Un lien renommé tient (le nom suit l'UUID), mais une cible supprimée retombe sur la première entité homonyme, dont
l'UUID est adopté en silence ; un lien perdu refait un parcours linéaire des tags à chaque frame.
Preuve : Scene.cpp:3517-3537 (`resolveEntityLink`) ; `systems/EntityLinkSystem.cpp:50-60` ; `EntityLink.h:27-30`.
Reco : corriger, S (repli par nom réservé à la migration, `EntityLinkMigration.cpp`).

**C-13** · faiblesse · basse · confirmé (code)
Une sauvegarde sérialise la scène en texte puis la re-parse (`YAML::Load`) pour l'imbriquer, idem pour l'état du HUD ;
l'état Lua local n'est pas sauvé (cohérent avec la doc).
Preuve : `SaveManager.cpp:178-209` ; `SaveManager.h:79, 129`.
Reco : corriger, S (émettre directement dans le document de sauvegarde) ; surveiller, M (sauvegarde différentielle).

**C-14** · étrangeté · basse · confirmé
Les systèmes sont enregistrés par phase, mais les règles d'un jeu de démonstration (statuts Victory/Death, textes
« Victory! » / « You loose! » dessinés par `Scene`) restent dans le moteur ; voir A-02.
Preuve : Scene.h:1011-1012, Scene.cpp:653-657, 847-880.
Reco : surveiller, M (déplacer Victory/Death dans le jeu ou le runner).

**E-05** · faiblesse · basse · confirmé (code)
Le snapshot YAML d'entité entière (avant/après par commande, profondeur 100) fait croître l'historique avec la taille
des entités : un monde voxel édité hors `VoxelEditCommand` est stocké deux fois par commande.
Preuve : `EntitySnapshot.h:23-27`, `commands/ComponentCommands.h:81-83, 150-152, 251-253`, `UndoManager.h:391`.
Reco : corriger, M (snapshot par composant : `serializeComponentToString` existe, `SceneSerializer.cpp:800-807`).

**E-12** · faiblesse · basse · confirmé (code)
Le service `LevelTransition` est partagé, mais l'orchestration diverge : le runner lit et parse le niveau en tâche de
fond, l'éditeur en synchrone ; le Play de l'éditeur ne teste pas le chemin asynchrone du jeu livré.
Preuve : `public/scene/LevelTransition.h` ; `document/SceneDocument.cpp:365-394` ; `runner/RunnerLayer.cpp:803-833`.
Reco : corriger, S (une seule orchestration dans le moteur, asynchrone des deux côtés).

**E-14** · faiblesse · basse · confirmé
Des gestes poussent encore directement dans l'`UndoManager` sans commande nommée (règle *Every Edit Gesture Is an
Editor Command*) : routage vers une couche et mise à jour / retour au prefab dans la hiérarchie, pinceau voxel du
viewport, édition de tilemap.
Preuve : `panel/SceneHierarchy.cpp:583, 757, 765` ; `panel/Viewport.cpp:2564` ; `document/TilemapDocument.cpp:449,
655, 1237`.
Reco : corriger, S (migrer ces gestes au fil des PR qui les touchent).

## Points de mesure (agent benchmark)

Couverts par `test/bench` (baseline `linux-bench`) : `scene/*` (copie, création, recherche UUID, transforms monde,
`setParent`), `serialize/*` (texte, binaire, entité), `frame/*` (mise à jour éditeur et runtime). Non mesurés :
restauration undo (`SubtreeSnapshot::restore`), `PrefabSerializer::instantiate/applyToInstance`, `SaveManager::save/load`,
`shiftWorldOrigin` à 10k entités, `onStartRuntime` (déjà journalisé par phase). À creuser :
`scene/set_parent/chain1000_build` 14,1 ms (14 µs par `setParent` sur une chaîne).

## Pistes pour l'avenir

1. **Références et assets par GUID** (entamé) : `EntityRef` (UUID) sans repli par nom (C-12).
2. **Gameplay hors de `Scene`** : Victory/Death en systèmes du jeu (C-14, A-02).
3. **Origine flottante par contrat** : chaque composant à position mémorisée déclare son décalage (aujourd'hui une fonction par composant dans `SceneWorldOrigin.cpp`).
4. **Undo par composant** puis sélection multiple sur les groupes existants (E-05, E-06).
5. **Écriture rapide** : émetteur rapidyaml ou binaire pour sauvegarde, export et snapshots (C-10, C-13).
6. **Prefabs de seconde génération** : imbrication, variants, versionnage réel (C-11).
7. **Découpage d'`EditorLayer` et de `Viewport`** dans une PR dédiée (E-07).
