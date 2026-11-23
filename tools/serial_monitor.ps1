#requires -Version 5.1
<#
.SYNOPSIS
    Auto-reconnecting serial monitor with raw log capture.

.DESCRIPTION
    Opens a Windows COM port at 115200 8N1 (configurable), prints incoming
    bytes to the console, and appends them to a timestamped log file. If the
    Pico resets and the CDC port disappears, the script waits and reopens it.

    Start this before resetting or flashing the board. The firmware waits
    1.5 seconds after USB stdio initialization, which gives the monitor time
    to reopen and catch the banner.

.EXAMPLE
    .\tools\serial_monitor.ps1 -Port COM5

.EXAMPLE
    .\tools\serial_monitor.ps1 -Port COM6 -LogDir C:\temp\phantom-logs
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Port,

    [int]$Baud = 115200,

    [string]$LogDir = (Join-Path $PSScriptRoot '..\logs')
)

$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force -Path $LogDir | Out-Null

$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$logPath = Join-Path $LogDir ("{0}_{1}.log" -f $Port, $stamp)
New-Item -ItemType File -Force -Path $logPath | Out-Null

Write-Host "Serial monitor: $Port at $Baud 8N1"
Write-Host "Log file:       $logPath"
Write-Host "Start this before resetting the Pico. Press Ctrl+C to stop."
Write-Host ''

while ($true) {
    $serial = $null

    try {
        $serial = [System.IO.Ports.SerialPort]::new(
            $Port,
            $Baud,
            [System.IO.Ports.Parity]::None,
            8,
            [System.IO.Ports.StopBits]::One
        )
        $serial.Handshake = [System.IO.Ports.Handshake]::None
        $serial.DtrEnable = $false
        $serial.RtsEnable = $false
        $serial.ReadTimeout = 250
        $serial.Open()

        Write-Host "[opened $Port]"

        while ($serial.IsOpen) {
            try {
                $text = $serial.ReadExisting()
                if ($text.Length -gt 0) {
                    [Console]::Out.Write($text)
                    [System.IO.File]::AppendAllText($logPath, $text)
                }
                else {
                    Start-Sleep -Milliseconds 25
                }
            }
            catch [System.TimeoutException] {
                Start-Sleep -Milliseconds 25
            }
        }
    }
    catch {
        Write-Host "[waiting for ${Port}: $($_.Exception.Message)]"
    }
    finally {
        if ($null -ne $serial) {
            try {
                if ($serial.IsOpen) {
                    $serial.Close()
                }
            }
            catch {
                # The port may already be gone after a reset.
            }
            $serial.Dispose()
        }
    }

    Start-Sleep -Milliseconds 400
}
