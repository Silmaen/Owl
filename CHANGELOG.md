# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- **Voxel data model** — new `owl::data::voxel` module (foundation for the v0.2.1 voxel engine): `BlockRegistry`
  (block-type table with per-face texture indices, render kind, collision flag, YAML round-trip), `Chunk`
  (16³ cubic block grid with dirty tracking and run-length encode/decode), and `VoxelWorld` (sparse chunk
  map with world-coordinate block access and floored negative-coordinate chunk resolution). New headless
  `voxel_tests` category (26 tests).

## [0.2.0] - 2026-06-02

### Added

- **Renderer stack architecture** — composable per-scene renderer pipeline: `RenderLayer` / `RenderStack` /
  `RenderLayerFactory` + `RendererTag` component, YAML round-trip (`RendererStack:` / per-scene `EnabledRenderers:`
  with overrides), Project Settings composition UI, and a dockable per-scene Scene Settings panel.
- **Raycasting renderer** — `RendererRaycast` / `RendererRaycastLayer`, per-column DDA, configurable FOV / max distance
  / sky / floor, `Tileset.FilterMode`, and the `scenes/raycast_demo.owl` sample (Wolfenstein 3D E1L1, 64×64).
- **Raycast world detail** — textured floors / ceilings (`emitTexturedBackdrop`) and uniform distance fog
  (`RaycastConfig.fogColor` / `fogStart` / `fogEnd`).
- **Raycast sprites (billboards)** — `SpriteRenderer` / `AnimatedSpriteRenderer` rendered as camera-facing strips with
  per-column z-buffer occlusion, plus per-sprite world-size and Z-offset overrides; the same components stay 2D on
  `Renderer2D` layers.
- **Variable wall heights and transparent walls** — `TileMeta.wallHeight` (`[0, 8]`) and `TileMeta.transparent`
  (alpha-only, up to 8 back-to-front hits per ray).
- **Doors and pushwalls** — animated `RaycastDoor` / `RaycastPushWall` components with built-in `interactionKey` or Lua
  activation and auto-managed Box2D collision; editor tile pickers and gizmos.
- **Tilemap system** — `scene::Tileset` (`.owltileset`) and a standalone `.owltilemap` `TilemapAsset`, edited in the
  `TilemapDocument` (multi-layer, Properties / Canvas / Palette, paint-erase strokes, per-stroke undo).
- **In-viewport camera markers** — every `component::Camera` entity draws a dot + facing arrow + FOV cone.
- **Scene transition effects** — `Fade{In,Out}` + `Wipe{Left,Right,Up,Down}` with custom tint; Lua `ui.transition_play`
  (and back-compat `ui.transition_fade_in/out`).
- **GPU compute foundation** — Slang `[shader("compute")]` programs, `gpu::ComputeShader`, `gpu::StorageBuffer`
  (with `getData()` readback on all backends), and `RenderCommand::storageBufferMemoryBarrier()`.
- **Compute pre-pass utilities** — `WorldTransformPass` (#33) and `RaycastDDAPass` (#35), both adopted this release, plus
  `BitonicSortPass` (#32) and `FrustumCullingPass` + indirect-draw API (#34) shipped and headless-tested for adoption in
  v0.3.0.
- **Per-renderer Vulkan descriptor blocks** — backend-neutral `gpu::RendererDescriptors` (no-op on Null / OpenGL).
- **Renderer test coverage** — `RendererTilemap_test` (instanced-path stats) and `BitonicSortPass` padding / empty-input
  cases.
- **Editor & build niceties** — snap-to-grid translation gizmo (`Snap` toggle + `Step`), ribbon dropdown buttons, ccache
  compiler launcher (`cmake/CompilerCache.cmake`), and the Lua `physics.set_gravity_scale(entity, scale)` API.

### Changed

- **Renderer modernization** — `Renderer2D` rewritten as instanced + per-instance SSBO (Phase 1); per-frame world
  matrices produced by the `WorldTransformPass` compute pre-pass, with instances carrying an `int32_t worldIndex`
  instead of a `mat4` (Phase 2); raycast walls drawn by `RaycastDDAPass` + `raycast_stripe.slang` in one instanced
  call (Phase 3). Public draw APIs preserved.
- **Tilemap rendering** routes through the instanced `RendererTilemap`: deferred into `Renderer2D::flush` (after the
  background, before sprites) and combined into a single drawcall for the whole scene — per-instance `layerZ` / atlas /
  `textureSlot`, distinct tilesets across the shader's 32-texture array. Replaces the per-cell `Renderer2D::drawQuad`
  fallback.
- **2D entity draws** drop the redundant `getWorldTransform` when `worldIndex` covers them; the world-transform cache
  stays for the raycast DDA, physics, `EntityLink`, text glyphs and the editor.
- **Performance** — scene loading ~550× faster (memory-tracker O (N²) fix, UUID→entity cache, in-memory SPIR-V cache;
  `raycast_demo` ~22 s → ~40 ms) plus async load; in-game wins via scratch-buffer pooling and per-pass render-loop
  caches.
- **`Scene.EnabledRenderers`** now overrides layer order, not just enable / disable.
- **Editor keyboard shortcuts** (Ctrl+S, Ctrl+Z, …) bypass `WantCaptureKeyboard`; modifier-less keys still yield to
  focused text widgets.
- **Tooling** — renderer sources reorganized by renderer kind; `CodeStyle` CI action bundled into one read-only command;
  CMake globbing made configure-aware; TeamCity switched to plugin-event triggers; Oxford English spelling sweep across
  comments and prose; version bumped to `0.2.0` and `doc/pages/roadmap.md` reorganized.

### Fixed

- **Tilemaps (and raycast door / pushwall tilesets) were missing from packaged
  games.** `AssetScanner` never followed the `Tilemap` component's
  `tilemapPath` nor the `RaycastDoor` / `RaycastPushWall` `tilesetPath`, so the
  `.owltilemap`, its `.owltileset`, and the atlas textures were absent from the
  `.owlpack`; and `Scene::resolveAllTilemapAssets` loaded those YAML assets
  from disk only. Both are now wired: the scanner packs the whole
  tilemap → tileset → texture chain (and standalone door / pushwall tilesets),
  and asset resolution tries the open pack before the filesystem (like the
  Lua-script path). Editor / loose-file runs were unaffected.
- **Vulkan compute pipeline** — `ComputeShader::dispatch` now records on a one-shot command buffer (was recording into a
  closed buffer → SIGSEGV); entry-point name corrected to `main`; Slang source lookup fixed to
  `shaders/<renderer>/slang/<name>.slang`; `world_transform.slang` wraps its matrix in a struct for correct
  column-major SPIR-V decorations.
- **Raycaster** — player rotation now applied; walls actually drawn; HUD no longer rotates with the camera; broken
  empty-cell convention, wall blurriness, stripe flicker under motion, cell-coordinate half-extent and FOV-aspect bugs
  fixed.
- **Editor** rendered tilemaps in 2D regardless of `RendererTag` — fixed.
- **Empty render-stack layers** caused multi-scene flicker — fixed in `Scene::renderWithStack`.
- **Hidden triggers** no longer fire — `Scene::onUpdateRuntime` skips `gameVisible == false` triggers, clearing timers
  and overlap state with a synthetic `onTriggerExit`.
- **Packaging** — packaged games now ship the engine assets they need (`PackReader::entrySize()` reports uncompressed
  size); asymmetric `CameraOrtho` projections fixed on Vulkan.
- **Build / CI** — mingw-gcc 15 link passes `-Wa,-mbig-obj` on Windows GCC; `get_git_hash` made resilient; the
  `HelpIndex` badge test skips cleanly offline.

## [0.1.1] - 2026-04-30

### Added

- **Full Markdown rendering for the in-editor help and live preview**
    - New `codeEditor::MarkdownDocument` parser backed by **md4c 0.5.2** (new
      DepManager recipe at `OwlDependencies/Libs/md4c/`) — CommonMark + GFM
      tables / strikethrough / autolinks. Public block model
      (`MdHeading`, `MdParagraph`, `MdCodeBlock`, `MdImage`, `MdTable`,
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
      documents the SceneHierarchy component header most recently hovered
      (`SceneHierarchy::lastHoveredComponentName`); falls back to the editor
      overview when nothing is hovered.
    - File ribbon tab gained a **Help** group; the Welcome screen surfaces a
      **Getting Started** entry pointing to the new
      `doc/pages/getting_started.md`.
- **Animation editor**
    - New reusable `.owlanim` asset (`scene::AnimationClip` —
      `source/owl/public/scene/AnimationClip.h`): texture, grid, frame range, frame
      duration, loop, optional speed curve, with YAML round-trip and unit tests in
      `test/scene_tests/AnimationClip_test.cpp`.
    - `nest::AnimationDocument` opens as a 4th document type (alongside Scene, Code,
      NodeGraph). Three-panel layout: live spritesheet preview, properties (texture
      drop target, columns / rows / first / last frame, frame duration, loop, speed
      curve via `gui::widgets::curveEditor`), and a frame-range timeline.
    - New `gui::widgets::sequencer()` widget (`source/owl/public/gui/widgets/Sequencer.h`)
      wraps `ImSequencer` from the existing imguizmo bundle — owl-friendly API
      (`SequencerEntry` / `SequencerOptions`), no third-party types leak through the
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
    - Font preview: Text component shows a sample-string rendering of the selected font
      ("Aa Bb 1!? éàüÇ" — covers lower/upper case, digits, punctuation and accented
      Latin-1 glyphs). Backed by a new `gui::FontPreviewCache` that lazily renders each
      font into a 256x64 framebuffer via `Renderer2D::drawString` and caches the result;
      pumped from `EditorLayer::onUpdate` and freed on `UiLayer::onDetach`.
    - Latin-1 glyph rendering fix — `Font::getGlyphBox` and `Renderer2D::drawString` no
      longer sign-extend `char` codepoints into garbage 32-bit values; the renderer
      additionally decodes UTF-8 source text into Latin-1 codepoints up front, so
      accented glyphs (`éàüÇ`…) coming from YAML scenes or Lua strings render correctly
      everywhere instead of falling back to `?` or `Ã©` byte pairs.
- **Sample project showcases v0.1.1 features**
    - Main-menu subtitle exercises UTF-8 / Latin-1 rendering
      (`Démo des fonctionnalités… caractères éàüÇ`).
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
    - Compact pin labels: **just the source identifier** prefixed by a single-glyph kind hint
      (`† DangerZone`, `★ VictoryZone`, `λ checkpoint`, plain `LevelPortal` for Teleport).
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
    - Output pin labels show the **source trigger entity name** parsed from the scene's YAML
      (e.g. `LevelPortal → level2 @ SpawnPoint`); transitions of type `Death` and `Victory` are
      now extracted too (they also serialize a `LevelName`) and styled distinctly:
      `[death] DangerZone → game_over`, `[victory] VictoryZone → victory`
    - Best-effort scan of attached Lua scripts for `scene.load_scene("...")` calls — adds extra
      output pins tagged `scene_lua_exit` so Lua-driven transitions appear alongside Trigger ones
    - Right-click menu: on a node → **Edit scene** (opens it in a new tab) / **Delete scene**
      (with confirmation modal); on empty space → **Add new scene...** (creates an empty `.owl`
      under `scenes/NAME.owl` and opens it). Canvas auto-rescans after create/delete
    - The global **Scene Hierarchy** and **Properties** panels now host SceneFlow content while
      that document is active: the hierarchy lists every scene (orphans drawn red, click-to-select,
      double-click to open), the properties panel shows the selected scene's path, transitions
      (colour-coded: red = death, green = victory, blue = Lua) and an "Open this scene" button.
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
    - 16 new unit tests: `test/gui_tests/NodeCanvas_test.cpp` (topology, link validator veto,
      cascaded link removal, selection round-trip, pin→node lookup, clear) and
      `test/gui_tests/NodeCanvasSerializer_test.cpp` (empty/full canvas round-trip, custom data
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
    - `TextureDecoder` helper (`renderer/TextureDecoder.h`) — CPU-only decode primitive
      (`peekImageSize`, `decodeImageBytes`, `decodeImageFile`) with per-thread stb_image flip state
      so multiple workers can decode concurrently
    - New `Texture2D::createFromSerializedAsync(name, scheduler)` returns immediately with a
      placeholder-sized Rgba8 texture filled white; worker decodes, termination callback uploads
      real pixels on the main thread and flips `LoadState` to `Ready` (or `Failed`, keeping the
      placeholder visible)
    - Dimensions peeked from the PNG/JPG header up front, so every texture is created at its real
      size from frame 0 — binding, UV coords and atlas math unaffected
    - `Texture2D::createFromSerializedForDeserialize()` wrapper used by scene components
      (`SpriteRenderer`, `AnimatedSpriteRenderer`, `BackgroundTexture`, `UIImage`) — async when an
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
          Playback (Play / Stop large, Pause / Step small), Gizmo (Translate / Rotate / Scale
          **large**), Package (Pack Scene large)
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
- Tests now get a `TARGET_RUNTIME_DLLS` generator-expression post-build copy next to their binary
  (belt-and-braces — picks up any DLL surfaced through the test's own link graph that the
  engine's post-build didn't cover)
- **Use-after-free SIGSEGV when closing a document tab** — closing a `SceneDocument` inline from
  `onImGuiRender` freed its Vulkan colour-attachment while ImGui's draw list still referenced it
  (the sampler then read freed GPU memory at `UiLayer::end()` submit). `EditorLayer::closeDocument`
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
- scene.quit() Lua API for clean exit
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
