package Build_LinuxX64

import Build_LinuxX64.buildTypes.*
import jetbrains.buildServer.configs.kotlin.*
import jetbrains.buildServer.configs.kotlin.Project

object Project : Project({
    id("Build_LinuxX64")
    name = "Linux x64"

    buildType(Build_LinuxX64_Gcc)
    buildType(Build_LinuxX64_Clang)

    params {
        param("platform", "Linux")
        param("architecture", "amd64")
    }
})
