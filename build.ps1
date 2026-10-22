param(
    [string]$ArduinoCli = 'arduino-cli'
)

$ErrorActionPreference = 'Stop'
$arpBuild = Join-Path $PSScriptRoot 'build'
$arpCompile = Join-Path $arpBuild 'arduino'
New-Item -ItemType Directory -Path $arpCompile -Force | Out-Null
$arpVersion = '0.2.0-beta.1'
$arpFqbn = 'esp32:esp32:esp32s3:USBMode=default,CDCOnBoot=default,FlashSize=16M,PartitionScheme=custom,PSRAM=disabled,CPUFreq=240,FlashMode=dio'
$arpBoardJson = & $ArduinoCli board details --fqbn $arpFqbn --format json
if ($LASTEXITCODE -ne 0) { throw 'Could not inspect installed ESP32 board package' }
$arpBoard = ($arpBoardJson -join "`n") | ConvertFrom-Json
if ($arpBoard.version -ne '3.3.2') {
    throw "Expected esp32:esp32@3.3.2; installed version is $($arpBoard.version)"
}
$arpLibraryJson = & $ArduinoCli lib list --format json
if ($LASTEXITCODE -ne 0) { throw 'Could not inspect installed Arduino libraries' }
$arpLibraries = (($arpLibraryJson -join "`n") | ConvertFrom-Json).installed_libraries
foreach ($arpDependency in @(@('Adafruit NeoPixel', '1.15.1'), @('NimBLE-Arduino', '2.3.1'))) {
    $arpInstalled = @($arpLibraries | Where-Object { $_.library.name -eq $arpDependency[0] })
    if ($arpInstalled.Count -ne 1 -or $arpInstalled[0].library.version -ne $arpDependency[1]) {
        throw "Install the pinned dependency: $($arpDependency[0])@$($arpDependency[1])"
    }
}

# Compile only. This script never opens a serial port or uploads system images.
& $ArduinoCli compile --fqbn $arpFqbn --build-property upload.maximum_size=2097152 `
    --build-path $arpCompile --warnings all $PSScriptRoot
if ($LASTEXITCODE -ne 0) { throw "Arduino compilation failed: $LASTEXITCODE" }

$arpApplication = Join-Path $arpCompile 'NeonOrbit.ino.bin'
$arpImage = [System.IO.File]::ReadAllBytes($arpApplication)
if ($arpImage.Length -gt 0x200000 -or $arpImage.Length -lt 24 -or $arpImage[0] -ne 0xE9) {
    throw 'Invalid application image, or App0 exceeds 2 MiB'
}
# ESP image extended header: little-endian chip ID 9 = ESP32-S3.
if (($arpImage[12] -bor ($arpImage[13] -shl 8)) -ne 9) { throw 'Image target is not ESP32-S3' }
$arpUpload = Join-Path $arpBuild "neon-orbit-v$arpVersion.bin"
Copy-Item -LiteralPath $arpApplication -Destination $arpUpload -Force
$arpHash = (Get-FileHash -LiteralPath $arpUpload -Algorithm SHA256).Hash.ToLowerInvariant()
$arpManifest = [ordered]@{
    name = 'Neon Orbit'
    version = $arpVersion
    application = "neon-orbit-v$arpVersion.bin"
    boardPackage = "esp32:esp32@$($arpBoard.version)"
    fqbn = $arpFqbn
    libraries = [ordered]@{ 'Adafruit NeoPixel' = '1.15.1'; 'NimBLE-Arduino' = '2.3.1' }
    bytes = $arpImage.Length
    sha256 = $arpHash
    app0Offset = '0x10000'
    app0MaximumBytes = 0x200000
    app1PreservedAt = '0x210000'
    prerelease = $true
    hardwareTested = $false
    validationNote = 'MIDI integration requires on-device USB/BLE interoperability testing; see VALIDATION.md.'
}
$arpManifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $arpBuild 'manifest.json') -Encoding utf8
Write-Output "App1 uploader application: $arpUpload"
Write-Output "Bytes: $($arpImage.Length); SHA256: $arpHash"
