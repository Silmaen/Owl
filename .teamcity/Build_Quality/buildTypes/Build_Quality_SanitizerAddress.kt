package Build_Quality.buildTypes

import jetbrains.buildServer.configs.kotlin.*

object Build_Quality_SanitizerAddress : BuildType({
    templates(_Self.buildTypes.GlobalBuild)
    name = "Sanitizer Address"

    params {
        param("cmake_preset", "linux-sanitizer-address")
    }

    dependencies {
        snapshot(Build_Quality_CodeStyle) {
            onDependencyFailure = FailureAction.FAIL_TO_START
        }
    }
})
