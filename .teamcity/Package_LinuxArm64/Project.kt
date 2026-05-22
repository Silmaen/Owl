package Package_LinuxArm64

import Package_LinuxArm64.buildTypes.*
import jetbrains.buildServer.configs.kotlin.*
import jetbrains.buildServer.configs.kotlin.Project

object Project : Project({
    id("Package_LinuxArm64")
    name = "Linux arm64"

    buildType(Package_LinuxArm64_AppNest)
    buildType(Package_LinuxArm64_Engine)

    params {
        param("docker_build_platform", "linux/arm64")
        param("docker_test_platform", "linux/arm64")
        param("platform", "in")
        param("architecture", "arm64")
    }
})
