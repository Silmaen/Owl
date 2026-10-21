import jetbrains.buildServer.configs.kotlin.*
import jetbrains.buildServer.configs.kotlin.buildSteps.script
import jetbrains.buildServer.configs.kotlin.failureConditions.BuildFailureOnMetric
import jetbrains.buildServer.configs.kotlin.failureConditions.failOnMetricChange
import jetbrains.buildServer.configs.kotlin.triggers.schedule

// GCC parity on `main` only: a pull request gets its verdict from Clang. Release only: Clang builds the Debug tree
// and measures the coverage, GCC adds its own warnings and optimiser.
val linuxGcc = presetBuild("Build_LinuxX64_Gcc", "GCC", "linux-gcc-release", onPullRequest = false)
// Builds the release and Doxygen on it: it runs on documentation-only pull requests too.
// It also measures the coverage: a line coverage more than one point below the last successful build fails it.
// On `main` it ends with the dependency report (`conan graph outdated`, informative: never fails the build).
// Once a night it also runs the whole suite (the `slow` tests the pushes leave out), on `main` when it changed.
val linuxClang = presetBuild("Build_LinuxX64_Clang", "Clang", "linux-clang-debug", onDraft = true, pathFilter = "") {
    triggers {
        schedule {
            id = "nightlyFullSuite"
            schedulingPolicy = daily {
                hour = 1
            }
            branchFilter = """
                +:main
                +:refs/heads/main
            """.trimIndent()
            withPendingChangesOnly = true
            buildParams {
                fullSuite()
            }
        }
    }
    steps {
        script {
            ciAction("DependencyReport", "Dependency_Report", displayName = "Dependency Report")
            conditions {
                equals("teamcity.build.branch.is_default", "true")
            }
        }
    }
    failureConditions {
        failOnMetricChange {
            id = "COVERAGE_DROP"
            metric = BuildFailureOnMetric.MetricType.COVERAGE_LINE_PERCENTAGE
            threshold = 1
            units = BuildFailureOnMetric.MetricUnit.DEFAULT_UNIT
            comparison = BuildFailureOnMetric.MetricComparison.LESS
            compareTo = build { buildRule = lastSuccessful() }
        }
    }
}

val linuxX64 = Project {
    id = RelativeId("Build_LinuxX64")
    name = "Build Linux x64"

    buildType(linuxGcc)
    buildType(linuxClang)
    buildTypesOrder = arrayListOf(linuxGcc, linuxClang)

    params {
        text("platform", "Linux", display = ParameterDisplay.HIDDEN, allowEmpty = true)
        text("architecture", "amd64", display = ParameterDisplay.HIDDEN, allowEmpty = true)
    }
}
