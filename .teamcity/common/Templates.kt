import jetbrains.buildServer.configs.kotlin.*
import jetbrains.buildServer.configs.kotlin.buildFeatures.XmlReport
import jetbrains.buildServer.configs.kotlin.buildFeatures.investigationsAutoAssigner
import jetbrains.buildServer.configs.kotlin.buildFeatures.perfmon
import jetbrains.buildServer.configs.kotlin.buildFeatures.xmlReport
import jetbrains.buildServer.configs.kotlin.buildSteps.ScriptBuildStep
import jetbrains.buildServer.configs.kotlin.buildSteps.script

/*
 * Global Build: configure, build, test, coverage, documentation and package steps, each one
 * gated by the parameters `DefineTeamCityVariables` sets from the preset at run time.
 * Tool Build: the two steps of a tool that only reads the sources (Code Style).
 * Their ids are the ones the server already knows; the dependency chain lives in the
 * factories, not here.
 */

val globalBuild = Template {
    id = RelativeId("GlobalBuild")
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
        // Secrets reach ci_action.py through its environment, never its command line
        // (visible to `ps`, echoed in the log). The referenced parameters are password
        // parameters on the server, so TeamCity masks their value everywhere.
        param("env.OWL_REMOTE_PASSWORD", "%remote_passwd%")
        param("env.OWL_DEPLOY_PASSWORD", "%deploy_passwd%")
        // Third-party provider for this build (read by cmake/BaseConfig.cmake). Run a configuration with
        // `conan` to try the Conan migration on its agent before switching the default.
        select("env.OWL_DEPENDENCY_PROVIDER", "depmanager", label = "Dependency provider",
            options = listOf("depmanager", "conan"))
        checkbox("publish_doc", "false", checked = "true", unchecked = "false")

        // teamcity-github-bridge: opt-in is the BRIDGE_GITHUB build feature
        // below. Project-level repo + connectionId live on _Self.Project.
        // Default here: no draft PRs, no doc-only PRs, no diff annotations.
        // Overridden per BT via bridgeOverride() (see BridgeHelpers.kt).
    }

    vcs {
        root(githubOwl)
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
                extraArgs = "-- --remote_url=%remote_url% --remote_login=%remote_login%")
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
                extraArgs = "--url=%deploy_url% --login=%deploy_login%")
            conditions {
                equals("run_package", "true")
                equals("teamcity.build.branch.is_default", "true")
            }
        }

        script {
            ciAction("PublishDoc", "Publish_Doc", displayName = "Publish Documentation",
                extraArgs = "--url=%deploy_url% --login=%deploy_login%")
            conditions {
                equals("run_package", "true")
                equals("teamcity.build.branch.is_default", "true")
                equals("publish_doc", "true")
            }
        }
    }

    triggers {
        mainBranchOnly()
    }

    features {
        investigationsAutoAssigner {
            id = "InvestigationsAutoAssigner"
        }
        // teamcity-github-bridge opt-in (v1.5.0+): the BT participates as
        // soon as this feature is attached. Default for this template: run on
        // ready PRs, but not on draft ones and not on a PR that changes only
        // documentation. The draft-friendly BTs and the two that annotate the
        // diff override it via bridgeOverride() — see BridgeHelpers.kt.
        // Everything PR-related (commitStatusPublisher, pullRequests
        // bundled feature) was retired in earlier passes; the plugin's
        // Check Runs + PrParameterProvider are the single sources of truth.
        githubBridge()
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


    // We have no native ARM agents; ARM64 builds run via Docker emulation on
    // amd64 hosts. Disabling RQ_3 lets ARM jobs land on any-arch agent.
    disableSettings("RQ_3")
}

val toolBuild = Template {
    id = RelativeId("CodeSylingCheck")
    // Keep the original (typo'd) ID so existing build history and references
    // in TeamCity storage stay bound. The Kotlin object name is fixed for
    // hygiene; the display name was also corrected.
    name = "Code Styling Check"
    description = "Check the code Style"

    params {
        param("docker_parameters", "")
        param("extra_tc_vars", "")

        // teamcity-github-bridge: opt-in is the BRIDGE_GITHUB feature below.
        // Project-level repo + connectionId live on _Self.Project.
        // CodeStyle is part of the draft-friendly fast-feedback subset so it
        // runs on draft PRs too.
    }

    vcs {
        root(githubOwl)
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
        mainBranchOnly()
    }

    features {
        investigationsAutoAssigner {
            id = "InvestigationsAutoAssigner"
        }
        // teamcity-github-bridge opt-in. CodeStyle is in the draft-friendly
        // subset → runOnDraftPr. It also runs on a doc-only PR (no path
        // filter: codespell and the markdown checks are exactly what such a
        // PR changes) and annotates the diff — every finding this gate reports
        // is emitted as a compiler-style diagnostic by ci/actions/code_style.py
        // precisely so the plugin can pin it to its line in the PR.
        // See GlobalBuild.kt for the rationale on retiring
        // commitStatusPublisher + bundled pullRequests.
        githubBridge()
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
}
