import jetbrains.buildServer.configs.kotlin.*

/**
 * Meta configuration whose only job is to turn red when any configuration a ready pull
 * request runs is red: the single check to require in the branch protection of `main`.
 *
 * A composite build: it uses no agent, it only aggregates its snapshot dependencies, which
 * the bridge and `skipIfCommitPassed` already run or reuse for the commit. Not run for
 * drafts nor `Experiment/…` pull requests (their verdict is the fast subset only), nor for
 * documentation-only pull requests (it carries the default path filter, so it does not pull
 * the whole matrix in for a typo fix).
 */
val prReady = BuildType {
    id = RelativeId("Build_Quality_PrReady")
    name = "PR Ready"
    type = BuildTypeSettings.Type.COMPOSITE

    vcs {
        root(githubOwl)
        showDependenciesChanges = true
    }

    features {
        // The check the branch protection of `main` requires: its name must not follow the tree.
        githubBridge(annotateDiff = false, checkName = "PR Ready")
    }

    dependencies {
        listOf(codeStyle, includeCheck, linuxClang, linuxGcc, windowsClang, windowsGcc,
                sanitizerAddress, sanitizerThread, sanitizerUndefinedBehavior,
                clangTidy, staticAnalyzer).forEach { gate ->
            snapshot(gate) {
                // A red gate makes this build red, naming the failed dependency.
                onDependencyFailure = FailureAction.ADD_PROBLEM
                onDependencyCancel = FailureAction.ADD_PROBLEM
                reuseBuilds = ReuseBuilds.SUCCESSFUL
            }
        }
    }
}
