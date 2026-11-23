#requires -Version 5.1
<#
.SYNOPSIS
    One-shot replay acceptance run against the Phantom Link AP.

.DESCRIPTION
    Creates/uses a Wi-Fi profile for the Phantom Link AP (SSID TMOBILE-1219), connects the named
    adapter, assigns a static address in 192.168.4.0/24 with no default
    gateway, runs replay_probe.ps1, then restores DHCP and disconnects.

    Requires an elevated PowerShell window (static IP configuration). The
    default route stays on the wired adapter because this script never adds a
    gateway to the Wi-Fi interface.

.PARAMETER SessionId
    Live session ID from the board's SESSION stats line.

.EXAMPLE
    .\tools\ap_replay_session.ps1 -SessionId 0x532768e2 -Seq 139000
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [uint32]$SessionId,

    [uint32]$Seq = 139000,

    [string]$InterfaceAlias = 'Wi-Fi 2',

    [string]$Ssid = 'TMOBILE-1219',

    [string]$Password = '12345678',

    [string]$Address = '192.168.4.3',

    [int]$PrefixLength = 24,

    [string]$Target = '192.168.4.1',

    [int]$Port = 4444,

    [int]$Repeat = 2,

    [string]$LogDir = (Join-Path $PSScriptRoot '..\logs')
)

$ErrorActionPreference = 'Stop'

$principal = New-Object Security.Principal.WindowsPrincipal(
    [Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'This script must run elevated (static IP configuration).'
}

New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$logPath = Join-Path $LogDir ('ap_replay_{0}.log' -f $stamp)
$consolePath = Join-Path $LogDir ('ap_replay_{0}.console.log' -f $stamp)
Start-Transcript -Path $consolePath -Force | Out-Null

$profileXml = Join-Path $env:TEMP 'phantom_link_wlan.xml'
@"
<?xml version="1.0"?>
<WLANProfile xmlns="http://www.microsoft.com/networking/WLAN/profile/v1">
  <name>$Ssid</name>
  <SSIDConfig>
    <SSID>
      <name>$Ssid</name>
    </SSID>
  </SSIDConfig>
  <connectionType>ESS</connectionType>
  <connectionMode>manual</connectionMode>
  <MSM>
    <security>
      <authEncryption>
        <authentication>WPA2PSK</authentication>
        <encryption>AES</encryption>
        <useOneX>false</useOneX>
      </authEncryption>
      <sharedKey>
        <keyType>passPhrase</keyType>
        <protected>false</protected>
        <keyMaterial>$Password</keyMaterial>
      </sharedKey>
    </security>
  </MSM>
</WLANProfile>
"@ | Set-Content -Path $profileXml -Encoding ASCII

$staticApplied = $false
try {
    Write-Host ("Profile: {0}" -f $profileXml)
    netsh wlan add profile filename="$profileXml" user=current | Out-Null

    Write-Host ("Connecting {0} to {1}..." -f $InterfaceAlias, $Ssid)
    netsh wlan connect name="$Ssid" ssid="$Ssid" interface="$InterfaceAlias" | Out-Null

    $connected = $false
    for ($i = 0; $i -lt 40; $i++) {
        Start-Sleep -Milliseconds 500
        $adapter = Get-NetAdapter -Name $InterfaceAlias -ErrorAction SilentlyContinue
        if ($adapter -and $adapter.Status -eq 'Up') {
            $connected = $true
            break
        }
    }
    if (-not $connected) {
        throw ("{0} did not reach Up state; is {1} in range?" -f $InterfaceAlias, $Ssid)
    }

    Write-Host ("Assigning {0}/{1} with no gateway..." -f $Address, $PrefixLength)
    Set-NetIPInterface -InterfaceAlias $InterfaceAlias -AddressFamily IPv4 -Dhcp Disabled
    Get-NetIPAddress -InterfaceAlias $InterfaceAlias -AddressFamily IPv4 -ErrorAction SilentlyContinue |
        Where-Object { $_.IPAddress -notlike '169.254.*' } |
        Remove-NetIPAddress -Confirm:$false -ErrorAction SilentlyContinue
    New-NetIPAddress -InterfaceAlias $InterfaceAlias -AddressFamily IPv4 `
        -IPAddress $Address -PrefixLength $PrefixLength | Out-Null
    $staticApplied = $true

    Write-Host 'Adapter IPv4 addresses now:'
    Get-NetIPAddress -InterfaceAlias $InterfaceAlias -AddressFamily IPv4 -ErrorAction SilentlyContinue |
        ForEach-Object {
            Write-Host ('  {0}/{1} origin={2} state={3}' -f $_.IPAddress, $_.PrefixLength, $_.PrefixOrigin, $_.AddressState)
        }

    Start-Sleep -Seconds 1
    $defaultRoute = Get-NetRoute -DestinationPrefix '0.0.0.0/0' -ErrorAction SilentlyContinue |
        Sort-Object RouteMetric | Select-Object -First 1
    if ($defaultRoute) {
        Write-Host ("Default route stays on: {0} -> {1}" -f $defaultRoute.InterfaceAlias, $defaultRoute.NextHop)
    }

    Write-Host ''
    & powershell -NoProfile -ExecutionPolicy Bypass `
        -File (Join-Path $PSScriptRoot 'replay_probe.ps1') `
        -SessionId $SessionId -Seq $Seq -Target $Target -Port $Port -Repeat $Repeat `
        2>&1 | Tee-Object -FilePath $logPath

    Write-Host ''
    Write-Host 'Waiting 3 s for the board serial log to settle...'
    Start-Sleep -Seconds 3
}
finally {
    if ($staticApplied) {
        Remove-NetIPAddress -IPAddress $Address -Confirm:$false -ErrorAction SilentlyContinue
    }
    Set-NetIPInterface -InterfaceAlias $InterfaceAlias -AddressFamily IPv4 -Dhcp Enabled -ErrorAction SilentlyContinue
    netsh wlan disconnect interface="$InterfaceAlias" | Out-Null
    Write-Host ("Restored DHCP and disconnected {0}." -f $InterfaceAlias)
    Write-Host ("Transcript: {0}" -f $logPath)
    Stop-Transcript -ErrorAction SilentlyContinue | Out-Null
}
