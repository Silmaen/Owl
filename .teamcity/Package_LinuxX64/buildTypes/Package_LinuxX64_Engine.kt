package Package_LinuxX64.buildTypes

import jetbrains.buildServer.configs.kotlin.*
import jetbrains.buildServer.configs.kotlin.buildFeatures.perfmon

object Package_LinuxX64_Engine : BuildType({
    templates(_Self.buildTypes.GlobalBuild)
    name = "Engine"

    params {
        param("cmake_preset", "package-engine-linux")
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
    
    disableSettings("TRIGGER_2")
})
