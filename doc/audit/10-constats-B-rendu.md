# Audit Owl — Axe B : rendu

> Revu le 2026-11-12 sur `Feature/sample-games-new` b4281d72. Périmètre : `source/owl/{public,private}/renderer/**`
> (backends `gpu/opengl`, `gpu/vulkan`, `gpu/null`), `engine_assets/shaders/**`, `test/unit/{render,renderer}_tests/**`
> et les appelants directs du rendu (`Scene.cpp`, `Viewport.cpp`, `RunnerLayer.cpp`, `UiLayer.cpp`).
> Lecture du code ; chiffres CPU tirés de `test/bench/baseline/linux-bench.json` (backend Null, aucune mesure GPU).
> « confirmé » = fait de code lu sans ambiguïté ; « plausible » = la conséquence dépend du GPU, du pilote ou d'une
> mesure (points PM-xx). Références externes de mémoire, non revérifiées (`30-etat-de-l-art.md`). Contexte :
> `00-cadrage.md`.

## Résumé

- Vulkan enregistre désormais une frame par command buffer (2 frames en vol, anneau transitoire, VMA, picking
  asynchrone) ; reste un encadrement de chaque transfert par deux barrières globales et quelques vidages synchrones
  (B-01).
- Un vrai frame graph (lectures/écritures déclarées, tri, élagage, pool de cibles) existe (B-22), mais c'est `Scene`
  qui enregistre les passes 3D (B-26).
- Tests d'image : 40 scènes de référence sur lavapipe/llvmpipe, présents dans l'image de build (B-28).
- Forces : Renderer2D instancié, tilemaps par chunks, voxel culé (B-08), Slang source unique précompilée (B-17),
  invariants Vulkan (B-21), profilage GPU (B-27).
- Restes : API publique teintée Vulkan (B-07), raycaster au pire cas (B-14), draw indirect à compteur sans appelant
  (B-16), MSAA limité à la swapchain (B-24), swapchain recréée de zéro (B-25).

Taille (lignes) : `private/renderer/gpu/vulkan` 11 565, `opengl` 4 069, `null` 1 483 ; `public/renderer` 13 648 ;
35 shaders Slang (8 compute).

Chemin d'un draw : `Renderer2D::drawQuad` remplit un `std::vector<QuadInstance>` (80 octets) ; `flush` écrit les SSBO
(`m_shadow`, puis `resolve()` vers une tranche de l'anneau transitoire de la frame), alloue un descriptor set par
draw ; `endFrame` soumet le command buffer unique avec la fence du slot.

## Constats

### Gravité moyenne

**B-01** · faiblesse · moyenne · confirmé (code), coût plausible (PM-01)
Dans une frame, chaque transfert (upload, clear hors passe, transition, copie, picking) est enregistré dans le command
buffer courant mais encadré de deux barrières mémoire `ALL_COMMANDS → ALL_COMMANDS` : le GPU se sérialise à chaque
fois. Hors frame (chargement), les transferts restent des one-shots synchrones ; `StorageBuffer::getData` et
`readColorAttachment` vident la frame et attendent la fence.
Preuve : `vulkan/internal/VulkanHandler.cpp:740-785` (`recordFullBarrier`, `recordTransfer`, `submitNow`),
725-735 (`flushFrame`) ; `internal/VulkanCore.cpp:541-577` ; `vulkan/StorageBuffer.cpp:111-112` ;
`vulkan/Framebuffer.cpp:270, 304, 350`.
Reco : corriger, M (barrières ciblées par ressource et par stage, déduites du frame graph ; synchronization2).

**B-08** · force · moyenne · confirmé, mesuré (CPU, backend Null)
Le rendu 2D est un renderer instancié piloté par SSBO (instance de 80 octets, matrices monde partagées par index) ;
tilemaps découpées en chunks GPU mis en cache et culés par chunk ; voxel avec culling CPU par chunk et transparent
trié.
Preuve : Renderer2D.cpp:58-71, 661, 815 ; RendererTilemap.cpp:160-282 ; RendererVoxel.cpp:459-474 ; bench
`renderer2d/quads_16_textures/10000` 0,63 ms, `renderer2d/circles/100000` 3,5 ms.
Reco : garder.

**B-17** · force · moyenne · confirmé
Une seule source Slang compile vers SPIR-V pour Vulkan et, via SPIRV-Cross, vers GLSL 450 pour OpenGL ; les shaders
du moteur sont précompilés au build (`OwlShaderBake`), les graphes de matériaux cuits à l'export.
Preuve : `utils/shaderFileUtils.cpp:146-222, 269` ; `source/tools/shader_bake/main.cpp` ; 35 shaders dans
`engine_assets/shaders/**` ; `GameExporter.cpp:269` ; `SlangCompute_test.cpp`, `shaderFileUtils_test.cpp`.
Reco : garder.

**B-22** · force · moyenne · confirmé
Ordre et activation des renderers pilotés par les données (projet, scène), au-dessus d'un frame graph : chaque passe
déclare ses lectures et écritures, le graphe trie (Kahn), élague les passes inutiles, alloue et recycle les cibles
transitoires. Il ne dérive ni barrières ni layouts (B-01).
Preuve : `RenderStack.cpp:183-263`, `public/renderer/RenderGraph.h:27-36`, `RenderGraph.cpp:211, 244-330` ;
`RenderGraph_test.cpp` (8 tests).
Reco : garder ; y brancher la synchronisation Vulkan (B-01).

**B-26** · faiblesse · moyenne · confirmé
Le frame graph est rempli par `Scene` : cascades d'ombres, ombres locales, occlusion, environnement, sondes de
réflexion, passes monde, post-process et encodage des ids sont enregistrés dans `Scene.cpp`, pas par les couches de
la pile ; le rendu 3D n'est pas un point d'extension et alourdit `Scene` (A-02).
Preuve : Scene.cpp:1072-1180 (`renderWithStack` : `graph.addPass`, `recordLightPasses`, `recordWorldPasses`…),
1286-2185 ; `m_shadowCascades`, `m_probeStates`, `m_postTargets` dans Scene.h ; `RenderGraph` statique dans
Renderer.h:222.
Reco : corriger, M/L (une couche 3D propriétaire de ses passes ; `Scene` ne fournit que les données).

**B-28** · force · moyenne · confirmé (code), exécution CI plausible
Les tests d'image rejouent 40 scènes de référence dans OwlRunner sur lavapipe/llvmpipe (2D, iso, raycast, voxel, PBR,
ombres, sondes, particules, skinning, HUD, mélange de styles) ; lavapipe et xvfb sont dans l'image de build, les tests
ne sont ignorés que sans eux.
Preuve : `test/unit/render_tests/RenderImage_test.cpp:141-147`, `test/unit/test_helper/assets/render_tests/scenes/`
(40 `.owl`) ; `test/unit/CMakeLists.txt:5, 107-126` ; `./docker_run.sh ls /usr/share/vulkan/icd.d` → `lvp_icd.json`,
`/usr/bin/xvfb-run`.
Reco : garder ; ajouter les validation layers « zéro message » (PM-09).

### Gravité basse

**B-07** · faiblesse · basse · confirmé
`beginBatch` et `nextSubpass` (notions Vulkan) restent dans l'API publique et la couche ImGui inclut les internes
Vulkan.
Preuve : `public/renderer/gpu/RenderAPI.h:247, 277` ; `private/gui/UiLayer.cpp:21-22`.
Reco : corriger, M (sortir ces deux appels de l'API publique, backend ImGui derrière RenderAPI, cf. A-09).

**B-14** · faiblesse · basse · confirmé (PM-07)
Le chemin GPU du raycaster lance un dispatch DDA puis un draw instancié couvrant le pire cas
(`numRays × kMaxHitsPerColumn`) au lieu d'un draw indirect.
Preuve : RendererRaycast.cpp:355-356, `utils/RaycastDDAPass.cpp`.
Reco : corriger, S/M (draw indirect alimenté par le compteur de hits).

**B-16** · étrangeté · basse · confirmé
OpenGL exige 4.5 ; `drawIndexedIndirect` à compteur n'existe qu'en 4.6 (ignoré avec un avertissement en 4.5) et n'a
plus d'appelant en production : le rendu mesh passe par `drawIndexedIndirectAt`.
Preuve : `opengl/RenderAPI.cpp:52-55, 214-233` ; `vulkan/RenderAPI.cpp:163` ; Renderer3D.cpp:899 ; aucun appel de
`drawIndexedIndirect(` hors `gpu/`.
Reco : retirer la variante à compteur ou la couvrir par un test d'image ; décider du sort d'OpenGL (P-09).

**B-21** · force · basse · confirmé
Invariants Vulkan propres (règles dans `.claude/rules/renderer.md`) : pipelines dédupliqués et refcountés, descriptor
sets recyclés à la fence, mémoire par VMA, deux frames en vol, libérations différées au serial de frame, objets nommés.
Preuve : `internal/VulkanHandler.cpp:269-273, 441-442, 499, 557` ; `internal/MemoryAllocator.cpp:64, 97` ;
`internal/DescriptorRing.cpp`, `internal/FrameRing.cpp` ; `internal/VulkanCore.cpp:157-164`.
Reco : garder.

**B-24** · étrangeté · basse · confirmé
Instance Vulkan en 1.3 ; le multisampling n'est accepté que sur la swapchain, toutes les cibles hors écran (donc tout
le chemin du frame graph) restent à 1 échantillon.
Preuve : `internal/VulkanCore.cpp:36` ; `vulkan/Framebuffer.cpp:68-71, 463, 566, 584` ;
`internal/VulkanHandler.cpp:324`.
Reco : surveiller (l'antialiasing passe par le post-process, à documenter).

**B-25** · faiblesse · basse · confirmé
La recréation de swapchain repart d'une swapchain nulle. MAILBOX n'est plus choisi qu'avec vsync coupé ou en mode
faible latence.
Preuve : `vulkan/Framebuffer.cpp:421` (`oldSwapchain = VK_NULL_HANDLE`) ; `internal/VulkanCore.cpp:462-465`.
Reco : surveiller ; passer l'ancienne swapchain, S.

**B-27** · force · basse · confirmé
Profilage GPU sur les deux backends : timestamps par command buffer (Vulkan) ou par frame (OpenGL), compteurs de
soumissions, d'attentes de fence et de temps d'attente de rythme.
Preuve : `vulkan/internal/FrameProfiler.cpp:91-98`, `FrameProfiler.h:131-137` ; `VulkanHandler.cpp:685, 697-711` ;
`opengl/RenderAPI.cpp:280-311`.
Reco : garder ; descendre au grain de la passe du frame graph (P-12).

### Décompte

| Nature    | Moyenne | Basse | Total |
|-----------|---------|-------|-------|
| Force     | 4       | 2     | 6     |
| Faiblesse | 2       | 3     | 5     |
| Risque    | 0       | 0     | 0     |
| Étrangeté | 0       | 2     | 2     |
| **Total** | 6       | 7     | 13    |

Owl a désormais des objets pipeline (`PipelineState`), des liaisons explicites (`bindTextures`) et un frame graph,
mais ni bind groups, ni barrières dérivées, ni rendu multithread, contrairement à SDL_GPU, NVRHI, Diligent ou WebGPU
(opinion, de mémoire).

## Points de mesure (agent benchmark)

Mesures GPU via `./docker_run.sh --gui` (RTX 5000 Ada, Intel UHD, llvmpipe/lavapipe), `linux-clang-release`, au moins
5 répétitions, médiane et écart interquartile.

- **PM-01** (B-01) : compteurs `FrameProfiler` (submits, fence waits) et temps GPU par frame, avec et sans les barrières
  globales, dans l'éditeur sur `engine_demo/scenes/platformer_house.owl` et une scène 3D (ombres + sondes).
- **PM-03** (B-08) : `renderer2d/*` de `test/bench` étendus à GL/Vulkan, 1k/10k/100k quads.
- **PM-07** (B-14) : `RendererRaycast` GPU vs CPU, `engine_demo/scenes/raycast_demo.owl`, 320/640/1280 rayons.
- **PM-08** (B-01, B-25) : `Viewport::onUpdate` complet, GL vs Vulkan, éditeur vs runner, MAILBOX vs FIFO.
- **PM-09** (B-24, B-28) : validation + sync validation sur les 40 scènes des tests d'image, objectif zéro message.
- **PM-11** (B-01) : chargement complet de `engine_demo/textures` (one-shots hors frame).

Outils à consigner dans `00-cadrage.md` si absents : `perf`, `ltrace`, RenderDoc (lavapipe et xvfb présents).

## Pistes pour l'avenir

Effort S (jours), M (semaines), L (mois), pour un mainteneur seul.

- **P-01** (M, largement fait) Synchronisation Vulkan : reste à remplacer les barrières globales par des barrières
  dérivées du graphe, synchronization2, timeline semaphores (B-01).
- **P-04** (L) Bind groups et RHI : la décision RHI tierce (SDL_GPU, NVRHI, wgpu-native/Dawn) reste ouverte.
- **P-06** (M, frame graph en place) Passes 3D sorties de `Scene` vers des couches, barrières et layouts dérivés
  (B-22, B-26).
- **P-08** (L) Rendu piloté par GPU, bindless : culling mesh et indirect déjà en production, à étendre.
- **P-09** (M) Sort d'OpenGL ; MoltenVK, cible web (P-10).
- **P-10** (L) WebGPU / Web : P-04, Slang vers WGSL, plus de vidage synchrone, build wasm (axe G).
- **P-11** (L) Rendu sur thread dédié : P-04, fin des singletons `RenderCommand`/`Renderer2D` (A-08).
- **P-12** (S) Outils GPU au grain de la passe : timestamps par passe du graphe, Tracy GPU, marqueurs RenderDoc.

Ordre proposé : P-12, puis P-01 (barrières dérivées) avec P-06, puis trancher P-04 avant P-10 et P-11.
