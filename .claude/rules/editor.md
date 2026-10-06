---
paths:
  - "source/owlnest/**"
---

# Owl Nest editor

User-facing reference: `doc/pages/editor.md`. Every authored object ships full editor support in the
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
  revert), `VoxelCommands`, `NodeGraphCommands`, `SceneFlowCommands`, `SceneSettingsCommands`. Scene
  commands derive from `SceneUndoCommand`.
- Property edits: `drawComponent<T>` wraps `renderProps()` in `panel::InspectorEditTracker`. A session opens
  on interaction with the body (click / release over it, active widget, key with the nav focus inside),
  captures **the component alone** (`SceneSerializer::serializeComponentToString`) and pushes one
  `ModifyEntityCommand` when it ends, prefab override marks included.
- **No per-frame serialization in the inspector**: nothing may serialize an entity or a component on a frame
  where nothing is edited (`InspectorEditTracker::serializationCount()` is the spy the tests check).
- Gizmo: `Viewport` captures the transform before/after manipulation and pushes `ModifyEntityCommand`.
- Selection is restored through each command's `selectAfterUndo` / `selectAfterRedo` hints.

## Icons

- SVG sources in `assets_sources/icons/<category>/` (`toolbar`, `browser`, `visibility`, `triggers`,
  `components`, `panels`, `actions`; `templates` is not rendered). **Never modify an SVG
  programmatically.**
- `IconBank::build()` substitutes `#ffffff` → theme text colour and `#ff00ff` → theme accent, rasterizes
  with lunasvg into a 64 px mipmapped atlas; `IconBank::rebuild(colors)` on theme change.
- Scene trigger overlays (512×512 PNG) are pre-rasterized by
  `poetry run python source/owlnest/assets/icons/generate_icons.py`.
- New icon: add the SVG in the right category, register it in `buildIconBank()` (`EditorLayer.cpp`).

## Panels

New panels follow the `SceneSettings` wiring pattern. `EditorLayer.cpp` is already ~2 900 lines: put new
logic in a panel, document or command, not in `EditorLayer`.
