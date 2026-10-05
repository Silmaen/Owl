---
paths:
  - "depmanager.yml"
  - "owl_engine.py"
  - "cmake/Depmanager.cmake"
---

# DepManager

Every third-party library comes from DepManager (`depmanager.yml`). **No vendoring**: never copy external
sources into the repo. Check the package catalogue before assuming a library is missing (e.g.
`imgui_color_text_edit` exists; ImGuizmo already bundles GraphEditor, ImSequencer, ImCurveEdit,
ImGradient, ImZoomSlider, ImLightRig).

## Rules

- Always `poetry run depmanager …` (inside `docker/run.sh`), never a bare `depmanager`.
- Pin exact versions; set `kind: static|shared` for every non header-only package.
- Link with `owl_target_link_libraries()` (`cmake/OwlUtils.cmake`), never a raw `find_package()`.
- Packages are fetched at configure time (`dm_load_environment()` in `cmake/Depmanager.cmake`).
- Removing a dependency: drop it from `depmanager.yml` and every matching `owl_target_link_libraries()`.

## Commands

```bash
docker/run.sh poetry run depmanager pack ls -p <name>:<version> -t <static|shared|header>
docker/run.sh poetry run depmanager pack ls <remote>
docker/run.sh poetry run depmanager pack pull -p <name>:<version> <remote>
docker/run.sh poetry run depmanager remote list
docker/run.sh poetry run depmanager build <recipe_dir> [-r] [-f]
```

## OwlEngine as a package (`owl_engine.py`)

- `poetry run depmanager build .` produces `owl_engine:<version>`, `shared` and `static` variants.
- Public deps declared in the recipe: EnTT, imgui. Editor and tests disabled, packaging mode on.
- Installs `lib/`, `include/`, `assets/`, `lib/cmake/OwlEngine/`.
- Consumers (e.g. OwlDrone): `find_package(OwlEngine CONFIG REQUIRED)` + `Owl::OwlEngine`. Moving a
  public header breaks them.
