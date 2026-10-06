import jetbrains.buildServer.configs.kotlin.BuildFeatures
import jetbrains.buildServer.configs.kotlin.BuildType
import jetbrains.buildServer.configs.kotlin.Dependencies
import jetbrains.buildServer.configs.kotlin.FailureAction
import jetbrains.buildServer.configs.kotlin.ReuseBuilds
import jetbrains.buildServer.configs.kotlin.Triggers
import jetbrains.buildServer.configs.kotlin.buildSteps.ScriptBuildStep
import jetbrains.buildServer.configs.kotlin.triggers.VcsTrigger
import jetbrains.buildServer.configs.kotlin.triggers.vcs

/**
 * Paths that cannot change the outcome of a C++ build: a pull request touching only these
 * is dropped by a configuration carrying this filter ("Skipped: paths out of scope").
 * Not applied to Code Style (codespell and markdown are its job) nor to Windows x64 Clang
 * (the only pull-request configuration building Doxygen).
 */
val CODE_ONLY_PATHS: String = """
    -:doc/*
    -:*.md
    -:.claude/*
    -:LICENSE
""".trimIndent()

/**
 * Snapshot dependencies on the configurations that must be green first. A failed or
 * cancelled gate stops the chain; a gate already green for the revision is reused.
 *
 * @param gates The configurations that must be green first.
 */
fun Dependencies.after(vararg gates: BuildType) {
    gates.forEach { gate ->
        snapshot(gate) {
            onDependencyFailure = FailureAction.FAIL_TO_START
            onDependencyCancel = FailureAction.FAIL_TO_START
            reuseBuilds = ReuseBuilds.SUCCESSFUL
            runOnSameAgent = false
        }
    }
}

/**
 * Builds a push to `main`, and nothing else: the bridge plugin enqueues builds from pull
 * request events only (it ignores `push`), so without this trigger a push to main would
 * build nothing at all.
 */
fun Triggers.mainBranchOnly() {
    vcs {
        id = "vcsTrigger"
        branchFilter = """
            +:main
            +:refs/heads/main
        """.trimIndent()
        enableQueueOptimization = true
        quietPeriodMode = VcsTrigger.QuietPeriodMode.DO_NOT_USE
    }
}

/**
 * GitHub App bridge settings, kept explicit rather than relying on the plugin defaults.
 * A configuration redefines the feature of its template by declaring it again: same id.
 *
 * @param triggerOnPrDraft also build pull requests still marked as draft (fast subset).
 * @param triggerOnPrReady build pull requests at all. Off for the packages and the
 *        configurations that run on `main` only.
 * @param annotateDiff write the findings on the diff.
 * @param pathFilter skip pull requests touching only these paths; empty runs on every PR.
 */
fun BuildFeatures.githubBridge(triggerOnPrDraft: Boolean = false,
                              triggerOnPrReady: Boolean = true,
                              annotateDiff: Boolean = true,
                              pathFilter: String = CODE_ONLY_PATHS) {
    feature {
        id = "github-bridge"
        type = "github-bridge"
        param("annotateDiff", annotateDiff.toString())
        param("publishChecks", "true")
        param("runOnApproval", "true")
        param("triggerOnBranch", "true")
        param("triggerOnPrReady", triggerOnPrReady.toString())
        if (triggerOnPrDraft)
            param("triggerOnPrDraft", "true")
        if (pathFilter.isNotEmpty())
            param("pathFilter", pathFilter)
        // A draft build already produced a verdict for this commit: republish it rather
        // than build again when the PR flips to ready. Owl merges by squash only.
        param("skipIfCommitPassed", "true")
        param("skipPhrase", "[skip ci]")
    }
}

/**
 * One `ci_action.py` step. Everything a build does lives in `ci/`: the DSL only says
 * which action runs on which preset.
 *
 * @param action The CI action.
 * @param stepId The step id, kept as the server knows it.
 * @param displayName The step name.
 * @param preset The CMake preset.
 * @param extraArgs Arguments appended to the command line.
 */
fun ScriptBuildStep.ciAction(action: String,
                            stepId: String,
                            displayName: String = action,
                            preset: String = "%cmake_preset%",
                            extraArgs: String = "") {
    name = displayName
    id = stepId
    scriptContent = "poetry run python3 ci_action.py $action $preset" +
            if (extraArgs.isNotEmpty()) " $extraArgs" else ""
}
