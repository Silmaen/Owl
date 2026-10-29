# Changelog {#page-changelog}

[TOC]

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Skeletal animation: `MeshLoader` reads the first glTF skin, its clips and its `socket_*` nodes, drawn skinned on the GPU (`skinned3d.slang`).
- `Animator` component: a state machine of clips with parameters, conditions, exit times and cross-fades, driven from Lua (`animator.*`).
- Mesh levels of detail: built by the cook (`OWLM` container) from the `.owlmeta` settings and picked at runtime by screen size.
- Mesh sockets: `MeshSockets` and `SocketAttachment` snap entities on a socket or a bone (`Scene::attachToSocket`, `socket.*`).
- Owl Nest: skeleton, clip timeline and LOD views in the mesh document, Animator and socket inspectors, `entity.attach_socket` / `entity.detach_socket`.
- `asset.set_import` and the Import panel set the levels of detail of a mesh.
- Owl Nest socket gizmo: click a socket marker (or select it in the inspector) and move it with the transform gizmo, one `entity.set_socket` per drag.
- Engine Demo `character_yard` scene: a skinned mannequin walking with a lantern on its hand socket (**C** from the menu).

- Asset GUIDs: every asset of a project opened in Owl Nest gets a `.owlmeta` sidecar, and `AssetRegistry` maps GUID and path.
- Asset references written by GUID (`gid:<guid>:<path hint>`): renaming or moving a file keeps every reference valid.
- Asset cook: the export cooks every packed asset through a content-addressed cache in `<project>/.owl/cache/cooked/`.
- Texture compression: KTX2 Basis Universal (UASTC, ETC1S) with mips, transcoded at load to BC7, ASTC 4x4 or RGBA8.
- Binary scenes: scenes, prefabs and HUDs are cooked to a binary tree loaded without parsing, 1.7x to 2.2x faster.
- Owl Nest Import panel and `asset.*` commands (import settings, rename, move, delete, cook, clear cache).
- `OwlNest --upgrade-assets` creates sidecars and rewrites references by GUID; `--export` takes `--cook-cache`.
- Project setting **ImGui overlay** (`imGui`): an exported game creates its ImGui layer and packs its fonts only when it is on.
- `readAssetBytes`, `findPackedAsset` and `isPackedAsset`: one lookup, the open pack first, for every loader.
- The export compiles the project shaders and bakes the font atlases into the pack (`ExportReport::shadersBaked`).
- `WorldPosition` component in double and a scene floating origin (`FloatingOrigin:`, `scene.set_floating_origin`) for worlds far from zero.
- A shift of the world origin carries the physics bodies, their interpolation and the behaviour rest positions along: a running scene goes on unbroken.
- Owl Nest moves the world origin with an undoable `scene.set_world_origin` (viewport Camera menu, Scene Settings field), the view following it.
- Lua `transform.get_world_position` / `set_world_position` in double and `scene.get_world_origin`.
- Engine Demo far orbit (`far_orbit.owl`, **K** from the menu): a station one astronomical unit away, sharp to the centimetre.
- Visual scripts (`.owlvs`): event graphs compiled to Lua at load, usable wherever a `.lua` script is, their variables as script properties.
- Owl Nest visual script editor: Blueprint-style canvas with inline values, node library to drag from, variables and node details in the Graph panel, graph / split / coloured Lua views, two-tone icons of the value types and node families, copy / paste, undo.
- Visual scripts: colour and vector values (`Color`, `Vector 2`, `Vector 3`), as variables, make / break nodes, and the `r g b a` / `x y z` parameters of a binding (also prefixed, `vx vy`, `cell_x cell_y`) as one pin that splits into its components; constants have only an output.
- Content Browser: visual scripts have an icon and a colour of their own.
- Engine Demo: the tavern cellar ladder is a visual script.
- Engine Demo: a tavern window glows with a rectangle area light casting soft shadows, and a room reflection probe makes the brass and copper reflect the fire.
- Engine Demo: the tavern is half a stop brighter, with a slight chromatic aberration and film grain.
- Script debugging: breakpoints on Lua lines (gutter, F9) and on visual-script nodes (F9, context menu), the game pausing once the script call ends (`ScriptDebugger`).
- Owl Nest **Script Debug** panel: the hit (script, line, entity, locals) with Continue and Open, and the variables of the scene, HUD and entity scripts (`ScriptInstance::getWatchValues`).
- Visual scripts show the last value of each wire during Play (`CompileMode::Debug`, `compileForDebug`) and outline the node the game stopped on.
- Visual-script functions and macros with typed parameters, results and execution exits (`GraphFunction`, `func.entry`, `func.return`, `func.call`), edited in the Functions tab of the Graph panel.
- Visual-script library graphs: the functions of another `.owlvs` of the project are palette nodes compiled into the caller, and the export packs the library files.
- HUD scripts: a `.owlhud` has a scene script (`SceneScript`, `Scene::getHudScript`) run with every scene that draws it.
- Inspector handler rows have an **Edit in Graph** button that creates the script of the scene or HUD when missing and opens the custom event of the handler.
- Console Lua mode: globals kept between lines, expression values printed, multi-line blocks, `help(editor)`, commands still accepted.
- Lua `editor` module, one function per registry command, errors and Play refusals as Lua errors, journal source `script`.
- Editor scripts: `.lua` files under `editor/` run from the Console menu or the Content Browser as one undo step, and `.owlmacro` files export to Lua.
- `ScriptConsole`: a Lua state for tools with a REPL and host functions, apart from the game sandbox.
- Procedural generation graphs: `.owlpcg` node graphs edited in Owl Nest generate a tilemap region from a seed (`TilemapAsset::generator`).
- Tileset editor: compose the atlas from source images by dropping files or folders, clear, move and grow slots, undoable.
- Tileset opacity map (`opacityTexture`): its red channel multiplies the atlas alpha in the tilemap, isometric, raycast and voxel shaders.
- Mipmapped pixel-art atlases: `FilterMode::PixelArt` builds mipmaps cell by cell, sampled inside the cell by the voxel, raycast and tilemap shaders.
- `gpu::TextureCube` on OpenGL, Vulkan and Null, filled from memory or a render target (`Framebuffer::copyAttachmentToCube`).
- `Light.angularRadius`: a directional light of apparent size casts soft shadows (PCSS), widens its highlight and sizes the sun disc of a `DayNightCycle` sky.
- Point and spot light shadows: a cube per point light, a perspective map per spot light, the nearest to the camera within 4 of each on OpenGL and 16 on Vulkan.
- Area lights: `Light` types `Rectangle` and `Disc` shaded through linearly transformed cosines, `Sphere` through a representative point, with soft cube shadows (PCSS) and their Owl Nest markers.
- Reflection probes: a `ReflectionProbe` component captures the scene into a prefiltered cube that replaces the environment reflections inside its box or sphere, with parallax, blending, realtime updates and its Owl Nest markers, inspector and commands.
- Post-process: chromatic aberration, film grain, depth of field and camera motion blur in `PostProcessSettings`, edited in the camera inspector.
- Node-graph link waypoints: right-click a link to add one, drag or delete it, **Alt**+click to cut the link, in visual scripts, PCG graphs and `NodeCanvas` graphs.
- Sound cues (`.owlsound`, `sound::SoundCue`): random weighted takes of one sound, trimmed and looped, with a volume and pitch varied at each play.
- `sound.*` and `locale.*` editor commands: every edit of a sound cue or of the localisation is journaled and reachable from the Console, macros and MCP.
- Owl Nest sound editor: takes on their waveform with draggable trim and loop markers, cue and take preview, `sound.create`, **New Sound Cue**, a double-click on an audio file.
- `SoundHelper::playAsset` plays an audio file or a cue for the scene sources, `sound.play` and the inspector; `readSoundDuration` gives a file's length.
- Localisation (`localisation.yml`, `data::localisation::Localisation`): languages with a font each, a fallback language and string tables, the export packing the exported languages only.
- Owl Nest localisation editor: languages, and a grid of keys by language with table filter, search, missing texts, bulk duplicate, move and delete.
- `UiText::textKey` shows a translated text in the HUD; Lua `tr` and the `locale` table; the built-in `language` setting.
- Engine Demo settings menu in English and French, with a language button.
- Owl Nest **Import settings...**, offered at the first start: the settings of a previous installation applied at the next start.
- Audio mixer: every sound plays on a bus (Master, Music, SFX, Ambient, Voice) with volume, mute, solo and fades, the scene mix saved as `Mixer:`.
- Music crossfades: `SoundHelper::playMusic` and Lua `sound.play_music` crossfade with equal-power curves, `sound.crossfade` for any two sounds.
- `ReverbZone` component: box or sphere volumes with EFX reverb presets (cave, hall, underwater...) and a filter, blended at the listener.
- Low-pass, high-pass and band-pass filters on sound sources, combined with the filter of the reverb zones.
- Doppler effect on moving spatial sources and the listener, from world positions in double (a floating origin shift is not motion).
- `AdaptiveMusic` component: synced looping layers, states fading them in and out with a stinger, driven by Lua or a game state key.
- Lua `sound` bus and music bindings: `set_bus_volume`, `set_bus_muted`, `play_music`, `stop_music`, `crossfade`, `set_music_state`.
- Owl Nest Mixer panel (`scene.set_mixer`, `sound.set_live_bus` in Play), reverb zones drawn and sized in the viewport, inspectors for zones, filters and adaptive music.
- Engine Demo sound yard (U from the main menu): a cave reverb, an underwater pool, a radio behind a wall, a passing cart and a music turning from calm to combat.

- GPU particles: `ParticleEmitter` simulated by a compute shader on OpenGL and Vulkan (CPU reference on the Null backend), up to 65536 particles per emitter.
- `RendererParticles` render-stack layer drawing particles and trails over the 2D, isometric, voxel and mesh layers, under the HUD.
- Alpha-blended particles sorted far to near on the GPU; additive particles through the new `BlendMode::Additive`.
- Colour over life (gradient) and size over life (curve), shapes, gravity, drag, spin, world or local space, bursts, loops and presets (`ParticlePreset`).
- Ribbon trails behind particles and behind entities (`Trail` component).
- Emissive particles and trails: a luminance in nits, pre-exposed like the lights, glowing through the bloom.
- Lens flares of the sun and the lights with a `flare` strength, occlusion tested on the GPU (camera `lensFlare*` settings).
- Particles and trails follow a shift of the floating origin.
- Owl Nest: particle emitter and trail inspectors, gradient editor, live viewport preview in Edit mode (play, pause, restart, stop, burst).
- Owl Nest **Create > Effects** and the `entity.create_particle_emitter` and `particles.preview` commands.
- Owl Nest appends a `RendererParticles` layer to the project stack for the first emitter, and draws a pickable sparkle at every emitter in the viewport.
- Lua `entity.particles_play` / `pause` / `stop` / `restart` / `emit` / `is_emitting` and `entity.set_trail_enabled`.
- Benchmarks `particles/simulate_cpu/*` and `particles/advance_gpu/256_emitters`; image tests of particles, 2D particles and trails, and lens flares.
- Engine Demo: embers with trails and smoke rise from the tavern hearth, whose fire flares in the lens.

### Changed

- `mesh3d.slang` shares its surface shading with the new `skinned3d.slang` through `common/mesh_surface.slangh`; its output is unchanged.
- Compute shaders resolve their `#include`s like the graphics shaders and the shader bake.
- Sample games are picked from the pool once the features of a release are done, none planned per release; hot reload of a C++ game module moves after 1.0.
- `Light.castShadows`, `shadowResolution` and `shadowBias` apply to point and spot lights too; the 3D shadow cascades move to texture slots 16 to 19.
- Shipped projects carry sidecars and GUID references; the Engine Demo PBR textures are cooked to ETC1S and UASTC.
- An exported game packs only its engine files and initialises only its renderer families (`RendererSet`): packs about 3.3 MiB smaller.
- An exported game reads its textures, sounds, fonts and shaders from the pack in memory: no `assets/` extraction, no cache written next to it.
- `GameExportEndToEnd` also plays every exported project on lavapipe and fails on a shader compiled at start or a file written beside the game.
- Widget callbacks (button, slider, list...) also reach the HUD script and the scene script, after the widget's own scripts and before the entity scripts.
- CI: one tree per configuration; the tested release tree is the one packaged, so no configuration builds a second preset.
- CI: Linux x64 Clang Release is the fast verdict (drafts, ready pull requests, doc-only pull requests, `main`); drafts run Code Style and it only.
- CI: coverage is measured on Linux x64 GCC (every ready pull request and `main`) and by Clang Debug nightly, each with its coverage-drop gate; Windows measures none.
- CI: Windows x64 Clang and Linux x64 GCC run on ready pull requests and `main` only, no longer on drafts nor `Experiment/*`.
- CI: `linux-cross-arm64` is a Release tree, tested under QEMU at night and packaged by Package Linux arm64.
- CI: the `slow` tests run in a nightly Slow Suite on the Linux x64 release tree; arm64 and Clang Debug leave them out.
- `/check --quick`: the fast loop for agents (touched tests and CodeStyle), with `--fix` to apply the formatters first.
- Docs: screenshots of the tower defence (title, world map, briefing, two levels) in Getting Started and the sample games page.
- Scene Flow and every `NodeCanvas` share the node style of visual scripts (`gui/widgets/NodeStyle.h`): one row per pin, title bar, coloured curves.
- Docs: the engine developer guide lists every renderer: the 2D, tilemap, isometric, 3D and post-process sections join the voxel and raycaster pages.
- Bench: the Linux baseline takes the binary scene load and far-orbit cases, and the 1000-box octree brute-force cull at 15.3 µs, a code layout drift of `main` the confirmation rounds reproduce.
- Settings, saves, logs and caches go to `user/` beside the executable (`platform::getUserDirectory`), the user profile only when it is read-only.
- The `volume_music` and `volume_sfx` settings, and the new `volume_ambient` and `volume_voice`, set the player's volume of the mixer buses; `volume_master` sets Master instead of the listener gain.
- The Null sound backend keeps the state of every source and of the listener, so the mixing logic is tested headless.

### Removed

- CI: Windows GCC configuration, the `package-linux` and `package-linux-cross-arm64` presets (`cmake/CMakePresetsPackage.json`), the `release_preset` mechanism and the Windows-only coverage step.
- Owl Nest: the Scene Flow view; the sample games chain their scenes from Lua, which it did not show.

### Fixed

- The Owl Nest **Log** follows the new messages again, and its lines can be copied (right click) or selected as text.
- `linux-clang-minimal` builds and passes again: shader baking and shader reload are tied to the render module, and the tests that need physics or the renderer skip without it.
- The export tests play each game on lavapipe only when `OWL_RENDER_TESTS` is on: the emulated arm64 build no longer times out on the Tower Defence.
- Owl Nest: the Hierarchy and Properties panels follow the active document whatever its type (code, node graph, animation), with an empty state when it has nothing to show.
- `NodeCanvas` (Scene Flow): the mouse wheel no longer zooms the graph while a menu over it scrolls, and links meet their pins.
- The ktx recipe probes the CPU type with the cross `--target`: the arm64 build no longer compiles the x86 AVX2 ASTC encoder.
- ThreadSanitizer ignores the lock-free hand-off inside OpenAL Soft: the exported Tower Defence, which plays a sound at start, passes again.
- GLFW on X11 keeps Xlib and its extensions loaded after `glfwTerminate`: LeakSanitizer no longer reports their caches as leaks when a game window closes.
- The coverage presets (`linux-gcc-debug`, `linux-clang-debug`) multiply the Lua time quota by 4: a gcov build of the Tower Defence scenarios ran out of it on a busy agent.
- `transcodeKtx2` refuses data shorter than a KTX2 header before libktx, which leaked its memory stream on it (Sanitizer Address).
- `PackReader::readEntry` is thread-safe: asynchronous texture loads no longer share one file stream unguarded.
- ETC1S texture cook under ASan: a local `ktx` recipe patches libktx to allocate the BasisLZ global data with `malloc`, as `ktxTexture2_Destroy` frees it with `free`.
- `Coverage` reads only the notes of its own build tree: another compiler's tree in the same checkout no longer makes gcov fail.
- `LuaSandbox.cppExceptionInBindingBecomesLuaError` runs under a roomy time quota: throwing C++ exceptions under QEMU no longer exceeds the 50 ms one.
- `FramePacer.WaitSleepsOutThePeriod` measures from the start of the period: a late thread under QEMU no longer makes the wait look short.
- The sanitizer presets give each test binary 30 minutes: the Tower Defence export end-to-end test outgrew 10 minutes under TSan.
- `owl_bench --filter` measures `hud/frame/1000_row_list` and `scene/set_parent/wide10000_build` alone: the Bench confirmation rounds of these two re-measured nothing (or crashed) and kept the full-run median.

## [0.5.0] - 2026-10-25

### Added

- Frame rate cap: `Application::setFrameRateLimit` and `frameRateLimit` in `config.yml`; Owl Nest settings *Frame Rate Limit* and *Background Frame Rate* (15 fps by default while another window has the focus).
- `lowLatencyPresent` in `config.yml` (*Engine parameters > Backends*): Vulkan MAILBOX for a lower input latency, without display pacing.
- Stats > **Measure at Full Load**: 3 s without vsync, frame rate limit or idle wait, then the real frame rate and its CPU and GPU cost (`panel::FullLoadProbe`); Vulkan rebuilds its swap chain when the vsync changes at runtime.
- Owl Nest idle mode: in Edit with nothing held or pending, the editor waits for an input (*Idle Wait*, 250 ms at most) instead of drawing every frame; MCP requests wake it.
- `core::FramePacer`, `Application::requestIdleWait`, `Scheduler::hasPendingTasks`, `FontPreviewCache::hasPending`, `Window::isFocused`, `waitEvents`, `wakeUp` and `setLowLatencyPresent`.
- `Renderer2D` layer keys `Billboard` and `DepthTest`: sprites in a 3D scene face the camera, are depth-tested and
  follow its sky, sun and exposure (`Renderer3D::getSurfaceLight`), 2D lights on the ground plane.
- Sample game Tower defence (`samples/tower_defence/`): a campaign of 10 levels in 3D on an inked world map, nine kinds
  of the Horde as 8-direction billboards, towers raised to level 3 then a power-up, a build wheel, spells, day and
  night with their effects, level modifiers and briefings, a skippable tutorial, unlocks and upgrades bought with the
  stars.
- Lua `require` of the modules of a project (sandboxed, packed by the export), modal `UiWindow`, `ui.set_size` and
  `ui.get_rect`, `RenderTarget.hiddenLayers` to keep markers out of a minimap.
- `AnimatedSpriteRenderer.lit`, and Lua `ui.set_text` / `ui.get_text` reach a world `TextRenderer`.
- Engine Demo tavern: a hub level played on foot with the `FirstPersonController` component.
- Lua `scene.spawn_prefab`, `entity.add_component`, `scene.pick_entity`, `time.set_scale` / `set_paused` and
  `ui.set_image`.
- `ProceduralSky`, `DayNightCycle`, `LightFlicker`, `Oscillator`, `Patrol`, `Drift`, `RenderTarget` and `FollowTarget`
  components.
- Silhouette outline of an entity (`Scene::setOutline`, `renderer::OutlinePass`) and picking of meshes and voxel worlds
  in 3D.
- Reversed-Z depth and camera-relative 3D shading, precise far from the origin.
- `renderer::RenderGraph`, HDR scene colour with per-camera tone mapping and a per-camera post-process stack (bloom,
  vignette, `.cube` LUT).
- `MeshRenderer` component and `RendererMesh` layer for `.gltf`, `.glb` and `.obj` meshes, with `renderer::MeshCache`.
- `Light` component (`Directional`, `Point`, `Spot`) with forward lights, cascaded shadow maps and physical units (lux,
  lumens, `ev100`).
- PBR materials (`.owlmat`: `Standard`, `Unlit`, `Toon`), image-based lighting through `Environment`, voxel
  `BlockSurface`.
- Mesh culling and batching: `data::geometry::LooseOctree`, instanced draws, GPU frustum culling, Hi-Z occlusion.
- `Light2D` component with 2D shadows, normal-mapped sprites, lit tilemaps and isometric tiles.
- `SceneScript` (one script per scene) and script properties `color`, `vec2`, `vec3`, `entity`, lists, ranges and reset
  buttons.
- Owl Nest: prefab, mesh and material documents, mesh drop and thumbnails, 3D viewport navigation, **Pilot Selected
  Camera**.
- Owl Nest: script hooks and handlers listed in the inspector with an **Open** button, layer overrides in Scene
  Settings.
- Help panel: mermaid diagrams as SVG (`tools/doc/mermaid_svg.py`), task lists, list indents, and pages describing the
  editor.
- Help panel: images fit table columns and enlarge on click, roadmap status as coloured tags, links to a heading,
  browser-like Back / Forward (mouse side buttons) restoring the scroll.
- A save keeps the tilemap cells changed in game; `core::task::Scheduler::parallelFor` prepares large scenes in
  parallel.

### Changed

- Roadmap replanned toward 1.0 at the end of 2026: v0.6.0 to v0.9.0 two weeks apart, a 0.9.x hardening phase, Android and the Web in 1.0, every target exported from every Owl Nest host, localisation, accessibility and a local crash reporter.
- Vulkan presents in FIFO under vsync instead of MAILBOX, and under Wayland in MAILBOX paced to the refresh rate: an idle Owl Nest no longer keeps the GPU at full load.
- The viewport frame chip shows the CPU and GPU cost of a frame, the waits for the display excluded (`FrameTimings::workMs`, `panel::FrameMeter`); the frame rate it allows and the draw calls stay in Stats.
- A viewport hidden behind another tab no longer draws its scene, unless the MCP server is on.
- Documentation reorganised: short README, one guide per audience, help sidebar grouped by theme, diagrams redrawn as
  mermaid.
- Meshes and voxel worlds are lit by the `Standard` model, depth is `Depth32F`, scene format 6 converts to physical
  lights and format 7 moves camera scripts to `SceneScript`.
- Build: C++ module scan off, lld on Clang presets, a warm ccache rebuild hits every unit; `Renderer2D::drawString`
  keeps unchanged laid-out strings.
- CI: benchmarks run on ready pull requests, Clang-Tidy is scoped by `clang-scan-deps`, heavy jobs and image tests are
  sharded or nightly.
- Help panel: hovered links underlined, internal and web links told apart, headings in clear H1–H4 steps.
- Lua API reference split into one generated page per theme behind an index (`getLuaReferencePages`).
- Owl Nest screenshots taken from a neutral path (`/home/user/OwlEngine`): no local path shows.
- The version is written only in `CMakeLists.txt`; Owl Nest shows it in its title bar and Lua `engine.version()` returns
  it.

### Removed

- The emulated arm64 TeamCity build (*Clang (emulated)*, weekly) and its `linux-emulated` preset: the cross compiled arm64 build covers arm64.
- `OWL_HELP_FETCH_BADGES` option: the help shows no remote badge.

### Fixed

- OpenGL on the SPIR-V path no longer draws the scene upside down under an upright HUD: the fragment origin is the OpenGL lower left, as on the GLSL path.
- F1 opens the help, and F5 to F10 drive Play, while a panel of Owl Nest holds the keyboard focus, not only over the viewport.
- **Pack Game** and **Pack Scene** no longer do nothing after a progress popup that ImGui closed by itself: the popup lets go once its operation ends, and a refused pack is logged.
- The native file dialogues get an absolute default folder with native separators, and paths go through UTF-8 both ways.
- Inspector rows of Owl Nest keep their fields and reset buttons inside the component frame: the field column no longer shifts by one indent inside the component group.
- A string script property is edited in a multiline field that wraps its text and grows up to eight lines.
- **Show > HUD** in the viewport hides the scene HUD while editing; Play always draws it (`Scene::setEditorHudVisible`).
- The villagers of the Engine Demo hub count five levels, the tavern included.
- `docker_run.sh --gui` writes the desktop entries into the host applications folder, so Owl Nest started from the image has its icon on Wayland.
- An export follows the scenes and assets named in a list-of-strings script property; it packs the external images of
  a `.glb`; Owl Nest no longer crashes on **Play** without a primary camera; Lua
  `save.*` listing never throws.
- GCC 15 HDR decoding, MinGW renderer test link, Vulkan blending on non-blendable formats and mirrored normal maps on
  Vulkan.
- Mesh entities drawing an unchanged file material share it, so the mesh pass instances them: one draw per model and
  material instead of one per entity.

## [0.4.0] - 2026-10-20

### Added

- Sample games in `samples/` (Breakout, Wolfenstein mini, Sokoban), installed with Owl Nest and listed under **Sample
  games** on the File screen.
- Isometric layer `RendererIsometric` with a 2:1 dimetric `IsometricCamera`, depth-tested billboards, `IsometricObject`
  component and an isometric viewport mode in Owl Nest.
- Isometric terrain: a corner-height grid on `TilemapAsset` with 19 slopes, terraforming, overlays, slope-aware picking
  and a node tool in the tilemap editor.
- Isometric Engine Demo: a 64x64 iso town with roads, junctions, buildings and vehicles with eight headings, generated
  by `tools/iso_atlas`.
- Tile-graph pathfinding (`scene::nav`) read from overlays or terrain rules, with `PathOptions` for vehicles and a
  navigation overlay (`path.set_debug`).
- Draw-order sort key: every 2D primitive carries a `depth`; isometric tilemaps keep persistent GPU chunk buffers.
- HUD asset: `.owlhud` files loaded by a `HudLibrary`, drawn by an implicit `HudPass` in pixels, found by `ui.find` and
  packed by the exporter.
- HUD widgets: `UiWindow`, `UiLayout`, `UiScrollView` and virtualised lists, `UiTooltip`, `UiToolCursor`, clipping
  (`Renderer2D::setScissor`) and relative units (`size_unit`, `offset_unit`).
- HUD keyboard and gamepad focus, and the `ui_scale` player setting.
- Owl Nest HUD editor, `hud.extract_canvas` command, and a read-only image viewer.
- Owl Nest action map editor (`input_actions.yml`) and the input action map with Lua `input.is_action_*`.
- Fixed simulation tick (`simulation.tickRate`, `SimulationClock`, Lua `on_tick`) and `InputFrame` edges in C++ and Lua.
- Gamepads: 16 slots with buttons, sticks, triggers, dead zones and response curves (`input::AxisResponse`), and Lua
  `input.is_gamepad_*`.
- `session::EditorSession` with a `HeadlessSession`, the `OwlNestMcp` JSON-RPC dispatcher, an opt-in MCP HTTP transport
  on `127.0.0.1` and the `play.run` tool.
- Editor console, command journal, `.owlmacro` macros, and command API v2 (`component.set`, `scene.save`, `edit.undo`,
  `edit.redo`).
- Text: aspect-correct layout with alignment, word wrap and fit modes, UTF-8 code points, an outline and background box,
  and a font document in Owl Nest.
- Persistent 32x32 tilemap chunks and `.owltilemap` format 2 (binary payload, migrated from format 1).
- Owl Nest `.slang` syntax highlighting, a **Show Current Frame** toggle in the Animation editor, a hideable frame stats
  chip.
- Lua `scene.find_entities(tag)`, `entity.get_link`, `entity.set_tint` and `entity.get_tint`.
- A `SoundSource` with `playOnStart` loads its sound on first use.
- Engine Demo: hub redesigned as an RPG village, rebuilt platformer, Wolfenstein E1M1 raycast level with a fog-of-war
  minimap, Kenney CC0 art, sounds and victory objectives.
- Windows binaries carry the Owl icon and version information, with optional code signing in the `Package` action.
- `owl_bench` group `mcp`, and scenario files take `project` and `assets`.

### Changed

- Engine Demo controls appear as timed hints, its HUDs scale with the interface size, and the project is named
  `OwlEngineDemo` (`sample_project/` becomes `engine_demo/`).
- Engine systems and scripts read input actions instead of keys; `RaycastDoor` / `RaycastPushWall` `interactionKey`
  becomes `interactionAction`.
- Repository layout: `docker_run.sh` at the root, tests, `bench/`, `fuzz/` under `test/`, shaders in
  `engine_assets/shaders/<renderer>/`, generators in `tools/demo/`.
- Text stays smooth at any size (font atlas distance range 6 px) and `Renderer2D` transients no longer allocate in a
  frame.
- Tilemaps render from cached chunks, culled against the view, without the 16384-cell cap.
- Owl Nest file dialogues on Linux use `xdg-desktop-portal` over D-Bus; Windows programs open no console window.
- Owl Nest documentation illustrated by real screenshots and the README showcases Owl Engine and its sample games.
- Roadmap replanned: later releases merged into v0.5.0 to v0.8.0, 1.0 expected 2027-10-31; sample games planned in
  `design/sample-games.md`.

### Deprecated

- Lua `input.is_key_pressed`, `is_key_just_*`, `is_mouse_button_pressed` and `is_mouse_button_just_*`: read an action
  instead.

### Removed

- Unused engine fonts, engine sample content (`engine_assets/{scenes,templates,textures,animations,tilesets}`) and
  unused Engine Demo assets and scenes.
- `UiPanel::layout`, `spacing` and `padding` (a `UiLayout` arranges children).

### Fixed

- Vulkan: a driver older than 1.3 no longer crashes (the OpenGL fallback opens its window first),
  `GraphContext::getVersion` reports the minor version and `drawIndexedIndirect` enables its feature.
- Owl Nest no longer crashes in the Stats panel after Stop, nor when Play is stopped from the viewport or F7, and
  switching project unloads the previous one.
- Play reads `game_settings.yml` like the runner and no longer leaves `isometric.set_cell` edits in the edit scene.
- Export packs the scene paths and sounds named in `LuaScript` properties and the `UiToolCursor` texture; Lua
  `gamestate.set` keeps digit strings as strings.
- Saving a scene or HUD keeps its entity order, and a Lua callback failing every frame logs once.
- Text drawing is about 5 times faster again and HUD text cheaper per frame.
- CI: `PublishDoc` and `PublishPackage` send only filled fields, a stalled `OwlRunner --scenario` ends after 120 s, and
  sanitizer and arm64 runs scale the Lua time quota.
- Packages no longer carry run leftovers and the Windows packages include the Slang standard modules.

## [0.3.0] - 2026-10-11

### Added

- Owl Nest default layout with Default and Hierarchy Left presets, a status bar, a command palette (`Shift` twice) and
  `OwlNest --project <dir>`.
- Owl Nest File screen and reorganised ribbon with quick access, duotone SVG icons and the Lucide icon font
  (`gui/IconFont.h`).
- GUI widget kit (`gui/widgets/`: property grids, toggles, checkboxes, tooltips, chips, `ToastQueue`), the Inter UI font
  and a style guide (`design/nest-style-guide.md`).
- Owl Nest panels in the inspector style: Hierarchy search and icons, Content Browser folder tree, family chips,
  previews of scenes, prefabs, sounds, tilesets and meshes.
- Owl Nest viewport overlays, 2D / 3D views, Frame selection, Local / World gizmo axes, camera views with a view cube,
  and context menus.
- Owl Nest Editor Settings in tabs, text size shortcuts (`Ctrl+=`, `Ctrl+-`, `Ctrl+0`) and **Open** / **Clear** buttons
  on asset fields.
- Owl Nest project templates (empty 2D, raycast, voxel, mixed), session restore, autosave with crash recovery
  (`RecoveryManager`) and the sample project on the File screen.
- Hot reload of textures, scenes, tilesets, tilemaps and Lua scripts, and live recompilation of Slang shaders
  (`platform::FileWatcher`, `app::HotReload`).
- Editor command API: `commands::CommandRegistry` (`entity.create`, `component.add`, `prefab.instantiate`, ...) with
  scenario `command` / `undo` / `redo` steps.
- `scene::SystemSchedule` (named systems in fixed phases) and `scene::ComponentRegistry` for game-defined components.
- `OwlShaderBake` precompiles engine shaders; actionable errors with a `Fix:` hint; typed Lua binding registry
  generating the [Lua API reference](lua-api.md).
- Versioned file formats (`FormatVersion`, migration chain in `core::DocumentFormat`) with atomic writes
  (`platform::writeFileAtomic`).
- Optional engine modules (`OWL_MODULE_RENDER`, `PHYSICS`, `AUDIO`, `SCRIPT`, `GUI`) and the `linux-clang-minimal`
  preset.
- `GameExporter` shared by *Pack Game*, `OwlNest --export` and tests; runner `--headless`, `--smoke-test` and
  `--scenario <file.owltest>`.
- `OwlRunner --frame-bench` with CPU phases, GPU timestamps, cold start and `--capture <png>`; `owl_bench` harness with
  a baseline.
- Conan 2 for every preset (`conanfile.py`, `conan.lock`, `conan/recipes/`), `conan create .` packaging checked by
  `test_package/`, and an optional binary cache.
- `./docker_run.sh` runs any build, test or CI command in the Docker build image (`--gui`, `--perf`).
- CI: `DependencyReport`, `Fuzz` (libFuzzer nightly), `Bench`, a Clang Static Analyzer configuration and the `PR Ready`
  composite check.
- CI: diff-scoped `ClangTidy`, a coverage gate, CodeStyle sub-checks (`python`, `secrets`, `test-assertions`,
  `doc-identifiers`) and Windows MinGW Conan profiles.

### Changed

- Scenes, prefabs and snapshots are read with rapidyaml (about 17 times faster) and prefab instantiation is about 60
  times faster.
- Vulkan records one command buffer with two frames in flight, uses a per-frame ring and the Vulkan Memory Allocator,
  and `readPixel` no longer stalls.
- Owl RHI named and documented, with `PipelineState` objects and explicit `bindTextures`.
- Physics runs at a fixed step (60 Hz, per-scene settings) on a multi-threaded Box2D solver; each scene owns its physics
  world and script bindings.
- `find_package(OwlEngine)` exposes EnTT as its only public dependency (imgui in the optional `Owl::Gui` target) and
  `libOwlEngine.so` exports only `OWL_API` symbols.
- Dependencies updated; windowing: `OWL_WINDOW_PLATFORM` (`auto`, `wayland`, `x11`), per-application `app_id`, clean
  `SIGINT` / `SIGTERM` exit.
- Performance: 10000 sprites under target, closed-form `math::Transform`, faster raycast, Lua bindings and Vulkan
  batching.
- Lighter pull-request CI on a single Ubuntu 26.04 image (GCC 15, Clang 22), arm64 cross-compiled with qemu tests, heavy
  jobs nightly.
- Roadmap rethought toward 1.0.0 and the changelog split into `CHANGELOG.md` and `doc/pages/changelog.md`.

### Deprecated

- `OWL_ENABLE_PROFILING` (use `OWL_PROFILER=chrome`) and `OWL_FORCE_X11=1` (use `OWL_WINDOW_PLATFORM=x11`).

### Removed

- Theme presets Dark, Dark Blue, Nord and Solarized, and the editor `theme.yml`.
- DepManager (`depmanager.yml`, `cmake/Depmanager.cmake`, `ConfigureRemote`), the unused `tinyxml2`, `zeus` and
  `debugbreak` dependencies and the LeakSanitizer preset.
- Dead code: `ScriptEngine::loadScript`, `parallelForEach`, `core::IFactory`, `RenderCommand::drawLine`,
  `Scene::updateRaycastDynamicWalls` and unused `NOLINT` names.

### Fixed

- Wayland: GLFW from a local recipe, no freeze after the first frame, framebuffer at window size, no multi-viewports.
- OpenGL: SPIR-V draws on NVIDIA, picking without stalls, `generateMips`, `R8` and `Rgba32F` uploads.
- Vulkan: samplers, background quad, descriptor sets and Owl Nest text with ImGui 1.92.9.
- Editor undo restores entities in place, deleting keeps children in place, and the dirty flag follows an undo
  generation counter.
- Prefab update and revert work in place, and exports rewrite `pat:` texture paths and pack voxel tilesets and `UiText`
  fonts.
- `scene.destroy_entity` is deferred to the end of the frame, Lua `on_collision(other_id)` is called, and a teleport to
  a missing level keeps the current one.

### Security

- CI secrets never reach argv or the log (`OWL_DEPLOY_PASSWORD`, `ci.utils.secrets`), and publication no longer runs the
  server's `api.py`.
- `.owlpack` reader hardened against forged packs, and CMake configure no longer prints `.env` values.
- Lua sandbox hardened (no bytecode, `string.dump` removed) with per-instance memory and time quotas (`ScriptQuotas`).

## [0.2.1] - 2026-06-27

### Added

- Voxel data model `owl::data::voxel` (`BlockRegistry`, 16³ `Chunk` with RLE, sparse `VoxelWorld`) and `ChunkMesher`
  with greedy meshing and ambient occlusion.
- Block metadata (orientation and state per block), transparent and water pass, and a `Renderer3D` depth-tested forward
  path with one directional light.
- `RendererVoxel` layer and `VoxelWorld` component, with voxel worlds rendered and navigable in the editor viewport.
- Procedural terrain (`math::PerlinNoise`, `TerrainGenerator`) streamed in and out around the camera.
- `VoxelPlayer` first-person controller with voxel collision, and block breaking and placing through `raycastVoxel`.
- Owl Nest voxel editor: Voxel Palette brush and `.owlvoxstruct` structures.
- `Camera3DController` and `FlyCamera` component, DCC-style editor camera navigation and `Window::setCursorMode`.
- New `voxel_blocks.owltileset` and regenerated 2D platformer and world-map atlases.

### Changed

- Module taxonomy reorganised (breaking, public namespaces changed): `app` split from `core`.

### Removed

- Chunk Inspector panel.

### Fixed

- `math::Matrix` initializer-list constructor read out of bounds.
- Vulkan: pipeline storm, device-destruction leak, descriptor update cascade, validation errors, batch-fence deadlock
  and render-pass incompatibility.
- OpenGL: all `Renderer2D` content was invisible; voxel atlas bleeding.

## [0.2.0] - 2026-06-02

### Added

- Renderer stack architecture: composable per-scene `RenderLayer` / `RenderStack` pipeline.
- Raycasting renderer `RendererRaycast` with textured floors and ceilings, fog, billboard sprites, variable and
  transparent walls.
- Animated `RaycastDoor` and `RaycastPushWall` components.
- Tilemap system: `scene::Tileset` (`.owltileset`), standalone `.owltilemap` `TilemapAsset` and an editor.
- Camera markers in the editor viewport and snap-to-grid translation gizmo.
- Scene transition effects (`Fade*`, `Wipe*`) with Lua `ui.transition_play`.
- GPU compute foundation (`gpu::ComputeShader`, `gpu::StorageBuffer`) with `WorldTransformPass`, `RaycastDDAPass` and
  `BitonicSortPass`.
- Per-renderer Vulkan descriptor blocks (`gpu::RendererDescriptors`).

### Changed

- `Renderer2D` rewritten as instanced with a per-instance storage buffer, and tilemaps routed through the instanced
  `RendererTilemap`.
- Scene loading about 550 times faster.
- `Scene.EnabledRenderers` overrides layer order, and editor shortcuts (`Ctrl+S`, `Ctrl+Z`, ...) bypass
  `WantCaptureKeyboard`.
- Renderer sources reorganised by kind; the `CodeStyle` CI action is one read-only command.

### Fixed

- Tilemaps and raycast tilesets were missing from packaged games; packaged games ship the engine assets they need.
- Vulkan `ComputeShader::dispatch` recorded into the wrong command buffer.
- Raycaster: player rotation, wall drawing and HUD rotating with the camera.
- Editor drew tilemaps in 2D regardless of `RendererTag`; empty render-stack layers flickered multi-scene views.
- Hidden triggers no longer fire.
- mingw-gcc 15 link on Windows and a resilient `get_git_hash`.

## [0.1.1] - 2026-04-30

### Added

- **Full Markdown rendering for the in-editor help and live preview**
    - New `codeEditor::MarkdownDocument` parser backed by **md4c 0.5.2** (new
      DepManager recipe at `OwlDependencies/Libs/md4c/`) — CommonMark + GFM
      tables / strikethrough / autolinks. Public block model (`MdHeading`, `MdParagraph`, `MdCodeBlock`, `MdImage`,
      `MdTable`,
      `MdList`, `MdBlockQuote`, `MdHRule`) covers everything the help pages use.
      Lazy implicit-paragraph creation handles md4c's tight-list quirk where
      `MD_BLOCK_P` is skipped inside `MD_BLOCK_LI`.
    - Rewritten `codeEditor::MarkdownPreview` walks the parsed block list and
      emits ImGui draw calls directly: scaled headings (1.60× / 1.30× / 1.15× of
      body via `PushFont(font, size)`), inline emphasis / strong / strikethrough
      / inline code, GFM tables with `BeginTable` (borders + row stripes), code
      blocks rendered through cached read-only `TextEditor` widgets with full
      syntax highlighting (Lua / C / C++ / Python / YAML / JSON / Markdown / XML
      / **Bash** — new `Language::Bash` definition with POSIX/bash keywords and
      common shell built-ins), block / inline images loaded via `lunasvg` (SVG)
      and `stb_image` (raster) with on-disk cache per source path. PNG/JPG
      textures load through the engine's `pat:` serialized form and are
      displayed with flipped UVs to compensate for stb_image's bottom-up loading.
    - External links and `https://` images are preserved in the rendered output;
      clicks open the user's default browser via the new
      `core::utils::openExternalUrl` helper (Linux: `xdg-open` via fork+execvp;
      Windows: `ShellExecuteW`; URL scheme restricted to `http(s)://` and
      `mailto:` for safety).
    - `cmake/HelpAssets.cmake` now scrubs Doxygen syntax at bundle time:
      `# Title {#page-anchor}` → `# Title`, `[TOC]` lines dropped,
      `(../images/foo.svg)` rewritten to `(images/foo.svg)`,
      `(engine_assets/<dir>/foo.png)` (used by the README logo) rewritten to
      `(images/foo.png)`, and `doc/images/` plus `engine_assets/logo/` copied
      into `engine_assets/help/images/`. HTTPS images referenced from any
      bundled markdown (e.g. the 24 shields.io badges in the README) are
      downloaded once via `file(DOWNLOAD)` into
      `engine_assets/help/images/badges/<sha1>.svg` and the references
      rewritten to local paths so the runtime renderer never has to fetch.
    - `nest::panel::HelpPanel` now defaults to the project README on first
      open; a draggable splitter between the page tree and the content pane
      lets the user resize the navigation column (clamped to a 120 px minimum
      per side).
    - `imgui_markdown` removed from `depmanager.yml` (replaced).
- **Live preview for markup documents**
    - `codeEditor::SvgPreview` rasterises the live SVG buffer through `lunasvg`
      into a `Texture2D` (cap 2048 px / side, ARGB-premul → RGBA-straight).
    - `CodeEditorDocument` now offers a vertical splitter with a draggable
      handle when the active language is Markdown or XML/SVG. Auto-enabled on
      load, toggleable via the new **Text → Preview** ribbon button.
- **In-editor help pages**
    - `cmake/HelpAssets.cmake` bundles `doc/pages/*.md` plus root README /
      CHANGELOG / CONTRIBUTING into `engine_assets/help/` at configure time and
      generates an `index.yml` (id, title, category, path).
    - `nest::panel::HelpPanel` reads the index, renders the selected page via
      `MarkdownPreview`, supports search, categorised navigation, back/forward
      history. Internal `[link](other.md)` clicks navigate within the panel.
    - `help.context` action (default shortcut **F1**) opens the help page that
      documents the SceneHierarchy component header most recently hovered (`SceneHierarchy::lastHoveredComponentName`);
      falls back to the editor
      overview when nothing is hovered.
    - File ribbon tab gained a **Help** group; the Welcome screen surfaces a **Getting Started** entry pointing to the
      new
      `doc/pages/getting_started.md`.
- **Animation editor**
    - New reusable `.owlanim` asset (`scene::AnimationClip` —
      `source/owl/public/scene/AnimationClip.h`): texture, grid, frame range, frame
      duration, loop, optional speed curve, with YAML round-trip and unit tests in
      `test/unit/scene_tests/AnimationClip_test.cpp`.
    - `nest::AnimationDocument` opens as a 4th document type (alongside Scene, Code,
      NodeGraph). Three-panel layout: live spritesheet preview, properties (texture
      drop target, columns / rows / first / last frame, frame duration, loop, speed
      curve via `gui::widgets::curveEditor`), and a frame-range timeline.
    - New `gui::widgets::sequencer()` widget (`source/owl/public/gui/widgets/Sequencer.h`)
      wraps `ImSequencer` from the existing imguizmo bundle — owl-friendly API (`SequencerEntry` / `SequencerOptions`),
      no third-party types leak through the
      public surface.
    - Contextual ribbon `Animation` tab when an `AnimationDocument` is active:
      Playback (Play / Pause / Stop), Frame (Previous / Next) and File (Save / Save
      As / Close) groups. Ribbon File → "New Animation" entry creates an untitled
      clip; double-click / drag-drop in the Content Browser routes `.owlanim` files to
      the document. Dedicated browser icon (filmstrip glyph).
    - `sample_project/animations/coin.owlanim` showcases the new asset (mirrors the
      level 2 coin animation, including the Smooth speed curve).
- **Scene Flow editing**
    - Visual create of teleport links — every scene node carries a ghost `+ Add teleport`
      output pin. Dragging it onto another scene's entry pin spawns a `Trigger`
      (`Type=Teleport`, `LevelName=<dest>`) entity in the source scene at world origin and
      wires the canvas link in one undoable step.
    - Visual delete of teleport links — pressing Delete on a Teleport link destroys the
      matching `Trigger` entity, removes the canvas pin, and erases the link in one undoable
      step.
    - Per-pin `targetName` editing — right-click a scene node → `Edit teleport target →
      <pin>` opens a modal that mutates the live `Trigger.targetName` and pushes a
      `ModifyEntityCommand` on the source scene's undo manager (rapid keystrokes coalesce).
    - New `commands::SceneFlowCompositeCommand` glues a `SceneUndoCommand` with a
      `NodeGraphUndoCommand` so a single undo step reverses both halves; complemented by
      `AddPinAndLinkCommand` / `RemovePinAndLinkCommand` for the canvas pin+link bundle.
    - New `EditorLayer::loadOrOpenSceneDocument(path)` synchronously opens a `SceneDocument`
      for an on-disk scene without yanking focus — used by Scene Flow link edits to mutate a
      scene that may not currently be in a tab.
    - Per-layer vertical centring in the BFS auto-layout so single-node layers no longer hug
      the top edge.
- **NodeCanvas widget polish**
    - Text level-of-detail: pin labels stop drawing below `0.6` zoom and node titles below
      `0.3`, leaving the silhouette + connectors visible for graph-overview navigation.
      Exposed via free helpers `gui::widgets::shouldDrawPinLabels` /
      `shouldDrawNodeTitles` for downstream reuse and unit tests.
    - New public pin-manipulation API on `NodeCanvas`: `addOutputPin` / `addInputPin` /
      `removeOutputPin` / `removeInputPin`. Pin removal automatically strips dangling links.
- **Inspector field interactions**
    - Drag-drop from Content Browser to inspector fields via shared
      `gui::widgets::assetDropTarget` helper (`source/owl/public/gui/widgets/AssetField.h`).
      Per-extension validation through a new `AssetKind` enum (Texture, Font, Sound,
      LuaScript, AnyScript, Scene, Prefab, Any). Reuses the existing `CONTENT_BROWSER_ITEM`
      ImGui payload — no new wire format. Drops accepted on Sprite/AnimatedSprite/
      BackgroundTexture/UIImage textures, Text font, SoundSource asset, LuaScript path.
    - `gui::widgets::textureField()` consolidates the previously inlined texture-picker
      pattern (thumbnail / popup / "Remove texture") into one helper used by every
      texture-aware component. Removes ~150 lines of duplicated drag-drop boilerplate
      from `gui/component/render.cpp`.
    - Texture thumbnails now show a `(loading...)` overlay while an async-loaded texture
      decodes (`LoadState::Pending`) and a red `(failed)` overlay on `LoadState::Failed`.
    - Font preview: Text component shows a sample-string rendering of the selected font ("Aa Bb 1!? éàüÇ" — covers
      lower/upper case, digits, punctuation and accented
      Latin-1 glyphs). Backed by a new `gui::FontPreviewCache` that lazily renders each
      font into a 256x64 framebuffer via `Renderer2D::drawString` and caches the result;
      pumped from `EditorLayer::onUpdate` and freed on `UiLayer::onDetach`.
    - Latin-1 glyph rendering fix — `Font::getGlyphBox` and `Renderer2D::drawString` no
      longer sign-extend `char` codepoints into garbage 32-bit values; the renderer
      additionally decodes UTF-8 source text into Latin-1 codepoints up front, so
      accented glyphs (`éàüÇ`…) coming from YAML scenes or Lua strings render correctly
      everywhere instead of falling back to `?` or `Ã©` byte pairs.
- **Sample project showcases v0.1.1 features**
    - Main-menu subtitle exercises UTF-8 / Latin-1 rendering (`Démo des fonctionnalités… caractères éàüÇ`).
    - Level-2 coins use a Smooth `AnimatedSpriteRenderer.speedCurve` so the rotation
      pulses (slow at the loop boundary, fast in the middle).
- **Curve editor for animated properties**
    - New `math::Curve` (`source/owl/public/math/Curve.h`) — sorted keyframe list with
      Constant / Linear / Smooth interpolation, flat-hold extrapolation, and YAML
      round-trip (handled inline in component serializers, default-empty curves omit
      `speedCurve` from the YAML to preserve byte-identical scene files).
    - New `gui::widgets::curveEditor` widget (`gui/widgets/CurveEditor.h`) — wraps
      ImCurveEdit from the existing imguizmo bundle (no new DepManager dependency).
      Drag points, double-click to add, right-click to remove; companion combo selects
      the interpolation mode.
    - First end-to-end consumer: `AnimatedSpriteRenderer.speedCurve` remaps per-frame
      animation advancement (`Scene::onUpdate` multiplies dt by
      `speedCurve.evaluate(progress)` when the curve is non-empty).
- **Scene Flow refinements**
    - Pin labels are now drawn **inside** the node frame (via a new `NodePin::labelColor` field
      and a `CustomDraw` override on `NodeCanvas`). GraphEditor receives `nullptr` for slot names
      so it stops rendering them outside the rect — labels stay aligned with their slot circles
      regardless of zoom
    - Compact pin labels: **just the source identifier** prefixed by a single-glyph kind hint (`† DangerZone`,
      `★ VictoryZone`, `λ checkpoint`, plain `LevelPortal` for Teleport).
      Destination is implicit from the link, no need to repeat in the label
    - **Right-click hit-test fix** — `GraphEditor` only sets `nodeOver` when a slot is hovered;
      clicks on the node body returned `-1`. `NodeCanvas` now does its own canvas-space hit-test
      against stored node rects when GraphEditor reports no node. Right-click on the body shows
      the Edit / Delete menu as expected
    - **Layered layout** replaces the random 4-column grid: BFS from `Project::firstScene`
      assigns each scene a column = its depth, orphans land in a "limbo" column on the right.
      Within each column, scenes are sorted alphabetically. Reduces link crossings substantially
      on the sample project
- **Scene Flow UX pass**
    - Nodes auto-size from their title and pin labels (`NodeCanvas::measureNode` exposes the
      `ImGui::CalcTextSize`-based size); the SceneFlow grid layout uses **actual** node widths /
      heights to compute column / row offsets so nothing overlaps regardless of label length
    - `NodeCanvas` now sets `GraphEditor::Options::mDrawIONameOnHover = false` — pin labels are
      drawn permanently (they used to only appear on hover, making the graph illegible at a glance)
    - Scene titles drop the `.owl` extension (implicit since every node is a scene)
    - Output pin labels show the **source trigger entity name** parsed from the scene's YAML (e.g.
      `LevelPortal → level2 @ SpawnPoint`); transitions of type `Death` and `Victory` are
      now extracted too (they also serialize a `LevelName`) and styled distinctly:
      `[death] DangerZone → game_over`, `[victory] VictoryZone → victory`
    - Best-effort scan of attached Lua scripts for `scene.load_scene("...")` calls — adds extra
      output pins tagged `scene_lua_exit` so Lua-driven transitions appear alongside Trigger ones
    - Right-click menu: on a node → **Edit scene** (opens it in a new tab) / **Delete scene**
      (with confirmation modal); on empty space → **Add new scene...** (creates an empty `.owl`
      under `scenes/NAME.owl` and opens it). Canvas auto-rescans after create/delete
    - The global **Scene Hierarchy** and **Properties** panels now host SceneFlow content while
      that document is active: the hierarchy lists every scene (orphans drawn red, click-to-select,
      double-click to open), the properties panel shows the selected scene's path, transitions (colour-coded: red =
      death, green = victory, blue = Lua) and an "Open this scene" button.
      Wired through new `Document::overridesGlobalPanels()` / `renderHierarchyPanel()` /
      `renderPropertiesPanel()` virtuals — generic for future node-graph documents
- **Node graph framework + Scene Flow view**
    - Reusable `gui::widgets::NodeCanvas` widget (public header
      `source/owl/public/gui/widgets/NodeCanvas.h`) — nodes with typed input/output pins, UUID-based
      identity, pan/zoom/selection, link validator, callbacks for link create/delete, node move,
      node select and **double-click** (detected in the wrapper since GraphEditor has no native
      double-click signal). Pimpl over `GraphEditor` from the existing `imguizmo` 1.92.7 DepManager
      package — the bundle ships GraphEditor + ImSequencer + ImCurveEdit + ImGradient + ImZoomSlider
        + ImLightRig alongside ImGuizmo, so no new dependency was needed
    - `gui::widgets::NodeCanvasSerializer` — domain-agnostic YAML round-trip (`.owlflow` format)
      for full canvas save/load, plus `serializeSubset`/`pasteSubset` for copy/paste with fresh UUIDs
    - `NodeGraphDocument` — third `DocumentType` alongside Scene and Code, generic node-graph
      document with its own `NodeGraphUndoManager` (a `UndoManager` typed for `NodeCanvas`); pastes, saves
      and loads through the serializer. Content Browser wires `.owlflow` double-click to a new
      `EditorLayer::openNodeGraphFile` handler; ribbon contextual tab "Graph" with save/close
    - Node-graph undo commands (`source/owlnest/sources/commands/NodeGraphCommands.{h,cpp}`):
      `AddNodeCommand`, `RemoveNodeCommand` (restores attached links on undo), `MoveNodeCommand`
      (merge-coalesces rapid drag into a single step), `AddLinkCommand`, `RemoveLinkCommand`
    - `SceneFlowDocument` — first consumer of the framework, specialises `NodeGraphDocument`
      to render the project's scenes as a graph: one node per `.owl` file, entry pin + one output
      pin per Teleport trigger found in that scene, links wired from output → destination entry,
      orphan scenes (unreachable from `Project::firstScene`) drawn with a red title. Double-click
      navigates to a scene via `EditorLayer::openScene`. Exposed from the ribbon File tab via
      a new "Views → Scene Flow" button. Link create/delete and per-pin target editing (writing
      new trigger entities back into the source scene) are deferred — they require composite
      `SceneUndo + NodeGraphUndo` commands and stay out of this first slice
    - 16 new unit tests: `test/unit/gui_tests/NodeCanvas_test.cpp` (topology, link validator veto,
      cascaded link removal, selection round-trip, pin→node lookup, clear) and
      `test/unit/gui_tests/NodeCanvasSerializer_test.cpp` (empty/full canvas round-trip, custom data
      preservation, malformed YAML rejection, subset serialize, paste with fresh UUIDs)
- **Undo system templatized over its target type**
    - Templatized `UndoCommand` + `UndoManager` (parameterized over the edited `Target`) in
      `source/owlnest/sources/UndoCommand.h` /
      `UndoManager.h` — both now header-only templates, `IUndoTarget` as common marker base
    - `SceneUndoCommand` / `SceneUndoManager` aliases preserve the current editor behaviour one-to-one;
      every existing command (`Entity*`, `Component*`, `Hierarchy*`, `Prefab*`) migrated to
      `SceneUndoCommand`
    - Unblocks a future node-graph undo stack parallel to the scene one (workstream B-5/B-6)
- **Async texture loading with placeholders**
    - `TextureDecoder` helper (`renderer/TextureDecoder.h`) — CPU-only decode primitive (`peekImageSize`,
      `decodeImageBytes`, `decodeImageFile`) with per-thread stb_image flip state
      so multiple workers can decode concurrently
    - New `Texture2D::createFromSerializedAsync(name, scheduler)` returns immediately with a
      placeholder-sized Rgba8 texture filled white; worker decodes, termination callback uploads
      real pixels on the main thread and flips `LoadState` to `Ready` (or `Failed`, keeping the
      placeholder visible)
    - Dimensions peeked from the PNG/JPG header up front, so every texture is created at its real
      size from frame 0 — binding, UV coords and atlas math unaffected
    - `Texture2D::createFromSerializedForDeserialize()` wrapper used by scene components (`SpriteRenderer`,
      `AnimatedSpriteRenderer`, `BackgroundTexture`, `UIImage`) — async when an
      `Application` is alive, synchronous fallback for `PackWriter` and unit tests
    - `RunnerLayer::finishTransition()` logs the count of still-pending textures after a teleport
      as a diagnostic trace — smooth scene changes in the runner, no more frame hitch on rich
      scenes
- **Async operations in editor** with progress modal (`AsyncProgressModal` panel)
    - Pack Game / Pack Scene: async with per-entry progress bar and cancel support
    - Save Scene: serialize on main thread, write file on background thread
    - Open Scene: read file on background thread, deserialize on main thread in callback
    - Content Browser: directory scanning cached and refreshed asynchronously (no more per-frame
      `directory_iterator`)
    - Runner scene transitions (`handleTeleportRequest`): load scene bytes in background, deserialize
      and swap on main thread, old scene stays rendered during load
- **Deferred shader compilation** with ImGui loading screen at startup
    - `Renderer::initContext()` / `Renderer::initShaders(callback)` split
    - Per-shader progress displayed before editor/runner starts
- **Packaging wizard panel**
    - Dedicated dialog opens when clicking Pack Game (replaces the bare folder dialog)
    - Fields: destination path with Browse button, target platform (read-only), compress/obfuscate options
    - Pre-packaging validation phase runs async and collects warnings for unresolvable texture,
      sound, font, script, and trigger-scene references (`AssetScanner::scanScene/scanProject`
      accept an optional `std::vector` of warning strings as output)
    - Validation modal shown if warnings: bullet list + "Proceed anyway" / "Cancel"
    - Async validation phase reuses its scanned assets for the pack phase (avoids double scan)
    - Detects missing `OwlRunner` executable and empty asset list
    - Post-pack build report: asset count, pack size (MiB), and total duration displayed in the
      completion modal
- **Ribbon menu + generic code editor**
    - New `gui::widgets::Ribbon` widget (`source/owl/public/gui/widgets/Ribbon.h` +
      `private/gui/widgets/Ribbon.cpp`) — Office-style horizontal banner with top-level tabs →
      groups → large buttons (32 px icon + label underneath) and small buttons (16 px icon + label
      on the right, 3 per column). Button callbacks drive enabled/checked/click state
    - `UiLayer::setTopBarCallback()` reserves space between `ImGui::Begin(OwlDockSpace)` and
      `ImGui::DockSpace()` for the ribbon. The classic `ImGuiWindowFlags_MenuBar` is gone
    - `Ribbon::setTabHighlighted()` renders the tab title with the theme accent (secondary) colour
      — applied to the **File** tab so it reads as the primary entry point
    - Tab-bar padding (`FramePadding {14, 6}`) gives the titles breathing room; `TabSelected`
      pushed brighter so the active tab stands out clearly against dimmed siblings
    - `EditorLayer` replaces the menu bar + floating Play/Pause toolbar + gizmo `ButtonBar` with
      a single ribbon, a contextual **Scene** / **Text** tab switches on the active document type
        - **File**: Project (New / Open large, Save / Save As / Close small), Recent (large button
          opens a popup listing `EditorSettings::recentProjects`), Package (Pack Game large),
          Session (**Exit large**)
        - **Edit**: History (Undo / Redo large), Settings (Engine / Editor / Project small)
        - **Scene**: File (New / Open large, Save / Save As / Import small, **Close large** last),
          Playback (Play / Stop large, Pause / Step small), Gizmo (Translate / Rotate / Scale **large**), Package (Pack
          Scene large)
        - **Text**: File (Save / **Close** large)
    - New `saveProjectAs()`: picks a destination folder, duplicates the current project
      recursively via `std::filesystem::copy`, then switches the editor to the new location
    - New `CodeEditorDocument` (`DocumentType::Code`): a second kind of `Document` that opens
      text/source files as their own tab. Backed by **imgui_color_text_edit** 1.92.6 pulled via
      DepManager (bumped `imgui` to 1.92.6-docking to match)
    - Built-in syntax highlighting: **Lua**, **C**, **C++**, **Python**, **JSON**, **Markdown**
      (from the library); custom definitions added for **YAML** and **SVG/XML** in
      `source/owlnest/sources/document/codeEditor/LanguageDefinitions.{h,cpp}`
    - ContentBrowser double-click routes
      `.lua`/`.py`/`.c`/`.cpp`/`.cc`/`.cxx`/`.h`/`.hpp`/`.hxx`/`.yml`/`.yaml`/`.json`/`.md`/`.markdown`/`.svg`/`.xml`
      to a new code-editor tab (or re-activates the existing one)
    - Status footer in each code editor shows language + line/column + INS/OVR. Ctrl+S saves;
      dirty tracked via `TextEditor::GetText() != savedText`
    - Rich rendering of Markdown and SVG inside the editor is deferred to a future release —
      added to the roadmap
- **Editor fonts**
    - Engine fonts (Roboto Regular / Bold / Italic, JetBrains Mono Regular) ship as loose TTFs in
      `engine_assets/fonts/{roboto,jetbrainsmono}/` and are loaded via `AddFontFromFileTTF`
      (resolved through `Application::getAssetDirectories()`). The previous `.embed` hex-array
      headers have been dropped from the binary
    - Dedicated code-editor font: **JetBrains Mono** monospace for column-aligned glyphs in the
      TextEditor widget, rasterised at the user-configured size
    - `UiLayer::setUiFontSize()` + `setCodeFontSize()` static setters applied from `main.cpp`
      before the `Application` is constructed — the atlas is built once in `UiLayer::onAttach`;
      size changes take effect on the next startup
    - New `EditorSettings::uiFontSize` (14–24, default **18**) and `codeEditorFontSize`
      (8–48, default **17**) sliders in the Editor Settings panel
    - Modal button widths (close-doc, welcome, pack wizard, pack validation, async progress) are
      now `ImGui::GetFontSize() * N` so they scale with the UI font size instead of clipping
- **Multi-document architecture**
    - New abstractions in `source/owlnest/sources/document/`: `Document` (interface),
      `DocumentManager` (open list + active tracking), `SceneDocument` (scene wrapper)
    - Scene-scoped state (`editorScene`, `activeScene`, path, Play/Pause/Stop, teleport request,
      save/load request, undo stack, **the Viewport panel**) moved from `EditorLayer` into
      `SceneDocument`
    - `EditorLayer` becomes a host of `DocumentManager`; all scene operations delegate to the
      active document
    - **Per-document Viewport**: each `SceneDocument` owns its own `Viewport` instance with its
      own framebuffer and a stable ImGui window id (`##scene_UUID`). ImGui's docking groups
      viewports that share a dock node as tabs automatically, and users can tear a tab off to
      see several scenes side-by-side
    - Dirty marker rendered via `ImGuiWindowFlags_UnsavedDocument`. No close `x` and no collapse
      button on viewport tabs (`ImGuiWindowFlags_NoCollapse`, `p_open = nullptr`) — scenes are
      closed via `Current > Close Scene` or `Ctrl+W`, which route through a "Discard changes /
      Cancel" modal when the document is dirty. New viewports auto-dock to the central node on
      first open
    - `File > Open Scene` opens in a new tab (or switches to the existing one when already open),
      `File > New Scene` creates an Untitled tab (or reuses a pristine one)
    - Play/Pause/Stop/Step toolbar + gizmo control bar are hidden when the active tab is not the
      document currently running; the toolbar positions itself over the *active* viewport's
      bounds (updated when the user switches tabs)
    - Per-document undo/redo stack (Ctrl+Z / Ctrl+Y acts on the active doc)
    - New shortcuts: `Ctrl+W` close active document, `Ctrl+Tab` / `Ctrl+Shift+Tab` cycle
    - Background simulation: non-active tabs in Play mode advance their physics/scripts without
      rendering (new `iRender` flag on `Scene::onUpdateRuntime`); only the active viewport
      writes to its framebuffer
    - `gui::BasePanel::onRender()` becomes virtual so specialised panels (the per-document
      Viewport) can provide their own `ImGui::Begin` with close button + unsaved-document flag
      while reusing the focus/hover/size bookkeeping
- **File type icons + icon buttons**
    - Per-extension content-browser icons built from the `base_file_ext_icon` template with a
      ribbon label and a central type glyph
    - Sound (`wav`, `mp3`, `ogg`, `flac`) — speaker; mesh (`obj`, `gltf`, `glb`, `fbx`) — isometric
      cube; source (`py`, `cpp`, `h`, `c`) — language-specific silhouette; docs (`md`) — markdown
      mark with down arrow
    - Central glyphs added to existing icons: `png`/`jpg` (image with sun + mountains), `svg`
      (vector anchor points), `glsl` (GPU chip), `owl` (isometric scene floor), `yml` (gear),
      `ttf` ("Aa" sample), `lua` (crescent moon + star), `json` (curly braces with colon dots)
    - Theme accent (secondary) now resolves to an amber/gold matching the Owl Nest brand (fixed
      `#ffc726`), no longer tied to `ImGuiCol_ButtonActive`
    - Clarity pass on non-browser icons for readability at 64 px atlas size:
        - Thickened strokes previously under 3 atlas-px: `save` internal lines, `circle` radius,
          `interaction` key outline
        - Redesigned `content_browser` (grid of four tiles, selected tile filled), `animated_sprite`
          (filmstrip with sprocket holes), `new_scene` (clean "+" in place of 8-point spark)
        - Replaced SVG `text`-rendered glyphs with paths in `interaction` (letter E) and
          `lua_callback` (code-block chevron + amber dot)
        - Disambiguated overlapping silhouettes: `add_child_entity` now shows a parent → child
          tree; `background` (night sky with stars + horizon), `sprite` (outlined landscape),
          `ui_image` (amber inner tile) are visually distinct instead of three image variants
    - Icons added to Welcome screen (New / Open Project), Pack Wizard (Browse / Start / Cancel),
      validation modal (Proceed / Cancel), AsyncProgressModal (Close / Cancel), Content Browser
      rename and delete dialogs, Log panel Clear, Settings/Parameters/Project Settings OK/Cancel
- **Sound preview in inspector**
    - Play / Stop button on the SoundSource component to preview the sound using the current
      volume and pitch settings (non-spatial)
- **Recent projects** persisted in `EditorSettings::recentProjects` (up to 10 entries)
    - "Open Recent" submenu in File menu with full-path tooltip per entry
    - Welcome screen modal when no project is loaded (New / Open / Recent list, closable via `x` or
      `File > Welcome Screen`)
- **Editor menu replaced by the ribbon** — the former `ImGui::BeginMenuBar` drop-downs, the
  floating Play/Pause toolbar, and the gizmo `ButtonBar` are all gone. All actions now live in
  the ribbon (File / Edit / Scene|Text tabs). "Show Stats" moved into the Editor Settings panel
- **Tooltips everywhere** with hover delay (~0.4s)
    - Descriptive tooltips on all 7 trigger types (Victory, Death, Target, Teleport, Timer,
      Interaction, LuaCallback) and their sub-fields
    - Tooltips on PhysicBody, Player, Camera, SoundSource, SoundListener, Canvas, UIRect,
      UIButton, UISlider fields
    - Reusable `fieldTooltip()` helper
- **Scene transitions** and packaging quality-of-life
    - `scene.quit()` Lua API for clean application exit
    - `SettingsManager::loadDefaultsFromString()` for pack-based settings loading
    - `PackWriter` progress callback + cancel check
    - `AssetScanner` now scans UIImage, AnimatedSpriteRenderer, all trigger LevelNames,
      `sound.play()` in Lua, and deferred `scene.load_scene` patterns
- **Documentation**
    - Mermaid diagram support in Doxygen (via CDN mermaid.js in header.html)
    - Mermaid diagrams in architecture, scene, scripting, editor, sound, physics doc pages
    - Doxygen favicon using the owl logo (32x32 + 16x16)
    - GitHub community files: CONTRIBUTING.md, CODE_OF_CONDUCT.md, SECURITY.md, CHANGELOG.md
    - `.claude/rules/documentation.md` with format conventions for markdown tables, Mermaid,
      roadmap, and CHANGELOG
- **Testing**
    - AssetScanner unit tests (18 tests covering all asset types, recursion, deduplication,
      cancellation)
- **Code quality**
    - Component-scoped `PushID(T::name())` in the templated `drawComponent` helper to prevent ImGui label collisions
      across components that share field names (e.g., "Colour" in multiple renderers)
    - Index-based `PushID` in LuaScript property loop

### Changed

- Version bumped to **0.1.1**
- Tests that touch process-level resources (freetype, GLFW, OpenAL, msdfgen, script) now serialize
  via CMake `RESOURCE_LOCK` to avoid sporadic SEGFAULTs in parallel ctest runs
- LuaBindings: removed unused `iArgIndex` parameter from `findEntity`
- `ContentBrowser`: mutation methods (create/import/rename/delete/drop) now flag `m_rescanRequested`
  instead of relying on per-frame directory scans
- UI rounding reduced across all theme presets — `windowRounding` / `tabRounding` /
  `controlsRounding` now 2–3 px (was 4–10) for a crisper, more technical look
- `UiLayer::codeFontSize()` is no longer a `constexpr 13.f` — it returns the user-configured size
  set via `UiLayer::setCodeFontSize()` at startup

### Fixed

- **Sample project main menu Lua flood** — `string.format("%d, %d", mx, my)` on the float mouse
  coordinates now `math.floor`s them first; was triggering
  `bad argument #2 to 'format' (number has no integer representation)` on every frame in
  `main_menu.lua:89`
- **ARM64 Linux CI restored** — Poetry's default venv cache (`~/.cache/pypoetry/virtualenvs/`)
  names venvs from `(project, pyproject, python version)` without the architecture. CI agents
  running different archs on a shared `$HOME` (or bind-mounted workspace) collided on the same
  venv path; the ARM64 runner then loaded the x86_64 `cryptography/_rust.abi3.so` and crashed
  at import with `cannot open shared object file`. `ci.utils.venv` now layers three checks in
  cheapest-first order: (1) no venv → skip; (2) compare an `arch-OS-impl-pyver` marker file
  inside the venv against the live host; (3) if the marker is missing/mismatched, run a
  functional `from cryptography.fernet import Fernet` test under `poetry run python`. When the
  test fails, `ci_action.py` exports `OWL_CI_REFRESH_VENV=1`; `cmake/Poetry.cmake` consumes it
  via `poetry env remove --all` before sync and (re)writes the marker afterwards. The check
  runs unconditionally (TC-via-Docker doesn't propagate `TEAMCITY_VERSION`, so an env-var gate
  would no-op), yet same-arch reruns still pay almost nothing thanks to the marker fast path.
- **Windows Debug test binaries no longer fail to load** (was
  `STATUS_DLL_NOT_FOUND` / `0xc0000135`). The helper
  `owl_target_link_libraries()` was setting `CMAKE_MAP_IMPORTED_CONFIG_DEBUG=Release`
  only around `find_package()`, but that variable is read at generate time (link resolution +
  `TARGET_RUNTIME_DLLS` generator-expression evaluation), not at find time. In Debug builds the linker picked
  `glfw3d.lib` (imports `glfw3d.dll`) while the post-build copy grabbed the release `glfw3.dll`
  — every test binary then failed to load. The mapping is now applied at top-level directory
  scope in `CMakeLists.txt`, gated by `OWL_USE_RELEASE_THIRD_PARTY`, so link and DLL copy agree
- Tests now get a `TARGET_RUNTIME_DLLS` generator-expression post-build copy next to their binary (belt-and-braces —
  picks up any DLL surfaced through the test's own link graph that the
  engine's post-build didn't cover)
- **Use-after-free SIGSEGV when closing a document tab** — closing a `SceneDocument` inline from
  `onImGuiRender` freed its Vulkan colour-attachment while ImGui's draw list still referenced it (the sampler then read
  freed GPU memory at `UiLayer::end()` submit). `EditorLayer::closeDocument`
  now queues the id in `m_deferredCloseIds` and drains at the start of the next frame, after the
  prior frame's ImGui commands are submitted
- **SIGSEGV in the `ImGuiLayer.creation` unit test** — `UiLayer::onAttach` called
  `OWL_CORE_ERROR` when the Roboto/JetBrains Mono TTFs couldn't be resolved, but `disableApp()`
  left the Log singleton uninitialised, so the error path deref'd null. The external-font block
  is now gated on `m_withApp`; standalone uses fall back to ImGui's built-in default font
- Pack filesystem errors no longer crash the editor: `create_directories`, `copy_file`, and
  `permissions` now use `std::error_code` with a user-facing error modal when the destination path
  conflicts with an existing file of the same name
- AssetScanner: absolute path resolution for sounds and scenes (`resolveSound`, `resolveScene`)
- AssetScanner: skip nonexistent scene files instead of adding them to the asset list
- `recursive_directory_iterator` crash on missing directories (AssetLibrary, FontLibrary)
- Mouse Y inversion in packed runner UI (UIInputSystem coordinates)
- `writeLinuxLauncher` undefined on Windows (guarded with an `OWL_PLATFORM_LINUX` ifdef)
- `game_settings.yml` not included in packed game
- Markdown table alignment across all doc pages
- Clang-tidy cognitive complexity in UIInputSystem and AssetScanner (extracted helper functions)
- LuaBindings: `const lua_State*` incompatibility with `lua_CFunction` signature (added NOLINT)
- Flaky tests: shared `output/test_tmp` directory replaced with unique per-test paths in
  `std::filesystem::temp_directory_path()`
- First-time Slang shader compilation no longer freezes the window — loading screen animates
  between each shader
- Test scene textures (`source/owlnest/assets/scenes/test_levels{,2}.owl`) used absolute
  `pat:/source/.../mario.png` paths inherited from a previous host layout — converted to
  portable `nam:textures/mario.png` so they resolve through the engine's asset directories.

## [0.1.0] - 2026-04-16

### Added

- Lua 5.5 scripting with full engine API (13 tables, 58 functions)
- In-game Canvas UI system (8 widget types: Text, Image, Button, Slider, ProgressBar, Panel, Rect)
- Game state and save/load system with multiple slots
- Two-layer settings system (game defaults + user overrides)
- Prefab system with UUID remapping and override tracking
- Undo/Redo command system with merge coalescing
- Extended trigger system (7 types: Victory, Death, Teleport, Timer, Interaction, LuaCallback, Target)
- Asset packing pipeline (.owlpack format with zstd compression)
- Runner with packaging (renamed executable, launcher script, metadata)
- Scene fade transitions (fade in/out)
- Sample game demonstrating all engine features (6 scenes, 12 scripts)
- AnimatedSpriteRenderer component
- SpriteRenderer tiling factor separated into X/Y
- scene.quit () Lua API for clean exit
- Sound pause/resume/volume control from Lua

### Fixed

- Mouse Y inversion in packed runner UI
- Assets not found in packed game (AssetScanner now scans UIImage, AnimatedSpriteRenderer,
  Death/Victory triggers, sound.play in Lua)
- recursive_directory_iterator crash on missing directories
- OpenAL source leak on scene end
- GameState lost on scene transitions
- UISlider callback never fired
- writeLinuxLauncher undefined on Windows

## [0.0.3] - 2026-04-09

### Added

- Sound effects, music, and spatial audio (OpenAL backend)
- Animated sprites with spritesheet grid support
- Mesh loading (OBJ, glTF, GLB, FBX)
- Configurable keymap
- Scene hierarchy with parent-child entities
- Asset packing (.owlpack binary format)
- Task system (Taskflow backend)
- Project system in Owl Nest
- Icon system with runtime SVG rendering

## [0.0.2] - 2026-03-03

### Added

- Backgrounds / skyboxes
- Slang shader migration
- Pause / unpause / frame stepping
- General settings management
- Scene-to-scene jumping

## [0.0.1] - 2025-02-06

### Added

- Initial release: minimal viable engine with scene-based games
