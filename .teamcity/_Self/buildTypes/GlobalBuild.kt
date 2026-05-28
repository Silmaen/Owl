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

        // teamcity-github-bridge: opt-in is the BRIDGE_GITHUB build feature
        // below. Project-level repo + connectionId live on _Self.Project.
        // Default here: triggerOnPrDraft=false → draft PRs do NOT trigger
        // builds for this template's children. Override per BT via
        // Build.kt::allowDraftPR() (which disables this feature and re-adds
        // it with triggerOnPrDraft=true).
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
            id = "TRIGGER_1"
            branchFilter = """
                +:main
                +:refs/heads/main
            """.trimIndent()
        }
        // TRIGGER_2 (VCS, feature-branch + PR refs) intentionally removed.
        // PR refs are enqueued by teamcity-github-bridge via the
        // BRIDGE_GITHUB feature on pull_request.opened / synchronize /
        // ready_for_review events. The "Plugin-event path" pattern (v1.4.0+)
        // eliminates the double-trigger we had when both VCS and the plugin
        // were enqueueing on every PR push.
    }

    features {
        investigationsAutoAssigner {
            id = "InvestigationsAutoAssigner"
        }
        // teamcity-github-bridge opt-in (v1.5.0+): the BT participates as
        // soon as this feature is attached. Default for this template:
        // run on non-PR branches + ready PRs, but NOT on draft PRs. The
        // 4 draft-friendly BTs override this via Build.kt::allowDraftPR().
        // Everything PR-related (commitStatusPublisher, pullRequests
        // bundled feature) was retired in earlier passes; the plugin's
        // Check Runs + PrParameterProvider are the single sources of truth.
        feature {
            id = "BRIDGE_GITHUB"
            type = "github-bridge"
            param("triggerOnPrDraft", "false")
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
