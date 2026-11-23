# Updates the GitHub MCP bearer header from the current gh CLI login.
# Never prints the token.

$ErrorActionPreference = 'Stop'
$config = 'C:\Users\Windows 10\.config\opencode\opencode.json'
$gh = 'C:\Program Files\GitHub CLI\gh.exe'

if (-not (Test-Path $gh)) { throw "gh.exe not found at $gh" }
$token = (& $gh auth token 2>&1 | Out-String).Trim()
if (-not $token.StartsWith('gho_') -and -not $token.StartsWith('github_pat_')) {
    throw "Unexpected token format; refusing to write config"
}
if (-not (Test-Path $config)) { throw "OpenCode config not found: $config" }

$cfg = Get-Content $config -Raw | ConvertFrom-Json
if (-not $cfg.mcp -or -not $cfg.mcp.servers -or -not $cfg.mcp.servers.github) {
    throw "github MCP server entry missing from $config"
}
$cfg.mcp.servers.github.headers.Authorization = "Bearer $token"
$cfg | ConvertTo-Json -Depth 12 | Set-Content $config -Encoding UTF8

Write-Output 'MCP TOKEN UPDATED'
Write-Output ("Token prefix: " + $token.Substring(0, 4) + '...')
