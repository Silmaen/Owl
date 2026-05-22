package Build_LinuxX64.buildTypes

import jetbrains.buildServer.configs.kotlin.*
import jetbrains.buildServer.configs.kotlin.buildFeatures.perfmon

object Build_LinuxX64_Clang : BuildType({
    templates(_Self.buildTypes.GlobalBuild)
    name = "Clang"

    params {
        param("cmake_preset", "linux-clang-debug")
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
