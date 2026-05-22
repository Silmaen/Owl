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
        // Whether this buildType runs on draft PRs. Default = no.
        // BuildTypes wanting to opt in (Linux x64 Clang, Windows x64 Clang,
        // SanitizerAddress) override to "true". Used by the DRAFT_PR_GUARD
        // first step, which queries the GitHub API to determine PR state
        // (the TC pullRequests plugin doesn't expose isDraft natively).
        checkbox("allow_draft_pr", "false", checked = "true", unchecked = "false")
        // Flag flipped to "true" by the DRAFT_PR_GUARD step when the build
        // should be skipped (draft PR, this buildType not opted in). Every
        // other step is gated on this being "false", so they all skip and
        // the build finishes SUCCESS in a few seconds. Avoids the cancelled
        // status that buildStop would produce (red on GitHub commits).
        checkbox("skip_pipeline", "false", checked = "true", unchecked = "false")
    }

    vcs {
        root(_Self.vcsRoots.HttpsGithubComSilmaenOwlGitRefsHeadsMain)
    }

    steps {
        // Draft PR guard — abort the build early if PR is in draft state and
        // this buildType is not opted in. Native (no Docker) for fast skip.
        // No-ops for non-PR builds (main) since pr is empty.
        script {
            name = "Draft PR guard"
            id = "DRAFT_PR_GUARD"
            scriptContent =
                "python3 ci_action.py CheckDraft %cmake_preset% -- " +
                "--pr=%teamcity.pullRequest.number% " +
                "--repo=Silmaen/Owl " +
                "--token=%github_access_token% " +
                "--allow-draft=%allow_draft_pr% " +
                "--build-number=%build.number%"
        }

        // Native step — runs on the agent host (no Docker) to populate
        // docker_image and other params for the rest of the pipeline.
        script {
            name = "Determine docker"
            id = "RUNNER_24"
            conditions { equals("skip_pipeline", "false") }
            scriptContent =
                "python3 ci_action.py DefineTeamCityVariables %cmake_preset% %extra_tc_vars%"
        }

        script {
            ciAction("ConfigureRemote", "Define_Remote", displayName = "Define Remote",
                extraArgs = "-- --remote_url=%remote_url% --remote_login=%remote_login% --remote_passwd=%remote_passwd%")
            conditions { equals("skip_pipeline", "false") }
        }

        script {
            ciAction("Clean", "Clean_Output_Folder", displayName = "Clean")
            conditions { equals("skip_pipeline", "false") }
        }

        script {
            ciAction("Clean", "Clean_Release", displayName = "Clean Release",
                preset = "%release_preset%")
            conditions {
                equals("skip_pipeline", "false")
                doesNotMatch("release_preset", "^${'$'}")
            }
        }

        script {
            ciAction("Build", "Build_Release", displayName = "Build")
            conditions { equals("skip_pipeline", "false") }
        }

        script {
            ciAction("Test", "Test_Release", displayName = "Test")
            conditions {
                equals("skip_pipeline", "false")
                equals("run_tests", "true")
            }
        }

        script {
            ciAction("Coverage", "Code_Coverage", displayName = "Code Coverage")
            conditions {
                equals("skip_pipeline", "false")
                equals("run_coverage", "true")
            }
        }

        script {
            ciAction("Build", "Build_Debug", displayName = "Build Release",
                preset = "%release_preset%")
            conditions {
                equals("skip_pipeline", "false")
                doesNotMatch("release_preset", "^${'$'}")
            }
        }

        script {
            ciAction("Test", "Test_Debug", displayName = "Test Release",
                preset = "%release_preset%")
            conditions {
                equals("skip_pipeline", "false")
                doesNotMatch("release_preset", "^${'$'}")
                equals("run_tests", "true")
            }
        }

        script {
            ciAction("Documentation", "Documentation")
            conditions {
                equals("skip_pipeline", "false")
                equals("run_documentation", "true")
            }
        }

        script {
            ciAction("Package", "Deploy", displayName = "Package")
            conditions {
                equals("skip_pipeline", "false")
                equals("run_package", "true")
            }
        }

        script {
            ciAction("PublishPackage", "Publish", displayName = "Publish Package",
                extraArgs = "--url=%deploy_url% --login=%deploy_login% --password=%deploy_passwd%")
            conditions {
                equals("skip_pipeline", "false")
                equals("run_package", "true")
                equals("teamcity.build.branch.is_default", "true")
            }
        }

        script {
            ciAction("PublishDoc", "Publish_Doc", displayName = "Publish Documentation",
                extraArgs = "--url=%deploy_url% --login=%deploy_login% --password=%deploy_passwd%")
            conditions {
                equals("skip_pipeline", "false")
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
                // ignoreDrafts is currently not effective with GitHub App
                // auth in TC 2026.1 — draft filtering is done in the
                // DRAFT_PR_GUARD step instead (calls GitHub API directly).
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
