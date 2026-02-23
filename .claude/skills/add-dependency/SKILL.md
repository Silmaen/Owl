---
name: add-dependency
description: Add a new C++ dependency to the Owl project via DepManager
---

# Add a new C++ dependency

Arguments: `<package-name>` and optionally `<version>` and `<kind>` (static/shared/header).

## Steps

1. Check if the package already exists in `depmanager.yml`.
2. If not, add it to `depmanager.yml` under `packages:`:
   ```yaml
   <package_name>:
     version: "<version>"
     kind: "<static|shared|header>"
   ```
   - If `kind` is not specified, ask the user (static, shared, or header-only).
   - If `version` is not specified, ask the user.
3. Add the `owl_target_link_libraries()` call in the appropriate `CMakeLists.txt`:
   ```cmake
   owl_target_link_libraries(<target> PRIVATE <package_name> REQUIRED ${THIRD_PARTY_RELEASE})
   ```
   - Engine internal deps: add to `source/owl/CMakeLists.txt` with `${ENGINE_NAME}Private INTERFACE`
   - Engine public deps: add to `source/owl/CMakeLists.txt` with `${ENGINE_NAME} PUBLIC`
   - App deps: add to `source/<app>/CMakeLists.txt` with `${OWL_PROJECT} PRIVATE`
4. Test the configure step:
   ```bash
   cmake --preset linux-gcc-release -S .
   ```
5. Report success/failure.
