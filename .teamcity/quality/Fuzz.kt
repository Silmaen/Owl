import jetbrains.buildServer.configs.kotlin.*
import jetbrains.buildServer.configs.kotlin.buildSteps.script

/**
 * The libFuzzer targets, nightly on `main`: `linux-fuzz` builds the fuzzers of `fuzz/` with AddressSanitizer,
 * the `Fuzz` action runs each one for five minutes and fails on any crash, leak or timeout. The failing inputs
 * (the `artifacts` folder of each fuzzer under `output/fuzz`) are published so they can be replayed locally.
 * Not part of PR Ready.
 */
val fuzz = presetBuild("Build_Quality_Fuzz", "Fuzzing", "linux-fuzz",
        onPullRequest = false, nightly = true) {
    artifactRules = """
        %artifact_path%
        output/fuzz/*/artifacts/** => fuzz-artifacts
    """.trimIndent()
    steps {
        script {
            ciAction("Fuzz", "Fuzz", displayName = "Fuzzing")
        }
    }
}
