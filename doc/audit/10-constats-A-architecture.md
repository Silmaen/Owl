# Audit Owl — constats de l'axe A : architecture du moteur

> Périmètre : `source/owl/public` et `source/owl/private` (197 k lignes), revu le 2026-11-12 sur
> `Feature/sample-games-new` b4281d72. Lecture seule ; préprocesseur mesuré via `./docker_run.sh` (sans arbre de build,
> rayon de recompilation non remesuré). « confirmé » = vérifié dans le code ou mesuré. Contexte et méthode :
> `00-cadrage.md`.

## Résumé

- Frontière publique/privée tenue : seuls EnTT et ImGui sont visibles des headers publics, Jolt et Luau compris (A-06).
- `Scene` a doublé (4 723 + 3 127 lignes, 116 membres) : elle porte désormais aussi ~1 900 lignes de rendu (ombres,
  sondes, frame graph) en plus des règles de jeu (A-02) ; trois agrégateurs dépassent 4 700 lignes (A-16).
- `EngineContext` possède une partie des services orphelins, mais rendu, son, input et log restent statiques et
  `Application::get()` reste le chemin d'accès (A-07, A-08).
- Clip-space centralisé (`ClipSpace.h`) : la fuite GPU se réduit à `UiLayer` (A-09) ; coût de header en hausse (A-12).
- Registre de composants toujours alimenté par quatre listes manuelles, 83 composants (A-05) ; deux bibliothèques
  YAML cohabitent (A-22).
- Pistes : sortir le rendu et le gameplay de `Scene`, finir `EngineContext`, registre sans liste.

## Constats

Format : ID · nature · gravité · statut. `Reco` = recommandation et effort.

### Gravité haute

**A-02** · faiblesse · haute · confirmé
`Scene` cumule conteneur ECS, hiérarchie, ordonnancement, replay, streaming voxel, index spatial, origine flottante,
règles de jeu (Victory/Death) et maintenant l'orchestration du rendu (cascades d'ombres, ombres locales, sondes de
réflexion, environnement, post-process, frame graph) : 116 membres, 47 includes dans le header.
Preuve : `private/scene/Scene.cpp` (4 723 l., 125 méthodes), `public/scene/Scene.h` (3 127 l.) ; rendu
Scene.cpp:1072-2987 (`renderWithStack`, `recordLightPasses`, `recordReflectionProbePasses`, `render2D`,
`renderMeshes`…) ; Victory/Death Scene.h:1011-1012, Scene.cpp:653, 847-880.
Reco : corriger, L (Scene = données + hiérarchie ; rendu vers les couches de la pile, gameplay vers un module ou le
jeu) ; croiser B-26.

**A-06** · force · haute · confirmé
Aucun include Jolt, Luau, Taskflow, spdlog, yaml-cpp, rapidyaml, Vulkan, VMA, glad, GLFW ni AL dans le public (pimpl,
façades) ; seuls EnTT et ImGui (`transitive_headers=True`) traversent l'API.
Preuve : grep des includes `<…>` de `source/owl/public` (seuls `entt/` et `imgui.h` sont tiers) ; pimpl
`public/physics/PhysicCommand.h`, `core/Serializer.h:37` ; `private/physics/JoltWorld.h` ; `private/core/external/`
(`ryml.h`) ; `conanfile.py:125, 161`.
Reco : garder.

### Gravité moyenne

**A-05** · faiblesse · moyenne · confirmé
Le registre de composants est ouvert aux projets aval, mais les 83 composants du moteur sont déclarés par quatre
listes manuelles (concept `isComponent`, tuples `Copiable`, `Serializable`, `Optional`) à synchroniser entre elles et
avec l'inspecteur.
Preuve : `public/scene/component/components.h:7-230`, `private/scene/ComponentRegistry.cpp:30-33`,
`public/gui/component/render.h` (81 `renderProps`).
Reco : corriger, S/M (alimenter le registre depuis la déclaration de chaque composant).

**A-07** · faiblesse · moyenne · confirmé
Les références sérialisées passent par GUID (`gid:<guid>:<hint>`, registre et sidecars `.owlmeta`), mais
`AssetLibrary::find` résout encore par nom avec parcours récursif « premier trouvé gagne », et l'Application reste le
localisateur de services implicite (assets, polices, pack, scheduler, `EngineContext`).
Preuve : `public/data/assets/AssetReference.h:46-97`, `private/data/assets/AssetRegistry.cpp:232` ;
`public/data/assets/AssetLibrary.h:250-310` ; 104 `Application::get()` dans 39 fichiers du moteur, 172 dans l'éditeur ;
Application.h:191 (aucun contrôle).
Reco : corriger, M (résolution par le registre GUID dans `AssetLibrary`, injection explicite du contexte).

**A-08** · faiblesse · moyenne · confirmé
`EngineContext` (possédé par l'Application) prend désormais réglages, transitions, caches de meshes et de matériaux,
bibliothèques HUD, dialogues, séquences, arbres de comportement ; mais rendu, son, input et log restent des classes
statiques, 16 renderers gardent un état global de fichier, et 16 membres statiques sont nommés `m_` au lieu de `s_`
(`.claude/rules/cpp-style.md`).
Preuve : `public/app/EngineContext.h`, Application.h:486, 668 ; Renderer.h:202-224 (dont `RenderGraph`),
RenderCommand.h:416-420, SoundCommand.h:251-253, Input.h:236-238, SoundSystem.h:96-99, Log.h:209-213 ;
`g_Data`/`g_state` dans 16 fichiers de `private/renderer/` (Renderer2D.cpp:224, Renderer3D.cpp:206,
RendererRaycast.cpp:142, RendererTilemap.cpp:107, RendererParticles.cpp:99, RendererIsometric.cpp:382…) ; singletons
Vulkan `VulkanCore`, `VulkanHandler`, `MemoryAllocator`, `FrameProfiler`, `Descriptors`.
Reco : corriger, M (faire entrer rendu, son, input dans `EngineContext`, puis l'injecter).

**A-12** · faiblesse · moyenne · confirmé (préprocesseur mesuré), rayon de recompilation non remesuré
Tout header Owl paie ~80 k lignes (Core.h → Assert.h → Log.h → `<format>`) ; `Scene.h` ~184 k (+9 % depuis 2026-10) ;
la PCH ne profite pas aux consommateurs du paquet.
Preuve : `clang++ -std=c++23 -E` dans l'image : Core.h 79 504 l., Scene.h 183 853 l. ; Core.h:146, Assert.h:11,
Log.h:14 ; Scene.h : 47 includes. Rayon de 2026-10 (`ninja -t deps`) : Core.h 409 objets, Application.h 125,
Scene.h 87.
Reco : corriger à faible coût, S (Assert.h sans Log.h complet, déclarations anticipées, Scene.h allégé avec A-02).

**A-14** · force · moyenne · confirmé
Dossier == namespace respecté (exceptions voulues : Core.h, Macros.h, expected.h, math.h, EntryPoint.h, `linux64` ;
écart : `debug/TrackerClient.h`) ; `OWL_API` systématique sur les classes non templates.
Preuve : script de comparaison dossier / `namespace owl::<dossier>` sur `source/owl/public`.
Reco : garder.

**A-15** · force · moyenne · confirmé
La pile de rendu reste le point d'extension ouvert (couche déclarée par clé, configurée par scène, ordonnée) et
s'appuie maintenant sur un vrai frame graph (B-22). Mais les passes 3D (ombres, sondes, environnement, post-process)
sont enregistrées par `Scene`, pas par des couches.
Preuve : `public/renderer/RenderLayerFactory.h:31, 67`, `RenderStack.cpp:183-263`, `public/renderer/RenderGraph.h` ;
Scene.cpp:1072-1180.
Reco : garder et généraliser (passes 3D en couches, modèle pour les systèmes ECS, A-02).

**A-16** · faiblesse · moyenne · confirmé (comptage)
La dette de taille se concentre dans trois agrégateurs qui ont doublé depuis 2026-10 : bindings Lua 5 615 l., inspecteur
5 302 l., scène 4 723 l. (+ header 3 127) ; suivent le compilateur de scripts visuels (2 152) et `PhysicCommand.cpp`
(1 948).
Preuve : `wc -l` : `private/script/LuaBindings.cpp`, `private/gui/component/render.cpp`, `private/scene/Scene.cpp`,
`private/script/visual/VisualScriptCompiler.cpp`, `private/physics/PhysicCommand.cpp`.
Reco : corriger, M (découpage par domaine : un fichier de bindings et d'inspecteur par module, avec A-02 et A-05).

### Gravité basse

**A-09** · faiblesse · basse · confirmé
La correction clip-space est centralisée (`toActiveClipSpace`, Z inversé), mais `UiLayer` inclut les internes Vulkan
et teste le backend une dizaine de fois.
Preuve : `public/renderer/ClipSpace.h`, `private/scene/SceneCamera.cpp:71` ; `private/gui/UiLayer.cpp:21-22, 390-503`.
Reco : corriger, S (interface `GuiBackend` derrière `RenderAPI`) ; croiser B-07.

**A-11** · faiblesse · basse · confirmé (comptage) / opinion (diagnostic)
`shared<>` reste l'ownership par défaut, y compris pour des propriétaires uniques (état de renderer, couches) :
ownership illisible, comptages atomiques inutiles, une seule référence faible. Pas de défaut fonctionnel ni de coût
mesuré.
Preuve : public 310 `shared<` / 39 `uniq<` / 1 `weak<` ; privé 168 `mkShared` / 59 `mkUniq` ; Renderer2D.cpp:224,
Renderer3D.cpp:206, RenderLayerFactory.h:31.
Reco : corriger progressivement, M (uniq par défaut, shared pour les assets) ; mesurer avant les chemins chauds.

**A-20** · force · basse · confirmé
L'Application a un ordre de démarrage/arrêt explicite avec repli sur le son Null, et possède `EngineContext` (A-08).
Preuve : `private/app/Application.cpp:156-209, 368-386`.
Reco : garder et étendre aux sous-systèmes statiques.

**A-21** · étrangeté · basse · plausible
Noms et structures identiques à Hazel (`Application::get()`, `Layer`/`LayerStack`, `RenderCommand`/`RenderAPI`
statiques, `ScriptableEntity`, `Timestep`, `OWL_CORE_ASSERT`) : Owl en a hérité simplicité et limites (Scene
monolithique, singletons), et s'en écarte en bien (pimpl, A-06) comme en mal (A-02). Filiation déduite, non déclarée.
Preuve : EntryPoint.h, `scene/component/NativeScript.h:11`, Application.h:622, 642.
Reco : surveiller (contexte, pas d'action).

**A-22** · étrangeté · basse · confirmé
Deux bibliothèques YAML cohabitent : rapidyaml lit scènes, prefabs et snapshots (`YamlNode`), yaml-cpp écrit tout,
fusionne les prefabs et sert l'interface `Serializer` des composants ; la sauvegarde ré-émet par yaml-cpp ce qu'elle
vient de sérialiser (C-13).
Preuve : `conanfile.py:137, 141` ; `private/core/YamlNode.h:4` (44 fichiers) ; `YAML::` dans 129 fichiers ;
`PrefabSerializer.cpp:97, 341-357` ; `SaveManager.cpp:179, 202`.
Reco : surveiller, M (finir la bascule vers une seule bibliothèque, côté écriture compris : cf. C-10).

### Récapitulatif

| Gravité | Force | Faiblesse | Étrangeté | Total |
|---------|-------|-----------|-----------|-------|
| haute   | 1     | 1         | 0         | 2     |
| moyenne | 2     | 5         | 0         | 7     |
| basse   | 1     | 2         | 2         | 5     |
| total   | 4     | 8         | 2         | 14    |

## Pistes pour l'avenir (axe K)

- **K-A2 Contexte moteur explicite** (M, entamé : `EngineContext`) : y faire entrer rendu, son, input, log, puis
  l'injecter au lieu de `Application::get()` (A-07, A-08).
- **K-A4 Scene = données + systèmes** (L, après K-A2) : rendu en couches, gameplay sorti, systèmes par phase (A-02,
  A-15, B-26).
- **K-A5 Registre de types sans liste** (M) : clé, sérialisation, copie, inspecteur et binding Lua depuis la
  déclaration du composant (A-05, A-16).
- **K-A7 Assets par GUID jusqu'au chargement** (M, entamé : registre, références `gid:`) : `AssetLibrary` résout par
  le registre (A-07).
- **K-A8 Backend ImGui derrière `RenderAPI`** (S, axe B) : dernier reste de la fuite GPU (A-09, B-07).
- **K-A9 Hygiène des headers** (S/M) : Assert sans `<format>`, déclarations anticipées (A-12).

Ordre conseillé (opinion) : K-A8 et K-A9, puis K-A2 → K-A5, puis K-A4 en chantier de version.
