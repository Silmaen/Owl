import jetbrains.buildServer.configs.kotlin.RelativeId
import jetbrains.buildServer.configs.kotlin.vcs.GitVcsRoot

/** Id of the GitHub App connection the bridge plugin publishes through. */
const val GITHUB_CONNECTION_ID: String = "CID_392f0141078df64b20e1bb01ada5697f"

/** The Owl repository. The id is the one the server already knows. */
val githubOwl = GitVcsRoot {
    id = RelativeId("HttpsGithubComSilmaenOwlGitRefsHeadsMain")
    name = "github Owl"
    url = "git@github.com:Silmaen/Owl.git"
    branch = "refs/heads/%owl_git_branch%"
    branchSpec = "%branch_specification%"
    userNameStyle = GitVcsRoot.UserNameStyle.NAME
    agentCleanPolicy = GitVcsRoot.AgentCleanPolicy.ALWAYS
    checkoutPolicy = GitVcsRoot.AgentCheckoutPolicy.USE_MIRRORS
    authMethod = uploadedKey {
        uploadedKey = "github connexion"
    }
}
