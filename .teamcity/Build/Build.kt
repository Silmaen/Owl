package Build

import _Self.bridgeOverride
import _Self.buildTypes.CodeStylingCheck
import _Self.buildTypes.GlobalBuild
import _Self.buildTypes.ciAction
import _Self.skipAutoPRs
import jetbrains.buildServer.configs.kotlin.*
import jetbrains.buildServer.configs.kotlin.buildSteps.ScriptBuildStep
import jetbrains.buildServer.configs.kotlin.buildSteps.script
import jetbrains.buildServer.configs.kotlin.failureConditions.BuildFailureOnMetric
import jetbrains.buildServer.configs.kotlin.failureConditions.failOnMetricChange

// ─────────────────────────────────────────────────────────────────────────────
//  Build & Test top-level project
//
//  Generates 13 buildTypes across 4 sub-projects:
//    • Linux x64 / Linux ARM64 / Windows x64  (Clang + GCC each, 6 total)
//    • Quality (CodeStyle + ClangTidy + IncludeCheck + 4 sanitizers, 7 total —
//      CodeStyle uses the CodeStylingCheck template, the others GlobalBuild)
//
//  All IDs are pinned explicitly via id("...") to preserve TC build history.
// ─────────────────────────────────────────────────────────────────────────────

// ── Standard (cross-OS) builds ───────────────────────────────────────────────

private data class StdVariant(
    val idSuffix: String,     // e.g. "Clang", "Gcc"
    val displayName: String,  // e.g. "Clang", "GCC"
    val compilerSlug: String, // e.g. "clang", "gcc" (for preset name)
)

private val stdVariants = listOf(
    StdVariant("Clang", "Clang", "clang"),
    StdVariant("Gcc", "GCC", "gcc"),
)

private fun stdBuildType(
    projectId: String,
    osSlug: String,
    variant: StdVariant,
    mainOnlyAutoTrigger: Boolean,
    extraConfig: BuildType.() -> Unit = {},
) = BuildType({
    id("${projectId}_${variant.idSuffix}")
    name = variant.displayName
    templates(GlobalBuild)
    params {
        param("cmake_preset", "$osSlug-${variant.compilerSlug}-debug")
    }
    if (mainOnlyAutoTrigger) {
        skipAutoPRs()
    }
    extraConfig()
})

// `mainOnlyAutoTriggerFor` lists the variant.idSuffix values that
// should call skipAutoPRs() — i.e. auto-run on main only, never on PR
// refs. Default = "Gcc" only, which matches the Linux convention
// (GCC is too slow to run on every push, so we keep it on main only).
// Windows historically runs both compilers on every branch, so it
// overrides to emptySet().
private fun stdPlatform(
    projectId: String,
    displayName: String,
    osSlug: String,
    platformParam: String,
    archParam: String,
    extraProjectParams: ParametrizedWithType.() -> Unit = {},
    perVariantConfig: Map<String, BuildType.() -> Unit> = emptyMap(),
    mainOnlyAutoTriggerFor: Set<String> = setOf("Gcc"),
) = Project({
    id(projectId)
    name = displayName

    stdVariants.forEach { variant ->
        buildType(
            stdBuildType(
                projectId, osSlug, variant,
                mainOnlyAutoTrigger = variant.idSuffix in mainOnlyAutoTriggerFor,
                extraConfig = perVariantConfig[variant.idSuffix] ?: {},
            )
        )
    }

    params {
        param("platform", platformParam)
        param("architecture", archParam)
        extraProjectParams()
    }
})

private val linuxX64 = stdPlatform(
    projectId = "Build_LinuxX64",
    displayName = "Linux x64",
    osSlug = "linux",
    platformParam = "Linux",
    archParam = "amd64",
    perVariantConfig = mapOf(
        // Linux x64 Clang is part of the draft-friendly fast-feedback subset,
        // and it is the reference Clang build: its compiler diagnostics are
        // the ones pinned to the PR diff (GCC's would duplicate them on the
        // same lines).
        "Clang" to {
            bridgeOverride(runOnDraftPr = true, annotateDiff = true)
        },
    ),
)

private val linuxArm64 = stdPlatform(
    projectId = "Build_LinuxArm64",
    displayName = "Linux arm64",
    osSlug = "linux",
    platformParam = "Linux",
    archParam = "amd64", // host architecture — ARM64 runs via Docker emulation
    extraProjectParams = {
        param("docker_build_platform", "linux/arm64")
        param("extra_tc_vars", "-- --emulated")
    },
)

private val windowsX64 = stdPlatform(
    projectId = "Build_WindowsX64",
    displayName = "Windows x64",
    osSlug = "windows",
    platformParam = "Windows",
    archParam = "amd64",
    // Default mainOnlyAutoTriggerFor = setOf("Gcc") — Windows GCC
    // runs on main only, like Linux GCC.
    perVariantConfig = mapOf(
        // Windows + Clang is in the draft-friendly subset AND has stricter
        // failure conditions on test count and artifact size regression
        // — historical guardrail for that toolchain. It annotates the diff
        // with the MinGW-only diagnostics Linux never sees, and it is the one
        // PR-side BT that must also run on a doc-only PR: its preset carries
        // OWL_ENABLE_DOCUMENTATION=ON, so Doxygen (WARN_AS_ERROR) reads
        // doc/ and the root markdown files here.
        "Clang" to {
            bridgeOverride(runOnDraftPr = true, annotateDiff = true, pathFilter = "")
            failureConditions {
                failOnMetricChange {
                    id = "BUILD_EXT_1"
                    metric = BuildFailureOnMetric.MetricType.TEST_COUNT
                    threshold = 20
                    units = BuildFailureOnMetric.MetricUnit.PERCENTS
                    comparison = BuildFailureOnMetric.MetricComparison.LESS
                    compareTo = build { buildRule = lastSuccessful() }
                }
                failOnMetricChange {
                    id = "BUILD_EXT_2"
                    metric = BuildFailureOnMetric.MetricType.ARTIFACT_SIZE
                    threshold = 10
                    units = BuildFailureOnMetric.MetricUnit.PERCENTS
                    comparison = BuildFailureOnMetric.MetricComparison.LESS
                    compareTo = build { buildRule = lastSuccessful() }
                }
            }
        },
    ),
)

// ── Quality sub-project (irregular contents) ────────────────────────────────

// Exposed (non-private) so GlobalBuild's snapshot dependency can reference it.
val QualityCodeStyle = BuildType({
    id("Build_Quality_CodeStyle")
    name = "Code Style"
    templates(CodeStylingCheck)
    params { param("cmake_preset", "linux-clang-debug") }
    // Serialise concurrent runs so the three idle agents do not each pick
    // up their own copy when several downstream BTs queue at the same time.
    // Combined with the default reuseBuilds=SUCCESSFUL on the snapshot
    // dependency, the first finished run is reused by the others instead
    // of spawning duplicates.
    maxRunningBuilds = 1
})

private val qualityClangTidy = BuildType({
    id("Build_Quality_ClangTidy")
    name = "Clang-Tidy"
    templates(GlobalBuild)
    params {
        param("cmake_preset", "linux-clang-tidy")
        param("platform", "Linux") // override parent's "in" since clang-tidy needs Linux
    }
    // The analysis itself, and the only BT that runs it. It is not hooked into
    // the compiler (cmake/Sanitizers.cmake leaves CMAKE_CXX_CLANG_TIDY unset):
    // it reads the compilation database the template's Build step produced,
    // which is what lets it analyse only the translation units a pull request
    // can change the verdict of. A compiler hook cannot skip a file.
    //
    // A BT's own steps run after the ones it inherits, so this lands at the end
    // of the template pipeline — for this preset that is right after Build,
    // since OWL_TESTING=OFF and no release/coverage/doc/package step applies.
    //
    // The three bridge parameters carry the pull request context. They are
    // always emitted for a BT carrying the `github-bridge` feature and empty
    // outside a pull request, in which case the action analyses everything.
    // `mergeBase` and not `baseSha`: the merge base is where the branches
    // diverged, so the diff is this PR's own change and not what landed on
    // `main` since.
    steps {
        script {
            ciAction("ClangTidy", "Clang_Tidy", displayName = "Clang-Tidy",
                extraArgs = "-- --is_pull_request=%teamcity.github.bridge.isPullRequest%" +
                    " --merge_base=%teamcity.github.bridge.pullRequest.mergeBase%" +
                    " --target_branch=%teamcity.github.bridge.pullRequest.targetBranch%")
        }
    }
    // clang-tidy findings are already `file:line:col: warning: … [check]` in
    // the log and belong to no other BT, so they are worth pinning to the diff.
    bridgeOverride(annotateDiff = true)
})

// Every header and source compiled alone against strict libc++ without the
// PCH (cmake/IncludeCheck.cmake): catches the transitive standard includes a
// recent libstdc++ (MSYS2 MinGW) no longer provides. Compile-only, so the
// template's full build and test steps are switched off.
private val qualityIncludeCheck = BuildType({
    id("Build_Quality_IncludeCheck")
    name = "Include Check"
    templates(GlobalBuild)
    params {
        param("cmake_preset", "linux-include-check")
        param("platform", "Linux")
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
    // The libc++ errors exist in no other BT, so they are pinned to the diff.
    bridgeOverride(annotateDiff = true)
})

private data class Sanitizer(
    val idSuffix: String,
    val displayName: String,
    val preset: String,
    val platformOverride: String? = null,
    val mainOnlyAutoTrigger: Boolean = false,
    val runOnDraft: Boolean = false,
)

// Display-name casing matches the existing UI convention (inconsistent on
// purpose — "Address" capitalised, the others lowercase — preserved to keep
// dashboards looking identical to today).
private val sanitizers = listOf(
    // Address sanitizer is in the draft-friendly subset.
    Sanitizer("SanitizerAddress", "Sanitizer Address", "linux-sanitizer-address",
        runOnDraft = true),
    Sanitizer("SanitizerThread", "Sanitizer thread", "linux-sanitizer-thread"),
    Sanitizer("SanitizerLeak", "Sanitizer leak", "linux-sanitizer-leak"),
    Sanitizer(
        "SanitizerUndefinedBehavior", "Sanitizer undefined behavior",
        "linux-sanitizer-undefined-behavior",
        platformOverride = "Linux",
        mainOnlyAutoTrigger = true,
    ),
)

private val sanitizerBuilds = sanitizers.map { s ->
    BuildType({
        id("Build_Quality_${s.idSuffix}")
        name = s.displayName
        templates(GlobalBuild)
        params {
            param("cmake_preset", s.preset)
            s.platformOverride?.let { param("platform", it) }
        }
        if (s.mainOnlyAutoTrigger) {
            skipAutoPRs()
        } else if (s.runOnDraft) {
            bridgeOverride(runOnDraftPr = true)
        }
    })
}

private val quality = Project({
    id("Build_Quality")
    name = "Quality"

    buildType(QualityCodeStyle)
    sanitizerBuilds.forEach { buildType(it) }
    buildType(qualityClangTidy)
    buildType(qualityIncludeCheck)

    params {
        // "in" matches both "Linux" and "Windows" via the substring requirement
        // on teamcity.agent.jvm.os.name (RQ_18). These jobs run in Linux Docker
        // containers but can be hosted by either kind of agent.
        param("platform", "in")
        param("architecture", "amd64")
    }
})

// ── Build (top-level parent of this file) ────────────────────────────────────

object BuildProject : Project({
    id("Build")
    name = "Build"
    description = "Build and Test Project"

    subProject(linuxX64)
    subProject(linuxArm64)
    subProject(windowsX64)
    subProject(quality)
})
