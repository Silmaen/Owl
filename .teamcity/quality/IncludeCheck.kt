import jetbrains.buildServer.configs.kotlin.*
import jetbrains.buildServer.configs.kotlin.buildSteps.ScriptBuildStep
import jetbrains.buildServer.configs.kotlin.buildSteps.script

/**
 * Every header and source compiled alone, without PCH, against libc++ with its transitive
 * includes removed (cmake/IncludeCheck.cmake): catches the missing standard includes a
 * newer standard library would break on, before the Windows agents do.
 */
val includeCheck = BuildType {
    id = RelativeId("Build_Quality_IncludeCheck")
    name = "Include Check"
    templates(globalBuild)

    params {
        param("cmake_preset", "linux-include-check")
        param("platform", "Linux")
        param("architecture", "amd64")
    }

    steps {
        script {
            name = "Include Check"
            id = "Include_Check"
            scriptContent = "poetry run python3 ci_action.py IncludeCheck %cmake_preset%"
            dockerImage = "%docker_image%"
            dockerImagePlatform = ScriptBuildStep.ImagePlatform.Linux
            dockerPull = true
            dockerRunParameters = "%docker_parameters%"
        }
    }
    disableSettings("Build_Preset", "Test_Preset")

    features {
        githubBridge()
    }

    // Level 1, in parallel with Code Style: it waits for nothing.
}
