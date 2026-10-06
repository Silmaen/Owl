import jetbrains.buildServer.configs.kotlin.*
import jetbrains.buildServer.configs.kotlin.buildSteps.script

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
 * @param gates What must be green first.
 * @param extra Configuration-specific settings.
 */
fun presetBuild(idValue: String, buildName: String, cmakePreset: String,
                onDraft: Boolean = false, onPullRequest: Boolean = true,
                pathFilter: String = CODE_ONLY_PATHS,
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
 * They sit at the end of the dependency chain, which makes them the checks worth
 * requiring before a merge: nothing reaches them unless every build and every sanitizer
 * went green first.
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
 * One package. Never triggered by a pull request: only a push to `main` or a manual run
 * packages anything; the check still appears on the commit it built. It waits for the
 * configuration that built and tested the same platform: an archive built from code whose
 * tests fail has no business existing.
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
        presetBuild(idValue, buildName, cmakePreset, onPullRequest = false,
                gates = listOf(codeStyle, tested)) {
            params {
                param("platform", platformName)
                param("architecture", "amd64")
                extraParams()
            }
        }
