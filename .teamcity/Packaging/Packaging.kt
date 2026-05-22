package Packaging

import _Self.buildTypes.GlobalBuild
import jetbrains.buildServer.configs.kotlin.*

// ─────────────────────────────────────────────────────────────────────────────
//  Packaging top-level project
//
//  Generates 6 buildTypes across 3 sub-projects:
//    • Linux x64 / Linux ARM64 / Windows x64  (Engine + AppNest each)
// ─────────────────────────────────────────────────────────────────────────────

private data class PackageKind(
    val idSuffix: String,    // "Engine" | "AppNest"
    val displayName: String, // "Engine" | "App - Nest"
    val presetMiddle: String,// "engine" | "app-nest"
)

private val kinds = listOf(
    PackageKind("Engine", "Engine", "engine"),
    PackageKind("AppNest", "App - Nest", "app-nest"),
)

private fun packageBuildType(
    projectId: String,
    osSlug: String,
    kind: PackageKind,
    disableFeatureBranchTrigger: Boolean,
) = BuildType({
    id("${projectId}_${kind.idSuffix}")
    name = kind.displayName
    templates(GlobalBuild)
    params {
        param("cmake_preset", "package-${kind.presetMiddle}-$osSlug")
    }
    if (disableFeatureBranchTrigger) {
        disableSettings("TRIGGER_2")
    }
})

private fun packagePlatform(
    projectId: String,
    displayName: String,
    osSlug: String,
    platformParam: String,
    archParam: String,
    extraProjectParams: ParametrizedWithType.() -> Unit = {},
    engineDisablesTrigger: Boolean = true,
    appNestDisablesTrigger: Boolean = false,
) = Project({
    id(projectId)
    name = displayName

    buildType(packageBuildType(projectId, osSlug, kinds[0], engineDisablesTrigger))
    buildType(packageBuildType(projectId, osSlug, kinds[1], appNestDisablesTrigger))

    params {
        param("platform", platformParam)
        param("architecture", archParam)
        extraProjectParams()
    }
})

private val linuxX64 = packagePlatform(
    projectId = "Package_LinuxX64",
    displayName = "Linux x64",
    osSlug = "linux",
    platformParam = "Linux",
    archParam = "amd64",
)

private val linuxArm64 = packagePlatform(
    projectId = "Package_LinuxArm64",
    displayName = "Linux arm64",
    osSlug = "linux",
    // "in" matches Linux/Windows agents — ARM64 builds run via Docker emulation,
    // so any OS host works.
    platformParam = "in",
    // Semantic target arch (artifact's, not agent's); RQ_3 (agent arch req) is
    // disabled in the GlobalBuild template, so this value is informational.
    archParam = "arm64",
    extraProjectParams = {
        param("docker_build_platform", "linux/arm64")
        param("docker_test_platform", "linux/arm64")
    },
    appNestDisablesTrigger = true,
)

private val windowsX64 = packagePlatform(
    projectId = "Package_WindowsX64",
    displayName = "Windows x64",
    osSlug = "windows",
    platformParam = "Windows",
    archParam = "amd64",
)

object PackagingProject : Project({
    id("Packaging")
    name = "Package"

    subProject(linuxX64)
    subProject(linuxArm64)
    subProject(windowsX64)
})
