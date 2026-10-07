# Owl benchmarks

`owl_bench` is a single executable of CPU micro-benchmarks for the engine, run headless on the Null render
backend. It uses a small in-house harness
(`bench/harness/Bench.h`): `steady_clock`, warm-up samples, auto-calibrated batch size, median, quartiles,
CSV / JSON output. It is off by default (`OWL_BENCHMARK=OFF`) and never built by CI.

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
compile a shader in the process, which is the case for a full run.

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
