import jetbrains.buildServer.configs.kotlin.*
import jetbrains.buildServer.configs.kotlin.failureConditions.BuildFailureOnMetric
import jetbrains.buildServer.configs.kotlin.failureConditions.failOnMetricChange

// GCC parity on `main` only: a pull request gets its verdict from Clang. Release only, as on Linux.
val windowsGcc = presetBuild("Build_WindowsX64_Gcc", "GCC", "windows-gcc-release", onPullRequest = false)

val windowsClang = presetBuild("Build_WindowsX64_Clang", "Clang", "windows-clang-debug", onDraft = true) {
    failureConditions {
        failOnMetricChange {
            id = "BUILD_EXT_1"
            metric = BuildFailureOnMetric.MetricType.TEST_COUNT
            threshold = 20
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
        // The agent builds natively (no image): its ccache, when installed, gets the settings Linux passes to Docker.
        param("env.CCACHE_DIR", "%teamcity.agent.home.dir%/user/cache_dir/ccache")
        param("env.CCACHE_MAXSIZE", "20G")
        param("env.CCACHE_COMPRESS", "1")
    }
}
