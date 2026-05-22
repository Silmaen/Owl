package Build_WindowsX64.buildTypes

import jetbrains.buildServer.configs.kotlin.*
import jetbrains.buildServer.configs.kotlin.buildFeatures.perfmon

object Build_WindowsX64_Gcc : BuildType({
    templates(_Self.buildTypes.GlobalBuild)
    name = "GCC"

    params {
        param("cmake_preset", "windows-gcc-debug")
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
