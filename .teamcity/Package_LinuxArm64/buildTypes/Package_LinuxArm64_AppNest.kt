package Package_LinuxArm64.buildTypes

import jetbrains.buildServer.configs.kotlin.*

object Package_LinuxArm64_AppNest : BuildType({
    templates(_Self.buildTypes.GlobalBuild)
    name = "App - Nest"

    params {
        param("cmake_preset", "package-app-nest-linux")
    }

    dependencies {
        snapshot(Build_Quality.buildTypes.Build_Quality_CodeStyle) {
            onDependencyFailure = FailureAction.FAIL_TO_START
        }
    }
    
    disableSettings("TRIGGER_2")
})
