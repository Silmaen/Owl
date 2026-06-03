# Module & Namespace Layout

The engine library (`source/owl/{public,private}`) is split into modules. **Folder maps 1:1 to
namespace** — `data/geometry/` is `owl::data::geometry`, no exceptions. When adding a file, place it by
the rules below; when unsure, match the closest existing precedent rather than inventing a new top-level
module.

## Rule per module

| Module     | What lives here                                                                                  |
|------------|--------------------------------------------------------------------------------------------------|
| `app`      | Application lifecycle: `Application`, `AppParams`, `EntryPoint`, the `layer/` stack (`Layer`, `LayerStack`). |
| `core`     | Foundation primitives every module may depend on: `Log`, `Assert`, `UUID`, `Serializer`, `Timestep`, `expected`, `IFactory`, `Environment`, `Macros`, `task/`, `utils/` (`StringUtils`). |
| `data`     | Owned data structures **and their loaders/libraries**: `geometry` (mesh + `MeshLoader`), `fonts` (Font + `FontLibrary`), `assets` (Asset + `AssetLibrary` + `pack` = `.owlpack` bundling/scanning), `extradata`, `voxel`, `meshrange` (mesh-iteration accessors). |
| `io`       | **External device / peripheral channels only**: `serial`, `video`. Not files, not asset bundling. |
| `platform` | OS / native-platform services: `FileDialog` (native picker), `fileToString` (`FileUtils`), `openExternalUrl`. |
| `scene`    | ECS world, **all** ECS components (`scene::component`), tilemap, prefab, save / settings.         |
| `renderer` | Draw path + `gpu/` backends + camera types and camera controllers.                               |
| `gui`      | ImGui widgets (`widgets`) and per-component render helpers (`component`).                          |
| `event`, `input`, `window`, `sound`, `script`, `debug`, `physics`, `math` | Their namesake domain. |

## Key distinctions (learned the hard way)

- **`io` is devices, not files.** `io/` is for external device / peripheral channels (`serial`, `video`)
  — **not** filesystem helpers, file dialogs, or asset bundling. Those are `platform/` and `data/`.
- **Asset bundling is data, not io.** `.owlpack` packing / reading / scanning lives in
  `data::assets::pack` (it operates on the engine's assets), not in `io/`.
- **A loader is not I/O.** A loader that *produces an engine data type* lives next to that type in `data/`
  (e.g. `MeshLoader` in `data/geometry`).
- **OS/native services are `platform`.** Native file dialogs, opening a URL, raw file-to-string —
  cross-cutting platform calls — go in `platform/`, not `core/utils` (kept for pure string helpers) nor `io/`.
- **`data::meshrange` ≠ `scene::component`.** `data::meshrange` holds the mesh-iteration accessor
  "components" (cursors over vertices/triangles) — they are *not* ECS components. ECS components
  (`Tilemap`, `SpriteRenderer`, …) all live in `scene::component`.
- **Camera controllers are renderer, not input.** `input/` is key/mouse codes + polling only.
- **Lifecycle is `app`, primitives are `core`.** Don't put `Application` / layer-stack types in `core`.

## Adding a top-level module

Rare. Prefer a sub-namespace of an existing module (a voxel data model went to `data/voxel`, not a
top-level `voxel/`). Only create a new top-level module for a genuinely new domain with no fit above.
`file(GLOB_RECURSE)` means new files/dirs need no CMake edit.

## Cost of a move

Folder==namespace means moving a header is a **public-API breaking change** for downstream consumers
(e.g. OwlDrone). Batch module moves into a dedicated reorg PR (ideally at a release kickoff), never as a
drive-by; update `CHANGELOG.md`, `architecture.md`, and this file in the same PR.
