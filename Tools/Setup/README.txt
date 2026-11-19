Skyrim Better Together Setup

Run SkyrimBetterTogetherSetup.exe and choose your Skyrim Special Edition folder.
The custom mod build is included: no separate ZIP or download is needed.
Choose Install / Update, or Uninstall this build to restore replaced files.
Keep setup outside the game folder.

Updates: SkyrimTogether.exe checks for a newer release when it starts and offers
"Update and restart". You can also run the newer all-in-one setup, or choose
another trusted release ZIP with Browse. Every player/server must use the same build.
This is a custom fork of Skyrim Together Reborn; upstream attribution and
licenses are retained. Internal plugin and runtime filenames stay compatible.

Requires Skyrim 1.7.104.0. Regular Setup needs Address Library v13 separately.
Friend Setup includes its matching database and backs up any existing copy.
Regular updates retain a dependency previously managed by Friend Setup. Normal
Windows .NET Framework 4.x is used; Python, PowerShell policies, and SDKs are
not required to run the wizard. Setup runs as your current user; if the game
directory denies writing, close setup and explicitly run it as administrator.

The selected game's .str-setup directory stores the file receipt, original
backups and transaction journal. Do not delete it while the mod is installed.
Locally edited managed files are preserved under .str-setup/preserved before
replacement/removal. Untracked files, saves, INIs and Address Library are never
removed. Empty directories and recovery backups remain after uninstall.

If another version was installed before setup, uninstall restores that previous
version's files; it does not remove other installations. First use Install / Update
to bring an existing manual installation under management. Interrupted updates
roll back on the next setup operation. Keep the package for repair/reinstallation.

Build: Tools/Setup/build.ps1 -OutputDirectory <release-folder> -Package <release-zip>
Check: Tools/Setup/check.ps1 [-Package <release-zip>] [-KeyFile <update-key.xml>]
(the updater checks sign their fixture releases with the release key, read in place)
Checks use disposable folders and retain their evidence. They never run Skyrim.
For shared Helgen, join at character creation and follow the same companion.
See HELGEN-STATUS.txt in the release for tested coverage and remaining limits.

Automatic updates: Data\SkyrimTogetherReborn\BetterTogetherUpdater.exe, started by the launcher as
  --check --game <folder with SkyrimSE.exe> --launcher-pid <pid> --relaunch <SkyrimTogether.exe> [--relaunch-args <args>]
asks https://api.github.com/repos/Dylan3124567/SkyrimBetterTogether/releases/latest for a version newer
than Data\SkyrimTogetherReborn\BetterTogetherVersion.txt (drafts and prereleases are ignored).
Exit 2 means quit now: a copy in %TEMP% downloads SkyrimBetterTogether-<version>.zip, installs it only if
its .sig verifies against the RSA key built into the updater, then starts the launcher again. A BetterNMM
instance gets the update in its Better Together mod; press Launch in BetterNMM afterwards. Any other exit
code (no update, offline after 4 s, "Not now", any error) means start the game.
Log: %LOCALAPPDATA%\SkyrimBetterTogether\updater.log. Tests: --yes (no dialogs), --feed <url>, --render-test <png>.

Signing key: Tools/Setup/new-signing-key.ps1 creates A:\SkyrimMultiplayerBuild\signing\update-key.xml once
(never overwritten; never commit or copy it, except to an offline backup) and prints the public key that
Updater.cs embeds. Without that file no installed copy accepts another update.
Release: Tools/Setup/release.ps1 -Version <x.y.z.w> -Payload <release.zip> -OutputDirectory <dir> [-Notes <file>]
writes SkyrimBetterTogether-<version>.zip (Better Together files plus version file and updater; Address
Library and loose documents left out), its .sig and SkyrimBetterTogetherSetup.exe, and prints the gh
command. -Publish runs it: gh release create v<version> ... --repo Dylan3124567/SkyrimBetterTogether.

Friend payload: python Tools/Setup/bundle.py <release-payload.zip> <friend.zip> --address-library <download.zip>
Friend EXE: Tools/Setup/build.ps1 -Package <friend.zip> -Friend
