import jetbrains.buildServer.configs.kotlin.*

// Cross compiled on the x64 agents (Clang --target=aarch64-linux-gnu, lld, the arm64 sysroot of the build image);
// only the tests run under qemu-user, through the agent's binfmt_misc. `main` only, Clang only (GCC is covered on
// x64), `linux-cross-arm64` = Debug without coverage, benchmarks or image tests (lavapipe under QEMU, CPU-dependent).
// Nightly: its tests under qemu held a Linux agent for most of an hour on every push.
val arm64Clang = presetBuild("Build_LinuxArm64_Clang", "Clang", "linux-cross-arm64", onPullRequest = false,
        nightly = true) {
    params {
        text("docker_build_platform", "linux/amd64", display = ParameterDisplay.HIDDEN, allowEmpty = true)
        fullSuite()
    }
}

val linuxArm64 = Project {
    id = RelativeId("Build_LinuxArm64")
    name = "Build Linux arm64"

    buildType(arm64Clang)

    params {
        text("platform", "Linux", display = ParameterDisplay.HIDDEN, allowEmpty = true)
        text("architecture", "amd64", display = ParameterDisplay.HIDDEN, allowEmpty = true) // host architecture: arm64 is cross compiled on x64
    }
}
