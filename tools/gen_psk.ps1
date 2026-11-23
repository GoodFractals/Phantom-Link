# Prints a random 32-byte PSK as a C initializer for proto/psk.h.
$bytes = New-Object byte[] 32
[System.Security.Cryptography.RandomNumberGenerator]::Create().GetBytes($bytes)
$lines = @()
for ($i = 0; $i -lt 32; $i += 8) {
    $chunk = ($bytes[$i..($i + 7)] | ForEach-Object { '0x{0:X2}' -f $_ }) -join ', '
    $lines += "    $chunk," 
}
Write-Output '#define PHANTOM_PSK_BYTES { \'
$lines | ForEach-Object { Write-Output $_ }
Write-Output '}'
