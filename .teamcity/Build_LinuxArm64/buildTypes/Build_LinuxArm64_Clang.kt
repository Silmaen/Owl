package Build_LinuxArm64.buildTypes

import jetbrains.buildServer.configs.kotlin.*

object Build_LinuxArm64_Clang : BuildType({
    templates(_Self.buildTypes.GlobalBuild)
    name = "Clang"

    params {
        param("cmake_preset", "linux-clang-debug")
    }

    dependencies {
        snapshot(Build_Quality.buildTypes.Build_Quality_CodeStyle) {
            onDependencyFailure = FailureAction.FAIL_TO_START
        }
    }
})
