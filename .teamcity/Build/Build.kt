package Build

import _Self.bridgeOverride
import _Self.buildTypes.CodeStylingCheck
import _Self.buildTypes.GlobalBuild
import _Self.buildTypes.ciAction
import _Self.after
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
//    • Quality (CodeStyle + IncludeCheck + 4 sanitizers + ClangTidy + Clang Static Analyzer, 8 total —
//      CodeStyle uses the CodeStylingCheck template, the others GlobalBuild)
//
//  All IDs are pinned explicitly via id("...") to preserve TC build history.
// ─────────────────────────────────────────────────────────────────────────────

// ── Standard (cross-OS) builds ───────────────────────────────────────────────

// Every configuration a pull request runs, in declaration order. The two
// analyses at the end of the chain depend on all of them, which makes the
// analyses the only checks worth requiring before a merge.
private val prGates = mutableListOf<BuildType>()

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
}).also { if (!mainOnlyAutoTrigger) prGates += it }

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
}).also { prGates += it }

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
    }).also { if (!s.mainOnlyAutoTrigger) prGates += it }
}

// clang-tidy, and the same binary restricted to the Clang static analyzer
// checks: one configuration per tool. Both read the compilation database the
// template's Build step produced (no compiler hook: cmake/Sanitizers.cmake
// leaves CMAKE_CXX_CLANG_TIDY unset) and decide their scope at run time. In a
// pull request they analyse only the translation units the diff can change the
// verdict of, from the merge base the bridge publishes (`mergeBase`, not
// `baseSha`: where the branches diverged, so the diff is this PR's own change);
// anywhere else they analyse everything.
//
// They close the dependency chain: nothing reaches them unless every
// configuration a pull request runs went green first, which makes them the
// checks to require in the branch protection of `main`. Findings are
// `file:line:col: warning: … [check]` lines owned by no other BT, so they are
// pinned to the diff.
private fun analysisBuild(idSuffix: String, displayName: String, tool: String) = BuildType({
    id("Build_Quality_$idSuffix")
    name = displayName
    templates(GlobalBuild)
    params {
        param("cmake_preset", "linux-clang-tidy")
        param("platform", "Linux") // override parent's "in": the analyses need Linux
    }
    steps {
        script {
            ciAction("ClangTidy", idSuffix, displayName = displayName,
                extraArgs = "-- --tool=$tool" +
                    " --is_pull_request=%teamcity.github.bridge.isPullRequest%" +
                    " --merge_base=%teamcity.github.bridge.pullRequest.mergeBase%" +
                    " --target_branch=%teamcity.github.bridge.pullRequest.targetBranch%")
        }
    }
    dependencies {
        after(*prGates.toTypedArray())
    }
})

// The id keeps the existing configuration's build history.
private val qualityClangTidy = analysisBuild("ClangTidy", "Clang-Tidy", "tidy")
private val qualityClangAnalyzer =
    analysisBuild("ClangAnalyzer", "Clang Static Analyzer", "analyzer")

private val quality = Project({
    id("Build_Quality")
    name = "Quality"

    buildType(QualityCodeStyle)
    sanitizerBuilds.forEach { buildType(it) }
    buildType(qualityClangTidy)
    buildType(qualityIncludeCheck)
    buildType(qualityClangAnalyzer)

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
