package _Self

// GitHub App connection ID for the Silmaen/Owl repo (visible in the
// URL of the project's Connections page). Hardcoded here so that
// both the storedToken references in build features and the
// tcgh.github.connectionId parameter resolve from a single source.
const val GITHUB_CONNECTION_ID: String = "CID_392f0141078df64b20e1bb01ada5697f"

// Full token storage ID used by TC's bundled pullRequests feature
// when authType = storedToken. Format: tc_token_id:<connectionId>:-1:<tokenUuid>.
const val GITHUB_TOKEN_ID: String = "tc_token_id:$GITHUB_CONNECTION_ID:-1:fc63f361-ae0d-4cd9-8feb-dabdd68f74a6"
