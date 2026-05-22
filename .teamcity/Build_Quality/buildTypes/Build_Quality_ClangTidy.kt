package Build_Quality.buildTypes

import jetbrains.buildServer.configs.kotlin.*
import jetbrains.buildServer.configs.kotlin.buildFeatures.perfmon

object Build_Quality_ClangTidy : BuildType({
    templates(_Self.buildTypes.GlobalBuild)
    name = "Clang-Tidy"

    params {
        param("cmake_preset", "linux-clang-tidy")
        param("platform", "Linux")
    }

    features {
        perfmon {
            id = "perfmon"
        }
    }

    dependencies {
        snapshot(Build_Quality_CodeStyle) {
            onDependencyFailure = FailureAction.FAIL_TO_START
        }
    }
})
