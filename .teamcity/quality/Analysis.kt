import jetbrains.buildServer.configs.kotlin.*
import jetbrains.buildServer.configs.kotlin.buildSteps.ScriptBuildStep
import jetbrains.buildServer.configs.kotlin.buildSteps.script

private val analysisGates = listOf(sanitizerAddress, sanitizerLeak, sanitizerThread, sanitizerUndefinedBehavior)

val clangTidy = analysisBuild("Build_Quality_ClangTidy", "Clang-Tidy", "tidy", analysisGates)
val staticAnalyzer = analysisBuild("Build_Quality_ClangAnalyzer", "Static Analyzer", "analyzer", analysisGates)

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
    disableSettings("Build_Release", "Test_Release")

    features {
        githubBridge()
    }

    dependencies {
        after(*analysisGates.toTypedArray())
    }
}

val analysis = Project {
    id = RelativeId("Analysis")
    name = "Analysis"

    buildType(clangTidy)
    buildType(staticAnalyzer)
    buildType(includeCheck)
    buildTypesOrder = arrayListOf(clangTidy, staticAnalyzer, includeCheck)

    params {
        param("platform", "Linux")
        param("architecture", "amd64")
    }
}
