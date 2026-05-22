package Build_WindowsX64

import Build_WindowsX64.buildTypes.*
import jetbrains.buildServer.configs.kotlin.*
import jetbrains.buildServer.configs.kotlin.Project

object Project : Project({
    id("Build_WindowsX64")
    name = "Windows x64"

    buildType(Build_WindowsX64_Clang)
    buildType(Build_WindowsX64_Gcc)

    params {
        param("platform", "Windows")
        param("architecture", "amd64")
    }
})
