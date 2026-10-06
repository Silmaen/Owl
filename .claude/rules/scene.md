---
paths:
  - "source/owl/*/scene/**"
  - "test/scene_tests/**"
  - "sample_project/**"
---

# Scene, hierarchy, prefab, save & settings

User-facing reference: `doc/pages/scene.md`. Keep it in sync when behaviour changes.

## File formats (versioning and writes)

- **Every file format the engine writes is versioned**: a root `FormatVersion` integer written through
  `core::emitFormatVersion`, read through `core::upgradeYamlDocument` / `upgradeDocument` with the format's
  `core::DocumentFormat` (absent ⇒ 1, newer ⇒ refused, older ⇒ migration chain). A new format gets its own
  descriptor and a `format()` accessor; changing a layout means appending a `MigrationStep` (never editing
  an old one) plus a test loading an old-version fixture.
- **Every write of these files goes through `platform::writeFileAtomic`** (temp file + flush + rename): no
  `std::ofstream` on a user document. A failed write keeps the old file and is reported (`false` /
  `WriteError`), never silent.

## Hierarchy

Every entity carries the mandatory `Hierarchy` component (`parentId` UUID + `childrenIds`); roots have
`parentId == 0`.

- `component::Transform` is the **local** transform; world = `Scene::getWorldTransform()`.
- Visibility is inherited: `Scene::isEffectivelyVisible()`.
- `Scene::setParent()` rejects cycles and recomputes the local transform to keep the world position.
- `destroyEntity()` reparents children to the grandparent; `destroyEntityWithChildren()` cascades.
  Both are immediate (editor). Runtime code (Lua, triggers) must use `destroyEntityDeferred()`: the
  subtree is destroyed by `flushPendingDestructions()` at the end of `onUpdateRuntime()`, with
  `on_destroy`, Box2D body removal and sound stop.
- `duplicateEntity()` makes a root copy; `duplicateSubtree()` duplicates recursively with new UUIDs.
- Only `parentId` is serialized; `childrenIds` is rebuilt after load by `rebuildHierarchyChildren()`.
- Physics: Box2D bodies ignore the hierarchy. `PhysicCommand` works in world space and converts back to
  local. A non-physics child follows its physics parent; two physics entities move independently.

## Prefab (`.owlprefab`)

- `PrefabSerializer` (static): serialize, instantiate, readInfo, applyToInstance, revertInstance.
- YAML with `Prefab:` / `Version:` / `Entities:`, same entity format as scenes.
- `PrefabLink` on the instance root: `prefabAssetPath`, `syncedVersion`, `uuidMapping`
  (instance ↔ canonical), `overriddenComponents`.
- Instantiation: load into a temp scene, fresh UUIDs, remap `parentId`, copy components through a
  serialization round-trip, add `PrefabLink`.
- `applyToInstance()` takes non-overridden components from the prefab and keeps overridden ones;
  `revertInstance()` clears overrides then applies.

## Game state & saves

- `GameState`: key-value store (`variant<int64_t, float, string, bool>`) living on `Scene`, copied across
  transitions, serialized in saves.
- `SaveManager` (static): `.owl_save` YAML in `~/.local/share/<game>/saves/` (Linux) or
  `%APPDATA%/<game>/saves/` (Windows): header, GameState, full scene, physics snapshots.
- Lua `save.load_game(slot)` only sets `Scene::saveLoadRequest`; RunnerLayer / EditorLayer apply it after
  `onUpdateRuntime()`. Physics velocities are restored via `PhysicCommand::applySnapshot` after
  `onStartRuntime()`.

## Settings

- `SettingsManager` (static, two layers): defaults from `game_settings.yml` (project assets) + user
  overrides in `settings.yml` (user dir).
- Built-in keys auto-applied by `applyBuiltins()`: `resolution_width`, `resolution_height`, `fullscreen`,
  `resizable`, `volume_master`, `volume_music`, `volume_sfx`.
- Runner order: `runner.yml` defaults → `game_settings.yml` → user `settings.yml` → `applyBuiltins()` →
  first scene load.

## Sample project

`sample_project/` is the feature showcase: every engine feature must be demonstrated there. Validate YAML
edits with `poetry run python -c 'import yaml; yaml.safe_load(open("<file>"))'`.
