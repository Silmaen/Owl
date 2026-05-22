package Build_LinuxArm64

import Build_LinuxArm64.buildTypes.*
import jetbrains.buildServer.configs.kotlin.*
import jetbrains.buildServer.configs.kotlin.Project

object Project : Project({
    id("Build_LinuxArm64")
    name = "Linux arm64"

    buildType(Build_LinuxArm64_Gcc)
    buildType(Build_LinuxArm64_Clang)

    params {
        param("docker_build_platform", "linux/arm64")
        param("extra_tc_vars", "-- --emulated")
        param("platform", "Linux")
        param("architecture", "amd64")
    }
})
