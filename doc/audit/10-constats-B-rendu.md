# Audit Owl — Axe B : rendu

> **Statut : première passe, 2026-10-05, vérification croisée faite le 2026-10-05** (lignes `Vérification`).
> Rédigé par l'agent de l'axe B. Périmètre : `source/owl/{public,private}/renderer/**` (backends `gpu/opengl`,
> `gpu/vulkan`, `gpu/null`), `engine_assets/shaders/**`, `test/renderer_tests/**`, et les appelants
> directs du rendu (`Scene.cpp`, `Viewport.cpp`, `RunnerLayer.cpp`, `UiLayer.cpp`).

**Méthode.** Lecture du code uniquement, aucun build ni aucune exécution : aucune mesure dans ce fichier.
Chaque constat distingue :

- **constaté** : lu dans le code, à la ligne citée ;
- **opinion** : jugement du rédacteur, signalé comme tel ;
- **à mesurer** : effet de performance déduit du code, renvoyé à la section « Points de mesure proposés ».

Le champ `Statut` vaut `confirmé` quand le fait de code est lu sans ambiguïté, `plausible` quand la
conséquence (bogue visible, coût réel) dépend du GPU, du pilote ou d'une mesure. Les références externes
viennent de la connaissance du rédacteur. Elles n'ont pas été revérifiées en ligne le 2026-10-05 et
restent à confirmer par l'agent « état de l'art ».

## Résumé

1. Le backend Vulkan vide la file GPU (`vkQueueWaitIdle`, `vkDeviceWaitIdle`) une dizaine de fois par
   frame dans l'éditeur : CPU et GPU travaillent en série, les deux frames en vol n'existent que sur le papier.
2. Sa correction tient à ces vidages et à l'attente de fence à chaque batch. UBO, SSBO et buffers
   d'instances sont uniques, ou réécrits en place, sans versionnage par draw : passer à de vraies frames en
   vol ferait apparaître des courses (vérification : risque conditionnel, B-04 abaissé à moyenne).
3. Le contenu des attachements entre deux passes de rendu d'une même frame n'est pas garanti (`loadOp`
   `DONT_CARE`, transitions depuis `UNDEFINED`). Ça marche sur NVIDIA par chance, pas par construction.
4. La limite « dernière écriture gagne » de `Renderer3D::drawMesh` existe toujours. Elle n'est qu'un
   cas d'un défaut général : un `setData` d'UBO n'est pas ordonné par rapport aux draws sous Vulkan,
   alors qu'il l'est sous OpenGL.
5. L'abstraction GPU est un plus petit dénominateur qui fuit Vulkan : `beginBatch`, `nextSubpass`,
   descripteurs déclarés par clé de chaîne, slot de texture ignoré, aucun objet pipeline (topologie
   déduite du nom de shader).
6. Point fort : Renderer2D est instancié, avec 4 draws par flush et les matrices monde de la scène dans
   un SSBO. La tilemap tient en un draw, le voxel cuit l'origine des chunks et trie le transparent.
7. Slang en source unique pour deux backends est un bon choix, mais les compute shaders ne passent pas
   par le cache SPIR-V : chaque lancement GL ou Vulkan recompile le DDA. Le coût « ~50 s » est réfuté par
   la mesure (74 ms à froid, ~20 ms par shader, 20-mesures.md P-13) : B-05 abaissé à basse.
8. Les tests ne tournent que sur le backend Null, où un dispatch compute ne fait rien : aucun shader
   compute ni aucune image rendue n'est vérifié.
9. Des briques GPU-driven existent (draw indirect, culling GPU, tri bitonique) mais rien en production
   ne s'en sert. `WorldTransformPass` recalcule sur GPU des matrices que le CPU vient de calculer.
10. Priorités : socle de synchronisation Vulkan (frames en vol, anneau d'upload, anneau d'uniformes),
    allocateur mémoire, tests d'image sur lavapipe/llvmpipe, puis seulement le rendu 3D moderne.

## Cartographie rapide

| Élément                                    | Taille (lignes) | Remarque                                            |
|--------------------------------------------|-----------------|-----------------------------------------------------|
| `private/renderer/gpu/vulkan/**`           | 8 174           | 3 × OpenGL ; pas de VMA, 3 gestions de descripteurs |
| `private/renderer/gpu/opengl/**`           | 2 607           | DSA partiel, SPIR-V via `glSpecializeShader`        |
| `private/renderer/gpu/null/**`             | 1 211           | No-op ; SSBO miroir hôte                            |
| `public/renderer/**` (en-têtes)            | 5 114           | 86 méthodes `virtual` dans `public/renderer/gpu`    |
| `private/renderer/*.cpp` + `utils/*.cpp`   | 4 607           | `RendererRaycast.cpp` seul : 1 187                  |
| Shaders Slang (`engine_assets/shaders/**`) | 13 fichiers     | 4 compute, 9 graphiques                             |
| Commits touchant `private/renderer`        | 49              | `git log --oneline -- source/owl/private/renderer`  |

Chemin d'un draw (constaté) : `Renderer2D::drawQuad` remplit un `std::vector<QuadInstance>`
(`Renderer2D.cpp:461-492`). `flush` (`Renderer2D.cpp:310-376`) ouvre un batch (`beginBatch` : attente de
fence, reset du command buffer, `vkCmdBeginRenderPass`, `VulkanHandler.cpp:462-513`), écrit les SSBO
(`vkMapMemory` + `memcpy` + `vkUnmapMemory`, `vulkan/StorageBuffer.cpp:48-66`), alloue un descriptor
set dans l'anneau (`DescriptorRing.cpp:81-108`) et l'écrit (`RendererDescriptors.cpp:208-310`). Chaque
draw lie pipeline et set (`VulkanHandler.cpp:602-619`) ; `endBatch` ferme la passe et soumet avec la
fence de la frame (`VulkanHandler.cpp:515-562`).

## Constats

Classement par gravité (haute, puis moyenne, puis basse). Une force reçoit une gravité selon son
importance pour le moteur.

### Gravité haute

```text
ID             : B-01
Axe            : Rendu
Nature         : faiblesse
Gravité        : haute
Preuve         : source/owl/private/renderer/gpu/vulkan/internal/VulkanCore.cpp:481-505 (endSingleTimeCommands :
                 vkQueueSubmit puis vkQueueWaitIdle) ; appelé par transitionImageLayout (internal/utils.cpp:150-155),
                 copyBuffer (internal/utils.cpp:68-75), copyBufferToImage/copyImageToBuffer (utils.cpp:177-210) ;
                 Framebuffer::bind/unbind : une transition one-shot par attachement couleur (vulkan/Framebuffer.cpp:67-104) ;
                 clearAttachment (vulkan/Framebuffer.cpp:245-297), appelé par RenderCommand::clear
                 (internal/VulkanHandler.cpp:384) et par Viewport.cpp:216 ; readPixel à chaque frame survolée
                 (Viewport.cpp:283 -> vulkan/Framebuffer.cpp:169-240 : vkAllocateMemory + 2 transitions + 1 copie) ;
                 ComputeShader::dispatch (vulkan/ComputeShader.cpp:231-246), appelé à chaque frame par
                 WorldTransformPass (Scene.cpp:626, 727, 807) ; VertexBuffer::setData (vulkan/Buffer.cpp:84-110), appelé
                 à chaque frame par la tilemap (RendererTilemap.cpp:251-252) ; StorageBuffer::getData -> vkDeviceWaitIdle
                 (vulkan/StorageBuffer.cpp:77), appelé deux fois par frame par le raycast (RendererRaycast.cpp:261-268).
Constat        : Constaté : une frame d'éditeur Vulkan vide la file GPU au moins 10 fois (2 transitions au bind,
                 2 clears, 1 compute, 3 pour readPixel, 2 au unbind), plus 1 par tilemap et 3 par raycast. Les
                 "2 frames en vol" de la swapchain (internal/VulkanHandler.cpp:152) sont donc annulées, et le
                 framebuffer de l'éditeur n'a qu'un command buffer et une fence (vulkan/Framebuffer.cpp:23-27, 45).
                 Le coût réel est à mesurer (PM-01).
Comparaison    : bgfx, sokol_gfx, SDL_GPU et WebGPU enregistrent transitions, clears et copies dans le command buffer
                 de la frame, ou dans un command buffer d'upload synchronisé par sémaphore. Aucun ne vide la file en
                 régime établi.
Recommandation : corriger, effort L. Enregistrer transitions, clears (loadOp CLEAR ou vkCmdClearAttachments) et
                 copies dans le command buffer courant ; ajouter un anneau de staging par frame en vol ; lire le pixel
                 de picking en asynchrone (lecture à N+2) ; supprimer le readback du raycast.
Statut         : confirmé (code) ; coût plausible, à mesurer
Vérification   : Réfutation échouée : endSingleTimeCommands fait toujours vkQueueWaitIdle (VulkanCore.cpp:502), le FB
                 éditeur a 2 attachements couleur (RGBA8 + RedInteger), d'où 2+2+1+3+2 = 10 vidages. De plus beginBatch
                 attend la fence de la frame à chaque batch (VulkanHandler.cpp:470) : les batchs d'une frame sont
                 sérialisés même sans ces vidages.
```

```text
ID             : B-02
Axe            : Rendu
Nature         : risque
Gravité        : haute
Preuve         : vulkan/Framebuffer.cpp:497 (couleur : loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE) ; vulkan/Framebuffer.cpp:486
                 (profondeur : loadOp = CLEAR) ; une passe de rendu par batch (internal/VulkanHandler.cpp:492-500, 515-519) ;
                 plusieurs batchs par frame sur le même framebuffer : un flush Renderer2D par couche (Scene.cpp:923-939),
                 débordement (Renderer2D.cpp:394-397), raycast (RendererRaycast.cpp:251), voxel puis 2D (Scene.cpp:901-916),
                 ImGui (gui/UiLayer.cpp:211-217) ; transitions depuis VK_IMAGE_LAYOUT_UNDEFINED après rendu
                 (vulkan/Framebuffer.cpp:75-76 au bind, 99-100 au unbind, 217-218 dans readPixel).
Constat        : Constaté : chaque nouveau batch déclare le contenu couleur précédent "indéfini" et efface la
                 profondeur. Unbind et readPixel transitionnent depuis UNDEFINED une image qui vient d'être rendue,
                 ce que la spécification autorise à jeter. Opinion : cela marche sur NVIDIA en desktop parce que le
                 pilote garde la mémoire, mais un GPU à tuiles (MoltenVK/macOS, Android, certains Intel ou Mesa) peut
                 effacer les couches précédentes. La profondeur du voxel est perdue pour la 2D qui suit (sans effet
                 visible aujourd'hui, la 2D ne teste pas la profondeur).
Comparaison    : les render graphs (Frostbite FrameGraph, GDC 2017 ; Unreal RDG) déduisent loadOp, storeOp et layouts
                 de l'usage déclaré. SDL_GPU et WebGPU imposent un loadOp explicite par passe.
Recommandation : corriger, effort M. loadOp LOAD pour les batchs 2..n (deux render passes compatibles : clear et
                 load) ; suivre le layout courant de chaque image au lieu de partir d'UNDEFINED ; à terme, une passe
                 par couche déclarée dans un graphe (voir Pistes).
Statut         : confirmé (code) ; symptôme plausible (non observé sur NVIDIA)
Vérification   : Réfutation échouée : loadOp couleur DONT_CARE (Framebuffer.cpp:497) et une render pass par batch
                 (VulkanHandler.cpp:492-500). Pire que décrit : le clear lui-même (vkCmdClearColorImage hors passe,
                 Framebuffer.cpp:262, 293) précède une passe DONT_CARE, il n'est donc pas garanti non plus.
```

```text
ID             : B-03
Axe            : Rendu
Nature         : faiblesse
Gravité        : haute
Preuve         : vulkan/internal/RendererDescriptors.cpp:156-172 (setUniformData = memcpy dans l'unique buffer mappé de
                 la frame) ; vulkan/UniformBuffer.cpp:395 (paramètre offset ignoré) ; opengl/UniformBuffer.cpp:26
                 (glNamedBufferSubData, ordonné par le pilote entre les draws) ; Renderer3D.cpp:126-127 (drawMesh écrit le
                 modèle par draw) ; Renderer2D.cpp:292-299 (beginScene écrit la caméra) avant l'attente de fence du batch
                 précédent (internal/VulkanHandler.cpp:470) ; couches successives : Scene.cpp:923-939,
                 Renderer2DLayer.cpp:48-56.
Constat        : Constaté : sous Vulkan, plusieurs écritures d'un même UBO dans une frame ne sont pas versionnées.
                 Plusieurs drawMesh donnent "dernière écriture gagne" (limite connue, toujours présente). Deux couches 2D
                 (World puis Screen) font une course : la caméra de la couche 2 est écrite pendant que le GPU peut encore
                 lire le batch 1. Sous OpenGL, les deux cas sont corrects. Les deux backends divergent donc en silence,
                 et la règle "une caméra 2D par frame" (.claude/rules/renderer.md) est une conséquence de ce défaut,
                 pas une propriété voulue.
Comparaison    : sokol_gfx (sg_apply_uniforms) et SDL_GPU (SDL_PushGPUVertexUniformData, SDL 3.2, janvier 2025)
                 copient les uniformes dans un anneau par frame avec offset dynamique ; WebGPU utilise des offsets
                 dynamiques de bind group ; NVRHI/Diligent versionnent les buffers "volatile/dynamic".
Recommandation : corriger, effort M. Anneau d'uniformes par frame en vol (UNIFORM_BUFFER_DYNAMIC + offset par draw)
                 ou push constants pour la matrice modèle ; c'est le prérequis des static meshes 3D.
Statut         : confirmé
Vérification   : Réfutation échouée : l'UBO n'est doublé que par frame en vol (index = frame courante de la swapchain,
                 constant dans la frame, VulkanHandler.cpp:661) ; Renderer2DLayer::onBeginFrame -> beginScene fait le
                 memcpy (RendererDescriptors.cpp:170) avant l'attente de fence de beginBatch, pendant que le batch
                 précédent peut encore lire.
```

```text
ID             : B-04
Axe            : Rendu
Nature         : risque
Gravité        : moyenne
Preuve         : vulkan/StorageBuffer.cpp:20-32, 48-66 (un seul VkBuffer HOST_VISIBLE par SSBO, réécrit en place) ;
                 Renderer2D.cpp:345, 353, 361, 369 (SSBO d'instances réécrits à chaque flush) ; Renderer2D.cpp:327
                 (mondes transitoires) ; vulkan/Buffer.cpp:84-110 (buffer d'instances de tilemap réécrit par copie) ;
                 vulkan/ComputeShader.cpp:208-229 (descriptor set compute unique, réécrit par vkUpdateDescriptorSets) ;
                 vulkan/Framebuffer.h:136 et vulkan/Framebuffer.cpp:569 ("samples" = nombre de frames en vol, 1 hors swapchain).
Constat        : Constaté : aucune ressource écrite par le CPU n'est dupliquée par frame en vol, sauf les UBO
                 (RendererDescriptors.cpp:142-153). Aujourd'hui, les vidages de B-01 rendent ces réécritures sûres.
                 Corriger B-01 seul introduirait des courses écriture-après-lecture sur les SSBO, les instances et le set
                 compute. Le backend est correct par accident de synchronisation, pas par conception.
Comparaison    : SDL_GPU ("cycling" des buffers à l'écriture), Diligent (USAGE_DYNAMIC), bgfx (transient buffers par frame).
Recommandation : corriger, effort M, avec B-01 et dans la même PR : buffers dynamiques à N copies (une par frame en vol)
                 ou anneau linéaire, et descriptor sets compute pris dans l'anneau existant.
Statut         : confirmé (code) ; mécanisme nuancé
Vérification   : Fait confirmé (un VkBuffer par SSBO, StorageBuffer.cpp:20-32), mais dans Renderer2D::flush les setData
                 suivent beginBatch (Renderer2D.cpp:321), qui attend la fence de la frame (VulkanHandler.cpp:470) : la
                 sûreté vient surtout de cette attente par batch, pas seulement des vidages de B-01. La course
                 n'apparaît que si l'on introduit de vraies frames en vol ; risque conditionnel à un chantier futur,
                 gravité abaissée de haute à moyenne.
```

```text
ID             : B-05
Axe            : Rendu
Nature         : faiblesse
Gravité        : basse
Preuve         : vulkan/ComputeShader.cpp:72-79 et opengl/ComputeShader.cpp:46 (compileSlangToSpirv sans cache) ;
                 RendererRaycast.cpp:307-308 (RaycastDDAPass::init dans Renderer::initShaders) et
                 utils/RaycastDDAPass.cpp:31 (ComputeShader::create) ; Scene.cpp:2170-2171 (WorldTransformPass à la
                 première frame) ; shaderFileUtils.cpp:185-194 (session globale Slang paresseuse) ;
                 .claude/rules/slang-shaders.md ("la première compilation Slang d'un processus prend ~50 s").
Constat        : Constaté : le cache SPIR-V (hash du source, shaderFileUtils.cpp:155-182) ne couvre que les shaders
                 graphiques. Le shader compute DDA est compilé à chaque démarrage GL ou Vulkan, même si la scène n'a pas
                 de raycast, ce qui recrée la session globale Slang. Si le chiffre de ~50 s par processus est juste, un
                 cache chaud ne sert à rien au démarrage. À mesurer (PM-02). Par ailleurs, la clé de cache
                 (std::hash du source seul) ignore la version de Slang, les macros et les modules importés : une montée
                 de version de Slang réutilise un SPIR-V périmé.
Comparaison    : bgfx (shaderc), Godot (cache par hash du source et de la version), Unreal (DDC) compilent hors ligne
                 ou mettent en cache toute variante. Slang peut embarquer son core module précompilé.
Recommandation : corriger, effort S à M. Passer les compute shaders par le même cache ; ajouter à la clé la version de
                 Slang, le profil, les macros et les dépendances ; compiler hors ligne au build (slangc) pour les
                 paquets ; ne créer RaycastDDAPass qu'à la demande.
Statut         : confirmé (code) ; coût réfuté par la mesure
Vérification   : Faits de code confirmés (compute sans cache, clé = std::hash du source seul), mais la prémisse
                 « ~50 s » est fausse : 20-mesures.md P-13 mesure 74 ms à froid et 17-22 ms par shader. Recompiler le DDA coûte
                 ~20 ms par démarrage ; aucun shader du moteur n'utilise import/#include, le défaut de clé reste
                 théorique hors montée de Slang. Gravité abaissée de haute à basse.
```

```text
ID             : B-06
Axe            : Rendu
Nature         : faiblesse
Gravité        : haute
Preuve         : test/renderer_tests/*.cpp (tous RenderAPI::Type::Null, par ex. BitonicSortPass_test.cpp:18,
                 WorldTransformPass_test.cpp:16, Renderer2D_test.cpp) ; null/ComputeShader.cpp:20-21 (dispatch no-op) ;
                 BitonicSortPass_test.cpp:70-90 (le test vérifie que les données ne sont PAS triées sur Null) ;
                 seuls tests de shader : compilation Slang (SlangCompute_test.cpp, Renderer3D_test.cpp:52-70).
Constat        : Constaté : aucun test n'exécute un backend réel, ne compare une image ni ne vérifie le résultat d'un
                 compute shader (tri bitonique, culling, DDA, transformations monde). Les tests Renderer2D vérifient les
                 compteurs CPU. Les bogues de B-02 à B-04 et les régressions visuelles passées (UBO binding 0 en OpenGL,
                 descripteurs Vulkan) sont hors de portée de la suite.
Comparaison    : bgfx (exemples comparés à des images de référence), Godot (tests de rendu en CI), Mesa et wgpu
                 (CTS sur lavapipe/llvmpipe) ; l'environnement Docker voit déjà llvmpipe (01-environnement.md §1).
Recommandation : corriger, effort M. Tests d'image headless sur lavapipe (Vulkan) et llvmpipe (GL) : rendu offscreen,
                 lecture du framebuffer, comparaison à une tolérance près ; tests compute à oracle CPU ; validation
                 layers activées et "zéro message" comme critère.
Statut         : confirmé
Vérification   : Réfutation échouée : grep de Type::OpenGL|Vulkan dans test/ ne trouve qu'application_test.cpp:82
                 (sérialisation d'AppParams, aucun rendu) ; les tests compute vérifient uniquement la compilation Slang
                 ou l'aller-retour hôte sur Null.
```

### Gravité moyenne

```text
ID             : B-07
Axe            : Rendu
Nature         : faiblesse
Gravité        : moyenne
Preuve         : public/renderer/gpu/RenderAPI.h:178-238 (beginBatch, beginTextureLoad, endTextureLoad, nextSubpass dans l'API
                 publique) ; public/renderer/gpu/RendererDescriptors.h:71-143 (déclaration de descripteurs Vulkan-only,
                 bloc actif en thread_local, clé de chaîne) ; public/renderer/gpu/UniformBuffer.h:57 (create(size, binding,
                 iRenderer)) ; vulkan/Texture.cpp:54-60 (bind(slot) : slot ignoré, l'ordre d'appel fait le slot) ;
                 internal/VulkanHandler.cpp:226 (topologie LINE_LIST si le shader s'appelle "line") ; VulkanHandler.cpp:266-284
                 (blend codé en dur) ; VulkanHandler.h:195 (iDoubleSided = true par défaut, jamais surchargé : culling
                 toujours désactivé) ; gui/UiLayer.cpp:211-214 (le module gui inclut VulkanHandler interne) ;
                 shaderFileUtils.cpp:124-153 + vulkan/Shader.cpp:162 (réflexion spirv-cross calculée puis jetée, layouts
                 recopiés à la main, par ex. Renderer2D.cpp:179-201).
Constat        : Constaté : l'abstraction n'a ni objet pipeline (blend, topologie, cull, formats), ni bind group, ni
                 command list. L'état passe par des bascules globales (setDepthTest, setDepthMask) et des conventions
                 implicites : ordre des bind de textures, clé de renderer en chaîne, ouverture du batch avant les binds
                 (.claude/rules/renderer.md). Le front-end Vulkan suit le modèle "état global OpenGL" plus des points
                 d'entrée Vulkan. Opinion : c'est le plus petit dénominateur commun avec des fuites, ni une RHI moderne,
                 ni une API GL pure.
Comparaison    : sokol_gfx, SDL_GPU, WebGPU, NVRHI et Diligent exposent tous un PSO immuable et un groupe de liaisons
                 explicite ; bgfx encode l'état par draw dans une clé triable.
Recommandation : remplacer à terme, effort L : introduire PipelineDesc et BindGroup, ou adopter une RHI (voir Pistes).
                 Court terme, effort S : sortir la topologie du nom de shader, brancher la réflexion sur la création des
                 layouts.
Statut         : confirmé
Vérification   : Réfutation échouée : Texture2D::bind(uint32_t) ignore le slot (vulkan/Texture.cpp:54-60), topologie via
                 iPipeLineName == "line" (VulkanHandler.cpp:226), et les deux seuls appels de pushPipeline
                 (vulkan/DrawData.cpp:50, 98) gardent iDoubleSided par défaut, donc cull NONE partout.
```

```text
ID             : B-08
Axe            : Rendu
Nature         : force
Gravité        : moyenne
Preuve         : Renderer2D.cpp:37-79 (instances std430 vérifiées par static_assert), Renderer2D.cpp:343-374 (4 draws
                 instanciés par flush, quel que soit le nombre de quads) ; Scene.cpp:983-998 + Renderer2D.cpp:333-334 (les
                 sprites de scène référencent la matrice monde par index dans un SSBO partagé, sans copie par quad) ;
                 RendererTilemap.cpp:184-262 (toutes les tilemaps en un draw instancié) ; RendererVoxel.cpp:99-106
                 (origine cuite dans les sommets), 196-237 (culling CPU par chunk, transparent trié d'arrière en avant).
Constat        : Constaté : le rendu 2D est un vrai renderer instancié, piloté par SSBO, pas un batcher de sommets à
                 la Hazel. Le coût par sprite côté CPU se réduit à un push_back de 80 octets. Les contournements voxel sont
                 documentés et cohérents. À mesurer (PM-03).
Comparaison    : même famille que les renderers 2D instanciés de Bevy (sprites) et du 2D de Godot 4 (batching par
                 instances).
Recommandation : garder.
Statut         : confirmé
```

```text
ID             : B-09
Axe            : Rendu
Nature         : faiblesse
Gravité        : moyenne
Preuve         : Renderer2D.cpp:30, 119-135 (4 096 matrices transitoires par batch : tout quad sans worldIndex et chaque
                 glyphe en consomme une, Renderer2D.cpp:577) ; Renderer2D.cpp:468-472 + vulkan/Texture.cpp:49-52 (recherche
                 de texture linéaire, comparaison par dynamic_cast) ; Renderer2D.cpp:414-417 et 429-430 (std::vector alloué
                 à chaque drawRect et drawPolyLine) ; Renderer2D.cpp:137-166, 500 (copie UTF-8 vers Latin-1 à chaque
                 drawString, glyphes > U+00FF remplacés par '?') ; public RendererDescriptors::ScopedActive(const
                 std::string&) construit une chaîne à chaque appel (gpu/RendererDescriptors.cpp:71).
Constat        : Constaté : la capacité affichée (20 000 quads par batch, Renderer2D.cpp:26) ne vaut que pour les
                 sprites de scène. Le texte, le HUD, les gizmos et les quads à transformation explicite forcent un flush
                 toutes les 4 096 instances ; sous Vulkan chaque flush coûte un submit, une attente de fence et un
                 effacement de profondeur. Le texte est limité au Latin-1.
Comparaison    : SDL_GPU, bgfx : buffers transitoires dimensionnés par frame, pas par batch.
Recommandation : corriger, effort S : dimensionner le SSBO transitoire comme les instances (20 000), indexer les
                 textures par identifiant (table de hachage ou slot mis en cache), supprimer les vecteurs temporaires,
                 décoder l'UTF-8 vers des codepoints.
Statut         : confirmé ; coût à mesurer (PM-03)
Vérification   : Réfutation échouée : g_maxTransientWorldsPerBatch = 4096 (Renderer2D.cpp:30) et chaque glyphe appelle
                 allocateTransientWorld (Renderer2D.cpp:577) ; utf8ToLatin1 remplace aussi toute séquence de 3 ou 4
                 octets par '?'.
```

```text
ID             : B-10
Axe            : Rendu
Nature         : étrangeté
Gravité        : moyenne
Preuve         : Renderer2D.cpp:338-374 (ordre fixe : fond, tilemaps, quads, cercles, lignes, texte) ;
                 opengl/RenderAPI.cpp:63-65 et internal/VulkanHandler.h:203 (profondeur désactivée en 2D) ; Scene.cpp:983-998
                 (sprites parcourus dans l'ordre de stockage EnTT, sans tri) ; seul tri 2D : les Canvas UI
                 (Scene.cpp:1983).
Constat        : Constaté : l'ordre d'affichage 2D vient de l'ordre de stockage EnTT et du type de primitive. Un cercle
                 est toujours dessiné au-dessus d'un sprite, et la coordonnée z d'un sprite ne change rien à l'ordre. Le
                 commentaire "2D is painter-ordered" (opengl/RenderAPI.cpp:63) suppose un ordre que rien ne garantit.
Comparaison    : Godot (z_index, y-sort), Unity (sorting layers, order in layer), Bevy (tri par z) ; bgfx trie par clé
                 de draw.
Recommandation : corriger, effort M : clé de tri (couche, z, matériau) puis tri des instances avant upload, ou
                 profondeur activée avec un pré-tri du transparent.
Statut         : confirmé
Vérification   : Réfutation échouée : aucun registry.sort ni tri d'instances dans scene/ et renderer/ hors Canvas
                 (Scene.cpp:1983). Seule atténuation : l'ordre des couches de RenderStack donne un ordre grossier entre
                 couches.
```

```text
ID             : B-11
Axe            : Rendu
Nature         : faiblesse
Gravité        : moyenne
Preuve         : vulkan/internal/utils.cpp:79-111 (createBuffer : un vkAllocateMemory par buffer) ; vulkan/Buffer.cpp:92-109
                 (staging alloué puis libéré à chaque setData) ; vulkan/Texture.cpp:64-123 (staging par upload, 3 vidages
                 de file) ; vulkan/Framebuffer.cpp:183-213 (staging alloué à chaque readPixel, donc à chaque frame) ;
                 vulkan/StorageBuffer.cpp:57-65 (vkMapMemory/vkUnmapMemory à chaque écriture) ;
                 internal/Descriptors.cpp:180-182 (une allocation par texture) ; depmanager.yml (ni VMA ni allocateur).
Constat        : Constaté : aucune sous-allocation. Un monde voxel (2 meshes par chunk, VB et IB séparés,
                 RendererVoxel.cpp:99-106) consomme 4 allocations par chunk, alors que maxMemoryAllocationCount vaut
                 couramment 4 096. Le mapping persistant n'est utilisé que pour les UBO (RendererDescriptors.cpp:149).
Comparaison    : VMA (AMD GPUOpen, v3) est la norme de fait ; Diligent, NVRHI, SDL_GPU et wgpu sous-allouent tous.
Recommandation : remplacer par VMA via DepManager, effort M ; buffers HOST_VISIBLE mappés en permanence ; anneau de
                 staging partagé.
Statut         : confirmé ; plafond d'allocations plausible sur un grand monde voxel (PM-06)
Vérification   : Réfutation échouée sur le code. Nuance : 4 096 est le minimum garanti par la spécification et la valeur
                 courante des pilotes Windows ; certains pilotes Linux annoncent bien plus, le plafond dépend donc de la
                 plateforme.
```

```text
ID             : B-12
Axe            : Rendu
Nature         : étrangeté
Gravité        : moyenne
Preuve         : Scene.cpp:2166-2223 (prepareWorldTransforms calcule worldMat = parent * local sur CPU pour chaque
                 entité, cpuWorlds) puis WorldTransformPass.cpp:60-95 (upload des locales et dispatch compute) ;
                 engine_assets/shaders/world_transform/slang/world_transform.slang (O(profondeur) par thread, plafonné à
                 64 sauts) ; Scene.cpp:626, 727, 807 (à chaque frame d'update) ; findEntityByUUID par parent et par enfant
                 (Scene.cpp:2200, 2216).
Constat        : Constaté : les matrices monde sont calculées deux fois par frame, sur CPU puis sur GPU, et le GPU
                 tronque silencieusement au-delà de 64 niveaux. Sous Vulkan la passe GPU ajoute un vidage de file
                 (B-01). Le résultat CPU suffirait : il pourrait être envoyé directement dans le SSBO des mondes.
Comparaison    : la plupart des moteurs (Godot, Bevy) propagent les transforms sur CPU par niveau et envoient le résultat.
Recommandation : corriger, effort S : envoyer cpuWorlds, ne garder la passe compute qu'en benchmark ; remplacer
                 findEntityByUUID par un index entt::entity dans Hierarchy (axe C).
Statut         : confirmé ; coût à mesurer (PM-04)
Vérification   : Réfutation échouée : prepareWorldTransforms remplit déjà m_worldTransformCache avec le monde CPU
                 (Scene.cpp:2213) avant mp_worldTransformPass->compute ; le shader plafonne à hops < 64
                 (world_transform.slang:57).
```

```text
ID             : B-13
Axe            : Rendu
Nature         : faiblesse
Gravité        : moyenne
Preuve         : RendererTilemap.cpp:142-171 (appendLayer : toutes les cellules de toutes les couches, à chaque frame, sans
                 culling) ; RendererTilemap.cpp:25, 242-245 (troncature à 16 384 cellules par frame, avertissement à chaque
                 frame) ; RendererTilemap.cpp:251-252 -> vulkan/Buffer.cpp:84-110 (staging alloué + copie + vidage de file,
                 pendant l'enregistrement de la passe Renderer2D) ; RendererTilemap.cpp:191 (std::vector de slots alloué
                 à chaque flush).
Constat        : Constaté : une tilemap statique est reconstruite et renvoyée chaque frame. Au-delà de 128 × 128
                 cellules visibles (toutes couches confondues), les cellules en trop disparaissent. Sous Vulkan,
                 l'upload contredit la règle "pas de création de ressource GPU dans une passe" (.claude/rules/renderer.md).
Comparaison    : Godot TileMap (quadrants mis en cache, rebâtis à la modification), LDtk/Tiled + moteurs : chunks
                 statiques avec culling.
Recommandation : corriger, effort M : buffer d'instances persistant par tilemap, marqué sale à l'édition, découpé en
                 chunks cullés ; instances dans un SSBO plutôt qu'un vertex buffer copié.
Statut         : confirmé ; coût à mesurer (PM-05)
Vérification   : Réfutation échouée : appendLayer parcourt toute la grille à chaque flush sans culling. Précision : la
                 limite de 16 384 porte sur les cellules non vides (tileIdx < 0 ignoré), visibles à l'écran ou non.
```

```text
ID             : B-14
Axe            : Rendu
Nature         : faiblesse
Gravité        : moyenne
Preuve         : RendererRaycast.cpp:251 (Renderer2D::nextBatch forcé) ; RendererRaycast.cpp:660 + utils/RaycastDDAPass.cpp:97-128
                 (dispatch compute = vidage) ; RendererRaycast.cpp:261-268 (relecture GPU vers CPU des compteurs et du
                 z-buffer à chaque frame, vkDeviceWaitIdle dans vulkan/StorageBuffer.cpp:77) ; RendererRaycast.cpp:292-294
                 (numRays × 8 instances dessinées quel que soit le nombre de touches, RaycastDDAPass.h:110).
Constat        : Constaté : le chemin GPU du raycaster synchronise CPU et GPU trois fois par frame. La relecture sert
                 aux statistiques et au z-buffer des sprites, calculé sur CPU. Le draw instancié couvre le pire cas au lieu
                 d'utiliser drawIndexedIndirect, qui existe (RenderAPI.h:257).
Comparaison    : un raycaster GPU classique garde le z-buffer sur GPU (texture 1D) et teste les sprites dans le
                 fragment shader.
Recommandation : corriger, effort M : sprites testés sur GPU, statistiques en différé (lecture à N+2), draw indirect.
Statut         : confirmé ; coût à mesurer (PM-07)
Vérification   : Réfutation échouée : dispatch DDA (one-shot + vidage), puis deux getData avec vkDeviceWaitIdle
                 (RendererRaycast.cpp:264, 267), et drawDataInstanced de numRays × kMaxHitsPerColumn
                 (RendererRaycast.cpp:292).
```

```text
ID             : B-15
Axe            : Rendu
Nature         : faiblesse
Gravité        : moyenne
Preuve         : RendererVoxel.cpp:86 -> Renderer3D.cpp:102-116 (un DrawData par mesh de chunk : VB + IB + id de pipeline) ;
                 RendererVoxel.cpp:145-171 (maillage synchrone sur le thread principal dans prepareWorld, appelé à chaque
                 frame par Viewport.cpp:219) ; RendererVoxel.cpp:156, 199-200, 226 + VoxelWorld.h:133 (chunkCoordinates
                 renvoie un std::vector, deux fois par frame, plus un unordered_set et trois vecteurs par frame) ;
                 Renderer3D.cpp:177-178 + vulkan/RenderAPI.cpp:67 (double bind pipeline + VB + IB par mesh) ;
                 vulkan/DrawData.cpp:239 et opengl/DrawData.cpp:21 (VB dimensionné sur le nombre d'INDICES).
Constat        : Constaté : chaque chunk visible coûte un rebind complet du pipeline et des buffers, et toute
                 modification de chunk est maillée puis envoyée sur le thread principal, avec vidages de file. La taille
                 du vertex buffer ne tient que parce qu'un mesh indexé a plus d'indices que de sommets.
Comparaison    : les moteurs voxel (Minecraft-like, Vintage Story, Veloren) regroupent les chunks dans un grand buffer
                 avec offsets et multi-draw indirect, et maillent sur des workers.
Recommandation : corriger, effort M : maillage dans le Scheduler (Taskflow), grand buffer de sommets sous-alloué
                 (après B-11), un seul bind, drawIndexed avec offsets puis indirect.
Statut         : confirmé ; coût à mesurer (PM-06)
Vérification   : Réfutation échouée : prepareWorld maille et téléverse dans la boucle (RendererVoxel.cpp:162-170),
                 drawMeshes fait mesh->bind() puis drawData qui refait iData->bind() (Renderer3D.cpp:177-178,
                 vulkan/RenderAPI.cpp:67), VB dimensionné sur iIndices.size() (vulkan/DrawData.cpp:239).
```

```text
ID             : B-16
Axe            : Rendu
Nature         : faiblesse
Gravité        : basse
Preuve         : opengl/RenderAPI.cpp:45 (exige seulement 4.5) ; opengl/Shader.cpp:139-141 (glShaderBinary SPIR-V +
                 glSpecializeShader : cœur 4.6 ou ARB_gl_spirv) ; opengl/RenderAPI.cpp:152 (glMultiDrawElementsIndirectCount :
                 4.6) ; window/glfw/Window.cpp:78-82 (aucun hint de version de contexte) ; opengl/Buffer.cpp:18-20, 26-28,
                 50-51 (glCreateBuffers puis glBindBuffer/glBufferData : DSA partiel) ; opengl/Framebuffer.cpp:211-213
                 (glReadPixels synchrone à chaque frame survolée) ; README.md:66 et doc/pages/renderer.md:10, 465 ("OpenGL 4.5").
Constat        : Constaté : le backend OpenGL exige en fait 4.6, alors que la documentation et la vérification
                 annoncent 4.5. Sur un pilote 4.5 sans ARB_gl_spirv, glSpecializeShader est un pointeur nul. Le
                 backend mêle DSA et bind-to-edit, et repose sur des points de liaison globaux (UBO binding 0 partagé,
                 règle de .claude/rules/renderer.md). Le picking bloque le pipeline à chaque frame.
Comparaison    : bgfx et sokol ciblent GL 3.3/ES 3 ; macOS est figé à GL 4.1, donc hors jeu pour Owl dans tous les cas.
Recommandation : corriger la doc et le contrôle (4.6 ou extension), effort S ; picking par PBO asynchrone, effort S ;
                 décider du sort d'OpenGL (voir Pistes).
Statut         : confirmé
Vérification   : Faits confirmés (contrôle >= 4.5 en RenderAPI.cpp:45, glSpecializeShader et
                 glMultiDrawElementsIndirectCount utilisés, aucun hint GLFW). Gravité abaissée de moyenne à basse : sur
                 les plateformes visées (Linux Mesa/NVIDIA/AMD, Windows), tout pilote actuel expose GL 4.6 ; reste un
                 écart de doc et un picking synchrone.
```

```text
ID             : B-17
Axe            : Rendu
Nature         : force
Gravité        : moyenne
Preuve         : shaderFileUtils.cpp:203-302 (un source Slang, deux cibles SPIR-V via BACKEND_VULKAN / BACKEND_OPENGL) ;
                 engine_assets/shaders/** (13 shaders, aucun GLSL dupliqué) ; tests de compilation headless
                 (SlangCompute_test.cpp, Renderer3D_test.cpp:52-70, shaderFileUtils_test.cpp).
Constat        : Constaté : une seule source de shader pour les deux backends, compilée sans GPU en CI, a supprimé
                 la classe de bogues "GLSL Vulkan et GLSL OpenGL divergents". Limites : réflexion inutilisée (B-07),
                 cache incomplet (B-05), compilation séquentielle sur le thread principal (Renderer.cpp:58-80).
Comparaison    : Slang est utilisé par NVIDIA (Falcor) et passé sous gouvernance Khronos (2024) ; il cible aussi WGSL et
                 Metal, ce qui ouvre WebGPU et macOS.
Recommandation : garder ; compléter par une compilation hors ligne et l'usage de la réflexion.
Statut         : confirmé
```

```text
ID             : B-18
Axe            : Rendu
Nature         : faiblesse
Gravité        : moyenne
Preuve         : public/renderer/gpu/Texture.h:99 (generateMips = true par défaut) ; doc/pages/renderer.md:673 ("Generate
                 mipmaps") ; gui/IconBank.cpp:260 (generateMips = true) ; opengl/Texture.cpp:76, 90 (glTextureStorage2D avec
                 1 niveau) ; vulkan/internal/Descriptors.cpp:159 (mipLevels = 1) ; aucun glGenerateTextureMipmap ni
                 vkCmdBlitImage dans source/ ; public/renderer/gpu/Texture.h:194 (setFilterMode no-op hors OpenGL), appelé
                 par RendererVoxel.cpp:153.
Constat        : Constaté : aucun backend ne génère de mipmaps. La spécification, la documentation et la consigne IA
                 (« atlas 64 px avec mipmaps » dans CLAUDE.md) décrivent une fonction qui n'existe pas. Le filtre
                 Nearest demandé par le voxel est ignoré sous Vulkan, d'où un rendu différent selon le backend.
Comparaison    : tous les RHI cités fournissent la génération de mips (ou un blit) et des samplers explicites.
Recommandation : corriger, effort S : génération par blits (Vulkan) et glGenerateTextureMipmap (GL), samplers
                 explicites (filtre, adressage) partagés ; corriger doc et CLAUDE.md d'ici là.
Statut         : confirmé
Vérification   : Réfutation échouée : aucun glGenerateTextureMipmap ni vkCmdBlitImage dans source/, glTextureStorage2D à
                 1 niveau, et les samplers Vulkan sont figés en LINEAR (internal/Descriptors.cpp:125-126), d'où le
                 Nearest ignoré.
```

```text
ID             : B-19
Axe            : Rendu
Nature         : risque
Gravité        : moyenne
Preuve         : app/Application.cpp:342 (beginFrame : vkAcquireNextImageKHR, internal/VulkanHandler.cpp:412-414) puis
                 owlnest/runner/RunnerLayer.cpp:274 (RenderCommand::clear -> clearAttachment(0) sur l'image de swapchain,
                 soumission one-shot sans attente du sémaphore imageAvailable, internal/VulkanCore.cpp:487-497) ;
                 vulkan/Framebuffer.h:163-164 (renderFinished indexé par frame en vol, pas par image de swapchain) ;
                 internal/VulkanHandler.cpp:533-539 (un batch suivant attend et signale le même sémaphore binaire).
Constat        : Constaté : dans le runner, l'image de swapchain est écrite avant que le moteur de présentation l'ait
                 rendue (le sémaphore d'acquisition n'est attendu qu'au premier batch). Les sémaphores de fin de rendu
                 réutilisés par frame en vol correspondent au cas signalé par les validation layers récentes (réutilisation
                 avant la fin de la présentation). Opinion : invisible tant que B-01 vide la file ; à vérifier sous
                 validation layers et sync validation (PM-09).
Comparaison    : le guide Khronos "Swapchain semaphore reuse" (2024) recommande un sémaphore renderFinished par
                 image de swapchain.
Recommandation : corriger, effort S : clear par loadOp dans la première passe ; renderFinished par image de swapchain.
Statut         : confirmé (code) ; symptôme plausible (dépend d'une exécution sous sync validation)
Vérification   : Réfutation échouée sur le code : clearAttachment(0) vise l'image acquise (attToImgIdx ->
                 m_currentImage, Framebuffer.cpp:612) et soumet avec waitSemaphoreCount = 0 (VulkanCore.cpp:490) ; en
                 plus, la passe suivante est en DONT_CARE (B-02), ce clear n'est pas garanti conservé.
```

### Gravité basse

```text
ID             : B-20
Axe            : Rendu
Nature         : étrangeté
Gravité        : basse
Preuve         : grep -rln "BitonicSortPass\|FrustumCullingPass\|drawIndexedIndirect" source test : seuls leurs propres fichiers,
                 les backends et test/renderer_tests/ ; RendererVoxel.cpp:196, 214 (le voxel n'utilise que les fonctions CPU
                 extractFrustumPlanes / isAabbVisible) ; vulkan/RenderAPI.cpp:176-193, opengl/RenderAPI.cpp:137-158.
Constat        : Constaté : culling GPU, tri bitonique et draw indirect avec compteur sont écrits, branchés aux deux
                 backends, mais sans appelant en production et sans test d'exécution réelle (B-06). Opinion : bonnes
                 briques pour un futur rendu piloté par GPU, mais du code mort à ce jour.
Comparaison    : —
Recommandation : surveiller : les garder seulement si un test GPU (lavapipe) les couvre, sinon les sortir du moteur.
Statut         : confirmé
```

```text
ID             : B-21
Axe            : Rendu
Nature         : force
Gravité        : basse
Preuve         : internal/VulkanHandler.cpp:171-206, 365-380 (pipelines dédupliqués par clé et refcountés) ;
                 DescriptorRing.cpp:81-116 (un descriptor set par draw, recyclé à la fence de soumission) ;
                 internal/VulkanHandler.cpp:294-296, 511-512 (profondeur en état dynamique) ;
                 internal/VulkanHandler.cpp:76-78 (descripteurs par renderer libérés avant vkDestroyDevice) ;
                 VulkanCore::setObjectName (RendererDescriptors.cpp:82-83) ; validation activable à l'exécution (vulkan/RenderAPI.cpp:36-39).
Constat        : Constaté : les correctifs Vulkan récents sont propres et documentés (règles dans
                 .claude/rules/renderer.md) : pas de pipeline par mesh, pas d'UPDATE_AFTER_BIND trompeur, objets nommés
                 pour les rapports de fuite, démontage ordonné.
Comparaison    : —
Recommandation : garder ; ces invariants sont la base du socle à reconstruire (B-01 à B-04).
Statut         : confirmé
```

```text
ID             : B-22
Axe            : Rendu
Nature         : force
Gravité        : basse
Preuve         : RenderStack.cpp:146-202 (pile de couches construite depuis la config projet et la config scène, fusion
                 YAML) ; Scene.cpp:923-939 (une couche = begin, rendu filtré, end) ; RenderLayerFactory.h (fabrique par clé).
Constat        : Constaté : l'ordre et l'activation des renderers sont pilotés par les données, par projet et par scène.
                 Opinion : ce n'est pas un render graph (aucune ressource déclarée, toutes les couches écrivent dans la
                 même cible, une soumission par couche), mais c'est le bon point d'ancrage pour en introduire un.
Comparaison    : Unity URP (renderer features), Godot (compositor effects) ; un graphe de type FrameGraph ajouterait
                 ressources transitoires et barrières.
Recommandation : garder, faire évoluer (voir Pistes).
Statut         : confirmé
```

```text
ID             : B-23
Axe            : Rendu
Nature         : faiblesse
Gravité        : basse
Preuve         : DescriptorRing.cpp:87-97 (acquire : balayage linéaire avec vkGetFenceStatus par entrée occupée) ;
                 DescriptorRing.cpp:89-91 (une entrée acquise mais jamais soumise reste occupée pour toujours) ;
                 RendererDescriptors.cpp:213-268 (4 std::vector alloués à chaque écriture de set) ; vulkan/UniformBuffer.cpp:396
                 (recherche par chaîne dans un unordered_map à chaque setData).
Constat        : Constaté : coût O(n) par draw en nombre de sets vivants, allocations par draw. Négligeable à 4 draws
                 par frame, sensible avec un draw par chunk voxel (B-15).
Comparaison    : pools réinitialisés en bloc par frame (vkResetDescriptorPool), pratique courante (Granite, Wicked Engine).
Recommandation : corriger, effort S : un pool par frame en vol, réinitialisé en bloc ; tableaux fixes au lieu de vecteurs.
Statut         : confirmé ; coût à mesurer (PM-06)
```

```text
ID             : B-24
Axe            : Rendu
Nature         : étrangeté
Gravité        : basse
Preuve         : internal/VulkanHandler.cpp:159 (getPipeline renvoie {} quand l'état est Running : condition inversée,
                 aucun appelant) ; vulkan/GraphContext.cpp:30 (minor = VK_API_VERSION_MAJOR) ; vulkan/Buffer.cpp:30-31
                 (Mat3/Mat4 traduits en R32_SFLOAT) ; internal/VulkanCore.cpp:27 (apiVersion 1.3 alors que la doc annonce
                 1.4) ; vulkan/Framebuffer.cpp:657-658 et internal/Descriptors.cpp:134-135 (compareEnable = VK_TRUE sur des
                 samplers de textures couleur) ; vulkan/Framebuffer.h:136 ("samples" désigne le nombre de frames en vol) ;
                 Vulkan : multisampling fixé à 1 (internal/VulkanHandler.cpp:260).
Constat        : Constaté : petits défauts latents. Le sampler de comparaison sur une image couleur échantillonnée
                 sans Dref a un résultat indéfini (à vérifier sous validation layers). Le nommage "samples" pour les
                 frames en vol prête à confusion avec le MSAA, absent.
Comparaison    : —
Recommandation : corriger, effort S (lot de nettoyage).
Statut         : confirmé
```

```text
ID             : B-25
Axe            : Rendu
Nature         : faiblesse
Gravité        : basse
Preuve         : internal/VulkanCore.cpp:407-415 (MAILBOX préféré dès qu'il existe, aucun réglage de vsync) ;
                 vulkan/Framebuffer.cpp:57, 146 (vkDeviceWaitIdle à chaque invalidate) et vulkan/Framebuffer.cpp:328
                 (oldSwapchain = VK_NULL_HANDLE).
Constat        : Constaté : recréation de swapchain simple et sûre mais bloquante. Opinion : MAILBOX par défaut fait
                 tourner le GPU sans limite (consommation sur portable) ; un réglage vsync de SettingsManager serait attendu.
Comparaison    : SDL_GPU expose les modes de présentation (VSYNC, MAILBOX, IMMEDIATE) ; Godot propose un réglage vsync.
Recommandation : surveiller ; exposer le mode de présentation, effort S.
Statut         : confirmé
```

### Décompte

Après vérification croisée (B-04 haute → moyenne, B-05 haute → basse, B-16 moyenne → basse ; les
constats restent rangés dans leur section d'origine).

| Nature    | Haute | Moyenne | Basse | Total |
|-----------|-------|---------|-------|-------|
| Force     | 0     | 2       | 2     | 4     |
| Faiblesse | 3     | 7       | 4     | 14    |
| Risque    | 1     | 2       | 0     | 3     |
| Étrangeté | 0     | 2       | 2     | 4     |
| **Total** | 4     | 13      | 8     | 25    |

## Comparaison brève avec les RHI et les render graphs

Opinion du rédacteur, données de mémoire, à confirmer par l'agent « état de l'art » (30-etat-de-l-art.md).

| Critère                | Owl                               | bgfx                 | sokol_gfx                      | SDL_GPU (3.2, 2025)  | NVRHI / Diligent                   | WebGPU (Dawn, wgpu)       |
|------------------------|-----------------------------------|----------------------|--------------------------------|----------------------|------------------------------------|---------------------------|
| Objet pipeline         | non (état global + nom de shader) | état encodé par draw | oui (`sg_pipeline`)            | oui                  | oui (PSO)                          | oui (`GPURenderPipeline`) |
| Uniformes par draw     | non versionnés (B-03)             | oui (uniform buffer) | oui (anneau)                   | oui (push uniforms)  | oui (dynamic buffers)              | oui (offsets dynamiques)  |
| Liaison des ressources | bind par slot implicite           | slots + samplers     | `sg_bindings`                  | slots par étage      | binding sets / SRB                 | bind groups               |
| Mémoire                | `vkAllocateMemory` par ressource  | interne              | interne                        | interne, "cycling"   | sous-allocation                    | interne                   |
| Barrières et layouts   | manuels, depuis UNDEFINED         | internes             | internes                       | internes             | suivi d'état automatique           | internes                  |
| Multithread            | non (singleton, thread principal) | encodeurs par thread | non                            | command buffers      | command lists                      | encodeurs                 |
| Cibles                 | GL 4.6, Vulkan                    | 10+ backends         | GL, GLES, Metal, D3D11, WebGPU | Vulkan, Metal, D3D12 | Vulkan, D3D11/12 (+Metal Diligent) | Vulkan, Metal, D3D12, Web |

Lecture : Owl a les inconvénients d'une RHI maison (maintenance de deux backends, 8 kLOC de Vulkan)
sans les garanties de base de ces bibliothèques (versionnage des uniformes, barrières correctes,
sous-allocation). Pour un moteur de cette taille, avec un seul mainteneur, adopter SDL_GPU, NVRHI ou
wgpu-native/Dawn est une option sérieuse face à la réécriture du socle Vulkan (voir Pistes, P-04).

Render graphs modernes (Frostbite FrameGraph, GDC 2017 ; Unreal RDG ; Granite ; Bevy render graph) :
passes déclarant leurs lectures et écritures, ressources transitoires aliasées, loadOp, storeOp et
barrières déduits, élimination des passes inutiles. Owl n'a aucun de ces mécanismes. `RenderStack` est
une liste ordonnée de couches qui écrivent toutes dans la même cible.

## Points de mesure proposés

Pour l'agent benchmark. Chaque point indique la fonction, le scénario, la métrique et l'outil. Les
mesures GPU passent par `docker/run.sh --gui`, sur les trois devices (RTX 5000 Ada, Intel UHD, llvmpipe ou
lavapipe), en preset `linux-clang-release`, au moins 5 répétitions, avec médiane et écart interquartile.

| ID    | Cible (fonction / chemin)                                                            | Scénario                                                                                                                                                | Métrique                                                                                                                                                                            | Constat lié            |
|-------|--------------------------------------------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|------------------------|
| PM-01 | `VulkanCore::endSingleTimeCommands`, `StorageBuffer::getData` (Vulkan)               | Éditeur, scène `sample_project/scenes/platformer_house.owl`, souris dans le viewport puis hors du viewport                                              | Nombre de `vkQueueWaitIdle` / `vkDeviceWaitIdle` par frame (uprobe `perf` sur libvulkan, ou `ltrace -c`) ; temps CPU de frame ; temps GPU (requêtes timestamp si ajoutées au bench) | B-01                   |
| PM-02 | `getOrCreateGlobalSession` + `compileSlangToSpirv`                                   | Démarrage à froid (sans `cache/shader`), puis à chaud, en GL et en Vulkan ; runner et éditeur                                                           | Temps jusqu'à la première frame ; part de la session globale Slang ; part du compute DDA                                                                                            | B-05                   |
| PM-03 | `Renderer2D::drawQuad`, `drawString`, `flush`                                        | Bench existant `bench/cases/Renderer2DBench.cpp`, étendu à OpenGL et Vulkan ; 1k/10k/100k quads avec `worldIndex` vs avec transform ; texte 10k glyphes | ns par quad (CPU) ; nombre de flush (`getStats().drawCalls`) ; temps de frame GPU                                                                                                   | B-08, B-09             |
| PM-04 | `Scene::prepareWorldTransforms` + `WorldTransformPass::compute`                      | Hiérarchies synthétiques : 10k racines plates ; 1 chaîne de profondeur 100 ; arbre 10 × 4 niveaux                                                       | Temps CPU de la passe CPU seule vs CPU + GPU ; exactitude au-delà de 64 niveaux                                                                                                     | B-12                   |
| PM-05 | `RendererTilemap::flushPending`                                                      | Tilemaps de 32², 128², 256² cellules, 1 à 4 couches, caméra fixe                                                                                        | Temps CPU par frame ; octets envoyés par frame ; cellules tronquées                                                                                                                 | B-13                   |
| PM-06 | `RendererVoxel::prepareWorld`, `drawVoxelWorld`, `Renderer3D::drawMeshes`            | `voxel_terrain.owl`, puis monde synthétique 16³ à 32³ chunks ; édition continue d'un bloc                                                               | Temps de maillage par chunk ; draws et binds par frame ; nombre d'allocations `vkAllocateMemory` vivantes ; mémoire GPU                                                             | B-11, B-15, B-23       |
| PM-07 | `RendererRaycast::drawTilemapWalls` (chemin GPU) vs chemin CPU                       | `raycast_demo.owl`, 320 / 640 / 1280 rayons                                                                                                             | Temps de frame CPU et GPU ; part des relectures (`getData`)                                                                                                                         | B-14                   |
| PM-08 | `Viewport::onUpdate` complet                                                         | Même scène, OpenGL vs Vulkan, éditeur vs runner                                                                                                         | Temps de frame ; écart entre backends ; FPS en MAILBOX vs FIFO                                                                                                                      | B-01, B-16, B-25       |
| PM-09 | Validation layers + sync validation (`VK_LAYER_KHRONOS_validation`, `validate_sync`) | Éditeur et runner, 300 frames sur chaque scène de `sample_project`, plus 2 couches 2D (World + Screen)                                                  | Nombre et identifiants des messages (VUID, SYNC-HAZARD-*)                                                                                                                           | B-02, B-03, B-19, B-24 |
| PM-10 | Contenu des attachements entre batchs                                                | lavapipe et Intel (Mesa) : scène voxel + 2D + HUD ; capture RenderDoc si ajouté à l'image                                                               | Image finale comparée à NVIDIA ; présence des couches précédentes                                                                                                                   | B-02                   |
| PM-11 | `Texture2D::setData` (Vulkan), `TextureLibrary`                                      | Chargement de `sample_project/textures` complet                                                                                                         | Temps total de chargement ; vidages de file par texture                                                                                                                             | B-01, B-11             |
| PM-12 | Mémoire par frame                                                                    | `TrackerAPI` (OWL_ENABLE_MEMORY_TRACKER) sur 600 frames d'éditeur, scène voxel et scène texte                                                           | Allocations par frame dans `renderer/**` (drawRect, voxel, descripteurs)                                                                                                            | B-09, B-15, B-23       |

Outils manquants à consigner dans `01-environnement.md` si absents : `perf` (uprobes), `ltrace`,
RenderDoc (capture headless), lavapipe (pilote Vulkan logiciel de Mesa).

## Pistes pour l'avenir

Ordonnées par dépendance. Effort : S (jours), M (semaines), L (mois), pour un mainteneur seul ; opinion du rédacteur.

| ID   | Chantier                                       | Intérêt                                                                                       | Briques préalables manquantes                                                                                                                                                                                           | Effort |
|------|------------------------------------------------|-----------------------------------------------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|--------|
| P-01 | Socle de synchronisation Vulkan                | Frames en vol réelles, fin des vidages, comportement correct hors NVIDIA                      | Command buffer par frame pour tout FB, anneau de staging, suivi des layouts, sémaphores timeline (cœur 1.2), synchronization2 (cœur 1.3)                                                                                | L      |
| P-02 | Anneau d'uniformes et données par draw         | `drawMesh` multiple, plusieurs caméras par frame, parité GL/Vulkan                            | P-01 ; UBO dynamiques ou push constants dans l'abstraction                                                                                                                                                              | M      |
| P-03 | Allocateur mémoire (VMA)                       | Plafond d'allocations levé, mapping persistant, aliasing futur                                | Ajout DepManager ; P-01 pour les buffers dynamiques                                                                                                                                                                     | M      |
| P-04 | Pipeline et liaisons explicites, ou RHI tierce | Fin des conventions implicites ; ouverture à Metal/D3D12/Web                                  | Décision : refonte maison (PipelineDesc, BindGroup, CommandList) ou adoption de SDL_GPU, NVRHI ou wgpu-native/Dawn                                                                                                      | L      |
| P-05 | Tests d'image et de compute sur lavapipe       | Filet de sécurité pour P-01 à P-04 ; couverture des compute shaders                           | lavapipe dans l'image Docker ; lecture de framebuffer fiable (B-02)                                                                                                                                                     | M      |
| P-06 | Render graph                                   | Passes déclarées, barrières et loadOp déduits, ressources transitoires (post-process, ombres) | P-01, P-04 ; `RenderStack` comme front-end ; formats de cible multiples (aujourd'hui une seule disposition d'attachements imposée)                                                                                      | L      |
| P-07 | Rendu 3D moderne                               | Static meshes, PBR, ombres, HDR, post-process                                                 | P-02 (matrice par draw), P-06 ; cibles HDR (RGBA16F absent de `AttachmentSpecification`) ; profondeur échantillonnable ; mipmaps (B-18) ; samplers explicites ; culling activé                                          | L      |
| P-08 | Rendu piloté par GPU                           | Voxel et scènes denses en quelques draws ; culling GPU                                        | Briques de B-20 testées (P-05) ; buffers sous-alloués (P-03) ; bindless : `runtimeDescriptorArray`, `descriptorBindingPartiallyBound` (seul `shaderSampledImageArrayNonUniformIndexing` est activé, VulkanCore.cpp:271) | L      |
| P-09 | Sort d'OpenGL                                  | Diviser la maintenance par deux                                                               | Vulkan corrigé (P-01) ; MoltenVK pour macOS (exige B-02) ; une cible web (P-10) pour remplacer le "repli universel"                                                                                                     | M      |
| P-10 | WebGPU / Web                                   | Démo navigateur, éditeur web à long terme                                                     | P-04 (modèle bind group) ; Slang vers WGSL ; plus de vidage synchrone (interdit sur le web) ; build wasm (axe G)                                                                                                        | L      |
| P-11 | Rendu sur thread dédié                         | CPU de frame divisé                                                                           | P-04 (command lists) ; fin des singletons `RenderCommand` / `Renderer2D` et de l'état thread_local des descripteurs                                                                                                     | L      |
| P-12 | Outils GPU                                     | Diagnostic des points ci-dessus                                                               | Requêtes timestamp, intégration Tracy (GPU zones), marqueurs RenderDoc (`vkCmdBeginDebugUtilsLabelEXT`)                                                                                                                 | S      |

Ordre proposé : P-12 et P-05 (mesurer et tester), puis P-01 avec B-04 dans la même PR, puis P-02 et P-03,
puis la décision P-04, qui conditionne P-06 à P-11. Mon avis : trancher P-04 avant d'investir dans P-07.
Un rendu 3D bâti sur l'abstraction actuelle hériterait de B-03 et de B-07.
