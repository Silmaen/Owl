package _Self

import jetbrains.buildServer.configs.kotlin.BuildType

// Extension helpers that customise the inherited teamcity-github-bridge
// feature (BRIDGE_GITHUB on the GlobalBuild / CodeStylingCheck templates)
// on a per-BT basis. Shared between Build.kt and Packaging.kt — both
// projects inherit the same template so they need the same overrides.
//
// Internally each helper disables the inherited feature and re-attaches
// a fresh one with a distinct id; the BridgeFeatureReader inside the
// plugin only honours the first github-bridge feature it finds, so the
// two helpers below are mutually exclusive on the same BT.

// Mark this BT as draft-friendly: runs on draft PRs as well as ready
// ones. The inherited BRIDGE_GITHUB carries triggerOnPrDraft=false; we
// re-attach the same feature with triggerOnPrDraft=true under a distinct
// id so it does not collide with the disabled inherited one.
fun BuildType.allowDraftPR() {
    disableSettings("BRIDGE_GITHUB")
    features {
        feature {
            id = "BRIDGE_GITHUB_DRAFT"
            type = "github-bridge"
            param("triggerOnPrDraft", "true")
        }
    }
}

// Keep this BT off automated PR triggers. Replaces the pre-v1.4
// "disable VCS TRIGGER_2" trick now that the feature-branch VCS
// trigger no longer exists. The plugin's PR branch-list override is
// set to "-:*" so the gate returns SUPPRESS_BRANCH_PR on every PR
// event — a "Skipped: branch out of scope" Check Run is posted (so
// reviewers see the BT was intentionally not run) and the build is
// not enqueued. Manual operator runs from the TC UI bypass this SOFT
// block (BridgeGate.decidePr short-circuits to ALLOW on manual).
fun BuildType.skipAutoPRs() {
    disableSettings("BRIDGE_GITHUB")
    features {
        feature {
            id = "BRIDGE_GITHUB_MAIN_ONLY"
            type = "github-bridge"
            param("prTriggerBranchesOverride", "-:*")
        }
    }
}
