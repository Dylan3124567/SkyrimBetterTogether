# Creates the release signing key once and prints its public half, which Updater.cs embeds as PublicKey.
# The private key stays in this one file: never commit, copy or print it. Losing it means players must reinstall with
# a setup carrying a new public key, so keep a backup of this file somewhere offline.
param([string]$KeyFile = 'A:\SkyrimMultiplayerBuild\signing\update-key.xml')
$ErrorActionPreference = 'Stop'
$rsa = New-Object Security.Cryptography.RSACryptoServiceProvider 3072
try {
    if (Test-Path -LiteralPath $KeyFile) {
        $rsa.FromXmlString([IO.File]::ReadAllText($KeyFile))
        Write-Output "Existing key kept: $KeyFile"
    } else {
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $KeyFile) | Out-Null
        # CreateNew: never replaces a key that appeared meanwhile
        $writer = New-Object IO.StreamWriter((New-Object IO.FileStream($KeyFile, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write)))
        try { $writer.Write($rsa.ToXmlString($true)) } finally { $writer.Dispose() }
        Write-Output "Created $KeyFile"
    }
    $public = $rsa.ToXmlString($false)
    $sha = [Security.Cryptography.SHA256]::Create()
    Write-Output "Public key (Updater.cs PublicKey):"
    Write-Output $public
    Write-Output ('SHA-256 of the public key XML: ' + [BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($public))).Replace('-', '').ToLowerInvariant())
} finally { $rsa.Dispose() }
