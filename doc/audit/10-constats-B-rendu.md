# Audit Owl — Axe B : rendu

> Revu le 2026-10-20 sur `main` 4e1a6a10. Périmètre : `source/owl/{public,private}/renderer/**` (backends
> `gpu/opengl`, `gpu/vulkan`, `gpu/null`), `engine_assets/shaders/**`, `test/unit/render_tests/**` et les appelants
> directs du rendu (`Scene.cpp`, `Viewport.cpp`, `RunnerLayer.cpp`, `UiLayer.cpp`).
> Lecture du code uniquement, aucune mesure. « confirmé » = fait de code lu sans ambiguïté ; « plausible » = la
> conséquence dépend du GPU, du pilote ou d'une mesure (points PM-xx). Références externes de mémoire, non
> revérifiées (`30-etat-de-l-art.md`). Contexte : `00-cadrage.md`.

## Résumé

- Vulkan soumet transitions, clears et copies hors passe en one-shot synchrone (B-01) ; l'image de swapchain est
  écrite avant acquisition effective et les sémaphores de fin de rendu sont mal indexés (B-19).
- Les SSBO passent par un `m_shadow` copié pendant l'enregistrement (B-04) ; risque conditionnel avec B-01.
- L'abstraction GPU fuit Vulkan (B-07) ; OpenGL exige 4.5 (B-16).
- Forces : Renderer2D instancié et tilemap/voxel économes (B-08), Slang source unique (B-17), invariants Vulkan
  propres (B-21), pile de rendu pilotée par les données (B-22).
- Tests d'image ignorés sans lavapipe (B-06) ; briques GPU-driven sans appelant (B-20).
- Priorités : synchronisation Vulkan, tests d'image, puis seulement le rendu 3D moderne.

Taille (lignes) : `private/renderer/gpu/vulkan` 10 595, `opengl` 3 561, `null` 1 335 ; `public/renderer` 7 145 ;
15 shaders Slang (3 compute).

Chemin d'un draw : `Renderer2D::drawQuad` remplit un `std::vector<QuadInstance>` ; `flush` ouvre un batch, écrit les
SSBO (`m_shadow`, puis `resolve()`), alloue et écrit un descriptor set (`RendererDescriptors.cpp`) ; `endBatch` ferme la
passe et soumet avec la fence de la frame.

## Constats

### Gravité moyenne

**B-01** · faiblesse · moyenne · confirmé (code), coût plausible (PM-01)
Les one-shots (transitions de layout, copies, clears hors passe) soumettent un command buffer et attendent sa fence :
le CPU attend le GPU à chaque appel et les frames en vol ne couvrent pas ces chemins.
Preuve : `private/renderer/gpu/vulkan/internal/VulkanCore.cpp:546-570` (`endSingleTimeCommands`),
`internal/utils.cpp`, `vulkan/Framebuffer.cpp:257-351`.
Reco : corriger, M (enregistrer transitions, clears et copies dans le command buffer courant ; anneau de staging
par frame ; picking asynchrone à N+2).

**B-08** · force · moyenne · confirmé (PM-03)
Le rendu 2D est un vrai renderer instancié piloté par SSBO : 4 draws par flush, push_back de 80 octets par sprite,
matrices monde partagées par index ; toutes les tilemaps en un draw ; voxel avec origine cuite, culling CPU par chunk
et transparent trié.
Preuve : Renderer2D.cpp:37-79, 333-374 ; Scene.cpp:983-998 ; RendererTilemap.cpp:184-262 ; RendererVoxel.cpp:99-106, 196-237.
Reco : garder.

**B-17** · force · moyenne · confirmé
Une seule source Slang compile vers SPIR-V pour les deux backends, sans GPU en CI : plus de GLSL divergents.
Preuve : `shaderFileUtils.cpp:203-302`, `engine_assets/shaders/**` (15 shaders), `SlangCompute_test.cpp`,
`Renderer3D_test.cpp:52-70`, `shaderFileUtils_test.cpp`.
Reco : garder.

**B-19** · risque · moyenne · confirmé (code), symptôme plausible (PM-09)
Dans le runner, l'image de swapchain est écrite par un clear one-shot sans attendre le sémaphore d'acquisition
(attendu seulement au premier batch) ; `renderFinished` est indexé par frame en vol, pas par image de swapchain, cas
signalé par les validation layers récentes.
Preuve : Application.cpp:342, `internal/VulkanHandler.cpp:412-414, 533-539`, `owlnest/runner/RunnerLayer.cpp:274`,
`internal/VulkanCore.cpp:548` (`waitSemaphoreCount = 0`), `vulkan/Framebuffer.h:163-164`, Framebuffer.cpp:612.
Reco : corriger, S (clear par loadOp dans la première passe ; `renderFinished` par image de swapchain).

### Gravité basse

**B-04** · risque · basse · confirmé (code), conditionnel
Pendant l'enregistrement un SSBO n'est pas écrit in place ; le VkBuffer unique n'est écrit directement que hors
enregistrement, ce qui suppose que le GPU ne le lit plus. Le risque se réveille si la synchronisation de B-01 change.
Preuve : `vulkan/StorageBuffer.cpp:20-32, 44-95`.
Reco : surveiller, avec B-01.

**B-06** · faiblesse · basse · confirmé
Les tests d'image headless (lavapipe/llvmpipe) sont ignorés (`GTEST_SKIP`) sans lavapipe : la suite ne vérifie alors
aucune image ni résultat de compute. Le backend Null ne fait que des dispatch no-op.
Preuve : `test/unit/render_tests/RenderImage_test.cpp`, `null/ComputeShader.cpp`.
Reco : corriger, S (lavapipe garanti dans l'image CI, tests compute à oracle CPU, validation layers « zéro message »).

**B-07** · faiblesse · basse · confirmé
`beginBatch` et `nextSubpass` (notions Vulkan) sont dans l'API publique et la couche ImGui inclut le VulkanHandler interne.
Preuve : `public/renderer/gpu/RenderAPI.h:225, 255` ; `private/gui/UiLayer.cpp:20`.
Reco : corriger, M (sortir ces deux appels de l'API publique, backend ImGui derrière RenderAPI, cf. A-09).

**B-14** · faiblesse · basse · confirmé (PM-07)
Le chemin GPU du raycaster lance un dispatch DDA puis un draw instancié couvrant le pire cas
(`numRays × kMaxHitsPerColumn`) au lieu de `drawIndexedIndirect`.
Preuve : RendererRaycast.cpp:301, `utils/RaycastDDAPass.cpp`, RenderAPI.h:291.
Reco : corriger, M (draw indirect, sprites testés sur GPU).

**B-16** · faiblesse · basse · confirmé
OpenGL exige 4.5 ; le draw indirect avec compteur n'existe qu'en 4.6 et `drawIndexedIndirect` est ignoré avec un
avertissement sur un pilote 4.5.
Preuve : `opengl/RenderAPI.cpp:45, 189-195`, `opengl/Shader.cpp:55, 105`.
Reco : décider du sort d'OpenGL (P-09) ; exiger 4.6 ou fournir un repli.

**B-20** · étrangeté · basse · confirmé
Culling GPU, tri bitonique et draw indirect avec compteur sont écrits et branchés aux deux backends, sans appelant en
production ni test d'exécution réelle (B-06) : bonnes briques, code mort à ce jour.
Preuve : grep `BitonicSortPass|FrustumCullingPass|drawIndexedIndirect` ; RendererVoxel.cpp:196, 214 (fonctions CPU) ;
`vulkan/RenderAPI.cpp:176-193`, `opengl/RenderAPI.cpp:137-158`.
Reco : surveiller ; les garder seulement si un test lavapipe les couvre.

**B-21** · force · basse · confirmé
Invariants Vulkan propres (règles dans `.claude/rules/renderer.md`) : pipelines dédupliqués et refcountés, un
descriptor set par draw recyclé à la fence, profondeur dynamique, objets nommés, démontage ordonné.
Preuve : `internal/VulkanHandler.cpp:76-78, 171-206, 294-296, 365-380, 511-512`, `DescriptorRing.cpp:81-116`,
`RendererDescriptors.cpp:82-83`, `vulkan/RenderAPI.cpp:36-39`.
Reco : garder ; base du socle à reconstruire (B-01, B-04).

**B-22** · force · basse · confirmé
Ordre et activation des renderers pilotés par les données, par projet et par scène. Ce n'est pas un render graph
(aucune ressource déclarée, même cible, une soumission par couche) mais le bon point d'ancrage.
Preuve : `RenderStack.cpp:146-202`, Scene.cpp:923-939, `RenderLayerFactory.h`.
Reco : garder, faire évoluer (P-06).

**B-24** · étrangeté · basse · confirmé
Instance Vulkan en 1.3, multisampling fixé à 1.
Preuve : `internal/VulkanCore.cpp:36`, `internal/VulkanHandler.cpp:323`.
Reco : surveiller.

**B-25** · faiblesse · basse · confirmé
La recréation de swapchain repart d'une swapchain nulle ; MAILBOX est préféré dès qu'il existe, le GPU tourne sans
limite (opinion : consommation sur portable).
Preuve : `internal/VulkanCore.cpp:459-460`, `vulkan/Framebuffer.cpp:412`.
Reco : surveiller ; passer l'ancienne swapchain, S.

### Décompte

| Nature    | Moyenne | Basse | Total |
|-----------|---------|-------|-------|
| Force     | 2       | 2     | 4     |
| Faiblesse | 1       | 5     | 6     |
| Risque    | 1       | 1     | 2     |
| Étrangeté | 0       | 2     | 2     |
| **Total** | 4       | 10    | 14    |

Owl n'a ni objet pipeline, ni bind groups explicites, ni render graph, ni rendu multithread, contrairement à
SDL_GPU, NVRHI, Diligent ou WebGPU (opinion, de mémoire) : face à l'extension du socle Vulkan, adopter l'un d'eux est
une option sérieuse pour un mainteneur seul (P-04).

## Points de mesure (agent benchmark)

Mesures GPU via `./docker_run.sh --gui` (RTX 5000 Ada, Intel UHD, llvmpipe/lavapipe), `linux-clang-release`, au moins
5 répétitions, médiane et écart interquartile.

- **PM-01** (B-01) : `endSingleTimeCommands` ; `vkWaitForFences` par frame (uprobe `perf`) dans l'éditeur sur
  `platformer_house.owl`, souris dans/hors viewport.
- **PM-03** (B-08) : `Renderer2D::drawQuad/drawString/flush` ; `bench/cases/Renderer2DBench.cpp` étendu à GL/Vulkan, 1k/10k/100k quads.
- **PM-07** (B-14) : `RendererRaycast::drawTilemapWalls` GPU vs CPU, `raycast_demo.owl`, 320/640/1280 rayons.
- **PM-08** (B-01, B-16, B-25) : `Viewport::onUpdate` complet, GL vs Vulkan, éditeur vs runner, MAILBOX vs FIFO.
- **PM-09** (B-19, B-24) : validation + sync validation, 300 frames par scène de `sample_project`, 2 couches 2D.
- **PM-11** (B-01) : `Texture2D::setData`, `TextureLibrary`, chargement complet de `sample_project/textures`.

Outils à consigner dans `00-cadrage.md` si absents : `perf`, `ltrace`, RenderDoc, lavapipe.

## Pistes pour l'avenir

Effort S (jours), M (semaines), L (mois), pour un mainteneur seul.

- **P-01** (L) Socle de synchronisation Vulkan : command buffer par frame, anneau de staging, suivi des layouts,
  timeline semaphores, synchronization2 (B-01, B-04, B-19).
- **P-04** (L) Pipeline et liaisons explicites, ou RHI tierce (SDL_GPU, NVRHI, wgpu-native/Dawn) : décision préalable.
- **P-05** (M) Tests d'image et de compute sur lavapipe, filet de P-01 et P-04 (B-06).
- **P-06** (L) Render graph : `RenderStack` en front-end ; après P-01 et P-04.
- **P-07** (L) Rendu 3D moderne (meshes, PBR, ombres, HDR, post-process) ; après P-06.
- **P-08** (L) Rendu piloté par GPU, bindless ; briques de B-20 testées (P-05).
- **P-09** (M) Sort d'OpenGL ; Vulkan fiabilisé (P-01), MoltenVK, cible web (P-10).
- **P-10** (L) WebGPU / Web : P-04, Slang vers WGSL, plus de vidage synchrone, build wasm (axe G).
- **P-11** (L) Rendu sur thread dédié : P-04, fin des singletons `RenderCommand`/`Renderer2D`.
- **P-12** (S) Outils GPU : requêtes timestamp, Tracy GPU, marqueurs RenderDoc.

Ordre proposé : P-12 et P-05, puis P-01 avec B-04, puis trancher P-04 (qui conditionne P-06 à P-11) avant d'investir
dans P-07 : un rendu 3D bâti sur l'abstraction actuelle hériterait de B-07.
