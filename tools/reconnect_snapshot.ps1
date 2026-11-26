# One-shot snapshot for ticket 18: read STATUS on both boards and append a
# row to the cycle log. Called after the operator confirms a power cycle.
param([ValidateSet('ap','sta')][string]$Board = 'sta', [int]$Cycle = 1, [string]$Log = '')
$ErrorActionPreference='Stop'
$repo = Split-Path -Parent $PSScriptRoot
if (-not $Log) {
  $Log = Join-Path $repo ("logs\reconnect18\cycles_{0}_manual.md" -f $Board)
}
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Log) | Out-Null

function Get-Link([string]$com) {
  $sp=$null
  for($a=0;$a -lt 12;$a++){
    try{ $sp=[System.IO.Ports.SerialPort]::new($com,115200); $sp.DtrEnable=$true; $sp.ReadTimeout=400; $sp.Open(); break }
    catch{ if($sp){try{$sp.Dispose()}catch{}}; $sp=$null; Start-Sleep -Milliseconds 300 }
  }
  if(-not $sp){ return 'unavailable' }
  try{
    Start-Sleep -Milliseconds 300
    $sp.Write('S'); Start-Sleep -Seconds 1
    $o=''; while($sp.BytesToRead -gt 0){ $o += $sp.ReadExisting() }
    $l=($o -split "`r?`n" | Where-Object {$_ -match 'link='} | Select-Object -First 1) -replace '\s+',' '
    return $l.Trim()
  } finally { if($sp){ try{$sp.Dispose()}catch{} } }
}

$ap = Get-Link 'COM5'
$sta = Get-Link 'COM4'
$bothUp = ($ap -match 'link=UP') -and ($sta -match 'link=UP')
$note = if($bothUp){'PASS'}else{'FAIL'}
Write-Output "AP:  $ap"
Write-Output "STA: $sta"
Write-Output "RESULT: $note"

if (-not (Test-Path $Log)) {
  "# Ticket 18 - reconnect cycles ($Board)`n`nBuild: $((Get-FileHash (Join-Path $repo 'firmware\main.c') -Algorithm SHA256).Hash.Substring(0,8))`n`n| Cycle | AP | STA | Result |`n|---|---|---|---|" | Out-File -FilePath $Log -Encoding UTF8
}
"| $Cycle | $ap | $sta | $note |" | Out-File -FilePath $Log -Append -Encoding UTF8
