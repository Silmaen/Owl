# Audit Owl — constats de l'axe A : architecture du moteur

> Périmètre : `source/owl/public` et `source/owl/private`, revu le 2026-10-20 sur `main` 4e1a6a10. Lecture seule ;
> mesures via `./docker_run.sh` sur `output/build/linux-clang-release`. « confirmé » = vérifié dans le code ou mesuré.
> Contexte et méthode : `00-cadrage.md`.

## Résumé

- Frontière publique/privée tenue : seuls EnTT et ImGui sont visibles des headers publics (A-06).
- `Scene` (1 446 + 2 339 lignes) porte encore des règles de jeu (A-02, A-16).
- ~25 singletons à cycle de vie dispersé, `Application::get()` omniprésent (A-07, A-08, A-20).
- Fuites de l'abstraction GPU et ownership `shared<>` par défaut (A-09, A-11) ; coût de header élevé (A-12).
- Registre de composants ouvert mais alimenté par listes manuelles (A-05) ; pile de rendu extensible (A-15).
- Pistes : contexte moteur explicite, gameplay hors de `Scene`, registre sans liste, conventions GPU centralisées.

## Constats

Format : ID · nature · gravité · statut. `Reco` = recommandation et effort.

### Gravité haute

**A-02** · faiblesse · haute · confirmé
Scene cumule conteneur ECS, hiérarchie, ordonnancement des sous-systèmes et rendu, plus des règles de jeu
(victoire/mort) ; les systèmes sont enregistrés par phase mais le gameplay reste dans le moteur.
Preuve : `source/owl/private/scene/Scene.cpp` (2 339 l.), `public/scene/Scene.h` (1 446 l.), Scene.h:610-611,
Scene.cpp:557-585, `SystemSchedule.h`.
Reco : corriger, M (Scene = données + hiérarchie ; gameplay vers un module ou le sample).

**A-06** · force · haute · confirmé
Aucun include Box2D, Lua, Taskflow, spdlog, yaml-cpp, Vulkan, glad, GLFW ni AL dans le public (pimpl, façades) ;
remplacer une de ces dépendances ne casse pas l'API.
Preuve : grep des includes de `source/owl/public` ; pimpl `PhysicCommand.h`, `ScriptEngine.h`, `Serializer.h`,
`Scheduler` ; spdlog confiné à `private/core/Log.cpp` ; `private/core/external/`.
Reco : garder.

### Gravité moyenne

**A-05** · faiblesse · moyenne · confirmé
Le registre de composants est ouvert aux projets aval, mais les composants du moteur sont déclarés par des listes
manuelles à synchroniser avec lui.
Preuve : `public/scene/ComponentRegistry.h`, `public/scene/component/components.h`.
Reco : corriger, S/M (alimenter le registre depuis la déclaration de chaque composant).

**A-07** · faiblesse · moyenne · confirmé
L'Application sert de localisateur de services implicite (assets, polices, fenêtre, pack, scheduler) ; la résolution
d'asset se fait par nom avec parcours récursif, « premier trouvé gagne ». `AssetLibrary` met les noms en cache : le
parcours n'a lieu qu'au premier chargement ou dans `list()`.
Preuve : 79 `Application::get()` dans 32 fichiers ; Application.h:176 (aucun contrôle) ; `public/data/assets/AssetLibrary.h`.
Reco : corriger, M (service AssetFileSystem indexé, injection explicite) ; coordonner avec les références d'assets
par GUID (axe K).

**A-08** · faiblesse · moyenne · confirmé
~25 singletons ou classes statiques ; ordre d'init/destruction explicite seulement pour rendu/son/fenêtre (A-20), le
reste vit jusqu'à la destruction statique, avec fuites d'état entre tests et réentrance. 13 membres statiques
nommés `m_` au lieu de `s_` (`.claude/rules/cpp-style.md:68`).
Preuve : Log.h:149-153, Application.h:420, Input.h:122-124, RenderCommand.h:283, Renderer.h:194-198,
SoundCommand.h:222, SettingsManager.h:201-205, SaveManager.h:117, ScreenTransition.h:214-228, UiInputSystem.h:51-53 ;
`g_Data`/`g_state` dans Renderer2D.cpp:117, Renderer3D.cpp:38, RendererVoxel.cpp:43, RendererRaycast.cpp:83,
RendererTilemap.cpp:59, BackgroundRenderer.cpp:39 ; cycle de vie : EntryPoint.h, Application.cpp:128-312,
Scene.cpp:366/417/504 ; aucun propriétaire pour SettingsManager, ScreenTransition, UiInputSystem, RenderLayerFactory.
Reco : corriger, M (`EngineContext` qui possède et ordonne les sous-systèmes).

**A-09** · faiblesse · moyenne · confirmé
L'abstraction GPU fuit : la correction clip-space Vulkan existe en trois copies (dont une inverse aussi `(1,3)`, sans
effet pour une ortho symétrique) et `UiLayer` connaît Vulkan ; 9 `switch` de backend dans les factories.
Preuve : `private/renderer/CameraOrtho.cpp:30-37`, `private/scene/SceneCamera.cpp:50-56`,
`private/renderer/CameraEditor.cpp:84-90` ; `private/gui/UiLayer.cpp:19` (VulkanHandler.h, 6 tests de backend l.115-209) ;
`private/renderer/gpu/*.cpp`.
Reco : corriger, S (`RenderAPI::clipSpaceCorrection()`, interface `GuiBackend`) ; croiser avec l'axe B (B-07).

**A-12** · faiblesse · moyenne · confirmé
Tout header Owl paie ~78 k lignes (Core.h → Assert.h → Log.h → `<format>`) ; `Scene.h` ~170 k ; modifier
`Application.h` ou `RenderLayer.h` recompile ~30 % de l'arbre ; la PCH ne profite pas aux consommateurs du paquet.
Preuve : `clang++ -E -H` : Core.h 276 headers / 77 953 l., Scene.h 168 483 l. (74 EnTT), owl.h 197 543 l. ;
`ninja -t deps` (rayon de recompilation, objets) : Core.h 409, vectors.h 408, Texture.h 166, Application.h 125 (sur 393),
RenderLayer.h 122, Scene.h 87 ; `private/owlpch.h` sur 367 objets.
Reco : corriger à faible coût, S (Assert.h sans Log.h complet, déclarations anticipées) ; mesure de build par l'axe G.

**A-14** · force · moyenne · confirmé
Dossier == namespace respecté (exceptions voulues : Core.h, expected.h, `linux64`) ; `OWL_API` systématique sur les
classes non templates.
Preuve : script `ns.py`.
Reco : garder.

**A-15** · force · moyenne · confirmé
La pile de rendu est le seul vrai point d'extension ouvert : couche déclarée par clé, configurée par scène, ordonnée ;
embryon de render graph. Scene.cpp la contourne encore en partie.
Preuve : `public/renderer/RenderLayerFactory.h`, `RenderStack.h` ; Scene.cpp:19, Scene.h:572-609.
Reco : garder et généraliser (modèle pour les systèmes ECS, A-02).

**A-16** · force · moyenne · confirmé (heuristique)
Hors Scene et quelques chargeurs, fonctions courtes ; la dette de taille tient à 4 agrégateurs (scène, inspecteur,
bindings, raycaster).
Preuve : `Scene::onUpdateRuntime` (Scene.cpp:423) la plus longue ; Scene.cpp 2 339, LuaBindings.cpp 2 277,
gui/component/render.cpp 1 662, RendererRaycast.cpp 1 195.
Reco : garder ; traiter les agrégateurs avec A-02.

### Gravité basse

**A-11** · faiblesse · basse · confirmé (comptage) / opinion (diagnostic)
`shared<>` est l'ownership par défaut, y compris pour des propriétaires uniques (état de renderer, application,
couches) : ownership illisible, comptages atomiques inutiles, aucune référence faible vers les assets. Pas de défaut
fonctionnel ni de coût mesuré.
Preuve : public 180 `shared<` / 30 `uniq<` / 1 `weak<` ; privé 111 `mkShared` / 38 `mkUniq` ; Renderer2D.cpp:117,
Renderer3D.cpp:38, RendererRaycast.cpp:83, RenderLayerFactory.h:14 ; `.claude/rules/cpp-style.md:173-181`.
Reco : corriger progressivement, M (uniq par défaut, shared pour les assets) ; mesurer avant les chemins chauds.

**A-20** · force · basse · confirmé
Pour ce qu'elle possède, l'Application a un ordre de démarrage/arrêt explicite avec repli sur le son Null ; base de
l'`EngineContext` (A-08).
Preuve : `private/app/Application.cpp`, l.200-208.
Reco : garder et étendre aux sous-systèmes orphelins.

**A-21** · étrangeté · basse · plausible
Noms et structures identiques à Hazel (`Application::get()`, `Layer`/`LayerStack`, `RenderCommand`/`RenderAPI`
statiques, `ScriptableEntity`, `Timestep`, `OWL_CORE_ASSERT`) : Owl en a hérité simplicité et limites (Scene
monolithique, singletons), et s'en écarte en bien (pimpl, A-06) comme en mal (A-02). Filiation déduite, non déclarée.
Preuve : EntryPoint.h, `scene/component/NativeScript.h:20`, `mp_imGuiLayer`.
Reco : surveiller (contexte, pas d'action).

### Récapitulatif

| Gravité | Force | Faiblesse | Étrangeté | Total |
|---------|-------|-----------|-----------|-------|
| haute   | 1     | 1         | 0         | 2     |
| moyenne | 3     | 5         | 0         | 8     |
| basse   | 1     | 1         | 1         | 3     |
| total   | 5     | 7         | 1         | 13    |

## Pistes pour l'avenir (axe K)

- **K-A2 Contexte moteur explicite** (M, sans prérequis) : `EngineContext` possédant Log, rendu, son, input, assets,
  scheduler (A-07, A-08).
- **K-A4 Scene = données + systèmes** (L, après K-A2) : gameplay sorti, systèmes par phase, systèmes parallèles (A-02).
- **K-A5 Registre de types sans liste** (M) : clé, sérialisation, copie, inspecteur et binding Lua depuis la
  déclaration du composant (A-05).
- **K-A7 Assets par service et GUID** (M/L, après K-A2) : AssetFileSystem puis références par GUID (A-07).
- **K-A8 Conventions GPU centralisées** (S, axe B) : clip-space et backend ImGui derrière `RenderAPI` (A-09).
- **K-A9 Hygiène des headers** (S/M) : Assert sans `<format>`, déclarations anticipées (A-12).

Ordre conseillé (opinion) : K-A8 et K-A9, puis K-A2 → K-A5, puis K-A4 et K-A7 en chantiers de version.
