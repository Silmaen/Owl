import jetbrains.buildServer.configs.kotlin.*

private val buildGates = listOf(linuxClang, windowsClang)

val sanitizerAddress = presetBuild("Build_Quality_SanitizerAddress", "Sanitizer Address",
        "linux-sanitizer-address", onDraft = true, gates = buildGates)
val sanitizerLeak = presetBuild("Build_Quality_SanitizerLeak", "Sanitizer Leak",
        "linux-sanitizer-leak", gates = buildGates)
val sanitizerThread = presetBuild("Build_Quality_SanitizerThread", "Sanitizer Thread",
        "linux-sanitizer-thread", gates = buildGates)
val sanitizerUndefinedBehavior = presetBuild("Build_Quality_SanitizerUndefinedBehavior",
        "Sanitizer Undefined Behavior", "linux-sanitizer-undefined-behavior", gates = buildGates)

val sanitizers = Project {
    id = RelativeId("Sanitizers")
    name = "Sanitizers"

    buildType(sanitizerAddress)
    buildType(sanitizerLeak)
    buildType(sanitizerThread)
    buildType(sanitizerUndefinedBehavior)
    buildTypesOrder = arrayListOf(sanitizerAddress, sanitizerLeak, sanitizerThread, sanitizerUndefinedBehavior)

    params {
        param("platform", "Linux")
        param("architecture", "amd64")
    }
}
