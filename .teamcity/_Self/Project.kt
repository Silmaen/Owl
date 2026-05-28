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
        // Only main is fetched directly. PR refs are managed by the
        // teamcity-github-bridge plugin on the templates (refs/pull/*/head
        // are pulled by TC's VCS root via the spec below; the plugin
        // reacts to pull_request webhooks to actually enqueue builds).
        // Push to a feature branch without an open PR = no CI activity.
        param("branch_specification", """
            +:refs/heads/(%owl_git_branch%)
            +:refs/(pull/*)/head
        """.trimIndent())

        // teamcity-github-bridge project-level config. The plugin's
        // BridgeFeatureReader reads these via `buildType.project.parameters`
        // (the InheritableUserParametersHolder inheritance path), so they
        // belong on the root project rather than on each template.
        // The other 4 keys (branchTrigger.enabled / branches, prTrigger.enabled / branches)
        // default to "enabled = true" and "all branches", which is what we want.
        param("teamcity.github.bridge.repo", "Silmaen/Owl")
        param("teamcity.github.bridge.connectionId", GITHUB_CONNECTION_ID)
    }

    subProject(Build.BuildProject)
    subProject(Packaging.PackagingProject)
})
