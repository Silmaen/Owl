package Build_Quality.buildTypes

import jetbrains.buildServer.configs.kotlin.*
import jetbrains.buildServer.configs.kotlin.buildFeatures.perfmon

object Build_Quality_CodeStyle : BuildType({
    templates(_Self.buildTypes.CodeSylingCheck)
    name = "Code Style"

    params {
        param("cmake_preset", "linux-clang-debug")
    }

    features {
        perfmon {
            id = "perfmon"
        }
    }
})
