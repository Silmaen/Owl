# Owl benchmarks

`owl_bench` is a single executable of CPU micro-benchmarks for the engine, run headless on the Null render
backend. It uses a small in-house harness
(`bench/harness/Bench.h`): `steady_clock`, warm-up samples, auto-calibrated batch size, median, quartiles,
CSV / JSON output. It is off by default (`OWL_BENCHMARK=OFF`); the CI compiles it on every pull request
(`linux-clang-debug`) and runs it every night on `main` against a baseline (see [In CI](#in-ci)).

## Build

Use a build directory of its own, inside the Docker image:

```bash
docker/run.sh cmake --preset linux-clang-release -S . -B output/build/linux-clang-release-bench -DOWL_BENCHMARK=ON
docker/run.sh cmake --build output/build/linux-clang-release-bench --target owl_bench
```

## Run

Run from the `bin/` directory (the dummy application looks up `engine_assets/` and `sample_project/` from
its working directory). Pin it to one performance core to cut the noise (CPUs 0-15 are P-cores on the
reference i9-13950HX):

```bash
cd output/build/linux-clang-release-bench/bin
../../../../docker/run.sh taskset -c 6 ./owl_bench --csv=bench.csv --json=bench.json
```

| Option              | Default | Meaning                                                      |
|---------------------|---------|--------------------------------------------------------------|
| `--filter=<text>`   | (all)   | Run only the benchmarks whose name contains `<text>`.        |
| `--exclude=<text>`  | (none)  | Skip the benchmarks whose name contains `<text>`.            |
| `--samples=<n>`     | 15      | Timed samples per benchmark.                                 |
| `--warmup=<n>`      | 2       | Untimed samples before the timed ones.                       |
| `--min-sample-ms=x` | 10      | Minimum duration of one sample (sets the batch size).        |
| `--csv=<file>`      | (none)  | Write the results as CSV.                                    |
| `--json=<file>`     | (none)  | Write the results, metrics and load average as JSON.         |
| `--verbose`         | off     | Keep engine warnings and errors in the output.               |

Groups (name prefixes): `scene`, `serialize`, `prefab`, `renderer2d`, `frame`, `voxel`, `script`,
`physics`, `sample`, `slang`. The first Slang compilation is only "cold" when `slang` is the first group to
compile a shader in the process, which is the case for a full run. `voxel/streaming` streams a procedural world
through a real `RendererVoxel` layer with frames paced at 60 Hz (about 12 s) and reports the main-thread CPU time per
frame (p50, p99, peak), the meshing work and the chunk appearance latency.


## In CI

The nightly *Benchmarks* configuration builds the `linux-bench` preset (release, no tests, no editor) and runs
`ci_action.py Bench linux-bench`: `owl_bench` writes `output/bench/linux-bench.json` (published as an artifact),
compared to `bench/baseline/linux-bench.json`. A benchmark whose median exceeds its baseline by more than 15 %
is measured again alone, and the faster of the two medians is kept; one still beyond the threshold fails the
build. Without a baseline the run only reports: commit the artifact of a nightly run as the baseline (or run the
action with `-- --update-baseline` on the same agent), and refresh it when a change is deliberately slower or
the agent changes.
The configuration is pinned to the agent `linux-build-hephaistos` (8 cores): the committed baseline is the
median of its 5 runs of 2026-10-08 (builds 34968 to 34988, listed in `context.baseline_runs`). Timings from
another agent do not compare (artemis is 20 % faster but varies 10.7 % between runs, against 1.7 % here).

```bash
docker/run.sh poetry run python ci_action.py Build linux-bench
docker/run.sh poetry run python ci_action.py Bench linux-bench -- --threshold=0.15
```

## Reading the output

Each line gives the median time of one body call, the fastest sample, the interquartile range in percent
of the median, the median divided by the number of items (entities, quads, frames...) and the batch size.
Lines without a time are metrics (sizes, counts, memory, correctness checks). The load average at start
and end is printed: a loaded machine makes the numbers unreliable.

## Profiling a case

`perf` is not usable in the image yet; use callgrind on one filtered case:

```bash
../../../../docker/run.sh valgrind --tool=callgrind --toggle-collect='*Scene::onUpdateEditor*' \
    ./owl_bench --samples=1 --warmup=0 --min-sample-ms=1 --filter=frame/editor_update/flat10000
```

## Frame bench (GPU, real runner)

`owl_bench` never touches a GPU. Whole frames on Vulkan or OpenGL are measured by the runner itself, in its
`--frame-bench` mode (built with the default presets, no `OWL_BENCHMARK` needed):

```bash
docker/run.sh --gui output/build/linux-clang-release/bin/OwlRunner \
    --frame-bench sample_project/scenes/raycast_demo.owl --backend vulkan --frames 1000 --warmup 120 \
    --out output/frame-bench/raycast_demo.vulkan.json
```

| Option              | Default  | Meaning                                                                       |
|---------------------|----------|-------------------------------------------------------------------------------|
| `--frame-bench <f>` | required | Scene to load; relative paths are resolved from the caller's directory.       |
| `--frames <n>`      | 1000     | Measured frames.                                                              |
| `--warmup <n>`      | 120      | Frames run before the measure (shader cache, streaming, first uploads).       |
| `--backend <b>`     | vulkan   | `vulkan`, `opengl` or `null` (headless: null window, no GUI).                 |
| `--out <file>`      | (none)   | JSON report: options, device, summary and one record per frame.               |
| `--project <dir>`   | (auto)   | Project with `owl_project.yml`; found by walking up from the scene otherwise. |
| `--size <WxH>`      | 1280x720 | Window size.                                                                  |
| `--timestep-ms <t>` | 16.667   | Fixed simulation step fed to the scene, whatever the real frame time.         |
| `--vsync`           | off      | Keep vertical synchronisation (the present mode is reported either way).      |
| `--validation`      | off      | Vulkan validation layers.                                                     |
| `--capture <png>`   | (none)   | Render offscreen and write the last frame as a PNG (image tests).             |

The run is deterministic: fixed time step, null input backend (no keyboard, mouse or cursor capture), the scene's
own primary camera, null sound, no `config.yml` and no user `settings.yml`. With `--capture`, asynchronous loads are
finished before every frame so the image does not depend on timing. It exits with 0 on success, 2 on a bad option or
scene, 3 when the scene quits early, 4 when the renderer cannot start, 5 when the report cannot be written, 6 when the
capture cannot be read back or written.

Per frame it records the wall time between two frame starts and its phases (`beginFrame`, scene update, scripts,
physics, render preparation, GUI, submission, present), the draw calls, the queue submissions, the
`vkQueueWaitIdle` / `vkDeviceWaitIdle` calls, the blocking fence waits besides the frame pacing (`fence_wait`: one-shot
uploads, mid-frame read-backs), and the GPU time from timestamp queries: `vkCmdWriteTimestamp` around
every Vulkan command buffer (`gpu_busy_ms` is their sum, `gpu_span_ms` first to last timestamp), `glQueryCounter`
(`GL_TIMESTAMP`) at `beginFrame` / `endFrame` on OpenGL. The text summary gives count, median, p95, p99,
interquartile range and maximum of every series.

It also measures the cold start of the real runner (`startup_ms` in the report, `start-up:` line in the summary):
`engine_ready` from the entry of `createApplication` to an engine able to load a scene (window, renderer, shader
compilation or cache), `first_frame` to the start of the first frame (scene loaded, render stack installed). The
dynamic loader and static initialisers before `main` are not counted. On lavapipe / llvmpipe (October 2026, warm
SPIR-V cache) the `mixed` image-test scene starts in about 365 ms to an engine ready and 400 ms to the first frame,
with either backend.

Pick the device with the driver's own variables: `VK_ICD_FILENAMES=/etc/vulkan/icd.d/nvidia_icd.json` (or
`/usr/share/vulkan/icd.d/intel_icd.json`, `lvp_icd.json`) for Vulkan,
`__NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia` or `__GLX_VENDOR_LIBRARY_NAME=mesa
MESA_LOADER_DRIVER_OVERRIDE=iris` for OpenGL. The `device` field of the report says which one actually ran.
Baseline and protocol: `doc/audit/20-mesures.md`, section 8.
