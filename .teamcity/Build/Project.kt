package Build

import jetbrains.buildServer.configs.kotlin.*
import jetbrains.buildServer.configs.kotlin.Project

object Project : Project({
    id("Build")
    name = "Build"
    description = "Build and Test Project"

    subProject(Build_LinuxArm64.Project)
    subProject(Build_LinuxX64.Project)
    subProject(Build_Quality.Project)
    subProject(Build_WindowsX64.Project)
})
