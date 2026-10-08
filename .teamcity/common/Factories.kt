import jetbrains.buildServer.configs.kotlin.*
import jetbrains.buildServer.configs.kotlin.buildSteps.script
import jetbrains.buildServer.configs.kotlin.triggers.schedule

/*
 * One function per family of configuration. The ids are set explicitly to the ones the
 * server already knows: a configuration is identified by its id, and a new id would lose
 * its build history.
 */

/**
 * One build configuration per CMake preset: only the preset changes.
 *
 * @param idValue The configuration id.
 * @param buildName The configuration name.
 * @param cmakePreset The preset to configure, build and test.
 * @param onDraft Also run on draft pull requests (the fast feedback subset).
 * @param onPullRequest Run on pull requests at all; off for the `main`-only ones.
 * @param pathFilter Skip pull requests touching only these paths; empty runs on all.
 * @param nightly Build `main` once a night (when it changed) instead of on every push: for
 *        the emulated arm64 configurations, which would hold the Linux agents for hours.
 * @param gates What must be green first.
 * @param extra Configuration-specific settings.
 */
fun presetBuild(idValue: String, buildName: String, cmakePreset: String,
                onDraft: Boolean = false, onPullRequest: Boolean = true,
                pathFilter: String = CODE_ONLY_PATHS,
                nightly: Boolean = false,
                gates: List<BuildType> = listOf(codeStyle),
                extra: BuildType.() -> Unit = {}) = BuildType {
    id = RelativeId(idValue)
    name = buildName
    templates(globalBuild)

    params {
        param("cmake_preset", cmakePreset)
    }

    features {
        githubBridge(triggerOnPrDraft = onDraft, triggerOnPrReady = onPullRequest, pathFilter = pathFilter)
    }

    dependencies {
        after(*gates.toTypedArray())
    }

    if (nightly) {
        disableSettings("vcsTrigger")
        triggers {
            schedule {
                id = "nightly"
                schedulingPolicy = daily {
                    hour = 2
                }
                branchFilter = """
                    +:main
                    +:refs/heads/main
                """.trimIndent()
                withPendingChangesOnly = true
            }
        }
    }
    extra()
}

/**
 * clang-tidy, and the same binary restricted to the Clang static analyzer checks: one
 * configuration per tool. The scope is decided at run time by the `ClangTidy` action from
 * the pull request context the bridge publishes: inside a pull request, only the
 * translation units the diff can change the verdict of, from the PR's merge base;
 * anywhere else, everything. They build the `linux-clang-tidy` preset first (the
 * compilation database and ninja's dependency database are their inputs).
 *
 * They run after Code Style only, in parallel with the builds and the sanitizers.
 *
 * @param idValue The configuration id.
 * @param buildName The configuration name.
 * @param tool `tidy` or `analyzer`.
 * @param gates What must be green first.
 */
fun analysisBuild(idValue: String, buildName: String, tool: String, gates: List<BuildType>) = BuildType {
    id = RelativeId(idValue)
    name = buildName
    templates(globalBuild)

    params {
        param("cmake_preset", "linux-clang-tidy")
    }

    steps {
        script {
            ciAction("ClangTidy", "Analyse", displayName = "Analyse",
                    extraArgs = "-- --tool=$tool" +
                            " --is_pull_request=%teamcity.github.bridge.isPullRequest%" +
                            " --merge_base=%teamcity.github.bridge.pullRequest.mergeBase%" +
                            " --target_branch=%teamcity.github.bridge.pullRequest.targetBranch%")
        }
    }

    features {
        githubBridge()
    }

    dependencies {
        after(*gates.toTypedArray())
    }
}

/**
 * The packages of one platform. Never triggered by a pull request, nor by each push to `main`:
 * they are published once a night from `main` (when it changed), because each one publishes to
 * the site; a manual run packages on demand. It waits for the configuration that built and tested
 * the same platform: an archive built from code whose tests fail has no business existing.
 * Built and published here: the platforms without a tested release tree (arm64, emulated).
 *
 * @param idValue The configuration id.
 * @param buildName The configuration name.
 * @param cmakePreset The packaging preset.
 * @param platformName The agent platform, `Linux` or `Windows`.
 * @param tested The configuration that built and tested that platform.
 * @param extraParams Platform-specific parameters (docker platform for arm64).
 */
fun packageBuild(idValue: String, buildName: String, cmakePreset: String, platformName: String,
                 tested: BuildType, extraParams: ParametrizedWithType.() -> Unit = {}) =
        presetBuild(idValue, buildName, cmakePreset, onPullRequest = false, nightly = true,
                gates = listOf(codeStyle, tested)) {
            params {
                param("platform", platformName)
                param("architecture", "amd64")
                param("publish_package", "true")
                extraParams()
            }
        }

/**
 * The packages of a platform whose tested build packages its release tree on `main`
 * (`Package_Release_Main`): this configuration rebuilds nothing, it publishes the archives (and
 * the documentation) of that build, same commit. Nightly, like [packageBuild].
 *
 * @param idValue The configuration id.
 * @param buildName The configuration name.
 * @param releasePreset The release preset of the tested build: the archives land in its tree.
 * @param platformName The agent platform, `Linux` or `Windows`.
 * @param tested The configuration that built, tested and packaged that platform.
 * @param archive The archive extension of the platform.
 */
fun publishBuild(idValue: String, buildName: String, releasePreset: String, platformName: String,
                 tested: BuildType, archive: String) =
        presetBuild(idValue, buildName, releasePreset, onPullRequest = false, nightly = true,
                gates = listOf(codeStyle, tested)) {
            params {
                param("platform", platformName)
                param("architecture", "amd64")
                param("publish_package", "true")
            }
            dependencies {
                artifacts(tested) {
                    buildRule = sameChainOrLastFinished()
                    artifactRules = """
                        OwlEngine-*.$archive => output/build/$releasePreset
                        OwlNest-*.$archive => output/build/$releasePreset
                        ?:Documentation.zip!** => output/build/$releasePreset/Documentation/html
                    """.trimIndent()
                }
            }
            // The archives come from the tested build: cleaning, building or packaging again would lose them.
            disableSettings("Clean_Output_Folder", "Build_Preset", "Test_Preset", "Deploy")
        }
