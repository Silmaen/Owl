import jetbrains.buildServer.configs.kotlin.*
import jetbrains.buildServer.configs.kotlin.buildSteps.script
import jetbrains.buildServer.configs.kotlin.failureConditions.BuildFailureOnMetric
import jetbrains.buildServer.configs.kotlin.failureConditions.failOnMetricChange

val linuxGcc = presetBuild("Build_LinuxX64_Gcc", "GCC", "linux-gcc-debug")
// Builds the release and Doxygen on it: it runs on documentation-only pull requests too.
// It also measures the coverage: a line coverage more than one point below the last successful build fails it.
// On `main` it ends with the dependency report (`conan graph outdated`, informative: never fails the build).
val linuxClang = presetBuild("Build_LinuxX64_Clang", "Clang", "linux-clang-debug", onDraft = true, pathFilter = "") {
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

// Every optional engine module off (OWL_MODULE_*): the headless core alone must build and pass its tests.
val linuxClangMinimal = presetBuild("Build_LinuxX64_ClangMinimal", "Clang Minimal Modules", "linux-clang-minimal")

val linuxX64 = Project {
    id = RelativeId("Build_LinuxX64")
    name = "Build Linux x64"

    buildType(linuxGcc)
    buildType(linuxClang)
    buildType(linuxClangMinimal)
    buildTypesOrder = arrayListOf(linuxGcc, linuxClang, linuxClangMinimal)

    params {
        param("platform", "Linux")
        param("architecture", "amd64")
    }
}
