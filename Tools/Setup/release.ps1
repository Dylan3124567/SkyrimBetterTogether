# Builds a release from a payload ZIP: SkyrimBetterTogether-<version>.zip for the updater (the Better Together files, its
# version file and the updater; no Address Library, which players install once and setup keeps), its signature, and
# SkyrimBetterTogetherSetup.exe with that ZIP inside. Only -Publish uploads them, as GitHub release v<version>.
param(
    [Parameter(Mandatory = $true)][string]$Version,
    [Parameter(Mandatory = $true)][string]$Payload,
    [Parameter(Mandatory = $true)][string]$OutputDirectory,
    [string]$KeyFile = 'A:\SkyrimMultiplayerBuild\signing\update-key.xml',
    [switch]$Publish,
    [string]$Repo = 'Dylan3124567/SkyrimBetterTogether',
    [string]$Notes)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem
$Version = ([Version]$Version).ToString()
$Payload = (Resolve-Path -LiteralPath $Payload).Path
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$OutputDirectory = (Resolve-Path -LiteralPath $OutputDirectory).Path
$zip = Join-Path $OutputDirectory "SkyrimBetterTogether-$Version.zip"
$setup = Join-Path $OutputDirectory 'SkyrimBetterTogetherSetup.exe'
Remove-Item -LiteralPath $zip, "$zip.sig", $setup -ErrorAction SilentlyContinue
$work = Join-Path $OutputDirectory ('build-' + [guid]::NewGuid().ToString('N'))
try {
    & "$PSScriptRoot\build.ps1" -OutputDirectory "$work\updater" | Out-Null
    $updater = "$work\updater\BetterTogetherUpdater.exe"
    # The updater's own Install.Payload and Updater.Sign, so the ZIP holds exactly what setup installs and the signature
    # is checked against the key the players' updaters trust. Loaded from bytes so the build folder can be removed.
    $assembly = [Reflection.Assembly]::Load([IO.File]::ReadAllBytes($updater))
    $install = $assembly.GetType('Install'); $updaterType = $assembly.GetType('Updater')
    $replaced = @($updaterType::VersionFile, 'SkyrimTogetherReborn/BetterTogetherUpdater.exe', 'SkyrimTogetherReborn/AddressLibrary-README.txt')
    $sha = [Security.Cryptography.SHA256]::Create()
    $sums = New-Object Text.StringBuilder
    $kept = New-Object Collections.Generic.List[string]
    $left = New-Object Collections.Generic.List[string]
    $source = [IO.Compression.ZipFile]::OpenRead($Payload)
    $target = [IO.Compression.ZipFile]::Open($zip, [IO.Compression.ZipArchiveMode]::Create)
    try {
        $add = {
            param([string]$name, [byte[]]$bytes)
            $stream = $target.CreateEntry($name, [IO.Compression.CompressionLevel]::Optimal).Open()
            try { $stream.Write($bytes, 0, $bytes.Length) } finally { $stream.Dispose() }
            [void]$sums.Append([BitConverter]::ToString($sha.ComputeHash($bytes)).Replace('-', '').ToLowerInvariant() + '  ' + $name + "`n")
            $kept.Add($name)
        }
        foreach ($entry in $source.Entries) {
            $name = $entry.FullName
            if ($entry.Name -eq '') { continue }
            if (-not $install::Payload($name) -or $name -like 'SKSE/Plugins/versionlib-*.bin' -or $replaced -contains $name) { $left.Add($name); continue }
            $buffer = New-Object IO.MemoryStream
            $stream = $entry.Open()
            try { $stream.CopyTo($buffer) } finally { $stream.Dispose() }
            & $add $name ($buffer.ToArray())
        }
        & $add $updaterType::VersionFile ([Text.Encoding]::ASCII.GetBytes($Version))
        & $add 'SkyrimTogetherReborn/BetterTogetherUpdater.exe' ([IO.File]::ReadAllBytes($updater))
        foreach ($required in 'SkyrimTogetherReborn/SkyrimTogether.exe', 'SkyrimTogetherReborn/STServer.dll', 'SkyrimTogether.esp') {
            if (-not $kept.Contains($required)) { throw "The payload has no $required" }
        }
        $stream = $target.CreateEntry('SHA256SUMS.txt').Open()
        try { $bytes = (New-Object Text.UTF8Encoding($false)).GetBytes($sums.ToString()); $stream.Write($bytes, 0, $bytes.Length) } finally { $stream.Dispose() }
    } finally { $target.Dispose(); $source.Dispose() }
    $signature = $updaterType::Sign($zip, $KeyFile)
    & "$PSScriptRoot\build.ps1" -OutputDirectory "$work\setup" -Package $zip | Out-Null
    Move-Item -LiteralPath "$work\setup\SkyrimBetterTogetherSetup.exe" -Destination $setup
} finally { if (Test-Path -LiteralPath $work) { Remove-Item -LiteralPath $work -Recurse -Force } }

Write-Output "Kept $($kept.Count) files; left out $($left.Count): $($left -join ', ')"
if (-not $Notes) {
    $Notes = Join-Path $OutputDirectory "SkyrimBetterTogether-$Version-notes.md"
    Set-Content -LiteralPath $Notes -Encoding UTF8 -Value @(
        "Skyrim Better Together $Version.",
        '',
        'Everyone in a session needs this same version.',
        'Installed copies update themselves when SkyrimTogether.exe starts. New players: run SkyrimBetterTogetherSetup.exe (needs Address Library v13 for Skyrim 1.7.104.0).')
}
$arguments = @('release', 'create', "v$Version", $zip, $signature, $setup, '--repo', $Repo, '--title', "Skyrim Better Together $Version", '--notes-file', $Notes)
if ($Publish) {
    & gh @arguments
    if ($LASTEXITCODE -ne 0) { throw 'gh release create failed' }
} else {
    Write-Output ('Not published (no -Publish). Would run: gh ' + (($arguments | ForEach-Object { if ($_ -match '\s') { '"' + $_ + '"' } else { $_ } }) -join ' '))
}
Get-Item -LiteralPath $zip, $signature, $setup | ForEach-Object { Write-Output ('{0}  {1:N0} bytes' -f $_.FullName, $_.Length) }
