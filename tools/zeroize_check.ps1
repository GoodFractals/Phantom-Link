# Ticket 36 - hardware zeroization check.
# Watch a board's serial for "SECRETS zeroized" while the operator holds the
# teardown chord (SELECT + DELETE/BACK for 2.5 s). Then confirm LINK is down
# and keys are re-derived on the next handshake (SSTATUS shows a fresh session).
param(
    [ValidateSet('ap','sta')][string]$Board = 'sta',
    [int]$Seconds = 20
)
$ErrorActionPreference='Stop'
$repo = Split-Path -Parent $PSScriptRoot
$map = Get-Content (Join-Path $PSScriptRoot 'board_map.json') -Raw | ConvertFrom-Json
$com = if ($Board -eq 'ap') { [string]$map.boards.ap.com } else { [string]$map.boards.sta.com }

Write-Host "=== Ticket 36 zeroization check ($Board / $com) ===" -ForegroundColor Yellow
Write-Host "Hold SELECT + DELETE (or BACK) on the $Board board for 2.5 s NOW."
$sp=$null
for($a=0;$a -lt 12;$a++){
  try{ $sp=[System.IO.Ports.SerialPort]::new($com,115200); $sp.DtrEnable=$true; $sp.ReadTimeout=300; $sp.Open(); break }
  catch{ $sp=$null; Start-Sleep -Milliseconds 300 }
}
if(-not $sp){ throw "Cannot open $com" }
$start=Get-Date; $buf=''; $zeroized=$false
while(((Get-Date)-$start).TotalSeconds -lt $Seconds){
  try{ $buf += $sp.ReadExisting() }catch{}
  if($buf -match 'SECRETS zeroized'){ $zeroized=$true; break }
  Start-Sleep -Milliseconds 100
}
try{$sp.Dispose()}catch{}
Write-Output ("zeroized_detected=" + $zeroized)
($buf -split "`r?`n" | Where-Object {$_ -match 'TEARDOWN|BYE|SECRETS|LINK LOST|SESSION'} | Select-Object -First 10)