package Build_Quality.buildTypes

import jetbrains.buildServer.configs.kotlin.*

object Build_Quality_SanitizerUndefinedBehavior : BuildType({
    templates(_Self.buildTypes.GlobalBuild)
    name = "Sanitizer undefined behavior"

    params {
        param("cmake_preset", "linux-sanitizer-undefined-behavior")
        param("platform", "Linux")
    }

    dependencies {
        snapshot(Build_Quality_CodeStyle) {
            onDependencyFailure = FailureAction.FAIL_TO_START
        }
    }
    
    disableSettings("TRIGGER_2")
})
