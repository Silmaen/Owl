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
        param("cmake_target", "all")
        param("owl_git_branch", "main")
        param("WatchBranchFilter", """
            +:*
            -:Experiment/*
            -:refs/heads/Experiment/*
            -:main
            -:refs/heads/main
        """.trimIndent())
        checkbox("publish_package", "true",
                  checked = "true", unchecked = "false")
        // Only main is fetched directly. PR refs are managed by the
        // pullRequest build feature on each template (refs/pull/*/head are
        // pulled automatically when a PR matches the feature's filters).
        // Push to a feature branch without an open PR = no CI activity.
        param("branch_specification", """
            +:refs/heads/(%owl_git_branch%)
            +:refs/(pull/*)/head
        """.trimIndent())
        param("cmake_options", "-j4")

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
