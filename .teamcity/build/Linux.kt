import jetbrains.buildServer.configs.kotlin.*

val linuxGcc = presetBuild("Build_LinuxX64_Gcc", "GCC", "linux-gcc-debug")
// Builds the release and Doxygen on it: it runs on documentation-only pull requests too.
val linuxClang = presetBuild("Build_LinuxX64_Clang", "Clang", "linux-clang-debug", onDraft = true, pathFilter = "")

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
