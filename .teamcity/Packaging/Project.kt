package Packaging

import jetbrains.buildServer.configs.kotlin.*
import jetbrains.buildServer.configs.kotlin.Project

object Project : Project({
    id("Packaging")
    name = "Package"

    subProject(Package_LinuxX64.Project)
    subProject(Package_LinuxArm64.Project)
    subProject(Package_WindowsX64.Project)
})
