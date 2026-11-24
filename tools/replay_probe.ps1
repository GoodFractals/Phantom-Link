#requires -Version 5.1
<#
.SYNOPSIS
    Send a stale Phantom Link frame to the AP and confirm it is rejected.

.DESCRIPTION
    Builds a valid protocol-v1 frame (magic, CRC32, auth padding) with a
    deliberately old sequence number and sends it over UDP to the AP. The
    board's replay window must drop it before the application sees it.

    Run this from a PC joined to the Phantom Link AP network (SSID TMOBILE-1219) with a static
    address in 192.168.4.0/24. The PC must not use 192.168.4.1 or .2.

    This is a plaintext acceptance probe. It proves replay rejection only;
    it says nothing about encryption.

.PARAMETER SessionId
    The live session ID. Read it from the board's "SESSION stats" line, e.g.
    id=0x532768e2 -> -SessionId 0x532768e2.

.PARAMETER Seq
    Sequence number to put in the frame. The default is far below the live
    window, so the receiver must report it as stale.

.PARAMETER Type
    Frame type to send. PING is the default because it has no payload and the
    board would answer a fresh PING with PONG; a rejected one gets no reply.

.EXAMPLE
    .\tools\replay_probe.ps1 -SessionId 0x532768e2 -Seq 139000

.EXAMPLE
    .\tools\replay_probe.ps1 -SessionId 0x532768e2 -Seq 139000 -DryRun

.EXAMPLE
    .\tools\replay_probe.ps1 -SelfTest
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $false)]
    [uint32]$SessionId = 0,

    [uint32]$Seq = 139000,

    [string]$RawHex = '',

    [ValidateSet('HELLO', 'HELLO_ACK', 'DATA', 'ACK', 'PING', 'PONG', 'BYE', 'SFO', 'ERROR')]
    [string]$Type = 'PING',

    [string]$Target = '192.168.4.1',

    [int]$Port = 4444,

    [string]$BindAddress = '192.168.4.3',

    [ValidateRange(1, 100)]
    [int]$Repeat = 2,

    [switch]$DryRun,

    [switch]$SelfTest
)

$ErrorActionPreference = 'Stop'

$TypeCodes = @{
    HELLO = 1
    HELLO_ACK = 2
    DATA = 3
    ACK = 4
    PING = 5
    PONG = 6
    BYE = 7
    SFO = 8
    ERROR = 9
}

function Get-Crc32 {
    param([Parameter(Mandatory = $true)][byte[]]$Data)

    [uint32]$crc = [uint32]::MaxValue
    [uint32]$poly = [uint32]3988292384
    foreach ($b in $Data) {
        $crc = [uint32]($crc -bxor [uint32]$b)
        for ($bit = 0; $bit -lt 8; $bit++) {
            if (($crc -band [uint32]1) -ne 0) {
                $crc = [uint32](($crc -shr 1) -bxor $poly)
            }
            else {
                $crc = [uint32]($crc -shr 1)
            }
        }
    }
    return [uint32]($crc -bxor [uint32]::MaxValue)
}

function New-Frame {
    param(
        [uint32]$Session,
        [uint32]$Sequence,
        [byte]$FrameType
    )

    $payload = [byte[]]@()
    $payloadLen = $payload.Length
    $covered = 18 + $payloadLen
    $frame = New-Object byte[] ($covered + 8)

    $frame[0] = [byte][char]'B'
    $frame[1] = [byte][char]'C'
    $frame[2] = 1
    $frame[3] = $FrameType
    $frame[4] = 0
    $frame[5] = [byte]$payloadLen
    [BitConverter]::GetBytes([uint32]$Session).CopyTo($frame, 6)
    [BitConverter]::GetBytes([uint32]$Sequence).CopyTo($frame, 10)
    [BitConverter]::GetBytes([uint32]0).CopyTo($frame, 14)
    if ($payloadLen -gt 0) {
        $payload.CopyTo($frame, 18)
    }

    $crc = Get-Crc32 -Data $frame[0..($covered - 1)]
    [BitConverter]::GetBytes([uint32]$crc).CopyTo($frame, $covered)

    return ,$frame
}

function Format-FrameHex {
    param([byte[]]$Bytes)
    return (($Bytes | ForEach-Object { $_.ToString('X2') }) -join ' ')
}

if ($SelfTest) {
    $check = Get-Crc32 -Data ([Text.Encoding]::ASCII.GetBytes('123456789'))
    if ($check -ne [uint32]3421780262) {
        throw ('CRC32 self-test failed: 0x{0:X8} (expected 0xCBF43926)' -f $check)
    }
    Write-Host 'CRC32 self-test: PASS (0xCBF43926)'
    return
}

if ($RawHex) {
    $clean = ($RawHex -replace '[^0-9A-Fa-f]', '')
    if ($clean.Length % 2 -ne 0) { throw 'RawHex must contain an even number of hex digits.' }
    $frame = New-Object byte[] ($clean.Length / 2)
    for ($i = 0; $i -lt $frame.Length; $i++) {
        $frame[$i] = [Convert]::ToByte($clean.Substring($i * 2, 2), 16)
    }
    $hex = Format-FrameHex -Bytes $frame
} else {
    if ($SessionId -eq 0) {
        throw 'SessionId is required (use -SessionId 0x........ from the SESSION stats line).'
    }
    $frame = New-Frame -Session $SessionId -Sequence $Seq -FrameType $TypeCodes[$Type]
    $hex = Format-FrameHex -Bytes $frame
}

Write-Host 'Phantom Link raw frame probe'
Write-Host ('  target:      {0}:{1}/udp' -f $Target, $Port)
Write-Host ('  bind:        {0}' -f $BindAddress)
if (-not $RawHex) {
    Write-Host ('  session:     0x{0:X8}' -f $SessionId)
    Write-Host ('  type:        {0} ({1})' -f $Type, $TypeCodes[$Type])
    Write-Host ('  seq:         {0} (stale)' -f $Seq)
}
Write-Host ('  wire length: {0} bytes' -f $frame.Length)
Write-Host ('  frame:       {0}' -f $hex)
Write-Host ''
Write-Host 'Expected on the receiving board:'
if ($RawHex) {
    Write-Host '  rejection logged (crypto or stale); accepted and crypto counters confirm.'
} else {
    Write-Host ('  RX stale type={0} seq={1} id=0x{2:X8} (... stale=N ...)' -f $Type, $Seq, $SessionId)
    Write-Host '  accepted must not increase.'
}
Write-Host ''

if ($DryRun) {
    Write-Host 'Dry run: nothing sent.'
    return
}

$deadline = (Get-Date).AddSeconds(20)
while (-not (Get-NetIPAddress -AddressFamily IPv4 -IPAddress $BindAddress -ErrorAction SilentlyContinue |
            Where-Object { $_.AddressState -eq 'Preferred' })) {
    if ((Get-Date) -gt $deadline) {
        throw ("Local address {0} did not reach Preferred state within 20 s. Join the TMOBILE-1219 AP and set a static address in 192.168.4.0/24 first." -f $BindAddress)
    }
    Start-Sleep -Milliseconds 300
}

# Give Windows a moment to finish duplicate-address detection before binding.
Start-Sleep -Milliseconds 500

$endpoint = New-Object System.Net.IPEndPoint(
    [System.Net.IPAddress]::Parse($BindAddress), 0)
$udp = New-Object System.Net.Sockets.UdpClient($endpoint)
try {
    for ($i = 1; $i -le $Repeat; $i++) {
        [void]$udp.Send($frame, $frame.Length, $Target, $Port)
        Write-Host ('Sent {0}/{1} to {2}:{3}' -f $i, $Repeat, $Target, $Port)
        Start-Sleep -Milliseconds 250
    }
}
finally {
    $udp.Close()
}

Write-Host 'Done. Compare the board log against the expected stale line above.'
