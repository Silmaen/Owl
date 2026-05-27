package _Self.buildTypes

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

        // teamcity-github-bridge: opt-in is the BRIDGE_GITHUB feature below.
        // Project-level repo + connectionId live on _Self.Project.
        // CodeStyle is part of the draft-friendly fast-feedback subset →
        // triggerOnPrDraft=true so it runs on draft PRs too.
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
        // teamcity-github-bridge opt-in. CodeStyle is in the draft-friendly
        // subset → triggerOnPrDraft=true. See GlobalBuild.kt for the
        // rationale on retiring commitStatusPublisher + bundled pullRequests.
        feature {
            id = "BRIDGE_GITHUB"
            type = "github-bridge"
            param("triggerOnPrDraft", "true")
        }
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
