# Coding Style Instructions

## Formatting

- Use tabs for indentation
- Comments in English
- Braces on same line for control structures

## Variable Naming

- Use descriptive names (no single letters except loop counters)
- Prefix member variables with `m_`
- Prefix input parameters with `i`
- Prefix output parameters with `o`
- Prefix input/output parameters with `io`
- Use camelCase for local variables

## C++ Style

- Prefer `const` for immutable variables
- Initialize variables at declaration
- Use `shared<T>` instead of `std::shared_ptr<T>`
- Use `mkShared<T>()` instead of `std::make_shared<T>()`
- Use early returns to reduce nesting
- Use trailing return type syntax for non-void functions

## Error Handling

- Use `OWL_CORE_ERROR` for errors
- Use `OWL_CORE_WARN` for warnings
- Use `OWL_CORE_INFO` for info messages
