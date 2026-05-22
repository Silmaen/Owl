package Build_WindowsX64.buildTypes

import jetbrains.buildServer.configs.kotlin.*
import jetbrains.buildServer.configs.kotlin.buildFeatures.perfmon
import jetbrains.buildServer.configs.kotlin.failureConditions.BuildFailureOnMetric
import jetbrains.buildServer.configs.kotlin.failureConditions.failOnMetricChange

object Build_WindowsX64_Clang : BuildType({
    templates(_Self.buildTypes.GlobalBuild)
    name = "Clang"

    params {
        param("cmake_preset", "windows-clang-debug")
    }

    failureConditions {
        failOnMetricChange {
            id = "BUILD_EXT_1"
            metric = BuildFailureOnMetric.MetricType.TEST_COUNT
            threshold = 20
            units = BuildFailureOnMetric.MetricUnit.PERCENTS
            comparison = BuildFailureOnMetric.MetricComparison.LESS
            compareTo = build {
                buildRule = lastSuccessful()
            }
        }
        failOnMetricChange {
            id = "BUILD_EXT_2"
            metric = BuildFailureOnMetric.MetricType.ARTIFACT_SIZE
            threshold = 10
            units = BuildFailureOnMetric.MetricUnit.PERCENTS
            comparison = BuildFailureOnMetric.MetricComparison.LESS
            compareTo = build {
                buildRule = lastSuccessful()
            }
        }
    }

    features {
        perfmon {
            id = "perfmon"
        }
    }

    dependencies {
        snapshot(Build_Quality.buildTypes.Build_Quality_CodeStyle) {
            onDependencyFailure = FailureAction.FAIL_TO_START
        }
    }
})
