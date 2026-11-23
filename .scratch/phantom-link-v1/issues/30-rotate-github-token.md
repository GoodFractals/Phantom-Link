# 30: Rotate GitHub token after MCP setup

**What to do:** Replace the GitHub token that was exposed during GitHub MCP
setup, then re-point the MCP server at the new token.

**Blocked by:** None

**Status:** closed

- [x] Revoke the GitHub CLI OAuth grant on GitHub:
      `https://github.com/settings/applications` → **Authorized OAuth Apps** →
      **GitHub CLI** → **Revoke**. (`gho_` tokens are OAuth tokens, not PATs,
      so they are not listed under Developer settings → Personal access tokens.)
- [x] Re-authenticate locally with `gh auth login` (browser flow) so GitHub CLI
      issues a fresh `gho_` token with `repo`, `read:org`, `workflow`, `gist` scopes.
- [x] Update `~/.config/opencode/opencode.json` → `mcp.servers.github.headers.Authorization`
      with the new bearer token, without printing it in chat or logs.
- [x] Verify the GitHub MCP tools load and `get_me` returns `GoodFractals`.
- [x] Verify `git push` to `GoodFractals/Phantom-Link` still works.
- [x] Confirm no old token remains in config, shell history, or saved files.

## Context

On 2026-11-22, while testing the remote GitHub MCP endpoint, the first line of
the `gho_` token was inadvertently printed in a tool output in the session
transcript. The token was never pasted into a GitHub PR, README, or repo file,
but it should be treated as exposed and rotated.

The MCP server is configured in `C:\Users\Windows 10\.config\opencode\opencode.json`
as a remote server at `https://api.githubcopilot.com/mcp/` with an
`Authorization: Bearer <token>` header. After rotation, OpenCode must be
restarted for the new header to take effect.

## Verification

2026-11-23 — COMPLETE. User revoked the GitHub CLI OAuth grant at
`github.com/settings/applications`, re-ran `gh auth login` using the browser
device-code flow (after correcting an initial "Paste an authentication token"
mistake), and confirmed `gh auth status` shows GoodFractals with
`gist, read:org, repo, workflow`. Helper `tools/rotate_github_mcp_token.ps1`
rewrote the MCP bearer header from `gh auth token` without printing it; config
was verified to match the current token exactly. GitHub MCP reloaded and
`get_me` returned GoodFractals / Anthony Borum. `git push` authenticated
cleanly. No token was written to any repo file; the config holds only the new
bearer value. Skill `github-token-rotate` created for future rotations.
