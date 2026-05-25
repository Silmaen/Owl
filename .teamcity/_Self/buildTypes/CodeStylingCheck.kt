package _Self.buildTypes

import _Self.GITHUB_CONNECTION_ID
import _Self.vcsRoots.HttpsGithubComSilmaenOwlGitRefsHeadsMain
import jetbrains.buildServer.configs.kotlin.*
import jetbrains.buildServer.configs.kotlin.buildFeatures.XmlReport
import jetbrains.buildServer.configs.kotlin.buildFeatures.investigationsAutoAssigner
import jetbrains.buildServer.configs.kotlin.buildFeatures.perfmon
import jetbrains.buildServer.configs.kotlin.buildFeatures.xmlReport
import jetbrains.buildServer.configs.kotlin.buildSteps.ScriptBuildStep
import jetbrains.buildServer.configs.kotlin.buildSteps.script
import jetbrains.buildServer.configs.kotlin.triggers.vcs

object CodeStylingCheck : Template({
    // Keep the original (typo'd) ID so existing build history and references
    // in TeamCity storage stay bound. The Kotlin object name is fixed for
    // hygiene; the display name was also corrected.
    id("CodeSylingCheck")
    name = "Code Styling Check"
    description = "Check the code Style"

    params {
        param("docker_parameters", "")
        param("extra_tc_vars", "")

        // teamcity-github-bridge opt-in. ignoreDrafts="false" keeps CodeStyle
        // running on draft PRs (it's part of the fast-feedback subset), while
        // the repo + connectionId pair is enough for v0.7.0's
        // BuildStatusCheckRunPublisher to publish lifecycle Check Runs.
        param("teamcity.github.bridge.ignoreDrafts", "false")
        param("teamcity.github.bridge.repo", "Silmaen/Owl")
        param("teamcity.github.bridge.connectionId", GITHUB_CONNECTION_ID)
    }

    vcs {
        root(_Self.vcsRoots.HttpsGithubComSilmaenOwlGitRefsHeadsMain)
    }

    steps {
        script {
            name = "Determine Docker"
            id = "Determine_Docker"
            scriptContent = "python3 -u ci_action.py DefineTeamCityVariables %cmake_preset% %extra_tc_vars%"
        }
        script {
            name = "Checking Code"
            id = "Checking_Code"
            scriptContent = "poetry run python3 -u ci_action.py CodeStyle %cmake_preset%"
            dockerImage = "%docker_image%"
            dockerImagePlatform = ScriptBuildStep.ImagePlatform.Linux
            dockerPull = true
            dockerRunParameters = "%docker_parameters%"
        }
    }

    triggers {
        vcs {
            id = "TRIGGER_3"
            branchFilter = "%WatchBranchFilter%"
        }
        vcs {
            id = "TRIGGER_4"
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
        // Everything PR-related is delegated to teamcity-github-bridge —
        // see GlobalBuild.kt for the rationale for retiring both
        // commitStatusPublisher and the bundled pullRequests feature.
        xmlReport {
            id = "BUILD_EXT_4"
            reportType = XmlReport.XmlReportType.GOOGLE_TEST
            rules = "output/build/**/test/*_Report.xml"
        }
        perfmon {
            id = "perfmon"
        }
    }

    requirements {
        contains("teamcity.agent.jvm.os.name", "%platform%", "RQ_1")
    }
})
