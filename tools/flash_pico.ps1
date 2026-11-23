# Detect a Phantom Link Pico by USB serial, then flash it.
# Identity never depends on the COM port number. Dry run unless -Flash is passed.
[CmdletBinding()]
param(
    [ValidateSet('ap', 'sta')]
    [string]$Board,

    [switch]$Flash
)

$ErrorActionPreference = 'Stop'

function Fail([string]$code, [string]$message) {
    Write-Output "ERROR=$code"
    Write-Output "MESSAGE=$message"
    exit 1
}

$repo = Split-Path -Parent $PSScriptRoot
$mapPath = Join-Path $PSScriptRoot 'board_map.json'
if (-not (Test-Path $mapPath)) { Fail 'NO_MAP' "Missing $mapPath" }
$map = Get-Content $mapPath -Raw | ConvertFrom-Json

# App mode: USB\VID_2E8A&PID_0009\<serial>. BOOTSEL: RP2350 volume + USBSTOR serial.
$appDevs = Get-CimInstance Win32_PnPEntity -ErrorAction SilentlyContinue |
    Where-Object { $_.DeviceID -match '^USB\\VID_2E8A&PID_0009\\[0-9A-Fa-f]{16}$' }
$appSerials = @($appDevs | ForEach-Object { ($_.DeviceID -split '\\')[-1] })

$bootVol = Get-CimInstance Win32_Volume -ErrorAction SilentlyContinue |
    Where-Object { $_.Label -eq 'RP2350' -and $_.DriveLetter } |
    Select-Object -First 1

if ($appSerials.Count -ge 1) { $mode = 'app' }
elseif ($bootVol) { $mode = 'bootsel' }
else { Fail 'NO_DEVICE' 'No Pico in app mode and no RP2350 BOOTSEL volume found' }

if ($mode -eq 'app') {
    if ($Board) {
        $entryForBoard = $map.boards.PSObject.Properties[$Board].Value
        if (-not $entryForBoard) { Fail 'NO_BOARD' "No '$Board' entry in board_map.json" }
        $wanted = [string]$entryForBoard.bootsel_usb_serial
        $found = @($appSerials | Where-Object { $_ -eq $wanted } | Select-Object -First 1)
        if ($found.Count -eq 0) {
            Fail 'SERIAL_MISMATCH' "No Pico with serial $wanted for $Board"
        }
        $serial = $found[0]
    } elseif ($appSerials.Count -eq 1) {
        $serial = $appSerials[0]
    } else {
        Fail 'AMBIGUOUS' 'Two Picos in app mode; pass -Board ap or -Board sta'
    }
} else {
    $drive = $bootVol.DriveLetter
    if (-not (Test-Path (Join-Path $drive 'INFO_UF2.TXT'))) {
        Fail 'WRONG_BOOTLOADER' "$drive is not an RP2350 bootloader"
    }
    $disk = Get-CimInstance Win32_DiskDrive -ErrorAction SilentlyContinue |
        Where-Object { $_.InterfaceType -eq 'USB' -and $_.Model -match 'RP2350' } |
        Select-Object -First 1
    if (-not $disk) { Fail 'NO_SERIAL' 'Cannot read the BOOTSEL USB serial' }
    $serial = ($disk.PNPDeviceID -split '&')[-2]
}

if (-not $Board) {
    foreach ($prop in $map.boards.PSObject.Properties) {
        if ([string]$prop.Value.bootsel_usb_serial -eq $serial) { $Board = $prop.Name; break }
    }
    if (-not $Board) { Fail 'UNKNOWN_SERIAL' "Serial $serial is not enrolled in board_map.json" }
}
$entry = $map.boards.PSObject.Properties[$Board].Value
if (-not $entry) { Fail 'NO_BOARD' "No '$Board' entry in board_map.json" }
$expected = [string]$entry.bootsel_usb_serial
$match = $expected -and ($serial -eq $expected)

$uf2Path = Join-Path $repo ([string]$entry.uf2)
if (-not (Test-Path $uf2Path)) { Fail 'NO_UF2' "Missing $uf2Path" }
$uf2 = Get-Item $uf2Path
$uf2Sha = (Get-FileHash $uf2Path -Algorithm SHA256).Hash
$mainHash = (Get-FileHash (Join-Path $repo 'firmware\main.c') -Algorithm SHA256).Hash
$buildId = $mainHash.Substring(0, 8).ToLowerInvariant()
$banner = "phantom-link v1 $buildId $($entry.role)"

$pt = Get-ChildItem (Join-Path $env:USERPROFILE '.pico-sdk\picotool') -Filter picotool.exe -Recurse -ErrorAction SilentlyContinue |
    Sort-Object FullName | Select-Object -Last 1
if ($mode -eq 'app' -and -not $pt) { Fail 'NO_PICOTOOL' 'picotool not found under ~/.pico-sdk/picotool' }

$stale = $false
foreach ($src in @('firmware\main.c', 'firmware\oled.h')) {
    $srcPath = Join-Path $repo $src
    if ((Test-Path $srcPath) -and ((Get-Item $srcPath).LastWriteTime -gt $uf2.LastWriteTime)) { $stale = $true }
}
if (Get-ChildItem (Join-Path $repo 'firmware\proto') -Filter '*.c' -ErrorAction SilentlyContinue |
        Where-Object { $_.LastWriteTime -gt $uf2.LastWriteTime }) { $stale = $true }

Write-Output 'DETECTED=True'
Write-Output "MODE=$mode"
Write-Output "BOARD=$Board"
Write-Output "BOOTSEL_SERIAL=$serial"
Write-Output "USB_SERIAL=$serial"
Write-Output "MAP_MATCH=$([bool]$match)"
Write-Output "UF2_NAME=$($uf2.Name)"
Write-Output "UF2_BYTES=$($uf2.Length)"
Write-Output "UF2_SHA256=$uf2Sha"
Write-Output "BUILD_ID=$buildId"
Write-Output "BANNER=$banner"
Write-Output "STALE=$stale"
Write-Output 'FLASHED=False'

if (-not $match) { Fail 'SERIAL_MISMATCH' "Serial $serial is not the enrolled serial for $Board" }
if (-not $Flash) { Write-Output 'STATUS=ready'; exit 0 }

if ($mode -eq 'app') {
    # Options must precede the positional filename in this picotool build.
    & $pt.FullName load --ser $serial -f -v -x $uf2Path 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) { Fail 'FLASH_FAILED' 'picotool load failed' }
} else {
    Copy-Item -LiteralPath $uf2Path -Destination (Join-Path $drive 'firmware.uf2') -Force
    $deadline = (Get-Date).AddSeconds(25)
    while ((Get-Date) -lt $deadline -and (Test-Path (Join-Path $drive 'INFO_UF2.TXT'))) {
        Start-Sleep -Milliseconds 250
    }
    if (Test-Path (Join-Path $drive 'INFO_UF2.TXT')) { Fail 'NO_REBOOT' 'BOOTSEL volume stayed mounted' }
}
Write-Output 'FLASHED=True'

# Wait for the same serial to reappear in app mode.
$deadline = (Get-Date).AddSeconds(40)
$back = $false
while ((Get-Date) -lt $deadline) {
    $dev = Get-CimInstance Win32_PnPEntity -ErrorAction SilentlyContinue |
        Where-Object { $_.DeviceID -eq "USB\VID_2E8A&PID_0009\$serial" }
    if ($dev) { $back = $true; break }
    Start-Sleep -Milliseconds 400
}
if (-not $back) { Fail 'NO_APP_REBOOT' "Pico $serial did not return to app mode" }
Write-Output 'APP_REBOOTED=True'
Write-Output 'STATUS=complete'
exit 0
