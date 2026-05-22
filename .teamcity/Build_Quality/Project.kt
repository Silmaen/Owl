package Build_Quality

import Build_Quality.buildTypes.*
import jetbrains.buildServer.configs.kotlin.*
import jetbrains.buildServer.configs.kotlin.Project

object Project : Project({
    id("Build_Quality")
    name = "Quality"

    buildType(Build_Quality_CodeStyle)
    buildType(Build_Quality_SanitizerThread)
    buildType(Build_Quality_SanitizerAddress)
    buildType(Build_Quality_ClangTidy)
    buildType(Build_Quality_SanitizerLeak)
    buildType(Build_Quality_SanitizerUndefinedBehavior)

    params {
        param("platform", "in")
        param("architecture", "amd64")
    }
})
