package _Self.buildTypes

import _Self.vcsRoots.HttpsGithubComSilmaenOwlGitRefsHeadsMain
import jetbrains.buildServer.configs.kotlin.*
import jetbrains.buildServer.configs.kotlin.buildFeatures.XmlReport
import jetbrains.buildServer.configs.kotlin.buildFeatures.commitStatusPublisher
import jetbrains.buildServer.configs.kotlin.buildFeatures.investigationsAutoAssigner
import jetbrains.buildServer.configs.kotlin.buildFeatures.perfmon
import jetbrains.buildServer.configs.kotlin.buildFeatures.pullRequests
import jetbrains.buildServer.configs.kotlin.buildFeatures.xmlReport
import jetbrains.buildServer.configs.kotlin.buildSteps.ScriptBuildStep
import jetbrains.buildServer.configs.kotlin.buildSteps.script
import jetbrains.buildServer.configs.kotlin.triggers.vcs

// Helper: configure the current ScriptBuildStep as a Dockerized
// `poetry run python3 ci_action.py <action> <preset>` invocation. Use inside a
// `script { ... }` block, then add `conditions { ... }` if needed.
// The first step in the pipeline ("Determine docker") is native (no Docker) — it
// runs `DefineTeamCityVariables` which sets `docker_image` for subsequent steps,
// so it cannot itself run in Docker. That step is written literally below.
private fun ScriptBuildStep.ciAction(
    action: String,
    stepId: String,
    displayName: String = action,
    preset: String = "%cmake_preset%",
    extraArgs: String = "",
) {
    name = displayName
    id = stepId
    scriptContent = "poetry run python3 ci_action.py $action $preset" +
        if (extraArgs.isNotEmpty()) " $extraArgs" else ""
    dockerImage = "%docker_image%"
    dockerImagePlatform = ScriptBuildStep.ImagePlatform.Linux
    dockerPull = true
    dockerRunParameters = "%docker_parameters%"
}

object GlobalBuild : Template({
    name = "Global Build"
    description = "build and test"

    artifactRules = "%artifact_path%"

    params {
        // Most of these are runtime-overwritten by ci_action.py
        // DefineTeamCityVariables (read from the CMake preset's vendor.silmaen
        // block). They're declared here so TC can reference them in script
        // conditions before the first step has run.
        param("cmake_preset", "")
        checkbox("run_coverage", "false", checked = "true", unchecked = "false")
        checkbox("run_tests", "true", checked = "true", unchecked = "false")
        param("artifact_path", "")
        param("docker_parameters", "")
        param("extra_tc_vars", "")
        checkbox("run_documentation", "false", checked = "true", unchecked = "false")
        checkbox("run_package", "false", checked = "true", unchecked = "false")
        param("release_preset", "")
        checkbox("publish_doc", "false", checked = "true", unchecked = "false")

        // teamcity-github-bridge plugin parameters. Presence of all three
        // enables the plugin's StartBuildPrecondition (drafts are held) and
        // the ready_for_review retrigger. To OPT OUT a specific buildType
        // (i.e. keep running it on drafts), override `tcgh.ignoreDrafts` to
        // "false" on that buildType. See Build.kt::allowDraftPR().
        // Plugin docs: /data/sources/Sources/IT/teamcity-github/doc/configuration.md
        param("tcgh.ignoreDrafts", "true")
        param("tcgh.github.repo", "Silmaen/Owl")
        param("tcgh.github.connectionId", "CID_392f0141078df64b20e1bb01ada5697f")
    }

    vcs {
        root(_Self.vcsRoots.HttpsGithubComSilmaenOwlGitRefsHeadsMain)
    }

    steps {
        // Native step — runs on the agent host (no Docker) to populate
        // docker_image and other params for the rest of the pipeline.
        script {
            name = "Determine docker"
            id = "RUNNER_24"
            scriptContent =
                "python3 ci_action.py DefineTeamCityVariables %cmake_preset% %extra_tc_vars%"
        }

        script {
            ciAction("ConfigureRemote", "Define_Remote", displayName = "Define Remote",
                extraArgs = "-- --remote_url=%remote_url% --remote_login=%remote_login% --remote_passwd=%remote_passwd%")
        }

        script {
            ciAction("Clean", "Clean_Output_Folder", displayName = "Clean")
        }

        script {
            ciAction("Clean", "Clean_Release", displayName = "Clean Release",
                preset = "%release_preset%")
            conditions {
                doesNotMatch("release_preset", "^${'$'}")
            }
        }

        script {
            ciAction("Build", "Build_Release", displayName = "Build")
        }

        script {
            ciAction("Test", "Test_Release", displayName = "Test")
            conditions {
                equals("run_tests", "true")
            }
        }

        script {
            ciAction("Coverage", "Code_Coverage", displayName = "Code Coverage")
            conditions {
                equals("run_coverage", "true")
            }
        }

        script {
            ciAction("Build", "Build_Debug", displayName = "Build Release",
                preset = "%release_preset%")
            conditions {
                doesNotMatch("release_preset", "^${'$'}")
            }
        }

        script {
            ciAction("Test", "Test_Debug", displayName = "Test Release",
                preset = "%release_preset%")
            conditions {
                doesNotMatch("release_preset", "^${'$'}")
                equals("run_tests", "true")
            }
        }

        script {
            ciAction("Documentation", "Documentation")
            conditions {
                equals("run_documentation", "true")
            }
        }

        script {
            ciAction("Package", "Deploy", displayName = "Package")
            conditions {
                equals("run_package", "true")
            }
        }

        script {
            ciAction("PublishPackage", "Publish", displayName = "Publish Package",
                extraArgs = "--url=%deploy_url% --login=%deploy_login% --password=%deploy_passwd%")
            conditions {
                equals("run_package", "true")
                equals("teamcity.build.branch.is_default", "true")
            }
        }

        script {
            ciAction("PublishDoc", "Publish_Doc", displayName = "Publish Documentation",
                extraArgs = "--url=%deploy_url% --login=%deploy_login% --password=%deploy_passwd%")
            conditions {
                equals("run_package", "true")
                equals("teamcity.build.branch.is_default", "true")
                equals("publish_doc", "true")
            }
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
        pullRequests {
            id = "PULL_REQUESTS"
            vcsRootExtId = "${HttpsGithubComSilmaenOwlGitRefsHeadsMain.id}"
            provider = github {
                authType = storedToken {
                    tokenId = "tc_token_id:CID_392f0141078df64b20e1bb01ada5697f:-1:fc63f361-ae0d-4cd9-8feb-dabdd68f74a6"
                }
                filterTargetBranch = "+:main"
                // Draft suppression is handled by teamcity-github-bridge:
                // when tcgh.ignoreDrafts=true on this build, its
                // StartBuildPrecondition holds the build for draft PRs
                // and re-enqueues on ready_for_review.
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

    dependencies {
        snapshot(Build.QualityCodeStyle) {
            onDependencyFailure = FailureAction.FAIL_TO_START
        }
    }

    // We have no native ARM agents; ARM64 builds run via Docker emulation on
    // amd64 hosts. Disabling RQ_3 lets ARM jobs land on any-arch agent.
    disableSettings("RQ_3")
})
