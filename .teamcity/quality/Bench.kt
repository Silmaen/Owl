import jetbrains.buildServer.configs.kotlin.*
import jetbrains.buildServer.configs.kotlin.buildSteps.script

/**
 * The engine benchmarks, nightly on `main` and on every ready pull request: `linux-bench` builds `owl_bench` in
 * release, the `Bench` action runs it and fails when a benchmark's median exceeds `bench/baseline/linux-bench.json`
 * by more than 15 %. The results are published (`output/bench/`) so a new baseline can be committed. A gate of
 * PR Ready. The custom-build dialog offers a calibration (`bench_calibrate`, `bench_rounds`): the benchmarks run
 * several times and the proposed baseline and thresholds are published instead of a comparison.
 */
val bench = presetBuild("Build_Quality_Bench", "Benchmarks", "linux-bench",
        onPullRequest = true, nightly = true) {
    params {
        checkbox("bench_calibrate", "false", label = "Calibrate",
                description = "Run the benchmarks several times and publish a proposed baseline and thresholds instead of comparing",
                checked = "true", unchecked = "false", display = ParameterDisplay.NORMAL)
        text("bench_rounds", "5", label = "Rounds", description = "Benchmark runs of a calibration",
                display = ParameterDisplay.NORMAL, allowEmpty = false)
        // No CTest run here: the field stays out of the custom build dialog.
        text("test_args", "", display = ParameterDisplay.HIDDEN, allowEmpty = true)
    }
    // Timings only compare on the agent that measured the baseline (run-to-run: hephaistos 1.7 %, artemis 10.7 %).
    requirements {
        equals("teamcity.agent.name", "linux-build-hephaistos")
    }
    artifactRules = """
        %artifact_path%
        output/bench/*.json
    """.trimIndent()
    steps {
        script {
            ciAction("Bench", "Bench", displayName = "Benchmarks",
                    extraArgs = "-- --calibrate=%bench_calibrate% --rounds=%bench_rounds%")
        }
    }
}
