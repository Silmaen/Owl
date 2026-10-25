import jetbrains.buildServer.configs.kotlin.*

// One configuration per platform publishes both archives (Engine SDK, Owl Nest), taken from the tested release tree.
val packageLinux = publishBuild("Package_LinuxX64_Engine", "Linux x64",
        "linux-clang-release", "Linux", linuxClangRelease, archive = "tar.gz")
val packageArm64 = publishBuild("Package_LinuxArm64_Engine", "Linux arm64",
        "linux-cross-arm64", "Linux", arm64Clang, archive = "tar.gz") {
    text("docker_build_platform", "linux/amd64", display = ParameterDisplay.HIDDEN, allowEmpty = true)
}
val packageWindows = publishBuild("Package_WindowsX64_Engine", "Windows x64",
        "windows-clang-release", "Windows", windowsClang, archive = "zip")

val packaging = Project {
    id = RelativeId("Packaging")
    name = "Package"
    description = "Delivering the engine and the Owl Nest editor"

    buildType(packageLinux)
    buildType(packageArm64)
    buildType(packageWindows)
    buildTypesOrder = arrayListOf(packageLinux, packageArm64, packageWindows)
}
