import jetbrains.buildServer.configs.kotlin.*

// Level 2: after Code Style only, in parallel with the builds and the sanitizers (each analysis
// builds its own preset).
private val analysisGates = listOf(codeStyle)

val clangTidy = analysisBuild("Build_Quality_ClangTidy", "Clang-Tidy", "tidy", analysisGates)
val staticAnalyzer = analysisBuild("Build_Quality_ClangAnalyzer", "Static Analyzer", "analyzer", analysisGates)

val analysis = Project {
    id = RelativeId("Analysis")
    name = "Analysis"

    buildType(clangTidy)
    buildType(staticAnalyzer)
    buildType(bench)
    buildTypesOrder = arrayListOf(clangTidy, staticAnalyzer, bench)

    params {
        param("platform", "Linux")
        param("architecture", "amd64")
    }
}
