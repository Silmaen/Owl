package Build_Quality.buildTypes

import jetbrains.buildServer.configs.kotlin.*

object Build_Quality_SanitizerLeak : BuildType({
    templates(_Self.buildTypes.GlobalBuild)
    name = "Sanitizer leak"

    params {
        param("cmake_preset", "linux-sanitizer-leak")
    }

    dependencies {
        snapshot(Build_Quality_CodeStyle) {
            onDependencyFailure = FailureAction.FAIL_TO_START
        }
    }
})
