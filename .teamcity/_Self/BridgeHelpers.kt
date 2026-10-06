package _Self

import jetbrains.buildServer.configs.kotlin.BuildFeatures
import jetbrains.buildServer.configs.kotlin.BuildType
import jetbrains.buildServer.configs.kotlin.Dependencies
import jetbrains.buildServer.configs.kotlin.FailureAction
import jetbrains.buildServer.configs.kotlin.ReuseBuilds

// Wiring for the teamcity-github-bridge plugin (server-side, v1.10.0), modelled on
// EvenementLoto.
//
// The per-BT opt-in is the `github-bridge` build feature: both templates carry
// one built by `githubBridge()` below, and a BT that needs different gates
// replaces it through `bridgeOverride()`. An inheriting BT cannot edit the
// params of a template's feature, so an override disables the inherited one
// and attaches a fresh one — the plugin honours a single `github-bridge`
// feature per BT, which is why the two ids are distinct.
//
// Only the knobs Owl actually uses are set; everything else keeps the plugin
// default. Rationale for each: doc/pages/continuous_integration.md.

// Feature id carried by both templates.
const val BRIDGE_FEATURE_ID: String = "BRIDGE_GITHUB"

// Feature id used by a per-BT override.
private const val BRIDGE_FEATURE_LOCAL_ID: String = "BRIDGE_GITHUB_LOCAL"

// Paths that cannot change the outcome of a C++ build. The plugin keeps a BT
// as soon as ONE file changed by the PR matches, so a doc-only PR matches
// nothing and is dropped with a "Skipped: paths out of scope" Check Run
// instead of running the whole matrix. Exclude-only spec: a file matches
// unless a rule covers it, and `*` spans `/`.
//
// Deliberately NOT applied to the two places whose inputs these paths are:
// the Code Style gate (codespell + markdown are its job) and Windows x64
// Clang (the only PR-side BT with `OWL_ENABLE_DOCUMENTATION=ON`, so Doxygen
// there consumes `doc/` and the root markdown files).
val CODE_ONLY_PATHS: String = """
    -:doc/*
    -:*.md
    -:.claude/*
    -:LICENSE
""".trimIndent()

// Put in a PR title or body to keep the bridge from triggering anything.
private const val SKIP_PHRASE: String = "[skip ci]"

// Build the `github-bridge` feature. The defaults describe the common case:
// runs on ready PRs but not on drafts, C++ paths only, findings pinned on the
// diff. The plugin enqueues builds from pull request events only (it ignores
// `push`), so a `main` build comes from the template's VCS trigger.
fun BuildFeatures.githubBridge(
    featureId: String = BRIDGE_FEATURE_ID,
    runOnDraftPr: Boolean = false,
    autoPrTrigger: Boolean = true,
    annotateDiff: Boolean = true,
    pathFilter: String = CODE_ONLY_PATHS,
) {
    feature {
        id = featureId
        type = "github-bridge"

        // Explicit, identical to the server state, rather than plugin defaults.
        param("publishChecks", "true")
        param("runOnApproval", "true")
        param("triggerOnBranch", "true")
        param("triggerOnPrDraft", runOnDraftPr.toString())
        // A main-only configuration never runs for a pull request; its Check
        // Run still appears on the commits `main` builds.
        param("triggerOnPrReady", autoPrTrigger.toString())

        if (pathFilter.isNotEmpty()) {
            param("pathFilter", pathFilter)
        }

        // A draft build already produced a verdict for this commit: republish
        // it rather than build it again when the PR flips to ready. Owl merges
        // by squash only, so a commit reaching `main` is always a new SHA and
        // never reuses a PR result.
        param("skipIfCommitPassed", "true")

        param("skipPhrase", SKIP_PHRASE)

        // Findings pinned to the lines they concern in the PR diff. Off only
        // where a finding would not be this pull request's doing.
        param("annotateDiff", annotateDiff.toString())
    }
}

// Replace the feature inherited from the template with one carrying different
// gates. One call per BT — a second would be ignored by the plugin.
fun BuildType.bridgeOverride(
    runOnDraftPr: Boolean = false,
    autoPrTrigger: Boolean = true,
    annotateDiff: Boolean = true,
    pathFilter: String = CODE_ONLY_PATHS,
) {
    disableSettings(BRIDGE_FEATURE_ID)
    features {
        githubBridge(
            featureId = BRIDGE_FEATURE_LOCAL_ID,
            runOnDraftPr = runOnDraftPr,
            autoPrTrigger = autoPrTrigger,
            annotateDiff = annotateDiff,
            pathFilter = pathFilter,
        )
    }
}

// Keep this BT off pull requests: it runs on `main` (VCS trigger) or by hand.
fun BuildType.skipAutoPRs() = bridgeOverride(autoPrTrigger = false)

// Snapshot dependencies on the configurations that must be green first. A
// failed or cancelled gate stops the chain, and a gate already green for the
// revision is reused instead of being run again.
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
