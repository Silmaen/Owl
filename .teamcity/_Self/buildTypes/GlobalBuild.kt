package _Self.buildTypes

import _Self.vcsRoots.HttpsGithubComSilmaenOwlGitRefsHeadsMain
import jetbrains.buildServer.configs.kotlin.*
import jetbrains.buildServer.configs.kotlin.buildFeatures.XmlReport
import jetbrains.buildServer.configs.kotlin.buildFeatures.commitStatusPublisher
import jetbrains.buildServer.configs.kotlin.buildFeatures.investigationsAutoAssigner
import jetbrains.buildServer.configs.kotlin.buildFeatures.perfmon
import jetbrains.buildServer.configs.kotlin.buildFeatures.xmlReport
import jetbrains.buildServer.configs.kotlin.buildSteps.ScriptBuildStep
import jetbrains.buildServer.configs.kotlin.buildSteps.script
import jetbrains.buildServer.configs.kotlin.triggers.vcs

object GlobalBuild : Template({
    name = "Global Build"
    description = "build and test"

    artifactRules = "%artifact_path%"

    params {
        param("cmake_preset", "")
        checkbox("run_coverage", "false",
                  checked = "true", unchecked = "false")
        checkbox("run_tests", "true",
                  checked = "true", unchecked = "false")
        param("artifact_path", "")
        param("docker_parameters", "")
        param("extra_tc_vars", "")
        checkbox("run_documentation", "false",
                  checked = "true", unchecked = "false")
        checkbox("run_package", "false",
                  checked = "true", unchecked = "false")
        param("release_preset", "")
        checkbox("publish_doc", "false",
                  checked = "true", unchecked = "false")
    }

    vcs {
        root(_Self.vcsRoots.HttpsGithubComSilmaenOwlGitRefsHeadsMain)
    }

    steps {
        script {
            name = "Determine docker"
            id = "RUNNER_24"
            scriptContent = "python3 ci_action.py DefineTeamCityVariables %cmake_preset% %extra_tc_vars%"
        }
        script {
            name = "Define Remote"
            id = "Define_Remote"
            scriptContent = "poetry run python3 ci_action.py ConfigureRemote %cmake_preset% -- --remote_url=%remote_url% --remote_login=%remote_login% --remote_passwd=%remote_passwd%"
            dockerImage = "%docker_image%"
            dockerImagePlatform = ScriptBuildStep.ImagePlatform.Linux
            dockerPull = true
            dockerRunParameters = "%docker_parameters%"
        }
        script {
            name = "Clean"
            id = "Clean_Output_Folder"
            scriptContent = "poetry run python3 ci_action.py Clean %cmake_preset%"
            dockerImage = "%docker_image%"
            dockerImagePlatform = ScriptBuildStep.ImagePlatform.Linux
            dockerPull = true
            dockerRunParameters = "%docker_parameters%"
        }
        script {
            name = "Clean Release"
            id = "Clean_Release"

            conditions {
                doesNotMatch("release_preset", "^${'$'}")
            }
            scriptContent = "poetry run python3 ci_action.py Clean %release_preset%"
            dockerImage = "%docker_image%"
            dockerImagePlatform = ScriptBuildStep.ImagePlatform.Linux
            dockerPull = true
            dockerRunParameters = "%docker_parameters%"
        }
        script {
            name = "Build"
            id = "Build_Release"
            scriptContent = "poetry run python3 ci_action.py Build %cmake_preset%"
            dockerImage = "%docker_image%"
            dockerImagePlatform = ScriptBuildStep.ImagePlatform.Linux
            dockerPull = true
            dockerRunParameters = "%docker_parameters%"
        }
        script {
            name = "Test"
            id = "Test_Release"

            conditions {
                equals("run_tests", "true")
            }
            scriptContent = "poetry run python3 ci_action.py Test %cmake_preset%"
            dockerImage = "%docker_image%"
            dockerImagePlatform = ScriptBuildStep.ImagePlatform.Linux
            dockerPull = true
            dockerRunParameters = "%docker_parameters%"
        }
        script {
            name = "Code Coverage"
            id = "Code_Coverage"

            conditions {
                equals("run_coverage", "true")
            }
            scriptContent = "poetry run python3 ci_action.py Coverage %cmake_preset%"
            dockerImage = "%docker_image%"
            dockerImagePlatform = ScriptBuildStep.ImagePlatform.Linux
            dockerRunParameters = "%docker_parameters%"
        }
        script {
            name = "Build Release"
            id = "Build_Debug"

            conditions {
                doesNotMatch("release_preset", "^${'$'}")
            }
            scriptContent = "poetry run python3 ci_action.py Build %release_preset%"
            dockerImage = "%docker_image%"
            dockerImagePlatform = ScriptBuildStep.ImagePlatform.Linux
            dockerPull = true
            dockerRunParameters = "%docker_parameters%"
        }
        script {
            name = "Test Release"
            id = "Test_Debug"

            conditions {
                doesNotMatch("release_preset", "^${'$'}")
                equals("run_tests", "true")
            }
            scriptContent = "poetry run python3 ci_action.py Test %release_preset%"
            dockerImage = "%docker_image%"
            dockerImagePlatform = ScriptBuildStep.ImagePlatform.Linux
            dockerPull = true
            dockerRunParameters = "%docker_parameters%"
        }
        script {
            name = "Documentation"
            id = "Documentation"

            conditions {
                equals("run_documentation", "true")
            }
            scriptContent = "poetry run python3 ci_action.py Documentation %cmake_preset%"
            dockerImage = "%docker_image%"
            dockerImagePlatform = ScriptBuildStep.ImagePlatform.Linux
            dockerRunParameters = "%docker_parameters%"
        }
        script {
            name = "Package"
            id = "Deploy"

            conditions {
                equals("run_package", "true")
            }
            scriptContent = "poetry run python3 ci_action.py Package %cmake_preset%"
            dockerImage = "%docker_image%"
            dockerImagePlatform = ScriptBuildStep.ImagePlatform.Linux
            dockerPull = true
            dockerRunParameters = "%docker_parameters%"
        }
        script {
            name = "Publish Package"
            id = "Publish"

            conditions {
                equals("run_package", "true")
                equals("teamcity.build.branch.is_default", "true")
            }
            scriptContent = "poetry run python3 ci_action.py PublishPackage %cmake_preset% --url=%deploy_url% --login=%deploy_login% --password=%deploy_passwd%"
            dockerImage = "%docker_image%"
            dockerImagePlatform = ScriptBuildStep.ImagePlatform.Linux
            dockerPull = true
            dockerRunParameters = "%docker_parameters%"
        }
        script {
            name = "Publish Documentation"
            id = "Publish_Doc"

            conditions {
                equals("run_package", "true")
                equals("teamcity.build.branch.is_default", "true")
                equals("publish_doc", "true")
            }
            scriptContent = "poetry run python3 ci_action.py PublishDoc %cmake_preset% --url=%deploy_url% --login=%deploy_login% --password=%deploy_passwd%"
            dockerImage = "%docker_image%"
            dockerImagePlatform = ScriptBuildStep.ImagePlatform.Linux
            dockerPull = true
            dockerRunParameters = "%docker_parameters%"
        }
    }

    triggers {
        vcs {
            id = "TRIGGER_2"
            branchFilter = "%WatchBranchFilter%"
        }
        vcs {
            id = "TRIGGER_1"
            branchFilter = """
                +:main
                +:refs/heads/main
            """.trimIndent()
        }
    }

    features {
        investigationsAutoAssigner {
            id = "InvestigationsAutoAssigner"
        }
        commitStatusPublisher {
            id = "BUILD_EXT_7"
            vcsRootExtId = "${HttpsGithubComSilmaenOwlGitRefsHeadsMain.id}"
            publisher = github {
                githubUrl = "https://api.github.com"
                authType = storedToken {
                    tokenId = "tc_token_id:CID_392f0141078df64b20e1bb01ada5697f:-1:fc63f361-ae0d-4cd9-8feb-dabdd68f74a6"
                }
            }
        }
        xmlReport {
            id = "BUILD_EXT_8"
            reportType = XmlReport.XmlReportType.GOOGLE_TEST
            rules = "output/build/**/test/*_Report.xml"
            verbose = true
        }
        perfmon {
            id = "perfmon"
        }
    }

    requirements {
        contains("teamcity.agent.jvm.os.name", "%platform%", "RQ_18")
        contains("teamcity.agent.jvm.os.arch", "%architecture%", "RQ_3")
    }
    
    disableSettings("RQ_3")
})
