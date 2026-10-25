import jetbrains.buildServer.configs.kotlin.*
import jetbrains.buildServer.configs.kotlin.buildSteps.script

// The coverage gate of every ready pull request and of `main`: GCC Debug builds and tests the tree and measures the
// line coverage; a coverage more than one point below the last successful build fails it.
val linuxGcc = presetBuild("Build_LinuxX64_Gcc", "GCC", "linux-gcc-debug") {
    failureConditions {
        coverageDrop()
    }
}
// The fast verdict, on drafts too: the release tree, its tests and Doxygen (it runs on documentation-only pull
// requests). On `main` the same tree writes the archives the Linux x64 package publishes, then the dependency report
// (`conan graph outdated`, informative: never fails the build).
val linuxClangRelease = presetBuild("Build_LinuxX64_ClangRelease", "Clang", "linux-clang-release", onDraft = true,
        pathFilter = "") {
    steps {
        script {
            ciAction("DependencyReport", "Dependency_Report", displayName = "Dependency Report")
            conditions {
                equals("teamcity.build.branch.is_default", "true")
            }
        }
    }
}
// Once a night on `main`, when it changed: Clang Debug runs the whole suite (the `slow` tests the pushes leave out)
// and measures its own coverage, beside the GCC one.
val linuxClang = presetBuild("Build_LinuxX64_Clang", "Clang Debug", "linux-clang-debug", onPullRequest = false,
        nightly = true) {
    params {
        fullSuite()
    }
    failureConditions {
        coverageDrop()
    }
}

val linuxX64 = Project {
    id = RelativeId("Build_LinuxX64")
    name = "Build Linux x64"

    buildType(linuxClangRelease)
    buildType(linuxGcc)
    buildType(linuxClang)
    buildTypesOrder = arrayListOf(linuxClangRelease, linuxGcc, linuxClang)

    params {
        text("platform", "Linux", display = ParameterDisplay.HIDDEN, allowEmpty = true)
        text("architecture", "amd64", display = ParameterDisplay.HIDDEN, allowEmpty = true)
    }
}
