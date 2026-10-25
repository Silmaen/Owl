import jetbrains.buildServer.configs.kotlin.*
import jetbrains.buildServer.configs.kotlin.failureConditions.BuildFailureOnMetric
import jetbrains.buildServer.configs.kotlin.failureConditions.failOnMetricChange

// The single Windows agent is the bottleneck: one release tree, on ready pull requests only (drafts get the Linux
// verdict), tested and, on `main`, packaged for the Windows x64 package to publish. No coverage: `gcovr.cfg` leaves
// the Windows-only code out, Linux measures the rest.
val windowsClang = presetBuild("Build_WindowsX64_Clang", "Clang", "windows-clang-release") {
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

    buildType(windowsClang)

    params {
        text("platform", "Windows", display = ParameterDisplay.HIDDEN, allowEmpty = true)
        text("architecture", "amd64", display = ParameterDisplay.HIDDEN, allowEmpty = true)
        // The agent builds natively (no image): its ccache, when installed, gets the settings Linux passes to Docker.
        text("env.CCACHE_DIR", "%teamcity.agent.home.dir%/user/cache_dir/ccache", display = ParameterDisplay.HIDDEN, allowEmpty = true)
        text("env.CCACHE_MAXSIZE", "20G", display = ParameterDisplay.HIDDEN, allowEmpty = true)
        text("env.CCACHE_COMPRESS", "1", display = ParameterDisplay.HIDDEN, allowEmpty = true)
    }
}
