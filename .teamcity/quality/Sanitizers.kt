import jetbrains.buildServer.configs.kotlin.*

// Level 2: after Code Style only, in parallel with the builds (each sanitizer builds its own
// preset). No LeakSanitizer job: on Linux LSan is part of ASan (`detect_leaks=1`).


val sanitizerAddress = presetBuild("Build_Quality_SanitizerAddress", "Sanitizer Address",
        "linux-sanitizer-address", onDraft = true)
// Address is the pull-request sanitizer; Thread and Undefined Behavior run nightly on `main`.
val sanitizerThread = presetBuild("Build_Quality_SanitizerThread", "Sanitizer Thread",
        "linux-sanitizer-thread", onPullRequest = false, nightly = true)
val sanitizerUndefinedBehavior = presetBuild("Build_Quality_SanitizerUndefinedBehavior",
        "Sanitizer Undefined Behavior", "linux-sanitizer-undefined-behavior", onPullRequest = false, nightly = true)

val sanitizers = Project {
    id = RelativeId("Sanitizers")
    name = "Sanitizers"

    buildType(sanitizerAddress)
    buildType(sanitizerThread)
    buildType(sanitizerUndefinedBehavior)
    buildTypesOrder = arrayListOf(sanitizerAddress, sanitizerThread, sanitizerUndefinedBehavior)

    params {
        param("platform", "Linux")
        param("architecture", "amd64")
    }
}
