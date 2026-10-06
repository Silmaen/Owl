package _Self

import _Self.buildTypes.*
import _Self.vcsRoots.*
import _Self.GITHUB_CONNECTION_ID
import jetbrains.buildServer.configs.kotlin.*
import jetbrains.buildServer.configs.kotlin.Project

object Project : Project({
    description = "Les configurations pour le moteur de jeu"

    vcsRoot(HttpsGithubComSilmaenOwlGitRefsHeadsMain)

    template(CodeStylingCheck)
    template(GlobalBuild)

    params {
        param("owl_git_branch", "main")
        // main plus the two allowed branch families. The bridge enqueues a
        // pull request's build on its head branch (prBuildRef = branch); a
        // push to a branch without an open pull request builds nothing.
        param("branch_specification", """
            +:refs/heads/(%owl_git_branch%)
            +:refs/heads/(Feature/*)
            +:refs/heads/(Experiment/*)
        """.trimIndent())

        // teamcity-github-bridge project-level config. The plugin's
        // BridgeFeatureReader reads these via `buildType.project.parameters`
        // (the InheritableUserParametersHolder inheritance path), so they
        // belong on the root project rather than on each template.
        param("teamcity.github.bridge.repo", "Silmaen/Owl")
        param("teamcity.github.bridge.connectionId", GITHUB_CONNECTION_ID)

        // The bridge must not trigger on non-PR branches: `main` belongs to
        // the VCS trigger on each template, and two enqueue paths for one
        // push is one too many. Publication is a separate axis — a `main`
        // build still reports its Check Run.
        // A pull request is built from its head branch (`Feature/*` or
        // `Experiment/*`, the only names allowed), so its builds carry the
        // branch name instead of `pull/N`.
        param("teamcity.github.bridge.prBuildRef", "branch")
        param("teamcity.github.bridge.prTrigger.enabled", "true")
        param("teamcity.github.bridge.branchTrigger.enabled", "true")
        param("teamcity.github.bridge.annotations.enabled", "true")

        // prTrigger.enabled / prTrigger.branches keep their defaults
        // ("enabled" / all branches); the per-BT PR gates live on the
        // github-bridge build feature (BridgeHelpers.kt).

        // A Check Run is named "TeamCity / <buildType.fullName>", i.e.
        // "TeamCity / Owl / Build / Linux x64 / Clang" here — mostly ancestry,
        // while GitHub's merge box truncates the END, the part that says which
        // build it was. Stripping the common prefix leaves
        // "Build / Linux x64 / Clang". Matched literally, ignored when it does
        // not match. This RENAMES the checks (GitHub keys a row on
        // (name, head_sha)): safe here because the repository's "main merging"
        // ruleset requires no status check by name — if one is ever added, it
        // must use the stripped name.
        param("teamcity.github.bridge.checkName.stripPrefix", "TeamCity / Owl / ")
    }

    subProject(Build.BuildProject)
    subProject(Packaging.PackagingProject)
})
