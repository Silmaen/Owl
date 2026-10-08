import jetbrains.buildServer.configs.kotlin.*
import jetbrains.buildServer.configs.kotlin.buildSteps.script

/**
 * The engine benchmarks, nightly on `main`: `linux-bench` builds `owl_bench` in release, the `Bench` action runs
 * it and fails when a benchmark's median exceeds `bench/baseline/linux-bench.json` by more than 15 %. The
 * results are published (`output/bench/`) so a new baseline can be committed. Not part of PR Ready: pull
 * requests only compile the benchmarks (Linux x64 Clang builds `linux-clang-debug` with `OWL_BENCHMARK=ON`).
 */
val bench = presetBuild("Build_Quality_Bench", "Benchmarks", "linux-bench",
        onPullRequest = false, nightly = true) {
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
            ciAction("Bench", "Bench", displayName = "Benchmarks")
        }
    }
}
