package Package_WindowsX64

import Package_WindowsX64.buildTypes.*
import jetbrains.buildServer.configs.kotlin.*
import jetbrains.buildServer.configs.kotlin.Project

object Project : Project({
    id("Package_WindowsX64")
    name = "Windows x64"

    buildType(Package_WindowsX64_Engine)
    buildType(Package_WindowsX64_AppNest)

    params {
        param("platform", "Windows")
        param("architecture", "amd64")
    }
})
