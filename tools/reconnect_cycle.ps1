# Ticket 18 - reconnect / power-cycle harness.
# For each cycle: wait for the operator to power-cycle one board, detect the
# serial's return, then read STATUS from both boards and log link state,
# session id and recovery time. Never writes firmware; read-only on the ports.
#
# Usage:
#   powershell -File tools\reconnect_cycle.ps1 -Board sta -Cycles 10
#   powershell -File tools\reconnect_cycle.ps1 -Board ap  -Cycles 10
[CmdletBinding()]
param(
    [ValidateSet('ap','sta')]
    [string]$Board = 'sta',
    [int]$Cycles = 10,
    [string]$OutFile = ''
)
$ErrorActionPreference = 'Stop'

$repo = Split-Path -Parent $PSScriptRoot
$map = Get-Content (Join-Path $PSScriptRoot 'board_map.json') -Raw | ConvertFrom-Json
$apCom  = [string]$map.boards.ap.com
$staCom = [string]$map.boards.sta.com
$apSer  = [string]$map.boards.ap.bootsel_usb_serial
$staSer = [string]$map.boards.sta.bootsel_usb_serial

if (-not $OutFile) {
    $OutFile = Join-Path $repo ("logs\reconnect18\cycles_{0}_{1}.md" -f $Board, (Get-Date -Format yyyyMMdd_HHmmss))
}
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $OutFile) | Out-Null

function Get-Status([string]$com, [string]$serial) {
    $sp = $null
    try {
        for ($a = 0; $a -lt 12; $a++) {
            try {
                $sp = [System.IO.Ports.SerialPort]::new($com, 115200)
                $sp.DtrEnable = $true
                $sp.ReadTimeout = 400
                $sp.Open()
                break
            } catch {
                if ($sp) { try { $sp.Dispose() } catch {} }
                $sp = $null
                Start-Sleep -Milliseconds 300
            }
        }
        if (-not $sp) { return "port $com unavailable" }
        Start-Sleep -Milliseconds 200
        $sp.Write('S')
        Start-Sleep -Seconds 1
        $o = ''
        while ($sp.BytesToRead -gt 0) { $o += $sp.ReadExisting() }
        $matches = @($o -split "`r?`n" | Where-Object { $_ -match 'link=' })
        if ($matches.Count -eq 0) { return 'no-status' }
        $link = [string]$matches[0] -replace '\s+', ' '
        return ([string]$link).Trim()
    } catch {
        return "ERR: $($_.Exception.Message)"
    } finally {
        if ($sp) { try { $sp.Dispose() } catch {} }
    }
}

function Wait-ForDevice([string]$serial, [int]$timeoutSec) {
    $deadline = (Get-Date).AddSeconds($timeoutSec)
    while ((Get-Date) -lt $deadline) {
        $dev = Get-PnpDevice -PresentOnly -ErrorAction SilentlyContinue |
            Where-Object { $_.InstanceId -like "*\$serial*" }
        if ($dev) { return $true }
        Start-Sleep -Milliseconds 250
    }
    return $false
}

$targetCom = if ($Board -eq 'ap') { $apCom } else { $staCom }
$targetSer = if ($Board -eq 'ap') { $apSer } else { $staSer }

$lines = @()
$lines += "# Ticket 18 - reconnect cycles ($Board)"
$lines += ""
$lines += "Date: $(Get-Date -Format o)"
$lines += "Build: $((Get-FileHash (Join-Path $repo 'firmware\main.c') -Algorithm SHA256).Hash.Substring(0,8))"
$lines += "Board power-cycled: $Board ($targetSer); peer untouched."
$lines += ""
$lines += "| Cycle | Board status | Peer status | Notes |"
$lines += "|---|---|---|---|"

for ($c = 1; $c -le $Cycles; $c++) {
    Write-Host "`n=== Cycle ${c} of ${Cycles}: power-cycle the ${Board} board now ===" -ForegroundColor Yellow
    Write-Host "Waiting for $targetSer to disappear..."
    # Wait until the device goes away (power pulled).
    $gone = $false
    $deadline = (Get-Date).AddSeconds(60)
    while ((Get-Date) -lt $deadline) {
        $dev = Get-PnpDevice -PresentOnly -ErrorAction SilentlyContinue |
            Where-Object { $_.InstanceId -like "*\$targetSer*" }
        if (-not $dev) { $gone = $true; break }
        Start-Sleep -Milliseconds 250
    }
    if (-not $gone) { Write-Host "Device never disappeared - aborting." -ForegroundColor Red; break }

    Write-Host "Device gone. Waiting for it to come back..."
    $t0 = Get-Date
    if (-not (Wait-ForDevice $targetSer 60)) {
        Write-Host "Device did not return within 60 s." -ForegroundColor Red
        $lines += "| $c | NO_RETURN | - | device absent after 60 s |"
        continue
    }
    $recovery = ((Get-Date) - $t0).TotalSeconds
    # Poll until both boards report LINK UP (max 15 s), then record.
    $targetStatus = 'no-status'
    $peerCom = if ($Board -eq 'ap') { $staCom } else { $apCom }
    $peerSer = if ($Board -eq 'ap') { $staSer } else { $apSer }
    $peerStatus = 'no-status'
    $deadline = (Get-Date).AddSeconds(15)
    while ((Get-Date) -lt $deadline) {
        $targetStatus = Get-Status $targetCom $targetSer
        $peerStatus = Get-Status $peerCom $peerSer
        if (([string]$targetStatus -match 'link=UP') -and ([string]$peerStatus -match 'link=UP')) { break }
        Start-Sleep -Milliseconds 500
    }
    $linkTime = ((Get-Date) - $t0).TotalSeconds

    $bothUp = ($targetStatus -match 'link=UP') -and ($peerStatus -match 'link=UP')
    $note = if ($bothUp) { "PASS usb_back=$([math]::Round($recovery,1))s" } else { "FAIL - check link" }
    Write-Host ("  target: $targetStatus`n  peer:   $peerStatus`n  $note") -ForegroundColor $(if ($bothUp) {'Green'} else {'Red'})

    $lines += "| $c | $targetStatus | $peerStatus | $note |"
    ($lines -join "`n") | Out-File -FilePath $OutFile -Encoding UTF8
}

$lines += ""
$lines += "USB-back time is re-enumeration only; the <10 s link criterion is judged from LINK UP after boot."
$lines += "Run both `-Board ap` and `-Board sta` to cover AP-restart and STA-restart cases."
($lines -join "`n") | Out-File -FilePath $OutFile -Encoding UTF8
Write-Host "`nLog: $OutFile" -ForegroundColor Cyan
