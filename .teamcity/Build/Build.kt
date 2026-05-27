package Build

import _Self.buildTypes.CodeStylingCheck
import _Self.buildTypes.GlobalBuild
import jetbrains.buildServer.configs.kotlin.*
import jetbrains.buildServer.configs.kotlin.failureConditions.BuildFailureOnMetric
import jetbrains.buildServer.configs.kotlin.failureConditions.failOnMetricChange

// Helper: mark this BT as draft-friendly (runs on draft PRs as well as
// ready ones). Disables the BRIDGE_GITHUB feature inherited from the
// GlobalBuild template (which carries triggerOnPrDraft=false) and
// re-attaches the same plugin feature with triggerOnPrDraft=true under
// a distinct id (BRIDGE_GITHUB_DRAFT) so it does not collide with the
// disabled inherited one.
private fun BuildType.allowDraftPR() {
    disableSettings("BRIDGE_GITHUB")
    features {
        feature {
            id = "BRIDGE_GITHUB_DRAFT"
            type = "github-bridge"
            param("triggerOnPrDraft", "true")
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Build & Test top-level project
//
//  Generates 11 buildTypes across 4 sub-projects:
//    • Linux x64 / Linux ARM64 / Windows x64  (Clang + GCC each, 6 total)
//    • Quality (CodeStyle + ClangTidy + 4 sanitizers, 6 total — minus CodeStyle
//      which actually lives in this project but uses the CodeStylingCheck
//      template, the others all use GlobalBuild)
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
    disableFeatureBranchTrigger: Boolean,
    extraConfig: BuildType.() -> Unit = {},
) = BuildType({
    id("${projectId}_${variant.idSuffix}")
    name = variant.displayName
    templates(GlobalBuild)
    params {
        param("cmake_preset", "$osSlug-${variant.compilerSlug}-debug")
    }
    if (disableFeatureBranchTrigger) {
        disableSettings("TRIGGER_2")
    }
    extraConfig()
})

// `disableFeatureBranchTriggerFor` lists the variant.idSuffix values that
// should skip TRIGGER_2 (the feature-branch watcher). Default = "Gcc" only,
// which matches the Linux convention (GCC is too slow to run on every push).
// Windows historically runs both compilers on every branch, so it overrides
// to emptySet().
private fun stdPlatform(
    projectId: String,
    displayName: String,
    osSlug: String,
    platformParam: String,
    archParam: String,
    extraProjectParams: ParametrizedWithType.() -> Unit = {},
    perVariantConfig: Map<String, BuildType.() -> Unit> = emptyMap(),
    disableFeatureBranchTriggerFor: Set<String> = setOf("Gcc"),
) = Project({
    id(projectId)
    name = displayName

    stdVariants.forEach { variant ->
        buildType(
            stdBuildType(
                projectId, osSlug, variant,
                disableFeatureBranchTrigger = variant.idSuffix in disableFeatureBranchTriggerFor,
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
        // Linux x64 Clang is part of the draft-friendly fast-feedback subset.
        "Clang" to {
            allowDraftPR()
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
    // Default disableFeatureBranchTriggerFor = setOf("Gcc") — Windows GCC
    // runs on main only, like Linux GCC.
    perVariantConfig = mapOf(
        // Windows + Clang is in the draft-friendly subset AND has stricter
        // failure conditions on test count and artifact size regression
        // — historical guardrail for that toolchain.
        "Clang" to {
            allowDraftPR()
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
})

private val qualityClangTidy = BuildType({
    id("Build_Quality_ClangTidy")
    name = "Clang-Tidy"
    templates(GlobalBuild)
    params {
        param("cmake_preset", "linux-clang-tidy")
        param("platform", "Linux") // override parent's "in" since clang-tidy needs Linux
    }
})

private data class Sanitizer(
    val idSuffix: String,
    val displayName: String,
    val preset: String,
    val platformOverride: String? = null,
    val disableFeatureBranchTrigger: Boolean = false,
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
        disableFeatureBranchTrigger = true,
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
        if (s.disableFeatureBranchTrigger) {
            disableSettings("TRIGGER_2")
        }
        if (s.runOnDraft) {
            allowDraftPR()
        }
    })
}

private val quality = Project({
    id("Build_Quality")
    name = "Quality"

    buildType(QualityCodeStyle)
    sanitizerBuilds.forEach { buildType(it) }
    buildType(qualityClangTidy)

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
