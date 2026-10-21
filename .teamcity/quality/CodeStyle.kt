import jetbrains.buildServer.configs.kotlin.*

/**
 * The gate every other configuration waits for. It runs on draft pull requests too and on
 * every pull request, documentation-only ones included: codespell and markdown are its job.
 */
val codeStyle = BuildType {
    id = RelativeId("Build_Quality_CodeStyle")
    name = "Code Style"
    templates(toolBuild)
    maxRunningBuilds = 1

    params {
        text("cmake_preset", "linux-clang-debug", display = ParameterDisplay.HIDDEN, allowEmpty = true)
        text("platform", "Linux", display = ParameterDisplay.HIDDEN, allowEmpty = true)
    }

    features {
        githubBridge(triggerOnPrDraft = true, pathFilter = "")
    }
}
