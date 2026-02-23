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

## File Header

Every file starts with a Doxygen header:
```cpp
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

| Element | Convention | Example |
|---------|-----------|---------|
| Classes/Types | PascalCase | `EditorLayer`, `WindowResizeEvent` |
| Member variables | `m_` prefix + camelCase | `m_activeScene`, `m_state` |
| Pointer members | `mp_` prefix | `mp_appWindow` |
| Static members | `s_` prefix | `s_instance` |
| Global statics | `g_` prefix | `g_DefaultFrequency` |
| Input parameters | `i` prefix | `iTimeStep`, `iScenePath` |
| Output parameters | `o` prefix | `oResult` |
| In/out parameters | `io` prefix | `ioEvent`, `ioEntity` |
| Local variables | camelCase | `eventDispatcher`, `levelPath` |
| Enum values | PascalCase | `State::Play`, `Type::Vulkan` |
| Functions/Methods | camelCase | `onUpdate`, `handleTeleportRequest` |

## Return Types

Always use **trailing return type** syntax for non-void functions:
```cpp
auto getValue() const -> int { return m_value; }
auto onKeyPressed(const event::KeyPressedEvent& ioEvent) -> bool;
```
Void functions use normal syntax: `void onUpdate(const core::Timestep& iTimeStep);`

## Smart Pointers

Use project aliases, never raw `std::shared_ptr` / `std::make_shared`:
```cpp
shared<Scene> m_scene;          // not std::shared_ptr<Scene>
uniq<Window> mp_window;         // not std::unique_ptr<Window>
auto s = mkShared<Scene>();     // not std::make_shared<Scene>()
auto w = mkUniq<Window>();      // not std::make_unique<Window>()
```

## Class Structure

1. Doxygen `@brief` comment
2. Rule of Five explicitly declared (delete copy/move if not needed):
```cpp
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
```cpp
enum struct State : uint8_t { Edit, Play, Pause };
```

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

## Diagnostic Suppression

Wrap external headers or unavoidable warnings:
```cpp
OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wshadow")
#include <external_header.h>
OWL_DIAG_POP
```
For clang-tidy: `// NOLINTBEGIN(check-name)` ... `// NOLINTEND(check-name)`

## Profiling

Use profiling macros at function/scope entry:
```cpp
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
