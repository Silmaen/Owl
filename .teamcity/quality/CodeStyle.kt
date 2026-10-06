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
        param("cmake_preset", "linux-clang-debug")
        param("platform", "Linux")
    }

    features {
        githubBridge(triggerOnPrDraft = true, pathFilter = "")
    }
}
