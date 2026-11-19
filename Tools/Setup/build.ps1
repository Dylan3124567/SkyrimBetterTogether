param([string]$OutputDirectory = 'A:\SkyrimMultiplayerBuild\Setup', [string]$Package, [switch]$Friend, [switch]$Modded, [switch]$Update)
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$compiler = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
$resources = @()
$name = if ($Update) { "SkyrimBetterTogetherModdedUpdate.exe" } elseif ($Modded) { "SkyrimBetterTogetherModdedFriendSetup.exe" } elseif ($Friend) { "SkyrimBetterTogetherFriendSetup.exe" } else { "SkyrimBetterTogetherSetup.exe" }
if ($Update) { $resources += "/define:FRIEND;MODDED;UPDATE" } elseif ($Modded) { $resources += "/define:FRIEND;MODDED" } elseif ($Friend) { $resources += "/define:FRIEND" }
if ($Package -and -not $Modded) { $resources += @("/resource:$((Resolve-Path -LiteralPath $Package).Path),Payload.zip") }
& $compiler @resources /nologo /target:winexe /platform:x64 /optimize+ /r:System.Windows.Forms.dll /r:System.Drawing.dll /r:System.IO.Compression.dll /r:System.IO.Compression.FileSystem.dll /r:System.Web.Extensions.dll "/out:$OutputDirectory\$name" "$PSScriptRoot\Setup.cs"
if ($LASTEXITCODE -ne 0) { throw 'Setup compilation failed' }
# Started by SkyrimTogether.exe from Data\SkyrimTogetherReborn; release.ps1 puts it in the release ZIP
& $compiler /nologo /target:winexe /platform:x64 /optimize+ /main:Updater /r:System.Windows.Forms.dll /r:System.Drawing.dll /r:System.IO.Compression.dll /r:System.IO.Compression.FileSystem.dll /r:System.Web.Extensions.dll "/out:$OutputDirectory\BetterTogetherUpdater.exe" "$PSScriptRoot\Setup.cs" "$PSScriptRoot\Updater.cs"
if ($LASTEXITCODE -ne 0) { throw 'Updater compilation failed' }
if ($Package -and $Modded) {
    # Too big to embed: Windows won't start an executable with a ~2 GB image. Setup.cs reads this back from its own file.
    $source = [IO.File]::OpenRead((Resolve-Path -LiteralPath $Package).Path)
    $target = [IO.File]::Open("$OutputDirectory\$name", [IO.FileMode]::Append, [IO.FileAccess]::Write)
    try {
        $source.CopyTo($target)
        $footer = [BitConverter]::GetBytes([long]$source.Length)
        $target.Write($footer, 0, 8)
        $magic = [Text.Encoding]::ASCII.GetBytes('SBTPACK1')
        $target.Write($magic, 0, 8)
    } finally { $target.Dispose(); $source.Dispose() }
}
Write-Output "$OutputDirectory\$name" "$OutputDirectory\BetterTogetherUpdater.exe"
