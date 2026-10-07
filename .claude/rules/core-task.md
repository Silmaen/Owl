---
paths:
  - "source/owl/*/core/task/**"
  - "source/owl/private/core/external/**"
  - "test/core_tests/**"
---

# Task system (Taskflow 4.1)

- Public API (`core/task/`): `Task`, `Scheduler`, `Timer` — no Taskflow type ever appears in a public
  header. Taskflow is a PRIVATE, header-only dependency.
- `SchedulerImpl` (pimpl) owns a `tf::Executor` sized to `hardware_concurrency()`.
- `Scheduler` is main-thread only (no mutex); worker tasks run on the pool; termination callbacks run on
  the main thread during `poll()`.
- Task results cross the boundary through a `std::promise` / `std::future` bridge, not `tf::Future`.
- `private/core/task/ParallelUtils.h`: `parallelForEach` / `parallelForIndex` (engine-internal).
- GCC quirk: Taskflow `for_each` / `for_each_index` need a `std::function`, not a raw lambda, when called
  from a non-inline context.
- Third-party headers go through a wrapper in `private/core/external/` (`taskflow.h`, `lua.h`, `slang.h`,
  `yaml.h`, `imgui.h`, `glfw3.h`, …) that suppresses their warnings; add one for any new noisy header.
- An application that needs Taskflow directly links it in its own `CMakeLists.txt`.
