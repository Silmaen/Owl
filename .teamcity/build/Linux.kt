import jetbrains.buildServer.configs.kotlin.*

val linuxGcc = presetBuild("Build_LinuxX64_Gcc", "GCC", "linux-gcc-debug")
val linuxClang = presetBuild("Build_LinuxX64_Clang", "Clang", "linux-clang-debug", onDraft = true)

val linuxX64 = Project {
    id = RelativeId("Build_LinuxX64")
    name = "Build Linux x64"

    buildType(linuxGcc)
    buildType(linuxClang)
    buildTypesOrder = arrayListOf(linuxGcc, linuxClang)

    params {
        param("platform", "Linux")
        param("architecture", "amd64")
    }
}
