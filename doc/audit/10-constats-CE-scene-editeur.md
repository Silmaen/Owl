# Audit Owl — constats des axes C (scène, ECS) et E (éditeur Owl Nest)

> Périmètre : `source/owl/{public,private}/scene/**` (~12,8 kLOC, dont `Scene.cpp` 2 595 lignes et `Scene.h`
> 794 lignes), `source/owlnest/` (~21,4 kLOC, dont `EditorLayer.cpp` 2 904 lignes). Branche `Feature/Bench`,
> commit `45e27892`, 2026-10-05. Lecture seule sur le code.
> **Sondes** : 4 fichiers gtest ad hoc (scratchpad de session `axeCE/`, non versionnés), compilés avec les drapeaux
> exacts de `owl_scene_tests_unit_test` (extraits par `ninja -t commands`) et liés à `libOwlEngine.so` de
> `output/build/linux-clang-release` (clang 22.1.8, `-O3`), exécutés dans l'image CI via `docker/run.sh --perf`.
> Les erreurs mémoire sont détectées par valgrind 3.22.0 (le build ASan existant pointe vers un cache `.edm` hors
> conteneur et n'était pas utilisable). Les sondes `EditorLayer`/undo incluent directement les `.cpp` de
> `source/owlnest/sources` (`EntitySnapshot.cpp`, `commands/*.cpp`).
> **Statut** : « confirmé » = vérifié dans le code ou reproduit par une sonde de l'auteur. Contre-vérification
> adverse (§7.3 du cadrage) faite le 2026-10-05 : 22 constats examinés, 0 réfuté, C-07 abaissée à basse (lignes
> « Vérification » ; sondes C-01, C-03, C-04, C-05, C-06, E-01, E-02 relancées, journal dans `verifCE/` du
> scratchpad). « mesuré » = 1 exécution sur la machine d'audit (32 threads), dispersion non mesurée : ordres de grandeur, pas des benchmarks.
> Recoupements avec l'axe A : `Scene` classe dieu (A-02), statiques physique/Lua (A-04), registre de composants
> fermé (A-05). Ils ne sont repris ici que pour leurs conséquences propres à la scène et à l'éditeur.

## Résumé

1. **Six défauts de correction reproduits**, dont deux crashs mémoire. Ils touchent des chemins courants : la pièce
   `coin.lua` du projet d'exemple, l'undo d'un déplacement au gizmo, « Update from Prefab », le Play d'un monde voxel.
2. `scene.destroy_entity(entity_id)` appelé depuis le script de l'entité libère la VM Lua en cours d'exécution
   (valgrind : *Invalid read*). Dans la boucle des triggers, le même appel corrompt le `Trigger` d'une autre entité.
3. L'undo par snapshot (détruire puis recréer l'entité) **détache les enfants** de toute entité parente modifiée.
   Ils deviennent des racines et sautent de position. Le drapeau « modifié » ment après undo + nouvelle édition et
   après fusion d'éditions : un document peut se fermer sans demande de sauvegarde.
4. `PrefabSerializer::applyToInstance` **aplatit la hiérarchie** de l'instance et la ramène à l'origine du prefab.
   L'éditeur ne renseigne jamais `overriddenComponents`, si bien que toute surcharge est écrasée.
5. `Scene::copy` (Play) partage les chunks voxel : ce qu'on casse en Play reste cassé dans la scène éditeur.
   Un `EntityLink` vers un nom absent fait segfaulter `onUpdateRuntime`.
6. EnTT sert de magasin de composants interrogé par vues (1 seul groupe, 0 signal). La hiérarchie passe par des
   UUID et des tables de hachage, ce qui a un coût, mais le cache de transforms monde par frame est bien conçu.
7. Le YAML est la monnaie universelle (scène, prefab, undo, inspecteur, sauvegarde). Mesuré : 1,49 s pour charger
   10 000 entités, 0,07 ms par snapshot. L'inspecteur sérialise l'entité entière 2 fois par composant ouvert et par frame.
8. Robustesse au chargement faible : cycles de hiérarchie et UUID dupliqués acceptés, aucun numéro de format de
   scène, aucun rollback partiel, écritures non atomiques. Un cycle de 2 entités dupliqué produit 1 048 577 entités.
9. L'éditeur expose bien chaque propriété des 31 composants. La règle « Editor Coverage » n'est pas tenue pour autant :
   sélection unique, pas de copier/coller, `game_settings.yml` et les prefabs ne s'éditent pas. Le système d'undo n'a **aucun test**.
10. `EditorLayer.cpp` découpe bien en 6 blocs extractibles (packaging 540 l., ruban 680 l., ouverture par type
    430 l., icônes 150 l.). `UndoManager<Target>` générique et le modèle documents sont de vraies forces.

## Tableau des constats

| ID   | Nature    | Gravité | Sujet                                                                      | Statut            |
|------|-----------|---------|----------------------------------------------------------------------------|-------------------|
| C-01 | faiblesse | haute   | `destroy_entity` Lua : use-after-free de la VM et corruption de `Trigger`  | confirmé (mesuré) |
| C-02 | faiblesse | haute   | `applyToInstance` aplatit la hiérarchie et écrase position et surcharges   | confirmé (mesuré) |
| C-03 | faiblesse | haute   | `Scene::copy` partage les chunks voxel entre éditeur et Play               | confirmé (mesuré) |
| C-04 | faiblesse | haute   | `EntityLink` vers une cible absente : segfault dans `onUpdateRuntime`      | confirmé (mesuré) |
| E-01 | faiblesse | haute   | Undo d'une modification de parent : enfants détachés et déplacés           | confirmé (mesuré) |
| E-02 | faiblesse | haute   | Drapeau « modifié » faux, fermeture sans demande de sauvegarde             | confirmé (mesuré) |
| E-03 | risque    | haute   | Système d'undo/redo sans aucun test                                        | confirmé          |
| C-05 | faiblesse | moyenne | `destroyEntity` rattache les enfants au grand-parent sans garder le monde  | confirmé (mesuré) |
| C-06 | risque    | moyenne | Chargement : cycles, UUID dupliqués, échec partiel sans rollback           | confirmé (mesuré) |
| C-08 | faiblesse | moyenne | Destruction à l'exécution non différée, `on_destroy` et corps Box2D omis   | confirmé (code)   |
| C-09 | étrangeté | moyenne | EnTT sous-exploité : hiérarchie par UUID haché, maps reconstruites/frame   | confirmé          |
| C-10 | faiblesse | moyenne | YAML : coût, pas de version de format, écritures non atomiques             | confirmé (mesuré) |
| C-11 | faiblesse | moyenne | Prefabs superficiels : pas d'imbrication ni de propagation de structure    | confirmé          |
| C-12 | faiblesse | moyenne | Références entre entités par nom (Tag) et non par UUID                     | confirmé          |
| C-13 | faiblesse | moyenne | `SaveManager` : triple parse YAML, version ignorée, écriture non atomique  | confirmé          |
| C-14 | étrangeté | moyenne | `Scene` porte le gameplay, le HUD et la physique du joueur voxel           | confirmé          |
| E-04 | faiblesse | moyenne | Inspecteur : 2 sérialisations YAML de l'entité par composant et par frame  | confirmé (code)   |
| E-05 | faiblesse | moyenne | Snapshot undo : restauration en O(N) de la scène, poids voxel, handles     | confirmé (mesuré) |
| E-06 | faiblesse | moyenne | « Editor Coverage » non atteinte : sélection unique, réglages, prefabs     | confirmé          |
| E-07 | faiblesse | moyenne | `EditorLayer.cpp` : 6 responsabilités, 2 904 lignes, 71 commits            | confirmé          |
| E-08 | faiblesse | moyenne | SceneFlow : double `addComponent<Transform>` sur une entité neuve          | confirmé (code)   |
| C-15 | force     | moyenne | Cache de transforms monde par frame + passe GPU, caches par passe bornés   | confirmé (mesuré) |
| C-16 | force     | moyenne | `Scene::copy` rapide, index UUID O(1) auto-réparant, parse/apply séparés   | confirmé (mesuré) |
| E-09 | force     | moyenne | `UndoManager<Target>` générique, undo par document, Play protégé           | confirmé          |
| E-10 | force     | moyenne | Inspection et édition exhaustives des propriétés des 31 composants         | confirmé          |
| C-07 | faiblesse | basse   | `getEntityCount()` vaut toujours 0 et les tests figent le défaut           | confirmé (mesuré) |
| C-17 | faiblesse | basse   | Caches de passe armés trop tôt : 1 frame de retard (visibilité, triggers)  | confirmé (code)   |
| C-18 | étrangeté | basse   | Limite de profondeur 64 incohérente entre chemins CPU et GPU               | confirmé (code)   |
| E-11 | faiblesse | basse   | Actions non annulables et prefab mal résolu (chemin, bibliothèque texture) | confirmé (code)   |
| E-12 | faiblesse | basse   | Téléportation dupliquée éditeur/runner, échec qui laisse un Play démonté   | confirmé (code)   |

Comptes : 30 constats. Par nature : 21 faiblesses, 2 risques (C-06, E-03), 3 étrangetés (C-09, C-14, C-18),
4 forces (C-15, C-16, E-09, E-10). Par gravité : 7 hautes, 18 moyennes, 5 basses (C-07 abaissée à la
contre-vérification).

## Constats — gravité haute

```text
ID             : C-01
Axe            : Scène / script
Nature         : faiblesse
Gravité        : haute
Preuve         : source/owl/private/script/LuaBindings.cpp:270-278 (luaSceneDestroyEntity → Scene::destroyEntity
                 immédiat) ; source/owl/public/scene/component/LuaScript.h:73 (uniq<ScriptInstance>) ;
                 source/owl/private/script/LuaEngine.cpp:64-66 (lua_close dans le destructeur) ;
                 source/owl/private/scene/Scene.cpp:654-685 (boucle view<Trigger>, `trigger` est une référence
                 dans le storage) ; source/owl/private/scene/SceneTrigger.cpp:79,159 (écritures sur `this` après
                 le callback) ; sample_project/scripts/coin.lua:32 (le cas d'usage livré).
                 Sondes : (1) ScriptInstance qui appelle scene.destroy_entity(entity_id) → valgrind « Invalid read
                 of size 4/8 » dans luaD_precall/lua_pcallk, mémoire libérée par close_state ← ~LuaEngine ←
                 ~LuaScript ← registry.destroy ← luaSceneDestroyEntity. (2) Scène joueur + pièce (Trigger
                 LuaCallback + LuaScript) + autre Trigger à x = 100 : après 1 onUpdateRuntime, l'autre Trigger
                 a wasOverlapping() == true alors qu'il ne chevauche rien (swap-and-pop d'EnTT : setOverlapping
                 écrit dans l'élément déplacé dans le slot libéré).
Constat        : Un script qui détruit sa propre entité, ce que fait la pièce du projet d'exemple, libère la
                 lua_State qui l'exécute (use-after-free). La boucle des triggers continue ensuite d'écrire dans
                 un composant qui appartient désormais à une autre entité. Rien ne casse visiblement en Release,
                 mais c'est un comportement indéfini, sur le chemin le plus démontré du projet.
Comparaison    : Bevy (Commands::despawn appliqué aux points de synchronisation), Unity (Destroy différé en fin
                 de frame), Unreal (destruction différée, MarkAsGarbage) : aucun ne détruit en plein appel de
                 script. EnTT interdit de détruire une autre entité de la vue en cours d'itération.
Recommandation : corriger, effort S : file de destruction différée vidée après la boucle de mise à jour (et
                 appel de on_destroy à ce moment). Ajouter le cas « coin » en test d'intégration.
Statut         : confirmé (mesuré, valgrind ; sonde relancée)
Vérification   : Sonde relancée : valgrind montre toujours « Invalid read » dans lua_pcallk après ~LuaEngine ←
                 registry.destroy, et l'autre Trigger lit wasOverlapping() = true. Aucun garde (isValid, file
                 différée, in_place_delete) ne neutralise.
```

```text
ID             : C-02
Axe            : Scène / prefab
Nature         : faiblesse
Gravité        : haute
Preuve         : source/owl/private/scene/PrefabSerializer.cpp:334 (destroyEntity de chaque entité de
                 l'instance, en ordre BFS) + Scene.cpp:315-330 (destroyEntity réécrit le parentId des enfants) ;
                 PrefabSerializer.cpp:303-311 (Transform non surchargé → pris dans le prefab) ;
                 `overriddenComponents` n'est écrit nulle part dans source/owlnest (grep : lecture seule dans
                 gui/component/render.cpp:782 et sérialisation PrefabLink.cpp:30-54).
                 Sonde : prefab racine + 1 enfant, instance déplacée à x = 10, applyToInstance →
                 child.parentId = 0, racine.childrenIds.size() = 0, racine x = 0.
                 Tests existants : test/scene_tests/PrefabSerializer_test.cpp:254-343 (prefabs à 1 entité,
                 surcharges posées à la main dans le test).
Constat        : « Update from Prefab » et « Revert to Prefab » transforment toute instance à plusieurs entités
                 en entités racines. Elles téléportent l'instance à la position stockée dans le prefab et
                 écrasent toute modification locale, puisque l'éditeur ne marque jamais de surcharge. La
                 fonctionnalité décrite dans CLAUDE.md (merge composant par composant avec surcharges) n'existe
                 que dans les tests unitaires.
Comparaison    : Unity enregistre les surcharges par propriété (PropertyModification) et exclut par défaut la
                 position de la racine ; Godot stocke les propriétés modifiées d'une scène instanciée dans le
                 .tscn parent.
Recommandation : corriger, effort M : merger les composants en place (sans détruire les entités), exclure le
                 Transform de la racine, détecter les surcharges dans drawComponent (diff YAML par composant
                 déjà disponible).
Statut         : confirmé (code)
Vérification   : Mécanisme relu : destroyEntity de la racine (1er de uuidMapping) remet parentId = 0 sur les enfants
                 avant leur propre sérialisation ; grep confirme qu'aucun code de source/owlnest n'écrit
                 overriddenComponents.
```

```text
ID             : C-03
Axe            : Scène / Play
Nature         : faiblesse
Gravité        : haute
Preuve         : source/owl/private/scene/Scene.cpp:275-298 (copy par copie de composant) ;
                 source/owl/public/data/voxel/VoxelWorld.h:37,41,144 (copie par défaut d'un
                 unordered_map<uint64_t, shared<Chunk>>) ; source/owlnest/sources/document/SceneDocument.cpp:106.
                 Sonde : bloc (1,2,3) = 1 dans la scène éditeur, Scene::copy, setBlock(…, 0) dans la copie →
                 la scène éditeur lit 0.
Constat        : La scène de Play partage les chunks de la scène éditeur. Tout bloc cassé ou posé en Play,
                 ainsi que les chunks streamés, est conservé après Stop. Aucune commande d'undo ne le couvre et le
                 document n'est pas marqué modifié. La prochaine sauvegarde enregistre donc l'état de jeu.
Comparaison    : Godot et Unity rechargent ou dupliquent profondément la scène pour le Play ; Bevy n'a pas de
                 copie implicite.
Recommandation : corriger, effort S : constructeur de copie profonde de data::voxel::VoxelWorld (ou
                 copy-on-write par chunk). Auditer les autres shared<> des composants copiés (Tilemap::asset,
                 tilesets : lecture seule a priori).
Statut         : confirmé (mesuré ; sonde relancée)
Vérification   : Sonde relancée : l'éditeur lit 0. VoxelWorld(const VoxelWorld&) = default copie les shared<Chunk>.
                 Nuance : les chunks nouvellement streamés en Play vont dans la map de la copie ; seuls les chunks
                 déjà présents sont partagés.
```

```text
ID             : C-04
Axe            : Scène / runtime
Nature         : faiblesse
Gravité        : haute
Preuve         : source/owl/private/scene/Scene.cpp:1950-1953 (rescanTag peut laisser linkedEntity invalide,
                 puis getWorldTransform(link.linkedEntity) sans test) ; Entity.h:89-92 (getComponent, assert
                 seulement en Debug).
                 Sonde : 1 entité avec EntityLink{"DoesNotExist"}, onStartRuntime, onUpdateRuntime → processus
                 terminé par SIGSEGV (code 139).
Constat        : Une faute de frappe dans le nom lié, ou la destruction de la cible par un script, fait planter le
                 jeu à la frame suivante. Le cas n'est ni testé ni signalé.
Comparaison    : —
Recommandation : corriger, effort S : `if (!link.linkedEntity) continue;` après rescan, avec un avertissement
                 émis une seule fois.
Statut         : confirmé (mesuré ; sonde relancée)
Vérification   : Sonde relancée : SIGSEGV (139). getWorldTransform(Entity{}) déréférence mp_scene nul ; une cible
                 détruite laisse un handle invalide passé à registry.get. Aucun test ni garde après rescanTag.
```

```text
ID             : E-01
Axe            : Éditeur / undo
Nature         : faiblesse
Gravité        : haute
Preuve         : source/owlnest/sources/commands/ComponentCommands.cpp:14-18 (restoreEntity = destroyEntity +
                 restore) ; source/owl/private/scene/Scene.cpp:315-330 (destroyEntity réécrit parentId des enfants
                 vers le grand-parent) ; SceneSerializer.cpp:188-199 (restore puis rebuildHierarchyChildren qui
                 repart des parentId) ; Viewport.cpp:682-690 (le gizmo pousse un ModifyEntityCommand).
                 Sonde : parent à x = 5, enfant à x monde = 0 ; ModifyEntityCommand (x = 8) puis undo →
                 enfant.parentId ≠ parent, parent.childrenIds = 0, x monde de l'enfant = -5.
Constat        : Annuler toute modification d'une entité qui a des enfants les détache et les déplace. C'est le cas
                 d'un déplacement au gizmo, d'une édition dans l'inspecteur ou d'un ajout ou retrait de composant.
                 Le redo ne répare rien. Comme c'est l'opération la plus fréquente de l'éditeur, le défaut est très
                 exposé.
Comparaison    : Unity (Undo.RecordObject) et Godot (UndoRedo, add_do_property) restaurent des propriétés et ne
                 recréent pas l'objet.
Recommandation : corriger, effort S à M : restaurer les composants en place (désérialiser dans l'entité existante,
                 garder Hierarchy) plutôt que détruire et recréer. Couvrir le cas dans les tests de E-03.
Statut         : confirmé (mesuré ; sonde relancée)
Vérification   : Sonde relancée : parentId ≠ parent, 0 enfant, x monde enfant = -5. Le gizmo pousse bien un
                 ModifyEntityCommand (Viewport.cpp:682-690) dont l'undo passe par restoreEntity = destroyEntity +
                 restore.
```

```text
ID             : E-02
Axe            : Éditeur / undo
Nature         : faiblesse
Gravité        : haute
Preuve         : source/owlnest/sources/UndoManager.h:193 (isDirty = taille de pile ≠ index sauvé) ;
                 UndoManager.h:72-80 (fusion dans le sommet sans changer la taille) ;
                 SceneDocument.h:66 ; EditorLayer.cpp:366-376 (fermeture sans modale si !isDirty()).
                 Sonde : (a) push, markSaved, undo, nouvelle édition → isDirty() = false ; (b) push, markSaved,
                 édition fusionnée (< 1 s, même entité) → isDirty() = false.
Constat        : Après « sauver, annuler, modifier » ou « sauver puis retoucher la même entité dans la seconde »,
                 le document se déclare propre. Le titre perd son `*` et la fermeture ne demande rien : des
                 modifications sont perdues sans avertissement. NodeGraphDocument compare un snapshot YAML et
                 n'a pas ce défaut (NodeGraphDocument.cpp:33).
Comparaison    : Qt QUndoStack invalide l'index propre dès qu'une commande au-delà de cet index est remplacée ou
                 fusionnée.
Recommandation : corriger, effort S : compteur de génération incrémenté à chaque push/merge/undo/redo, ou
                 interdire la fusion avec la commande sauvegardée et invalider m_savedIndex quand la pile redo
                 qui le contient est vidée.
Statut         : confirmé (mesuré ; sonde relancée)
Vérification   : Sonde relancée : isDirty() = false dans les deux cas. Le cas (a) est courant ; le cas (b) exige une
                 retouche de la même entité moins d'1 s après la précédente, plus rare mais réel.
```

```text
ID             : E-03
Axe            : Éditeur / qualité
Nature         : risque
Gravité        : haute
Preuve         : grep -rln 'ModifyEntityCommand\|UndoManager' test/ → aucun résultat ; les en-têtes de
                 source/owlnest/sources sont pourtant déjà dans les include paths des tests
                 (MarkdownPreview_test.cpp vit dans test/scene_tests). Les sondes E-01/E-02 se compilent en
                 incluant directement EntitySnapshot.cpp et commands/*.cpp : le code est testable sans GPU.
Constat        : La pièce la plus délicate de l'éditeur (snapshots, fusion, dirty, sélection, prefab) n'a aucun test.
                 E-01, E-02 et C-02 sont passés en release 0.2.1 alors que chacun se reproduit en moins de
                 20 lignes de test.
Comparaison    : —
Recommandation : corriger, effort S : catégorie de tests `owlnest_tests` liant les sources non-GUI de l'éditeur
                 (commands, UndoManager, EntitySnapshot, Project) ; propriétés à tester : undo∘redo = identité
                 sur la scène sérialisée, hiérarchie préservée, dirty cohérent.
Statut         : confirmé
Vérification   : grep 'ModifyEntityCommand|UndoManager|EntitySnapshot|UndoCommand' sur test/ : 0 fichier ; aucun autre
                 dossier de test dans le dépôt. Gravité haute maintenue au vu de E-01, E-02 et C-02.
```

## Constats — gravité moyenne

```text
ID             : C-05
Axe            : Scène / hiérarchie
Nature         : faiblesse
Gravité        : moyenne
Preuve         : source/owl/private/scene/Scene.cpp:315-343 (parentId réécrit, Transform local inchangé) ;
                 commands/EntityCommands.cpp:47-62 (undo par setParent, qui recalcule le local depuis le monde
                 courant, déjà faux). Sonde : parent x = 5, enfant x monde = 0 ; DeleteEntityCommand → x monde
                 enfant = -5 ; undo → x monde = -5, x local = -10.
Constat        : Supprimer un parent seul fait sauter ses enfants, et l'undo ne les remet pas en place. L'ordre des
                 frères est aussi perdu (setParent ajoute en fin). La documentation (`.claude/rules/scene.md`)
                 décrit un simple rattachement au grand-parent, sans dire que la position monde change.
Comparaison    : Godot et Unity conservent la position monde lors d'un détachement par défaut
                 (Transform.SetParent(worldPositionStays = true)).
Recommandation : corriger, effort S : recalculer local = inverse(monde grand-parent) × monde enfant dans
                 destroyEntity ; l'undo doit restaurer le Transform local et l'index de frère capturés.
Statut         : confirmé (mesuré ; sonde relancée)
Vérification   : Sonde relancée : monde -5 après suppression, local -10 après undo. setParent (Scene.cpp:2311-2326)
                 préserve le monde courant, déjà faux, ce qui fige l'erreur au lieu de la réparer.
```

```text
ID             : C-06
Axe            : Scène / sérialisation
Nature         : risque
Gravité        : moyenne
Preuve         : source/owl/private/scene/Scene.cpp:2407-2425 (rebuildHierarchyChildren n'orphelinise que les
                 parents absents, pas les cycles) ; Scene.cpp:2347-2405 et EntitySnapshot.cpp:28-47,
                 PrefabSerializer.cpp:33-43 (parcours sans ensemble « visité ») ; Scene.cpp:302-313
                 (insert_or_assign : UUID dupliqué accepté) ; SceneSerializer.cpp:96-106 (catch(...) → false
                 sans retirer les entités déjà créées).
                 Sondes : fichier « A parent de B, B parent de A » → chargement OK, 0 racine, entités invisibles
                 de l'arbre ; duplicateSubtree(A) → 1 048 577 entités en 0,98 s (saturation des identifiants
                 EnTT) ; deux entités « Entity: 1 » → chargement OK, findEntityByUUID(1) renvoie la seconde.
Constat        : Un fichier corrompu ou fusionné à la main (conflit git) peut contenir un cycle ou un doublon. Il se
                 charge sans erreur, puis gèle ou fait exploser la mémoire à la première duplication, suppression
                 en cascade ou création de prefab. Sur erreur YAML, la scène reste à moitié remplie.
Comparaison    : Godot valide les chemins de nœuds au chargement ; Unity détecte les GUID dupliqués et en régénère.
Recommandation : corriger, effort S : passe de validation après chargement (cycles cassés vers la racine,
                 doublons ré-UUIDés, avertissement), ensemble visité dans les parcours, chargement dans une scène
                 temporaire puis échange.
Statut         : confirmé (mesuré) ; partiel pour le rollback
Vérification   : Cycle relancé : 1 048 577 entités. Nuance : les appelants (EditorLayer.cpp:1725, 1998 ;
                 RunnerLayer.cpp:220-240) chargent dans une scène neuve jetée ou ferment l'appli en cas d'échec : la «
                 scène à moitié remplie » ne fuit pas.
```

```text
ID             : C-07
Axe            : Scène / qualité
Nature         : faiblesse
Gravité        : basse
Preuve         : source/owl/private/scene/Scene.cpp:2086-2091 (registry.storage<Entity>() : Entity est la classe
                 enveloppe, pas entt::entity) ; test/scene_tests/SceneRuntime_test.cpp:396-405 (commentaire qui
                 reconnaît le défaut et « fige le comportement ») ; SceneSerializer_test.cpp:26,68 et
                 SceneExtra_test.cpp:87 (EXPECT_EQ(copy->getEntityCount(), sc->getEntityCount()) : 0 == 0).
                 Sonde : 2 createEntity → getEntityCount() = 0.
Constat        : L'API publique renvoie toujours 0. Trois tests d'aller-retour de sérialisation et de copie
                 reposent sur cette égalité et ne vérifient donc rien. Le test qui documente le défaut le
                 transforme en comportement attendu.
Comparaison    : —
Recommandation : corriger, effort S : `registry.view<component::ID>().size()` (ou storage<entt::entity>()
                 ->free_list()) ; réécrire les trois assertions.
Statut         : confirmé (code)
Vérification   : Exact, mais aucun appelant de getEntityCount() hors tests (grep source/) : gravité abaissée de
                 moyenne à basse. Il y a 5 assertions 0 == 0, pas 3 (SceneSerializer_test.cpp:26,68,91,
                 SceneExtra_test.cpp:87, Scene_test.cpp:76).
```

```text
ID             : C-08
Axe            : Scène / runtime
Nature         : faiblesse
Gravité        : moyenne
Preuve         : source/owl/private/scene/Scene.cpp:315-343 (destroyEntity : ni onDestroy du script, ni retrait du
                 corps Box2D, ni arrêt du son) ; LuaBindings.cpp:270-278 ; PhysicCommand.cpp:68-100 (corps créés
                 une fois dans init, indexés par bodyId) ; sample_project/scripts/coin.lua:39-41 (on_destroy
                 attendu par le script d'exemple).
Constat        : La destruction d'une entité en cours de partie n'a aucun cycle de vie : on_destroy n'est pas
                 appelé, un corps physique éventuel reste dans le monde Box2D comme collisionneur fantôme, une
                 source sonore continue. Le seul chemin propre est onEndRuntime pour toute la scène.
Comparaison    : Bevy (hooks on_remove / observers), Godot (NOTIFICATION_PREDELETE, queue_free), EnTT lui-même
                 (on_destroy<T>() signal) offrent un point d'accroche unique.
Recommandation : corriger, effort M : brancher registry.on_destroy<PhysicBody/LuaScript/SoundSource>() vers les
                 sous-systèmes, combiné à la file différée de C-01.
Statut         : confirmé (code)
Vérification   : Aucun b2DestroyBody dans source/ hors destruction du monde ; onDestroy n'est appelé qu'en
                 Scene.cpp:501 (onEndRuntime). Le mécanisme du corps fantôme est donc certain, même sans reproduction.
```

```text
ID             : C-09
Axe            : ECS
Nature         : étrangeté
Gravité        : moyenne
Preuve         : grep sur source/owl et source/owlnest : 1 seul `registry.group` (Scene.cpp:983), 0
                 on_construct/on_destroy/on_update/observer/sort/entt::handle/entt::snapshot ; Scene.h:386
                 (`entt::registry registry;` public, accédé directement par PhysicCommand, LuaBindings,
                 SceneTrigger, render.cpp) ; Hierarchy = parentId + vector<UUID> (Hierarchy.h) ; chaque
                 niveau de parent coûte un findEntityByUUID haché (Scene.cpp:2147-2156, 2193-2215, 2266-2280) ;
                 prepareWorldTransforms vide puis remplit deux unordered_map à chaque frame (Scene.cpp:2173,
                 2209-2210) ; index UUID et caches invalidés à la main par onComponentAdded (Scene.cpp:2428-2595).
Constat        : EnTT (3.15.0) sert de magasin de composants interrogé par vues. Ses mécanismes structurants
                 sont ignorés : signaux pour l'index UUID et les invalidations, `entt::entity` pour les liens
                 parent/enfant, tri de storage pour l'ordre topologique, snapshot pour la copie. Il en résulte
                 2 tables de hachage reconstruites par frame (allocation d'un nœud par entité, à rebours de la
                 règle « pas d'allocation par frame »). Le choix des UUID dans Hierarchy simplifie toutefois la
                 sérialisation et l'undo (opinion : c'est le vrai motif).
Comparaison    : Bevy : ChildOf/Children portent des Entity et la propagation suit la détection de changement ;
                 flecs : relation ChildOf native, requêtes « cascade » ordonnées. Les deux évitent le hachage par
                 niveau.
Recommandation : surveiller, effort M : garder les UUID en sérialisation et ajouter un cache entt::entity du
                 parent (résolu au chargement et à setParent) ; remplacer les deux unordered_map par des vecteurs
                 indexés par entt::to_entity ; brancher l'index UUID sur on_construct/on_destroy<ID>.
Statut         : confirmé
Vérification   : grep relancé : 1 seul registry.group (Scene.cpp:983), 0 signal ni observer. prepareWorldTransforms
                 vide m_entityToWorldIndex (unordered_map) à chaque frame : clear() libère les nœuds, d'où
                 l'allocation par entité.
```

```text
ID             : C-10
Axe            : Sérialisation
Nature         : faiblesse
Gravité        : moyenne
Preuve         : SceneSerializer.cpp:62 (`Scene: untitled` en dur, aucun champ de version de format) ;
                 SceneSerializer.cpp:79-83, PrefabSerializer.cpp:86-91, SaveManager.cpp:112-117,
                 SettingsManager.cpp:138 (ofstream direct : ni fichier temporaire + rename, ni test d'erreur
                 d'écriture pour la scène et le prefab) ; serializeEntity dupliqué
                 (SceneSerializer.cpp:23-28 / PrefabSerializer.cpp:26-31) et deserializeEntity dupliqué
                 (SceneSerializer.cpp:30-53 / PrefabSerializer.cpp:46-66).
                 Mesure (entités Tag+Transform+Visibility+Hierarchy+SpriteRenderer, chaînes de profondeur 16) :
                 N = 1 000 : sérialisation 30 ms (277 Kio), chargement 126 ms ;
                 N = 10 000 : sérialisation 282 ms (2,7 Mio), chargement 1 492 ms (~150 µs/entité).
Constat        : Le YAML est lisible et diffable, ce qui sert l'éditeur et git. Mais aucune migration n'est
                 possible faute de version de format. Un crash pendant l'enregistrement tronque la scène. Le coût
                 de chargement (~150 µs par entité simple) fixe un plafond pratique de quelques milliers
                 d'entités par niveau si l'on vise une transition rapide.
Comparaison    : Godot : .tscn texte avec `format=3` + .scn binaire ; Unity : YAML versionné
                 (serializedVersion) avec import binaire ; Bevy : scènes RON reflétées.
Recommandation : corriger (version + écriture atomique, S) ; surveiller (format binaire cuit pour le runtime
                 et les .owlpack, M, à arbitrer avec l'axe K).
Statut         : confirmé
Vérification   : SceneSerializer.cpp:62 écrit « Scene: untitled » sans clé de version ; serialize() ouvre un ofstream
                 direct sans fichier temporaire. Les durées sont linéaires entre 1k et 10k (126 / 149 µs par entité),
                 cohérentes.
```

```text
ID             : C-11
Axe            : Prefab
Nature         : faiblesse
Gravité        : moyenne
Preuve         : PrefabSerializer.cpp:78 (Version toujours 1 : syncedVersion ne sert à rien) ;
                 PrefabSerializer.cpp:278-279 (applyToInstance ne parcourt que uuidMapping : entités ajoutées
                 ou retirées du prefab ignorées) ; PrefabSerializer.cpp:335 (shared<Scene> non possédant par
                 constructeur d'aliasing, aussi EntitySnapshot.cpp:27) ; pas de document d'édition de .owlprefab
                 (EditorLayer.cpp:774-775 : double-clic = instancier) ; pas de prefab imbriqué.
Constat        : Le système de prefab est un « copier depuis un fichier » avec rafraîchissement de composants.
                 La structure ne se propage pas, aucun prefab n'est imbriqué ou dérivé (variant), le prefab ne
                 s'édite pas sur place et il n'y a pas de versionnage effectif. Ajouté à C-02, cela empêche de
                 s'appuyer sur les prefabs pour un niveau réel.
Comparaison    : Unity (nested prefabs + variants, mode d'édition isolé), Godot (scènes instanciées
                 imbriquées, « Editable Children »), Unreal (Blueprint/Level Instance).
Recommandation : surveiller puis remplacer, effort L : prefab = scène instanciée par référence (GUID d'asset),
                 surcharges par propriété, édition en document dédié.
Statut         : confirmé
Vérification   : PrefabSerializer.cpp:77 émet Version 1 en dur ; applyToInstance ne parcourt que uuidMapping ; le
                 double-clic sur .owlprefab instancie (EditorLayer.cpp:774-775). Aucun contre-exemple trouvé.
```

```text
ID             : C-12
Axe            : Scène / références
Nature         : faiblesse
Gravité        : moyenne
Preuve         : EntityLink.h (linkedEntityName : string) ; Scene.cpp:1896-1913, 1918-1925 (résolution par Tag,
                 rescan linéaire des Tag en cas d'échec : O(N) par lien par frame) ; SceneTrigger.cpp:96-101
                 (cible de téléportation par Tag, scan de view<Tag, Transform>) ; Lua scene.find_entity(name)
                 (sample_project/scripts/coin.lua:27) ; duplicateSubtree ne remappe aucun lien
                 (Scene.cpp:2376-2405).
Constat        : Les références entre entités passent par des noms non uniques. Dupliquer un ensemble lié, ou
                 renommer une cible, casse le lien en silence, et le cas « cible absente » fait planter (C-04).
                 Les UUID existent pourtant partout.
Comparaison    : Unity et Godot sérialisent des références d'objet (fileID / NodePath) et les remappent à la
                 duplication.
Recommandation : corriger, effort M : référence par UUID (champ sérialisé), nom gardé en affichage, remap dans
                 duplicateSubtree et l'instanciation de prefab (la table uuidRemap existe déjà).
Statut         : confirmé
Vérification   : resolveAllEntityLinks et rescanTag résolvent par Tag ; la téléportation same-level scanne view<Tag,
                 Transform> (SceneTrigger.cpp:97). duplicateSubtree ne touche pas EntityLink.
```

```text
ID             : C-13
Axe            : Sauvegarde / réglages
Nature         : faiblesse
Gravité        : moyenne
Preuve         : SaveManager.cpp:62 puis 86 (scène sérialisée en chaîne puis re-parsée par YAML::Load pour
                 l'insérer) et 143-153 (ré-émise en chaîne puis re-parsée au chargement) : 3 parses pour un
                 aller-retour ; SaveManager.cpp:77 (version écrite, jamais relue) ; SaveManager.cpp:112-117
                 (écriture directe) ; SaveManager.h:117, SettingsManager.h:201-205 (état statique global) ;
                 le jeu de réglages game_settings.yml n'a pas d'éditeur (seul EditorLayer.cpp:2693 l'empaquète).
Constat        : Une sauvegarde est une copie complète de la scène, plus le GameState et les vitesses. Au coût
                 mesuré en C-10, une sauvegarde d'un niveau de 10 000 entités se compte en secondes, sur le
                 thread principal. Une coupure pendant l'écriture détruit la sauvegarde précédente du même slot.
                 L'état Lua (variables locales des scripts) n'est pas sauvé, ce qui est cohérent avec la
                 documentation mais limite les usages. Les classes statiques imposent un seul jeu et une seule
                 scène par processus (cf. A-04).
Comparaison    : Godot laisse le jeu décider (ConfigFile / ResourceSaver) ; Unreal (USaveGame) ne sérialise que
                 les propriétés marquées SaveGame, pas tout le niveau.
Recommandation : corriger (écriture atomique, parse unique, vérification de version, S) ; surveiller
                 (sauvegarde différentielle par rapport à la scène source, M).
Statut         : confirmé (code)
Vérification   : SaveManager.cpp:62, 86, 141-150 : 1 parse à la sauvegarde, 2 au chargement ; « version » jamais relue
                 ; ofstream direct. « Se compte en secondes » pour 10k entités reste une extrapolation non mesurée (≈
                 0,3 s + 1 parse).
```

```text
ID             : C-14
Axe            : Scène / architecture
Nature         : étrangeté
Gravité        : moyenne
Preuve         : Scene.cpp : rendu UI 136-240, onUpdateRuntime 510-720, Victory/Death et texte « You loose! »
                 dupliqués 534-576 et 741-779, streaming et physique du joueur voxel (mouvement, collisions,
                 interactions) 1136-1476, HUD/toast 1477-1570, raycast 1571-1808, tilemaps 1809-1895 ; 35
                 #include ; 46 commits sur le fichier.
Constat        : La boucle de jeu est une séquence fixe codée en dur dans onUpdateRuntime : NativeScript, Lua,
                 FlyCamera, VoxelPlayer, portes raycast, entrées joueur, physique, liens, transforms, son,
                 triggers, sprites animés, rendu. Aucun système n'est enregistrable et aucune phase n'est nommée.
                 Les règles d'un jeu de démonstration y sont mêlées (statuts Victory/Death, contrôleur voxel).
                 Voir A-02 pour la mesure de couplage.
Comparaison    : Bevy (Schedules + SystemSets ordonnés), Godot (process/physics_process par nœud), flecs
                 (pipelines de phases).
Recommandation : surveiller, effort L : extraire des systèmes (VoxelPlayerSystem, RaycastSystem, UiSystem,
                 TriggerSystem) avec une liste de phases explicite ; déplacer Victory/Death dans le jeu ou le
                 runner.
Statut         : confirmé
Vérification   : Victory/Death et « You loose! » dupliqués relus (Scene.cpp:535-570 et 745-773) ; 35 #include, 46
                 commits.
```

```text
ID             : E-04
Axe            : Éditeur / performance
Nature         : faiblesse
Gravité        : moyenne
Preuve         : source/owlnest/sources/panel/SceneHierarchy.cpp:644-657 (serializeEntityToString avant et après
                 renderProps, pour chaque composant ouvert, à chaque frame) ; VoxelWorld.cpp:126-140 (un monde
                 non procédural sérialise tous ses chunks encodés) ; SceneHierarchy.cpp:686-703 (le renommage
                 construit une commande, la jette et la reconstruit).
                 Mesure : serializeEntityToString d'une entité à 5 composants ≈ 0,06-0,07 ms.
Constat        : La détection de modification de l'inspecteur coûte 2 × (nb de composants ouverts) sérialisations
                 complètes par frame. Pour une entité simple, l'estimation tourne autour de 1 ms par frame
                 (opinion, extrapolation de la mesure). Pour un monde voxel édité à la main, chaque frame
                 réencode tout le monde 2 à 6 fois (non mesuré). Le principe « diff YAML » est pourtant robuste
                 et générique.
Comparaison    : Les inspecteurs ImGui testent d'ordinaire le retour des widgets
                 (IsItemDeactivatedAfterEdit / valeur de retour de Drag*) ; Unity n'enregistre l'undo qu'au
                 changement.
Recommandation : corriger, effort S : sérialiser le seul composant (pas l'entité) et seulement si un widget a été
                 actif ce frame (ImGui::IsAnyItemActive / retour de renderProps).
Statut         : confirmé (code)
Vérification   : SceneHierarchy.cpp:644-657 relu : 2 serializeEntityToString de l'entité entière par composant ouvert
                 et par frame, sans test d'activité de widget. L'estimation ~1 ms reste une opinion.
```

```text
ID             : E-05
Axe            : Éditeur / undo
Nature         : faiblesse
Gravité        : moyenne
Preuve         : SceneSerializer.cpp:188-199 (deserializeEntityFromString appelle rebuildHierarchyChildren sur
                 toute la scène à chaque entité restaurée) ; EntitySnapshot.cpp:50-69 (restauration d'un
                 sous-arbre entité par entité, puis setParent par enfant) ; UndoManager.h:216 (100 commandes,
                 2 snapshots YAML par commande) ; ComponentCommands.cpp:14-18 (le handle entt change à chaque
                 undo, d'où les sélections et EntityLink::linkedEntity périmés).
                 Mesure : restauration d'1 entité dans une scène de 1 000 / 10 000 entités : 15,8 / 7,2 ms
                 (1 échantillon chacun, dispersion inconnue) ; snapshot : 0,06-0,07 ms.
Constat        : Le modèle « snapshot YAML d'entité » est simple et générique, mais l'undo d'un sous-arbre de k
                 entités coûte k fois une reconstruction de la hiérarchie entière. La mémoire de l'historique
                 croît avec la taille des entités (un monde voxel édité est stocké 2 fois par commande, hors
                 VoxelEditCommand delta). La recréation d'entité produit E-01.
Comparaison    : Unreal (transactions par objet, delta sérialisé), Godot (do/undo par propriété) : coût
                 proportionnel au changement, pas à l'objet.
Recommandation : corriger, effort M : restauration en place, rebuild de hiérarchie local au sous-arbre ; à terme
                 snapshot par composant (la clé YAML du composant suffit).
Statut         : confirmé (code) ; mesure non probante
Vérification   : deserializeEntityFromString appelle bien rebuildHierarchyChildren (SceneSerializer.cpp:194). Mais la
                 mesure contredit le O(N) (1k : 15,8 ms, 10k : 7,2 ms) : un coût fixe domine, la part O(N) n'est pas
                 démontrée.
```

```text
ID             : E-06
Axe            : Éditeur / couverture
Nature         : faiblesse
Gravité        : moyenne
Preuve         : SceneHierarchy.h:175 (`scene::Entity m_selection;` : sélection unique) ; aucune occurrence de
                 copy/paste/clipboard d'entité dans source/owlnest/sources ; game_settings.yml : aucun panneau
                 (grep SettingsManager dans owlnest → runner seulement) ; .owlprefab : pas de document,
                 surcharges jamais marquées (C-02) ; SceneHierarchy.cpp:738 (menu « Add Component » alimenté par
                 OptionalComponents, qui contient PrefabLink : on peut ajouter un PrefabLink vide à la main).
Constat        : Au regard de `.claude/rules/ongoing-quality.md` (*Editor Coverage*), l'inspection et l'édition
                 des propriétés sont atteintes (E-10). Les autres exigences ne le sont pas. Il n'y a pas de
                 sélection multiple, donc ni déplacement ni suppression de groupe d'entités. Il n'y a ni copier
                 ni coller. Les réglages de jeu ne s'éditent qu'en YAML, et les prefabs ne s'éditent pas comme
                 objets. La règle est donc enfreinte pour l'objet le plus « en collection » du moteur : l'entité.
Comparaison    : Sélection multiple, copier/coller et édition de prefab isolée sont le socle commun de Godot,
                 Unity et Unreal.
Recommandation : corriger, effort M : `std::vector<UUID>` de sélection + commande composite (CompositeCommand
                 existe déjà dans SceneFlowCommands) ; panneau Game Settings sur le modèle SceneSettings.
Statut         : confirmé
Vérification   : SceneHierarchy.h:175 : un seul scene::Entity ; aucun copier/coller d'entité ; SettingsManager
                 n'apparaît que dans le runner. game_settings.yml ne s'ouvre qu'en éditeur de code, ce que la règle
                 qualifie de régression.
```

```text
ID             : E-07
Axe            : Éditeur / architecture
Nature         : faiblesse
Gravité        : moyenne
Preuve         : source/owlnest/sources/EditorLayer.cpp (2 904 lignes, 91 méthodes et 33 membres dans
                 EditorLayer.h, 71 commits) : buildIconBank 41-190 ; routage documents et modales 219-475 ;
                 onAttach 475-653 (179 l.) ; ruban 973-1653 (buildRibbon 193 l. + 6 onglets) ; ouverture et
                 création par type d'asset 1654-2081 ; play/teleport/save-load 2091-2160 ; projet 2135-2253 ;
                 packaging 2309-2850 (packScene, wizard, validation, startPackGame 221 l., zip, lanceur Linux,
                 copie des .so).
Constat        : EditorLayer reste l'agrégateur de tout ce qui n'a pas de panneau. La règle « mettre la nouvelle
                 logique dans un panneau, document ou commande » (`.claude/rules/editor.md`) est respectée
                 depuis qu'elle existe. La dette restante se découpe en blocs nets, sans dépendance croisée forte.
Comparaison    : Godot sépare EditorNode, EditorExport (plateformes), EditorFileSystem et les plugins d'éditeur.
Recommandation : corriger, effort M : extraire GamePackager (≈ 540 l.), RibbonBuilder (≈ 680 l.), AssetOpener /
                 table « extension → document » (≈ 430 l., supprime les chaînes de if sur l'extension en
                 EditorLayer.cpp:328-355 et 760-777), IconBank setup (150 l.). Cible : EditorLayer < 1 000 l.
Statut         : confirmé
Vérification   : wc -l : 2 904 lignes ; git log : 71 commits sur EditorLayer.cpp. Découpage en blocs cohérent avec la
                 lecture.
```

```text
ID             : E-08
Axe            : Éditeur / SceneFlow
Nature         : faiblesse
Gravité        : moyenne
Preuve         : source/owlnest/sources/document/SceneFlowDocument.cpp:522-523 (createEntity, qui ajoute déjà un
                 Transform (Scene.cpp:304), suivi de addComponent<Transform>()) ; Entity.h:59-63
                 (OWL_CORE_ASSERT(!hasComponent) en Debug, emplace EnTT sur un slot occupé en Release).
Constat        : Créer un lien de téléportation dans la vue SceneFlow déclenche une assertion en Debug. En
                 Release, c'est un emplace sur un composant existant, qui relève du comportement indéfini côté EnTT.
Comparaison    : —
Recommandation : corriger, effort S : supprimer la ligne (et ajouter un test de la commande composite).
Statut         : confirmé (code)
Vérification   : SceneFlowDocument.cpp:522-523 relu ; createEntityWithUUID ajoute déjà Transform (Scene.cpp:304). En
                 Release, l'emplace EnTT sur une entité déjà présente duplique l'entrée du storage : corruption, pas
                 un simple doublon.
```

```text
ID             : C-15
Axe            : Scène / transforms
Nature         : force
Gravité        : moyenne
Preuve         : Scene.cpp:2166-2222 (prepareWorldTransforms : parcours préfixe unique, ordre topologique, miroir
                 CPU + SSBO GPU via WorldTransformPass) ; Scene.cpp:2131-2164 (getWorldTransform : cache par
                 passe, repli sur la remontée de chaîne) ; Scene.h:496-541 (fenêtres de validité documentées :
                 m_inUpdatePass, m_worldTransformCacheActive armé seulement après scripts/physique/liens).
                 Mesure (sans cache, chaînes de profondeur ≤ 16) : getWorldTransform sur toutes les entités
                 1 000 → 1,0 ms, 10 000 → 9,2 ms (≈ 0,9 µs/appel) ; isEffectivelyVisible 10 000 → 2,1 ms.
Constat        : La question « getWorldTransform recalcule-t-il récursivement à chaque appel ? » a une réponse
                 nuancée. Oui hors frame (éditeur, gizmo, tests). Non pendant la partie rendu d'une frame, où un
                 seul parcours alimente le CPU et le GPU. Les invariants des caches sont écrits à côté du code,
                 ce qui est rare.
Comparaison    : Godot (dirty flag propagé, global transform caché par nœud) et Bevy (propagation par change
                 detection) évitent aussi la remontée, mais incrémentalement ; Owl recalcule tout à chaque frame.
Recommandation : garder ; surveiller le coût du parcours complet à grande échelle (voir points de mesure).
Statut         : confirmé (mesuré)
```

```text
ID             : C-16
Axe            : Scène
Nature         : force
Gravité        : moyenne
Preuve         : Scene.cpp:275-298 (copy par storage, sans YAML) ; Scene.cpp:2093-2108 (findEntityByUUID : index
                 O(1), vérification, repli sur scan + réparation) ; SceneSerializer.h / .cpp:113-180
                 (parseBuffer séparé d'applyParsed, chronométrés, utilisés par la téléportation
                 SceneDocument.cpp:169-175) ; LuaScript.h:28-50 (copie qui exclut l'instance runtime).
                 Mesure : Scene::copy 1 000 → 0,40 ms, 10 000 → 4,1 ms.
Constat        : Le passage au Play ne repasse pas par YAML et coûte ~0,4 µs par entité, ce qui le rend
                 imperceptible jusqu'à des dizaines de milliers d'entités, à la réserve près de C-03. La
                 séparation parse/apply prépare un chargement asynchrone (le parse YAML, partie la plus chère, ne
                 touche pas la scène).
Comparaison    : Hazel (copie par registre, même idiome) ; Godot duplique l'arbre de nœuds.
Recommandation : garder.
Statut         : confirmé (mesuré)
```

```text
ID             : E-09
Axe            : Éditeur / undo
Nature         : force
Gravité        : moyenne
Preuve         : UndoManager.h (template UndoManager<Target>, réutilisé par les documents NodeGraph, Tilemap,
                 Tileset, Animation) ; SceneDocument.cpp:242-260 (undo/redo refusés hors mode Edit) ;
                 UndoCommand.h (sélection après undo/redo par UUID, robuste aux recréations) ;
                 ComponentCommands.cpp:72-88 (fusion par entité, horodatage repris → glisser continu = 1 étape) ;
                 VoxelCommands (delta, pas snapshot).
Constat        : L'architecture de commandes est saine et homogène entre documents. Ses défauts (E-01, E-02,
                 E-05) tiennent à l'implémentation du snapshot et du dirty, pas au modèle, et se corrigent sans
                 refonte.
Comparaison    : Équivalent fonctionnel de QUndoStack / Godot UndoRedo ; moins que les transactions d'Unreal
                 (pas de regroupement explicite multi-commandes hors SceneFlowCompositeCommand).
Recommandation : garder ; ajouter un « begin/end group » pour les opérations multiples (sélection multiple E-06).
Statut         : confirmé
```

```text
ID             : E-10
Axe            : Éditeur / couverture
Nature         : force
Gravité        : moyenne
Preuve         : source/owl/public/gui/component/render.h:240-249 (DrawableComponents : 31 composants) ;
                 render.cpp:173-1133 (un renderProps par composant). Script de comparaison (champs publics de
                 chaque composant et de ses sous-objets SceneTrigger, SceneBody, ScenePlayer, SceneSound,
                 confrontés au corps de renderProps) : les seuls champs non exposés sont des états runtime
                 (AnimatedSpriteRenderer::m_elapsedTime/m_playing, RaycastDoor/PushWall::bodyId/keyHeldLastTick,
                 LuaScript::instance, 15 champs runtime de VoxelPlayer). Undo de toute édition via drawComponent
                 (SceneHierarchy.cpp:644-657), gizmo (Viewport.cpp:641-690), sélection par picking
                 (Viewport.cpp:283) pour sprites, cercles, textes, tilemaps et UI.
Constat        : Chaque propriété authorable des 31 composants est inspectable, éditable et annulable. Tilemap,
                 tileset, animation, graphe de nœuds et voxel ont des documents dédiés. Le socle « inspecteur »
                 de la règle Editor Coverage est atteint, ce qui est remarquable pour un moteur de cette taille.
Comparaison    : —
Recommandation : garder ; corriger les manques structurels listés en E-06.
Statut         : confirmé
```

## Constats — gravité basse

```text
ID             : C-17
Axe            : Scène / runtime
Nature         : faiblesse
Gravité        : basse
Preuve         : Scene.cpp:513-517 (cache de visibilité armé avant les scripts) et LuaBindings.cpp:434-437
                 (ui.set_visible modifie gameVisible pendant cette fenêtre) ; Scene.cpp:624-626 (cache de
                 transforms armé avant la boucle des triggers 654-685, dont la téléportation SceneTrigger.cpp:
                 96-118 modifie des Transform).
Constat        : Un script qui masque une entité, ou un trigger qui téléporte le joueur, est rendu avec une frame de
                 retard. Pour la visibilité, c'est même le cas pour les descendants déjà mis en cache. C'est
                 contraire à la promesse écrite en Scene.h:508-511 (« Visibility flags don't mutate mid-pass »).
Comparaison    : —
Recommandation : corriger, effort S : armer le cache de visibilité après les scripts ; exécuter les triggers
                 avant prepareWorldTransforms ou invalider l'entrée du joueur téléporté.
Statut         : confirmé (code)
```

```text
ID             : C-18
Axe            : Scène / hiérarchie
Nature         : étrangeté
Gravité        : basse
Preuve         : Scene.cpp:2140 et 2259 (maxDepth = 64 dans getWorldTransform / isEffectivelyVisible), 2298
                 (détection de cycle de setParent limitée à 64) ; prepareWorldTransforms (2166-2222) sans limite.
Constat        : Au-delà de 64 niveaux, le monde calculé hors frame (gizmo, éditeur, getWorldTransform hors cache)
                 diffère du monde rendu, et setParent ne détecte plus un cycle. Le cas est rare (hiérarchies
                 générées) mais silencieux, avec un simple avertissement de log.
Comparaison    : —
Recommandation : surveiller, effort S : une seule constante, et un refus explicite de setParent au-delà.
Statut         : confirmé (code)
```

```text
ID             : E-11
Axe            : Éditeur / prefab
Nature         : faiblesse
Gravité        : basse
Preuve         : SceneHierarchy.cpp:444-445 (« Unlink Prefab » : removeComponent sans commande) ;
                 SceneHierarchy.cpp:418-427 (snapshot « after » de Update from Prefab pris sur la hiérarchie
                 aplatie par C-02, et iEntity potentiellement périmé dans la suite du menu) ;
                 EditorLayer.cpp:775 (double-clic : prefabAssetPath = filename seul, sous-dossier perdu, donc
                 « Update » introuvable ensuite, SceneHierarchy.cpp:410-415) ; EditorLayer.cpp:328-333 (les
                 chemins de scènes et prefabs sont résolus par Renderer::getTextureLibrary().find).
Constat        : Plusieurs actions de prefab ne s'annulent pas ou dépendent de la façon dont on a instancié
                 (glisser-déposer contre double-clic). La résolution de chemins passe par la bibliothèque de
                 textures, couplage surprenant entre l'éditeur et le renderer.
Comparaison    : —
Recommandation : corriger, effort S : commande pour Unlink, chemin relatif complet au double-clic, résolveur
                 d'assets unique (cf. A-07).
Statut         : confirmé (code)
```

```text
ID             : E-12
Axe            : Éditeur / Play
Nature         : faiblesse
Gravité        : basse
Preuve         : SceneDocument.cpp:127-187 et source/owlnest/runner/RunnerLayer.cpp:376-… (deux implémentations de
                 la résolution de niveau et de la téléportation) ; SceneDocument.cpp:153-165 (onEndRuntime appelé
                 avant l'ouverture du fichier : en cas d'échec de lecture, de parse ou d'apply, le document reste
                 en Play avec scripts et physique arrêtés).
Constat        : La logique de transition de niveau existe en double. L'éditeur et le jeu livré peuvent donc
                 diverger, et un niveau cible manquant ou corrompu fige la session de Play au lieu de l'arrêter
                 proprement.
Comparaison    : —
Recommandation : corriger, effort S : déplacer la transition dans le moteur (service de chargement de niveau)
                 et ne démonter l'ancienne scène qu'après succès.
Statut         : confirmé (code)
```

## Ordre de mise à jour d'une frame de jeu (constaté)

`Scene::onUpdateRuntime` (Scene.cpp:510-720), sans système enregistrable :

```mermaid
flowchart LR
    A[caches de passe vidés<br/>visibilité armée] --> B[NativeScript.onUpdate]
    B --> C[Lua on_update]
    C --> D[FlyCamera]
    D --> E[VoxelPlayer<br/>mouvement + collisions]
    E --> F[portes / murs raycast]
    F --> G[entrées Player]
    G --> H[Box2D step<br/>+ sync transforms]
    H --> I[EntityLink]
    I --> J[prepareWorldTransforms<br/>cache monde armé]
    J --> K[son : listener + sources]
    K --> L[triggers<br/>callbacks Lua]
    L --> M[sprites animés]
    M --> N[streaming voxel + rendu<br/>pile de renderers + transition]
```

Les callbacks de triggers (L) peuvent créer ou détruire des entités et téléporter. Ils s'exécutent donc après la
préparation des transforms (C-17) et en pleine itération de vue (C-01).

## Points de mesure proposés (pour l'agent benchmark)

Toutes les fonctions ci-dessous sont appelables sans GPU, à l'exception de `prepareWorldTransforms` et des chemins de
rendu. La sonde de cet audit montre qu'on peut compiler un test contre `libOwlEngine.so` en reprenant les drapeaux
de `owl_scene_tests_unit_test`. Pour l'éditeur, il suffit d'inclure les `.cpp` de `source/owlnest/sources/commands`.

| Point de mesure                       | Fonction(s)                                                | Tailles / formes suggérées                                     | Chiffre de cet audit (1 essai)     |
|---------------------------------------|------------------------------------------------------------|----------------------------------------------------------------|------------------------------------|
| Transform monde hors cache            | `Scene::getWorldTransform`                                 | N = 1k, 10k, 100k ; profondeur 1, 16, 64 ; arbre large (1 → N) | 10k, prof. ≤ 16 : 9,2 ms           |
| Préparation des transforms par frame  | `Scene::prepareWorldTransforms` (Null/GL/Vulkan)           | idem ; part CPU (2 unordered_map) contre part GPU              | non mesuré (requiert renderer)     |
| Visibilité héritée                    | `Scene::isEffectivelyVisible`                              | idem, avec et sans `m_inUpdatePass`                            | 10k : 2,1 ms                       |
| Copie de scène (Play)                 | `Scene::copy`                                              | N = 1k à 100k ; + 1 VoxelWorld de 64 / 512 chunks              | 10k : 4,1 ms                       |
| Sérialisation de scène                | `SceneSerializer::serializeToString`                       | N = 1k à 100k ; composants 3 / 8 / 15 par entité               | 10k : 282 ms, 2,7 Mio              |
| Chargement de scène                   | `parseBuffer` puis `applyParsed` (séparément)              | idem ; `sample_project/scenes/*.owl`                           | 10k : 1 492 ms                     |
| Snapshot undo                         | `SceneSerializer::serializeEntityToString`                 | entité simple, entité Tilemap 256², VoxelWorld 64 chunks       | 0,06-0,07 ms (simple)              |
| Restauration undo                     | `deserializeEntityFromString` / `SubtreeSnapshot::restore` | N scène = 1k, 10k, 100k ; sous-arbre k = 1, 10, 100            | 1 entité : 7-16 ms                 |
| Coût inspecteur par frame             | 2 × `serializeEntityToString` × composants ouverts         | entité 5 / 10 composants ; VoxelWorld authoré                  | dérivé : ~1 ms (opinion)           |
| Instanciation / mise à jour de prefab | `PrefabSerializer::instantiate`, `applyToInstance`         | prefab 1, 10, 100 entités ; scène hôte 1k / 10k                | non mesuré                         |
| Résolution UUID                       | `Scene::findEntityByUUID`                                  | succès indexé, échec (scan complet), après destroy             | non mesuré                         |
| Frame logique sans rendu              | `onUpdateRuntime(ts, false)`                               | 1k–10k entités avec Trigger, EntityLink, LuaScript             | non mesuré                         |
| Liens par nom                         | `updateEntityLinks` (rescan Tag)                           | L liens × N entités, cible absente vs présente                 | non mesuré (cible absente = crash) |
| Sauvegarde / chargement de partie     | `SaveManager::save` / `load`                               | scène 1k / 10k, avec PhysicBody dynamiques                     | non mesuré                         |
| Démarrage runtime                     | `onStartRuntime` (log interne déjà chronométré par phase)  | sample_project, nombre de scripts Lua                          | déjà journalisé par le moteur      |

## Comparaison brève avec l'état de l'art

Source : connaissance de l'agent, **non revérifiée en ligne le 2026-10-05**. À sourcer et dater par l'agent
`30-etat-de-l-art.md`.

| Sujet               | Owl                                                   | Godot 4                                          | Bevy                                               | Unity / Unreal                                                  |
|---------------------|-------------------------------------------------------|--------------------------------------------------|----------------------------------------------------|-----------------------------------------------------------------|
| Modèle de scène     | ECS EnTT + composant Hierarchy (UUID)                 | Arbre de nœuds, héritage de classes              | ECS archétypes, relation ChildOf/Children (Entity) | GameObject + composants / Actor + composants                    |
| Transforms monde    | Recalcul complet par frame, cache par passe, SSBO GPU | Dirty flag propagé, cache par nœud               | Système de propagation sur change detection        | Cache par objet invalidé à la modification                      |
| Ordonnancement      | Séquence codée en dur dans `onUpdateRuntime`          | process / physics_process par nœud, priorités    | Schedules, SystemSets, parallélisme automatique    | Ordre d'exécution configurable / tick groups                    |
| Destruction runtime | Immédiate, sans hook (C-01, C-08)                     | `queue_free` différé                             | Commands différées, hooks on_remove                | Destroy différé / MarkAsGarbage                                 |
| Prefabs             | Copie depuis fichier + rafraîchissement par composant | Scènes instanciées imbriquées, overrides stockés | Scènes (RON) sans overrides natifs                 | Nested prefabs + variants, overrides par propriété / Blueprints |
| Undo éditeur        | Snapshot YAML d'entité, détruire puis recréer         | UndoRedo par propriété ou méthode                | (éditeur en cours)                                 | Undo.RecordObject (diff sérialisé) / FScopedTransaction         |
| Sérialisation       | YAML texte, sans version, chargement ~150 µs/entité   | .tscn texte versionné + .scn binaire             | Réflexion + RON                                    | YAML versionné + import binaire / uasset binaire                |

## Pistes pour l'avenir

1. **Corriger d'abord la correction** (court terme, v0.2.x, effort total ~M) : C-01, C-02, C-03, C-04, E-01, E-02,
   avec la catégorie `owlnest_tests` (E-03) comme filet. Chaque correctif tient en quelques dizaines de lignes, et
   les sondes de cet audit sont déjà des tests de non-régression.
2. **Cycle de vie des entités** : file de commandes différées (create/destroy/reparent) vidée entre phases, et
   signaux EnTT `on_destroy` vers physique, son et Lua. C'est la brique préalable à tout ECS parallèle (axe K,
   multithreading).
3. **Références et assets par GUID** : `EntityRef` (UUID) à la place des noms, prefabs référencés par GUID d'asset.
   La même brique sert le pipeline d'assets (§5 du cadrage : « références d'assets par GUID plutôt que par chemin »).
4. **Systèmes et phases** : sortir de `Scene` le joueur voxel, le raycast, l'UI, les triggers et Victory/Death sous
   forme de systèmes enregistrés dans des phases nommées. On obtient un ordre de mise à jour explicite, testable et
   ouvert aux jeux tiers (prolonge A-02 et A-05).
5. **Transforms incrémentaux** : dirty flag par sous-arbre (ou tri topologique du storage `Transform` maintenu à
   setParent) au lieu du parcours complet par frame et des deux tables de hachage. Le gain est à mesurer d'abord
   (points de mesure ci-dessus).
6. **Undo par composant et en place** : snapshot de la clé YAML d'un seul composant, restauration sans recréation,
   regroupement de commandes. On obtient l'undo exact, une mémoire proportionnelle au changement et la base de la
   sélection multiple.
7. **Format de scène versionné + forme binaire cuite** : version de format et migrations dès maintenant. Format
   binaire (ou `entt::snapshot` + archive) pour le runtime et les `.owlpack`, le YAML restant le format source de
   l'éditeur.
8. **Prefabs de seconde génération** : imbrication, variants, surcharges par propriété détectées automatiquement par
   le diff de l'inspecteur, document d'édition isolé. Effort L, à démarrer après 3 et 6.
9. **Découpage d'`EditorLayer`** (E-07) dans une PR de réorganisation dédiée : GamePackager, RibbonBuilder,
   AssetOpener. Le packaging pourrait d'ailleurs devenir un outil en ligne de commande réutilisable par la CI.
