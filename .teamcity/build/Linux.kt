import jetbrains.buildServer.configs.kotlin.*
import jetbrains.buildServer.configs.kotlin.failureConditions.BuildFailureOnMetric
import jetbrains.buildServer.configs.kotlin.failureConditions.failOnMetricChange

val linuxGcc = presetBuild("Build_LinuxX64_Gcc", "GCC", "linux-gcc-debug")
// Builds the release and Doxygen on it: it runs on documentation-only pull requests too.
// It also measures the coverage: a line coverage more than one point below the last successful build fails it.
val linuxClang = presetBuild("Build_LinuxX64_Clang", "Clang", "linux-clang-debug", onDraft = true, pathFilter = "") {
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
        param("platform", "Linux")
        param("architecture", "amd64")
    }
}
