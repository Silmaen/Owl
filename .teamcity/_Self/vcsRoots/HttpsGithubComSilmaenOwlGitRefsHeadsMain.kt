package _Self.vcsRoots

import jetbrains.buildServer.configs.kotlin.*
import jetbrains.buildServer.configs.kotlin.vcs.GitVcsRoot

object HttpsGithubComSilmaenOwlGitRefsHeadsMain : GitVcsRoot({
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
})
