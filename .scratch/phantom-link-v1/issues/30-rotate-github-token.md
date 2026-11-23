# 30: Rotate GitHub token after MCP setup

**What to do:** Replace the GitHub token that was exposed during GitHub MCP
setup, then re-point the MCP server at the new token.

**Blocked by:** None

**Status:** ready-for-human

- [ ] Revoke the old `gho_` token in GitHub developer settings.
- [ ] Create a replacement fine-grained token with `repo`, `read:org`, and
      `workflow` scopes (same scopes the current token has).
- [ ] Re-authenticate locally: `gh auth login` (browser flow), or
      `gh auth refresh -s repo -s read:org -s workflow`.
- [ ] Update `~/.config/opencode/opencode.json` → `mcp.servers.github.headers.Authorization`
      with the new bearer token, without printing it in chat or logs.
- [ ] Verify the GitHub MCP tools load and `get_me` returns `GoodFractals`.
- [ ] Verify `git push` to `GoodFractals/Phantom-Link` still works.
- [ ] Confirm no old token remains in config, shell history, or saved files.

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

2026-11-22 — ticket created from user request; scheduled as first task for the
next work session.
