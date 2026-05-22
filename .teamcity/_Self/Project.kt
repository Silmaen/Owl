package _Self

import _Self.buildTypes.*
import _Self.vcsRoots.*
import jetbrains.buildServer.configs.kotlin.*
import jetbrains.buildServer.configs.kotlin.Project

object Project : Project({
    description = "Les configurations pour le moteur de jeu"

    vcsRoot(HttpsGithubComSilmaenOwlGitRefsHeadsMain)

    template(CodeSylingCheck)
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
        param("branch_specification", """
            +:refs/heads/main
            +:refs/heads/(Feature/*)
            +:refs/heads/(Experiment/*)
        """.trimIndent())
        param("cmake_options", "-j4")
    }

    subProject(Packaging.Project)
    subProject(Build.Project)
})
