---
paths:
  - "**/*.cpp"
  - "**/*.h"
  - "**/*.hpp"
---

# C++ Code Style (C++23)

## Formatting

- **Tabs** for indentation (width 4), continuation indent 8
- **120-character** column limit
- Braces attach to previous line (Allman not used)
- No indentation inside namespaces
- Closing namespace comment: `}// namespace owl::module`
- Pointer/reference binds left: `int* ptr`, `const std::string& ref`
- `#pragma once` (no include guards)
- Insert newline at end of file

## Spacing

- **One blank line between consecutive function/method declarations** (including `= delete` / `= default` rule-of-five members).
- **One blank line after `OWL_PROFILE_FUNCTION()`** before the function body proper.
- **Blank line BEFORE `OWL_DIAG_PUSH`** and **AFTER `OWL_DIAG_POP`** — but **no blank** immediately after `PUSH` or before `POP`. Treat the `PUSH ... POP` block as a contiguous unit:
  ```c++
  // ...code above...

  OWL_DIAG_PUSH
  OWL_DIAG_DISABLE_CLANG("-Wshadow")
  #include <external_header.h>
  OWL_DIAG_POP

  // ...code below...
  ```
- **No blank line between `template<...>` (or `[[attribute]]`) and the declaration that follows** — they form a single declaration.
- **No blank line between an access specifier (`public:` / `private:` / `protected:`) and the first member** that follows.

## File Header

Every file starts with a Doxygen header:
```c++
/**
 * @file FileName.h
 * @author Silmaen
 * @date DD/MM/YYYY
 * Copyright (c) YYYY All rights reserved.
 * All modification must get authorization from the author.
 */
```

## Include Order

1. `"owlpch.h"` first in `.cpp` files (precompiled header, engine library only)
2. Local project headers (quoted `"..."`)
3. External/third-party headers (angle brackets `<...>`)
4. Standard library headers (angle brackets `<...>`)

## Naming Conventions

### Identifier shape

| Element           | Convention              | Example                             |
|-------------------|-------------------------|-------------------------------------|
| Classes/Types     | PascalCase              | `EditorLayer`, `WindowResizeEvent`  |
| Member variables  | `m_` prefix + camelCase | `m_activeScene`, `m_state`          |
| Pointer members   | `mp_` prefix            | `mp_appWindow`                      |
| Static members    | `s_` prefix             | `s_instance`                        |
| Global statics    | `g_` prefix             | `g_DefaultFrequency`                |
| Input parameters  | `i` prefix              | `iTimeStep`, `iScenePath`           |
| Output parameters | `o` prefix              | `oResult`                           |
| In/out parameters | `io` prefix             | `ioEvent`, `ioEntity`               |
| Local variables   | camelCase               | `eventDispatcher`, `levelPath`      |
| Enum values       | PascalCase              | `State::Play`, `Type::Vulkan`       |
| Functions/Methods | camelCase               | `onUpdate`, `handleTeleportRequest` |

### Verb conventions

Use the same verb for the same intent everywhere:

| Verb          | Meaning                                                      | Examples                                  |
|---------------|--------------------------------------------------------------|-------------------------------------------|
| `getX` / `setX` | Read / write of a property; **never** `updateX` for a setter | `getWindowTitle()`, `setWindowTitle()`    |
| `isX`         | State predicate ("is in state X")                            | `isOpen()`, `isValid()`, `isEmpty()`      |
| `hasX`        | Containment / presence predicate                             | `hasComponent()`, `hasOpenPack()`         |
| `canX`        | Capability predicate                                         | `canPlay()`, `canShowPreview()`           |
| `wasX`        | Past-tense / consume-once predicate                          | `wasDirty`, `wasSwapped`                  |
| `onX`         | Event handler / lifecycle callback                           | `onUpdate`, `onAttach`, `onKeyPressed`    |
| `createX`     | Factory creating a new owned instance                        | `createEntity`, `createApplication`       |
| `destroyX`    | Tear down a previously-created instance                      | `destroyEntity`, `destroyEntityWithChildren` |
| `release`     | GPU/OS resource cleanup (lower-level than `destroy`)         | `Texture::release`, `Buffer::release`     |
| `clear`       | Empty a container without destroying the holder              | `Scene::clear`, `LogBuffer::clear`        |
| `init` / `shutdown` | One-shot bring-up / tear-down of a subsystem           | `Log::init`, `Log::invalidate`            |

`update*` is reserved for time-stepped updates (e.g. `onUpdate`, `updateDescriptor`); never use it as a synonym for "set".

### Class suffixes

| Suffix       | Use for                                                       |
|--------------|---------------------------------------------------------------|
| `*Manager`   | Stateful coordinator owning a list/map of subordinate objects |
| `*Handler`   | Single-method event/callback dispatcher                       |
| `*Controller`| User-input → object-state translator                          |
| `*Layer`     | A registered `Layer` in the engine layer stack                |
| `*Command`   | An undo-aware action object                                   |
| `*Document`  | A tab-bound editable artifact                                 |

Do **not** introduce `*Service`, `*Helper`, `*Util` — fold those helpers into a free function in a `utils` namespace.

### Abbreviations

Treat acronyms as words (PascalCase capitalises only the first letter); the first letter is the only one in caps:

| Abbreviation | Class form                       | Member form     |
|--------------|----------------------------------|-----------------|
| UI           | `UiPanel`, `UiText`, `UiLayer`   | `m_uiLayer`     |
| ID           | `EntityId`, `LinkId`             | `m_entityId`    |
| UUID         | `Uuid`                           | `m_uuid`        |
| API          | `RenderApi`, `SoundApi`          | `m_renderApi`   |
| GLFW / GPU   | `GpuFramebuffer`, `GlfwInput`    | `m_gpuContext`  |
| YAML         | `YamlSerializer`                 | `m_yamlNode`    |
| HTTP / URL   | `HttpClient`, `UrlBuilder`       | `m_httpHeaders` |

The only exception is `OWL_API` (a macro export tag), which keeps the all-caps spelling because macros use SCREAMING_SNAKE_CASE.

## Return Types

Always use **trailing return type** syntax for non-void functions:
```c++
auto getValue() const -> int { return m_value; }
auto onKeyPressed(const event::KeyPressedEvent& ioEvent) -> bool;
```
Void functions use normal syntax: `void onUpdate(const core::Timestep& iTimeStep);`

## Parameters

- **Non-trivial types** (`std::string`, `std::vector<...>`, `std::filesystem::path`, `math::mat*`, `shared<...>`) are passed by `const T&`.
- **Trivial types** (`int`, `float`, `bool`, `size_t`, small enums) are passed by value.
- Use `Type&&` for sink parameters where ownership transfer is intentional (rare; document the reason).
- **Event handlers receive `const Event&`** by default. Use `Event&` only when the handler is allowed to flag the event as handled.

## Result / failure handling

Pick the kind based on what the caller needs to recover from:

| Operation kind                      | Convention                          | Example |
|-------------------------------------|-------------------------------------|---------|
| Lookup that may legitimately miss   | `std::optional<T>`                  | `Application::loadFromPack(...) -> std::optional<std::vector<uint8_t>>` |
| Operation that produces a value but may fail with context | **`owl::expected<T, ErrorEnum>`** | `PackReader::open(...) -> owl::expected<void, PackError>` |
| Side-effect call where success/failure is enough | `bool` (true on success) | `Window::setIcon(path) -> bool` |
| Engine init that must succeed       | Throw / `assert` / `OWL_CORE_CRITICAL` | `Vulkan::createInstance` |

`owl::expected<T, E>` is the project's wrapper alias for `std::expected` (or `zeus::expected` on toolchains that lack it). Use it whenever the caller needs to **know why** an operation failed, not just that it failed. Define a per-module error enum (`enum struct XxxError : uint8_t { ... }`) and return `owl::unexpected{XxxError::SomeReason}` rather than logging-and-returning-false.

Prefer `owl::expected` over `bool` whenever:
- the caller may want to act differently per failure cause,
- the failure is recoverable (the caller can retry or fall back),
- the function would otherwise need an out-parameter for an error message/code.

Never use sentinel values (`-1`, empty string, `nullptr`) as silent error indicators on the public API — the caller should not have to know the sentinel.

Every `return false;` / `return std::nullopt;` / `return owl::unexpected{...}` on an error path **must** be preceded by an `OWL_CORE_WARN` or `OWL_CORE_ERROR` log explaining the cause. Silent failures are bugs.

## Smart Pointers

Use project aliases, never raw `std::shared_ptr` / `std::make_shared`:
```c++
shared<Scene> m_scene;          // not std::shared_ptr<Scene>
uniq<Window> mp_window;         // not std::unique_ptr<Window>
auto s = mkShared<Scene>();     // not std::make_shared<Scene>()
auto w = mkUniq<Window>();      // not std::make_unique<Window>()
```

## Class Structure

1. Doxygen `@brief` comment
2. Rule of Five explicitly declared (delete copy/move if not needed):
```c++
class MyClass final : public Base {
public:
    MyClass(const MyClass&) = delete;
    MyClass(MyClass&&) = delete;
    auto operator=(const MyClass&) -> MyClass& = delete;
    auto operator=(MyClass&&) -> MyClass& = delete;

    MyClass();
    ~MyClass() override = default;
    // public interface...

private:
    // private members...
};
```
3. Use `explicit` on single-argument constructors
4. Use `final` on leaf classes, `override` on virtual methods
5. Use `[[nodiscard]]` on getters and important return values

## Enumerations

Use `enum struct` with explicit underlying type:
```c++
enum struct State : uint8_t { Edit, Play, Pause };
```

## Doxygen Comments

The Doxygen format used across the codebase:

- **Block style**: always `/** ... */` for any multi-line comment, function, class, struct, or enum.
- **`///`** is reserved for **single-line comments on a member variable or an enum value**, e.g. `bool m_dirty = false;///< True when unsaved changes exist.`
- `@brief` **must be on its own line**, with the description on the **next line indented by one extra space** after the `*`:
  ```c++
  /**
   * @brief
   *  Short description sentence.
   */
  ```
- For a function/method, document **every parameter** and the **return value** when present:
  ```c++
  /**
   * @brief
   *  Resolve an entity by its UUID.
   * @param[in] iUuid The UUID to search for.
   * @return The entity, or an invalid entity if not found.
   */
  [[nodiscard]] auto findEntityByUUID(core::UUID iUuid) const -> Entity;
  ```
- Parameter direction tags: `@param[in]`, `@param[out]`, `@param[in,out]` matching the `i`/`o`/`io` prefix.
- `@return` describes what the function returns; omit for `void` returns.
- **`@tparam` is mandatory for every template parameter** — even simple ones:
  ```c++
  /**
   * @brief
   *  Convert a YAML node into a 3D vector.
   * @tparam T Stored scalar type.
   * @param[in] iNode YAML node expected to be a 3-element sequence.
   * @param[out] oVector Vector to populate on success.
   * @return True on success.
   */
  template<typename T>
  auto decode(const Node& iNode, math::Vector<T, 3>& oVector) -> bool;
  ```
- Copyright header in every file uses `Copyright (c) YYYY All rights reserved.` (the parenthesised `(c)`, never the `©` glyph).

## Logging

Use engine macros (never `std::cout`, `printf`, `std::cerr`):
```
OWL_CORE_TRACE / OWL_TRACE       — verbose debug
OWL_CORE_INFO / OWL_INFO         — informational
OWL_CORE_WARN / OWL_WARN         — warnings
OWL_CORE_ERROR / OWL_ERROR       — errors
OWL_CORE_CRITICAL / OWL_CRITICAL — fatal
```
`_CORE` variants are for engine code, non-`_CORE` for application code.

### Level guidance

| Level    | When to use                                                                |
|----------|-----------------------------------------------------------------------------|
| TRACE    | Frame-level / instrumented diagnostics. Must be cheap (no allocations) and stripped at higher log levels. |
| INFO     | One-shot lifecycle events: subsystem initialised, project opened, pack written. |
| WARN     | Recoverable failure or degraded operation: missing optional asset, fallback path taken, deprecated input. |
| ERROR    | The caller's request could not be fulfilled. Always paired with an early return or a state transition to `Error`. |
| CRITICAL | The engine cannot continue: GPU init failure, OOM, corrupted state. Followed by abort/exit. |

### Message format

`Subsystem: action result. [optional context]` — examples:

```
OWL_CORE_INFO("Vulkan: validation layers enabled.")
OWL_CORE_WARN("AssetLibrary: texture {} not found, falling back to white.", iName)
OWL_CORE_ERROR("PackReader: corrupted TOC at offset {} (expected magic {:#x}).", off, kMagic)
```

- First word after the `Subsystem:` prefix is **capitalised**.
- Sentences end with a period (`.`).
- Subsystem prefix mandatory in infrastructure code (Vulkan, IO, script, sound, GPU); optional but encouraged elsewhere.
- No trailing newline (the logger adds it).
- Avoid emitting from hot loops (`onUpdate`, render passes). If you must, gate with `OWL_CORE_FRAME_TRACE` (sampled at the configured frame frequency).

## Diagnostic Suppression

Wrap external headers or unavoidable warnings:
```c++
OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wshadow")
#include <external_header.h>
OWL_DIAG_POP
```
For clang-tidy: `// NOLINTBEGIN(check-name)` ... `// NOLINTEND(check-name)`

## Profiling

Use profiling macros at function/scope entry — followed by a blank line:
```c++
void MyClass::onUpdate() {
    OWL_PROFILE_FUNCTION()

    // ...
    {
        OWL_PROFILE_SCOPE("SubTask")
        // ...
    }
}
```

## Patterns

- Early returns to reduce nesting
- `if constexpr` for compile-time branches in templates
- Structured bindings: `auto [mx, my] = ImGui::GetMousePos();`
- Init-statements in `if`: `if (const auto val = compute(); val.has_value())`
- `std::optional` for fallible lookups
- `std::format` for string formatting (C++23)
- `constexpr` and `noexcept` where applicable
