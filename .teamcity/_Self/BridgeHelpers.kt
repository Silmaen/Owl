package _Self

import jetbrains.buildServer.configs.kotlin.BuildFeatures
import jetbrains.buildServer.configs.kotlin.BuildType

// Wiring for the teamcity-github-bridge plugin (server-side, v1.10.0).
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

// Inline PR review comment that starts the build configurations the bridge
// never triggers on its own (the heavy, main-only ones). A comment-triggered
// build is stamped `triggerSource=command` (plugin v1.9.0) and gated like a
// manual Run, so the `-:*` PR-branch override below does not remove it.
private const val FULL_MATRIX_COMMENT: String = "/ci full"

// Build the `github-bridge` feature. The defaults describe the common case:
// runs on ready PRs but not on drafts, C++ paths only, no diff annotations.
// The non-PR path is off project-wide (see Project.kt), so a `main` build
// comes from the template's VCS trigger, never from the bridge.
fun BuildFeatures.githubBridge(
    featureId: String = BRIDGE_FEATURE_ID,
    runOnDraftPr: Boolean = false,
    autoPrTrigger: Boolean = true,
    annotateDiff: Boolean = false,
    pathFilter: String = CODE_ONLY_PATHS,
) {
    feature {
        id = featureId
        type = "github-bridge"

        param("triggerOnPrDraft", runOnDraftPr.toString())

        if (!autoPrTrigger) {
            // Every PR event returns SUPPRESS_BRANCH_PR → a "Skipped: branch
            // out of scope" Check Run is posted (so reviewers see the BT was
            // intentionally not run) and nothing is enqueued. A manual Run in
            // TeamCity or the comment phrase above still runs it.
            param("prTriggerBranchesOverride", "-:*")
            param("commentTrigger", FULL_MATRIX_COMMENT)
        }

        if (pathFilter.isNotEmpty()) {
            param("pathFilter", pathFilter)
        }

        // A draft build already produced a verdict for this commit: republish
        // it rather than build it again when the PR flips to ready. Owl merges
        // by squash only, so a commit reaching `main` is always a new SHA and
        // never reuses a PR result.
        param("skipIfCommitPassed", "true")

        param("skipPhrase", SKIP_PHRASE)

        // Compiler diagnostics pinned to the lines they concern in the PR
        // diff. Left to the configurations whose diagnostics are distinct, so
        // one compile error is not annotated six times on the same line.
        param("annotateDiff", annotateDiff.toString())
    }
}

// Replace the feature inherited from the template with one carrying different
// gates. One call per BT — a second would be ignored by the plugin.
fun BuildType.bridgeOverride(
    runOnDraftPr: Boolean = false,
    autoPrTrigger: Boolean = true,
    annotateDiff: Boolean = false,
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

// Keep this BT off automated PR triggers: auto-runs on `main` only, and on a
// PR only when a reviewer asks for it with the comment phrase.
fun BuildType.skipAutoPRs() = bridgeOverride(autoPrTrigger = false)
