import jetbrains.buildServer.configs.kotlin.*

// One configuration per platform publishes both archives (Engine SDK, Owl Nest). The x64 ones come from the tested
// release tree; arm64 has none (emulated, debug only) and builds its own.
val packageLinux = publishBuild("Package_LinuxX64_Engine", "Linux x64",
        "linux-clang-release", "Linux", linuxClang, archive = "tar.gz")
val packageArm64 = packageBuild("Package_LinuxArm64_Engine", "Linux arm64",
        "package-linux", "Linux", arm64Clang) {
    param("docker_build_platform", "linux/arm64")
    param("docker_test_platform", "linux/arm64")
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
