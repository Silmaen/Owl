package Package_LinuxX64

import Package_LinuxX64.buildTypes.*
import jetbrains.buildServer.configs.kotlin.*
import jetbrains.buildServer.configs.kotlin.Project

object Project : Project({
    id("Package_LinuxX64")
    name = "Linux x64"

    buildType(Package_LinuxX64_AppNest)
    buildType(Package_LinuxX64_Engine)

    params {
        param("platform", "Linux")
        param("architecture", "amd64")
    }
})
