---
paths:
  - "source/owlnest/**"
---

# Owl Nest editor

User-facing reference: the editor pages listed under *The editor* in `doc/help.yml` (`doc/pages/editor*.md`);
internals in `doc/pages/editor_internals.md`. Every authored object ships full editor support in the
same PR (see `ongoing-quality.md`, *Editor Coverage*).

## Project system

- `owl_project.yml`: name, version, author, description, icon, window settings, asset directories.
- The runner reads `runner.yml` at startup (title, icon, size, fullscreen, resizable, `PackFile`).
- Window title reflects the active project, with `*` when the undo stack is dirty.

## Undo / redo (`sources/UndoCommand.h`, `UndoManager.h`, `commands/`)

- `UndoCommand`: `undo()`, `redo()`, `description()`, `mergeWith()`, `typeId()`.
- `UndoManager`: two stacks, 1 s merge window, max depth 100, dirty tracking. Owned by `EditorLayer`,
  passed as a pointer to `SceneHierarchy` and `Viewport`.
- `EntitySnapshot` captures/restores an entity through `SceneSerializer::serializeEntityToString()`.
- Commands: `EntityCommands` (create, delete, duplicate ± subtree), `ComponentCommands` (add, remove,
  modify with merge), `HierarchyCommands` (reparent, unparent), `PrefabCommands` (instantiate, apply /
  revert), `VoxelCommands`, `NodeGraphCommands`, `SceneSettingsCommands`. Scene
  commands derive from `SceneUndoCommand`.
- Property edits: `drawComponent<T>` wraps `renderProps()` in `panel::InspectorEditTracker`. A session opens
  on interaction with the body (click / release over it, active widget, key with the nav focus inside),
  captures **the component alone** (`SceneSerializer::serializeComponentToString`); when it ends the entity is put
  back as it was and `component.set replace=true` runs through the panel's `CommandTarget` (journaled, undoable).
- **No per-frame serialization in the inspector**: nothing may serialize an entity or a component on a frame
  where nothing is edited (`InspectorEditTracker::serializationCount()` is the spy the tests check).
- Gizmo: `Viewport` captures the entity before the drag, puts it back at the end and runs `entity.set_transform`.
- Journal / console / macros: `CommandRegistry::setJournal` records every `execute` (`CommandContext::source`) into
  the session's `CommandJournal`; `commands/CommandLine.h` (syntax, `$n`), `commands/Macro.h` (`.owlmacro`, replay =
  `UndoManager::beginGroup` / `endGroup`, `cancelGroup` on a failing line), `session/Console.h`, `panel/ConsolePanel.h`.
- `ActionMapDocument` (`input_actions.yml`) keeps its own `UndoManager<input::ActionMap>`: every edit is a method of the
  document (whole-map before / after command, slider drags merge) and `BindingCapture` is the pure "press a key" state
  machine; saving reloads the project map in `EngineContext`.
- `HudDocument` (`document/HudDocument.h`) is the HUD editor of a scene document (an `.owlhud` always, a scene through
  `SceneDocument::setHudMode`): preview resolution, backdrop, snapping, selection and the gizmo gestures, no scene and
  no history of its own; `panel::HudEditor` draws it in place of the viewport. A gesture previews on the scene, then
  puts it back and runs one `hud.*` command (`commands/HudCommands.h`: `HudRectsCommand`, `HudReparentCommand`).
- Selection is restored through each command's `selectAfterUndo` / `selectAfterRedo` hints.
- **Command API** (`commands/CommandRegistry.h`, library `OwlNestCommands`, also linked by the runner): a discrete
  scene change is a named command run by `CommandRegistry::execute` (or `CommandTarget::execute` from a panel),
  never a direct scene write followed by a `push`. A new discrete action adds a `CommandSpec` (typed `ArgSpec`s,
  description) and a test in `owlnest_tests/CommandRegistry_test.cpp`; it is then reachable from scenarios too.
  Only gestures previewed live (brushes, node moves) `push` an already-applied command.

## Icons

- SVG sources in `assets_sources/icons/<category>/` (`toolbar`, `browser`, `visibility`, `triggers`,
  `components`, `panels`, `actions`, `ribbon`, `graph` for the visual script types and node families; `templates` is
  not rendered), installed in `assets/icons/` (looked up there first, then in `assets_sources/`). **Never modify an
  SVG programmatically.**
- `IconBank::build()` substitutes `#ffffff` / `white` → theme text colour and `#ff00ff` → theme accent, rasterizes
  with lunasvg at 16 / 24 / 32 px × DPI and the 64 px cell; draw with `getIcon(name, size)` at a whole-pixel position.
  `IconBank::rebuild(colors)` (through `utils::applyTheme`) on theme change.
- Action glyphs: Lucide font, `gui::icon::*` (`gui/IconFont.h`) inside labels; add a name there from the
  `lucide-static` `info.json` when one is missing.
- Scene trigger overlays (512×512 PNG) are pre-rasterized by
  `poetry run python tools/editor/generate_icons.py`.
- New icon: add the SVG in the right category, register it in `utils::buildIconBank()` (`EditorResources.cpp`).

## Panels

New panels follow the `SceneSettings` wiring pattern. `EditorLayer` only routes: the ribbon is `RibbonBuilder`,
packaging `GamePackager`, project and asset opening `EditorLayerProject.cpp` (extension table `classifyAsset`). Put
new logic in a panel, document or command, not in `EditorLayer`.

## Script debugging and Lua console

- `panel::ScriptDebugPanel` (**Script Debug**, docked with the Console): the `BreakpointHit` with Continue / Open and the
  variables of the scene, HUD and entity scripts; `EditorLayer::renderScriptDebug` pauses the game on a hit and feeds
  the open graphs their probes.
- `session/LuaConsole.h`: the Lua mode of the Console over `script::ScriptConsole`, with the `editor` module built from
  the `CommandRegistry`; editor scripts (`.lua` under `editor/`) run as one `UndoManager` group.
