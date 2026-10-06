import jetbrains.buildServer.configs.kotlin.*
import jetbrains.buildServer.configs.kotlin.failureConditions.BuildFailureOnMetric
import jetbrains.buildServer.configs.kotlin.failureConditions.failOnMetricChange

val windowsGcc = presetBuild("Build_WindowsX64_Gcc", "GCC", "windows-gcc-debug")

// Builds Doxygen: it runs on documentation-only pull requests too.
val windowsClang = presetBuild("Build_WindowsX64_Clang", "Clang", "windows-clang-debug",
        onDraft = true, pathFilter = "") {
    failureConditions {
        failOnMetricChange {
            id = "BUILD_EXT_1"
            metric = BuildFailureOnMetric.MetricType.TEST_COUNT
            threshold = 20
            units = BuildFailureOnMetric.MetricUnit.PERCENTS
            comparison = BuildFailureOnMetric.MetricComparison.LESS
            compareTo = build { buildRule = lastSuccessful() }
        }
        failOnMetricChange {
            id = "BUILD_EXT_2"
            metric = BuildFailureOnMetric.MetricType.ARTIFACT_SIZE
            threshold = 10
            units = BuildFailureOnMetric.MetricUnit.PERCENTS
            comparison = BuildFailureOnMetric.MetricComparison.LESS
            compareTo = build { buildRule = lastSuccessful() }
        }
    }
}

val windowsX64 = Project {
    id = RelativeId("Build_WindowsX64")
    name = "Build Windows x64"

    buildType(windowsGcc)
    buildType(windowsClang)
    buildTypesOrder = arrayListOf(windowsGcc, windowsClang)

    params {
        param("platform", "Windows")
        param("architecture", "amd64")
    }
}
