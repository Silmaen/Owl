import jetbrains.buildServer.configs.kotlin.*

// Emulated through Docker on the x64 agents, so `main` only, once a night, and kept light: Clang only (GCC is covered
// on x64), `linux-emulated` = Debug without coverage, benchmarks or image tests (lavapipe under QEMU, CPU-dependent).
val arm64Clang = presetBuild("Build_LinuxArm64_Clang", "Clang", "linux-emulated", onPullRequest = false, nightly = true)

val linuxArm64 = Project {
    id = RelativeId("Build_LinuxArm64")
    name = "Build Linux arm64"

    buildType(arm64Clang)
    buildTypesOrder = arrayListOf(arm64Clang)

    params {
        param("platform", "Linux")
        param("architecture", "amd64") // host architecture: arm64 runs through Docker emulation
        param("docker_build_platform", "linux/arm64")
        param("extra_tc_vars", "-- --emulated")
    }
}
