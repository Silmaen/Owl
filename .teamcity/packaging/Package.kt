import jetbrains.buildServer.configs.kotlin.*

val packageLinuxEngine = packageBuild("Package_LinuxX64_Engine", "Engine Linux x64",
        "package-engine-linux", "Linux", linuxClang, nightly = true)
val packageLinuxNest = packageBuild("Package_LinuxX64_AppNest", "Nest Linux x64",
        "package-app-nest-linux", "Linux", linuxClang, nightly = true)
val packageArm64Engine = packageBuild("Package_LinuxArm64_Engine", "Engine Linux arm64",
        "package-engine-linux", "Linux", arm64Clang, nightly = true) {
    param("docker_build_platform", "linux/arm64")
    param("docker_test_platform", "linux/arm64")
}
val packageArm64Nest = packageBuild("Package_LinuxArm64_AppNest", "Nest Linux arm64",
        "package-app-nest-linux", "Linux", arm64Clang, nightly = true) {
    param("docker_build_platform", "linux/arm64")
    param("docker_test_platform", "linux/arm64")
}
val packageWindowsEngine = packageBuild("Package_WindowsX64_Engine", "Engine Windows x64",
        "package-engine-windows", "Windows", windowsClang, nightly = true)
val packageWindowsNest = packageBuild("Package_WindowsX64_AppNest", "Nest Windows x64",
        "package-app-nest-windows", "Windows", windowsClang, nightly = true)

val packaging = Project {
    id = RelativeId("Packaging")
    name = "Package"
    description = "Delivering the engine and the Owl Nest editor"

    buildType(packageLinuxEngine)
    buildType(packageLinuxNest)
    buildType(packageArm64Engine)
    buildType(packageArm64Nest)
    buildType(packageWindowsEngine)
    buildType(packageWindowsNest)
    buildTypesOrder = arrayListOf(packageLinuxEngine, packageLinuxNest, packageArm64Engine, packageArm64Nest,
            packageWindowsEngine, packageWindowsNest)
}
