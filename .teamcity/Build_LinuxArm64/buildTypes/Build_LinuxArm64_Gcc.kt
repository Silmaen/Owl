package Build_LinuxArm64.buildTypes

import jetbrains.buildServer.configs.kotlin.*

object Build_LinuxArm64_Gcc : BuildType({
    templates(_Self.buildTypes.GlobalBuild)
    name = "GCC"

    params {
        param("cmake_preset", "linux-gcc-debug")
    }

    dependencies {
        snapshot(Build_Quality.buildTypes.Build_Quality_CodeStyle) {
            onDependencyFailure = FailureAction.FAIL_TO_START
        }
    }
    
    disableSettings("TRIGGER_2")
})
