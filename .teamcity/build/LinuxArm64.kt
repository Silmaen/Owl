import jetbrains.buildServer.configs.kotlin.*

// Emulated through Docker on the x64 agents: 50 to 90 minutes each, so `main` only, once a night.
val arm64Gcc = presetBuild("Build_LinuxArm64_Gcc", "GCC", "linux-gcc-debug", onPullRequest = false, nightly = true)
val arm64Clang = presetBuild("Build_LinuxArm64_Clang", "Clang", "linux-clang-debug", onPullRequest = false, nightly = true)

val linuxArm64 = Project {
    id = RelativeId("Build_LinuxArm64")
    name = "Build Linux arm64"

    buildType(arm64Gcc)
    buildType(arm64Clang)
    buildTypesOrder = arrayListOf(arm64Gcc, arm64Clang)

    params {
        param("platform", "Linux")
        param("architecture", "amd64") // host architecture: arm64 runs through Docker emulation
        param("docker_build_platform", "linux/arm64")
        param("extra_tc_vars", "-- --emulated")
    }
}
