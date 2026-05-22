package Package_WindowsX64.buildTypes

import jetbrains.buildServer.configs.kotlin.*

object Package_WindowsX64_Engine : BuildType({
    templates(_Self.buildTypes.GlobalBuild)
    name = "Engine"

    params {
        param("cmake_preset", "package-engine-windows")
    }

    dependencies {
        snapshot(Build_Quality.buildTypes.Build_Quality_CodeStyle) {
            onDependencyFailure = FailureAction.FAIL_TO_START
        }
    }
    
    disableSettings("TRIGGER_2")
})
