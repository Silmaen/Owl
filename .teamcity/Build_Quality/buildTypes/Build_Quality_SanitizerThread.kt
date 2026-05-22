package Build_Quality.buildTypes

import jetbrains.buildServer.configs.kotlin.*

object Build_Quality_SanitizerThread : BuildType({
    templates(_Self.buildTypes.GlobalBuild)
    name = "Sanitizer thread"

    params {
        param("cmake_preset", "linux-sanitizer-thread")
    }

    dependencies {
        snapshot(Build_Quality_CodeStyle) {
            onDependencyFailure = FailureAction.FAIL_TO_START
        }
    }
})
