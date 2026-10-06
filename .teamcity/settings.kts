import jetbrains.buildServer.configs.kotlin.*

/*
Entry point of the "Owl" project, laid out like EvenementLoto's.

This file holds nothing but the project itself: its parameters, the VCS root, the
templates it registers and the sub-projects in the order of the dependency chain.
Everything else is split to mirror that hierarchy:

    common/Vcs.kt           the git VCS root and the GitHub App connection id
    common/Templates.kt     Global Build and Tool Build
    common/Helpers.kt       the VCS trigger, the GitHub bridge feature, the dependencies
    common/Factories.kt     one function per family of configuration
    quality/CodeStyle.kt    the gate every other configuration waits for
    build/Linux.kt          Build Linux x64
    build/LinuxArm64.kt     Build Linux arm64 (emulated, main only)
    build/Windows.kt        Build Windows x64
    quality/Sanitizers.kt   the four sanitizers
    quality/Analysis.kt     clang-tidy, the static analyzer, the include check
    packaging/Package.kt    the engine and editor packages

A declaration cannot live in this file if another file needs it: the top-level values of
a `.kts` are members of the script's own class, invisible from a `.kt` beside it.

Everything a build does lives in `ci/` and is driven by `ci_action.py <Action> <preset>`;
the DSL only describes which presets exist, where they run and in which order.
*/

version = "2026.2"

project {
    description = "Les configurations pour le moteur de jeu"

    vcsRoot(githubOwl)

    template(globalBuild)
    template(toolBuild)

    params {
        param("owl_git_branch", "main")
        // main plus the only two allowed branch families.
        param("branch_specification", """
            +:refs/heads/(%owl_git_branch%)
            +:refs/heads/(Feature/*)
            +:refs/heads/(Experiment/*)
        """.trimIndent())
        // Pull requests are built by the GitHub App bridge on their head branch
        // (prBuildRef = branch): TeamCity shows `Feature/…`, not `pull/N`. Branch pushes
        // are not built by the bridge, so main relies on the templates' VCS trigger.
        param("teamcity.github.bridge.repo", "Silmaen/Owl")
        param("teamcity.github.bridge.connectionId", GITHUB_CONNECTION_ID)
        param("teamcity.github.bridge.prBuildRef", "branch")
        param("teamcity.github.bridge.prTrigger.enabled", "true")
        param("teamcity.github.bridge.branchTrigger.enabled", "true")
        param("teamcity.github.bridge.annotations.enabled", "true")
        // What GitHub shows for each check: the tail of the name, not the ancestry. The
        // prefix must match exactly, and a protection rule names a check literally.
        param("teamcity.github.bridge.checkName.stripPrefix", "TeamCity / Owl / ")
    }

    // The gate, at the root.
    buildType(codeStyle)

    subProject(linuxX64)
    subProject(windowsX64)
    subProject(linuxArm64)
    subProject(sanitizers)
    subProject(analysis)
    subProject(packaging)

    // Two levels, as parallel as the agents allow. Level 1: Code Style and Include Check,
    // which wait for nothing. Level 2: every build, sanitizer and analysis, after Code Style
    // only. Packages run on `main` only, after the build that tested their platform.
    subProjectsOrder = arrayListOf(
            RelativeId("Build_LinuxX64"),
            RelativeId("Build_WindowsX64"),
            RelativeId("Build_LinuxArm64"),
            RelativeId("Sanitizers"),
            RelativeId("Analysis"),
            RelativeId("Packaging"),
    )
}
