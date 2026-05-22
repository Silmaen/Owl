package _Self

import _Self.buildTypes.*
import _Self.vcsRoots.*
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
        """.trimIndent())
        param("cmake_options", "-j4")
    }

    subProject(Build.BuildProject)
    subProject(Packaging.PackagingProject)
})
