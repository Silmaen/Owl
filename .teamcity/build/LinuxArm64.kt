import jetbrains.buildServer.configs.kotlin.*

// Cross compiled on the x64 agents (Clang --target=aarch64-linux-gnu, lld, the arm64 sysroot of the build image);
// only the tests run under qemu-user, through the agent's binfmt_misc. `main` only, Clang only (GCC is covered on
// x64), `linux-cross-arm64` = Debug without coverage, benchmarks or image tests (lavapipe under QEMU, CPU-dependent).
val arm64Clang = presetBuild("Build_LinuxArm64_Clang", "Clang", "linux-cross-arm64", onPullRequest = false) {
    params {
        param("docker_build_platform", "linux/amd64")
    }
}

// The former fully emulated build (the whole toolchain under QEMU, 1 to 2 hours), kept once a week during the
// transition to the cross build, as a reference on a native arm64 userland.
val arm64Emulated = presetBuild("Build_LinuxArm64_Emulated", "Clang (emulated)", "linux-emulated",
        onPullRequest = false) {
    params {
        param("docker_build_platform", "linux/arm64")
        param("extra_tc_vars", "-- --emulated")
    }
    weeklyOnMain()
}

val linuxArm64 = Project {
    id = RelativeId("Build_LinuxArm64")
    name = "Build Linux arm64"

    buildType(arm64Clang)
    buildType(arm64Emulated)
    buildTypesOrder = arrayListOf(arm64Clang, arm64Emulated)

    params {
        param("platform", "Linux")
        param("architecture", "amd64") // host architecture: arm64 is cross compiled (or emulated) on x64
    }
}
