# Profiling {#page-profiling}

[TOC]

How to profile Owl: the `OWL_PROFILE_*` macros, the Tracy backend behind them, GPU zones, allocations and logs.

## The macros are the only API

Engine and game code instrument themselves with the macros of `debug/Profiler.h` (included by `owl.h`); nobody
includes Tracy, which is a private dependency of the engine:

| Macro                           | Effect                                                         |
|---------------------------------|----------------------------------------------------------------|
| `OWL_PROFILE_FUNCTION()`        | Zone named after the enclosing function, until the scope ends. |
| `OWL_PROFILE_SCOPE("name")`     | Named zone, until the scope ends.                              |
| `OWL_PROFILE_FRAME_MARK()`      | End of a frame (called by `Application::run`).                 |
| `OWL_PROFILE_THREAD_NAME("n")`  | Name the calling thread on the timeline.                       |
| `OWL_PROFILE_BEGIN_SESSION`/END | Chrome backend only: open / close a JSON file.                 |

```c++
void MySystem::onUpdate(const core::Timestep& iStep) {
	OWL_PROFILE_FUNCTION()

	{
		OWL_PROFILE_SCOPE("MySystem broadphase")
		// ...
	}
}
```

In the Tracy backend a zone costs one call into the engine and a flag test while no profiler is connected; the macro
declares a static source location, so names must be string literals.

## Choosing the backend: `OWL_PROFILER`

| Value            | What the macros do                                                       | When                         |
|------------------|--------------------------------------------------------------------------|------------------------------|
| `none` (default) | Nothing: compiled out                                                    | Every build, CI, releases    |
| `tracy`          | Tracy client: CPU zones, frames, thread names, GPU zones, allocations    | Any performance work         |
| `chrome`         | Legacy JSON files (`OwlProfile-*.json`, `chrome://tracing`), one per run | No Tracy profiler at hand    |

The default stays `none` in every build type: the macros cost nothing and nothing listens on the network. Tracy is
meant for a **Release** build (the Debug build has coverage flags and measures something else). Its client is built
with `TRACY_ON_DEMAND`: until a profiler connects, zones are not recorded and nothing is buffered, so a `tracy` build
can stay on for a whole session and be attached to when a problem shows up. `OWL_ENABLE_PROFILING=ON` is deprecated
and means `chrome`.

```bash
docker/run.sh cmake --preset linux-clang-release -S . -B output/build/linux-clang-release-tracy -DOWL_PROFILER=tracy
docker/run.sh cmake --build output/build/linux-clang-release-tracy
```

Tracy comes from ConanCenter (`tracy/0.13.1`, conanfile option `tracy`, set by `cmake/Conan.cmake` from
`OWL_PROFILER`).

## Capturing

1. Start the program built with `OWL_PROFILER=tracy` (Owl Nest, the runner, `owl_bench`, a test binary). The client
   listens on TCP port 8086.
2. Connect a Tracy profiler **of the same version** (0.13.x): the GUI (`tracy-profiler`) or the command-line
   `tracy-capture -o capture.tracy -a 127.0.0.1`, from a host build or a release of
   <https://github.com/wolfpld/tracy>. From the Docker image, run the program with `docker/run.sh --gui` (host network
   is not shared: publish the port, or run the profiler in the same container).
3. The Owl Nest *Stats* panel shows `Profiler: Tracy (connected)` once attached.

The build image has neither `tracy-profiler` nor `tracy-capture` yet (see `doc/audit/01-environnement.md`): the CI
checks that the client builds and answers a capture handshake, not a full capture.

## What the timeline shows

- **CPU zones**: every `OWL_PROFILE_*` of the engine (layer stack, renderers, scene update, scripts, physics...).
- **Frames**: one frame mark per `Application::run` iteration.
- **Threads**: `Main` and every Taskflow worker (`Worker N`), named by a Taskflow worker interface.
- **GPU zones**:
    - OpenGL: one `OpenGL frame` zone from `RenderAPI::beginFrame` to `RenderAPI::endFrame`, timed with
      `GL_TIMESTAMP` queries, collected after each buffer swap.
    - Vulkan: one `Vulkan batch` zone per command buffer submit (each framebuffer batch), with timestamp queries
      collected at the start of the next batch, outside the render pass. Skipped with a warning when the device has
      no `timestampComputeAndGraphics`.
    - Finer GPU zones (per render pass, per renderer) need a backend-neutral GPU scope macro; left for the
      Owl RHI work.
- **Allocations**: only with the memory tracker (`-DOWL_ENABLE_MEMORY_TRACKER=ON`), whose `operator new` /
  `operator delete` overrides forward every allocation to Tracy's memory view.

Not covered yet: Lua zones (`TracyLua`), lock contention (`LockableBase`), log messages on the timeline.

## Memory tracker

`OWL_ENABLE_MEMORY_TRACKER` is **off in every build type**, Debug included: it takes a global mutex and records two
list nodes and two map entries per live allocation, which doubles the live allocation set and skews any Debug
timing (audit D-11). Turn it on for a leak report at exit, the Owl Nest memory readout, or Tracy's memory view;
`OWL_ENABLE_STACKTRACE` implies it.

## Logs on hot paths

- A message below the runtime verbosity (`Log::setVerbosityLevel`) evaluates neither its format nor its arguments.
- `OWL_LOG_LEVEL` (`trace` by default) is the lowest level compiled in: `-DOWL_LOG_LEVEL=info` removes every
  `OWL_*_TRACE` at compile time, arguments included.
- `OWL_CORE_FRAME_TRACE` is a trace sampled once every `Log::setFrameFrequency` frames (100 by default): the only
  log allowed in a per-frame path.
- The log file is flushed on warnings and errors only, no longer on every displayed message.
- Client macros (`OWL_INFO`...) always go to the `APP` logger, engine macros (`OWL_CORE_INFO`...) to `OWL`.

## Measured cost

`owl_bench --exclude=slang` (Release, Clang 22, Conan, one pinned core, best of two interleaved runs, 169
benchmarks), median time compared with the build before Tracy (2026-10-05):

| Build                           | Median ratio | Range        | Notes                                                     |
|---------------------------------|--------------|--------------|-----------------------------------------------------------|
| `OWL_PROFILER=none`             | 1.000        | 0.76 to 1.06 | Same code as before: the macros are empty                 |
| `OWL_PROFILER=tracy`, no client | 1.000        | 0.73 to 1.18 | `frame` group +0.5 %; worst: per-quad zones of `drawQuad` |

A zone sitting in a per-item loop (`Renderer2D::drawQuad` and its world-index lookup: +16 % on 10 000 quads, about
2 ns per quad) is the visible cost without a client; zone the loop, not the item. With a client connected, the
`frame` benchmarks streamed about 12 MB/s of compressed events.
