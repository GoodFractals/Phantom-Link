#requires -Version 5.1
<#
.SYNOPSIS
    Ticket 14 path-1 evidence: a Windows pktmon capture of a real Phantom Link
    encrypted DATA frame, plus a check that the known plaintext is absent.

.DESCRIPTION
    Builds a valid protocol-v1 encrypted DATA frame with tools/frame_probe.c
    (same production crypto as the firmware), sends it as one UDP datagram,
    and captures it with pktmon at the NDIS layer. pktmon sits above WPA2, so
    this proves the application-layer payload is unreadable even to a party
    that has already decrypted the Wi-Fi layer.

    IMPORTANT LIMITATION: this is not a true 802.11 monitor capture. It proves
    the app-layer ciphertext claim only. The no-PSK RF capture is the ESP32
    job (ticket 14, second evidence item).

    pktmon requires an elevated PowerShell. Use -SkipCapture to generate and
    check the frame without admin rights.

.EXAMPLE
    .\tools\capture_probe.ps1 -SkipCapture

.EXAMPLE
    # Run from an elevated PowerShell:
    .\tools\capture_probe.ps1 -SessionId 0x040cbde1 -Seq 139000
#>
[CmdletBinding()]
param(
    [uint32]$SessionId = 0x040cbde1,
    [uint32]$Seq = 139000,
    [string]$Message = 'LINK ALIVE NO TEXT!!!',

    [string]$Target = '',
    [int]$Port = 4444,

    [string]$HostBuildDir = (Join-Path $PSScriptRoot '..\firmware\host\build'),
    [string]$OutDir = (Join-Path $PSScriptRoot '..\logs\capture14'),

    [switch]$SkipCapture
)

$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

function Get-Sha256Hex {
    param([string]$Path)
    return (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash
}

function Find-Bytes {
    param([byte[]]$Haystack, [byte[]]$Needle)
    if ($Needle.Length -eq 0 -or $Needle.Length -gt $Haystack.Length) { return -1 }
    for ($i = 0; $i -le $Haystack.Length - $Needle.Length; $i++) {
        $match = $true
        for ($j = 0; $j -lt $Needle.Length; $j++) {
            if ($Haystack[$i + $j] -ne $Needle[$j]) { $match = $false; break }
        }
        if ($match) { return $i }
    }
    return -1
}

function Get-GatewayAddress {
    $a = Get-NetRoute -DestinationPrefix '0.0.0.0/0' -ErrorAction SilentlyContinue |
         Where-Object { $_.NextHop -and $_.NextHop -ne '0.0.0.0' } |
         Select-Object -First 1
    if (-not $a) { throw 'No IPv4 default gateway found; pass -Target explicitly.' }
    return $a.NextHop
}

# --- 1. Build the frame generator -------------------------------------------
cmake -S (Join-Path $PSScriptRoot '..\firmware\host') -B $HostBuildDir | Out-Null
cmake --build $HostBuildDir --config Release --target frame_probe | Out-Null

$exeCandidates = @(
    (Join-Path $HostBuildDir 'Release\frame_probe.exe'),
    (Join-Path $HostBuildDir 'frame_probe.exe')
)
$exe = $exeCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (-not $exe) { throw 'frame_probe.exe was not produced by the build.' }

# --- 2. Generate a valid encrypted DATA frame ------------------------------
$sessionHex = '{0:X8}' -f $SessionId
$lines = & $exe capture $sessionHex $Seq $Message
$frameLine = $lines | Where-Object { $_ -like 'FRAME:*' }
$plainLine = $lines | Where-Object { $_ -like 'PLAINTEXT:*' }
if (-not $frameLine -or -not $plainLine) {
    throw ("Unexpected frame_probe output:`n{0}" -f ($lines -join "`n"))
}

$hex = ($frameLine -replace '^FRAME:\s*', '').Replace(' ', '').Trim()
if ($hex.Length % 2 -ne 0) { throw 'Frame hex has an odd number of digits.' }

$frame = [byte[]]::new($hex.Length / 2)
for ($i = 0; $i -lt $frame.Length; $i++) {
    $frame[$i] = [Convert]::ToByte($hex.Substring($i * 2, 2), 16)
}
$knownPlaintext = ($plainLine -replace '^PLAINTEXT:\s*', '').Trim()

# Target the gateway so the datagram leaves the NIC and is seen by pktmon as a
# real transmit. A local-only address is either logged as a drop (excluded by
# etl2pcap) or handled internally (never reaches the NDIS hooks).
if (-not $Target) { $Target = Get-GatewayAddress }

Write-Host 'Ticket 14 path-1 capture probe'
Write-Host ('  session:   0x{0}' -f $sessionHex)
Write-Host ('  seq:       {0}' -f $Seq)
Write-Host ('  target:    {0}:{1}/udp' -f $Target, $Port)
Write-Host ('  frame:     {0} bytes' -f $frame.Length)
Write-Host ('  plaintext: {0}' -f $knownPlaintext)

# --- 3. Negative control on the frame bytes themselves ---------------------
$frameAscii = [Text.Encoding]::ASCII.GetString($frame)
$frameContainsPlaintext = $frameAscii.Contains($knownPlaintext)
Write-Host ('  frame contains known plaintext: {0}' -f $frameContainsPlaintext)
if ($frameContainsPlaintext) { throw 'FAIL: readable plaintext found in frame.' }

if ($SkipCapture) {
    Write-Host 'Dry run (-SkipCapture): no pktmon capture, no evidence write.'
    return
}

# --- 4. pktmon capture around one UDP datagram -----------------------------
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'pktmon needs admin. Re-run this script from an elevated PowerShell, or use -SkipCapture.'
}

if (-not (Get-Command pktmon -ErrorAction SilentlyContinue)) {
    throw 'pktmon.exe was not found on PATH.'
}

$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$etlPath = Join-Path $OutDir ("capture14_{0}.etl" -f $stamp)
$pcapPath = Join-Path $OutDir ("capture14_{0}.pcapng" -f $stamp)
$evidencePath = Join-Path $OutDir ("capture14_{0}.md" -f $stamp)

Write-Host '  starting pktmon capture...'
& pktmon start --capture --pkt-size 0 --comp all --file-name $etlPath | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'pktmon start failed.' }

try {
    $endpoint = New-Object System.Net.IPEndPoint([Net.IPAddress]::Any, 0)
    $udp = New-Object Net.Sockets.UdpClient($endpoint)
    try {
        [void]$udp.Send($frame, $frame.Length, $Target, $Port)
    }
    finally { $udp.Close() }
    Write-Host ('  sent {0} bytes to {1}:{2}' -f $frame.Length, $Target, $Port)
}
finally {
    Write-Host '  stopping pktmon capture...'
    & pktmon stop | Out-Null
    if ($LASTEXITCODE -ne 0) { Write-Warning 'pktmon stop returned a non-zero code.' }
}

& pktmon etl2pcap $etlPath --out $pcapPath | Out-Null
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $pcapPath)) {
    throw 'pktmon etl2pcap failed to produce a pcapng.'
}

# --- 5. Scan the capture for the known plaintext ---------------------------
$pcapBytes = [IO.File]::ReadAllBytes($pcapPath)
$pcapAscii = [Text.Encoding]::ASCII.GetString($pcapBytes)
$pcapContainsPlaintext = $pcapAscii.Contains($knownPlaintext)
$frameOffset = Find-Bytes -Haystack $pcapBytes -Needle $frame

Write-Host ('  pcapng size: {0} bytes' -f $pcapBytes.Length)
Write-Host ('  frame byte offset in pcapng: {0}' -f $frameOffset)
Write-Host ('  capture contains known plaintext: {0}' -f $pcapContainsPlaintext)

# A PASS is only meaningful if the frame actually landed in the capture.
if ($frameOffset -lt 0) {
    throw 'FAIL: frame bytes are not in the capture; refusing to write a vacuous evidence file.'
}
if ($pcapContainsPlaintext) { throw 'FAIL: readable plaintext found in capture.' }

$buildId = (& git -C (Join-Path $PSScriptRoot '..') rev-parse --short HEAD).Trim()
$frameHash = (Get-FileHash -Algorithm SHA256 -InputStream (
    [IO.MemoryStream]::new([Text.Encoding]::ASCII.GetBytes($hex)))).Hash
$etlHash = Get-Sha256Hex $etlPath
$pcapHash = Get-Sha256Hex $pcapPath

$report = @"
# Ticket 14 - path-1 packet capture evidence

Date: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
Build: ``$buildId``

## Setup

- Frame generator: ``tools/frame_probe.c`` mode ``capture`` (production
  ``pc_suite_speck_ctr_cmac.seal`` and ``pc_kdf`` from ``PHANTOM_PSK_BYTES``).
- Session: ``0x$sessionHex``, seq: ``$Seq``.
- Known plaintext (kept out of the capture): ``$knownPlaintext``.
- Capture: Windows ``pktmon`` at NDIS layer, full packet size, one UDP datagram
  to ``$Target`:$Port``.

## Results

| Check | Result |
|---|---|
| Frame length | $($frame.Length) bytes |
| Known plaintext present in frame bytes | $frameContainsPlaintext |
| Known plaintext present in pcapng | $pcapContainsPlaintext |
| Frame byte offset in pcapng | $frameOffset (must be >= 0) |

## Artifacts

| File | SHA256 |
|---|---|
| ``$(Split-Path $pcapPath -Leaf)`` | ``$pcapHash`` |
| ``$(Split-Path $etlPath -Leaf)`` | ``$etlHash`` |
| frame hex (text) | ``$frameHash`` |

## Claim boundary

This shows the application-layer payload is unreadable at a point **above**
WPA2 (i.e. after the Wi-Fi layer has been decrypted). It is NOT a true 802.11
monitor capture and does NOT cover the no-PSK RF case; that requires the ESP32
sniffer and is a separate evidence item for ticket 14.
"@

Set-Content -LiteralPath $evidencePath -Value $report -Encoding UTF8
Write-Host ''
Write-Host ('Evidence: {0}' -f $evidencePath)
Write-Host ('  pcapng SHA256: {0}' -f $pcapHash)
Write-Host 'PASS: no readable message text in the capture.'
