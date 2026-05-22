package Package_LinuxArm64.buildTypes

import jetbrains.buildServer.configs.kotlin.*

object Package_LinuxArm64_Engine : BuildType({
    templates(_Self.buildTypes.GlobalBuild)
    name = "Engine"

    params {
        param("cmake_preset", "package-engine-linux")
    }

    dependencies {
        snapshot(Build_Quality.buildTypes.Build_Quality_CodeStyle) {
            onDependencyFailure = FailureAction.FAIL_TO_START
        }
    }
    
    disableSettings("TRIGGER_2")
})
