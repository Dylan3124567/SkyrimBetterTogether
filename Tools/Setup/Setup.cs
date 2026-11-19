using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Threading.Tasks;
using System.Web.Script.Serialization;
using System.Windows.Forms;
using Microsoft.Win32;

public class Entry { public string Hash; public bool Original; }
public class Receipt { public string Package; public Dictionary<string, Entry> Files = new Dictionary<string, Entry>(StringComparer.OrdinalIgnoreCase); }
public class Journal { public Dictionary<string, bool> Files = new Dictionary<string, bool>(); }

public static class Install
{
    public const string AddressLibrary = "SKSE/Plugins/versionlib-1-7-104-0.bin";
    public const string Bundled = "Included Skyrim Better Together build";
    const string OverlayMagic = "SBTPACK1";
    // In a package that only carries what changed, so the files of the last full install stay as they are.
    public const string UpdateMarker = "UPDATE-ONLY.txt";
    public static bool HasBundle { get { long start, length; return typeof(Install).Assembly.GetManifestResourceNames().Contains("Payload.zip") || FindOverlay(out start, out length); } }
    // Windows won't start an executable with a ~2 GB image, so a mod pack rides after the image instead of inside it as a
    // resource. Layout: the program, the package, its length as 8 bytes, then the magic.
    static bool FindOverlay(out long start, out long length)
    {
        start = length = 0;
        using (var self = new FileStream(typeof(Install).Assembly.Location, FileMode.Open, FileAccess.Read, FileShare.Read))
        {
            if (self.Length < 16) return false;
            self.Seek(-16, SeekOrigin.End);
            var footer = new byte[16];
            if (self.Read(footer, 0, 16) != 16 || Encoding.ASCII.GetString(footer, 8, 8) != OverlayMagic) return false;
            length = BitConverter.ToInt64(footer, 0);
            start = self.Length - 16 - length;
            return length > 0 && start > 0;
        }
    }
    static void CopyOverlay(long start, long length, Stream output)
    {
        using (var self = new FileStream(typeof(Install).Assembly.Location, FileMode.Open, FileAccess.Read, FileShare.Read))
        {
            self.Seek(start, SeekOrigin.Begin);
            var buffer = new byte[1 << 20];
            for (long left = length; left > 0;)
            {
                int read = self.Read(buffer, 0, (int)Math.Min(buffer.Length, left));
                if (read <= 0) throw new IOException("This setup file is incomplete. Copy it again.");
                output.Write(buffer, 0, read); left -= read;
            }
        }
    }
    public static string Apply(string game, string package, bool uninstall, Action<string> progress, bool checkGame = true)
    {
        if (uninstall || package != Bundled) return Run(game, package, uninstall, progress, checkGame);
        long start, length;
        bool overlay = FindOverlay(out start, out length);
        var place = Locate(game);
        if (overlay)
        {
            Idle(place);
            if (!File.Exists(Safe(place.Game, "SkyrimSE.exe"))) throw new IOException("SkyrimSE.exe not found.");
        }
        // A mod pack is too big to assume room on the system drive, the game's drive is checked for space anyway
        var folder = overlay ? Safe(place.Root, StateName + "/bundle-" + Guid.NewGuid().ToString("N")) : Path.Combine(Path.GetTempPath(), "SkyrimBetterTogether-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(folder);
        try
        {
            var zip = Path.Combine(folder, "SkyrimBetterTogether-bundled.zip");
            progress("Preparing the included build...");
            using (var output = File.Create(zip))
            {
                if (overlay) CopyOverlay(start, length, output);
                else using (var input = typeof(Install).Assembly.GetManifestResourceStream("Payload.zip"))
                {
                    if (input == null) throw new IOException("This setup has no included build. Choose a release ZIP.");
                    input.CopyTo(output);
                }
            }
            return Run(game, zip, false, progress, checkGame);
        }
        finally { Directory.Delete(folder, true); }
    }
    public const string StateName = ".str-setup";
    static readonly JavaScriptSerializer Json = new JavaScriptSerializer { MaxJsonLength = 32 * 1024 * 1024 };
    public static string Hash(string file) { using (var h = SHA256.Create()) using (var f = File.OpenRead(file)) return BitConverter.ToString(h.ComputeHash(f)).Replace("-", "").ToLowerInvariant(); }
    static void Write(string file, object value) { File.WriteAllText(file, Json.Serialize(value)); }
    static T Read<T>(string file) { return Json.Deserialize<T>(File.ReadAllText(file)); }
    // Replaces the file rather than writing into it: mod managers hard link into Data, and their own copy must stay intact.
    static void Copy(string from, string to) { Directory.CreateDirectory(Path.GetDirectoryName(to)); Remove(to); File.Copy(from, to); }
    static void Remove(string file) { if (File.Exists(file)) File.Delete(file); }
    public static string Safe(string root, string relative)
    {
        if (String.IsNullOrWhiteSpace(relative) || relative.IndexOfAny(new[] { ':', '\\' }) >= 0 || relative.StartsWith("/") || relative.Split('/').Any(x => x == ".." || x == "." || x.Length == 0 || x.EndsWith(".") || x.EndsWith(" ")))
            throw new IOException("Unsafe package path: " + relative);
        foreach (var segment in relative.Split('/'))
        {
            string stem = segment.Split('.')[0].ToUpperInvariant();
            if (segment.IndexOfAny(Path.GetInvalidFileNameChars()) >= 0 || new[] { "CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9", "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9" }.Contains(stem))
                throw new IOException("Invalid Windows file name: " + segment);
        }
        var full = Path.GetFullPath(Path.Combine(root, relative.Replace('/', Path.DirectorySeparatorChar)));
        var prefix = Path.GetFullPath(root).TrimEnd('\\') + "\\";
        if (!full.StartsWith(prefix, StringComparison.OrdinalIgnoreCase)) throw new IOException("Path escaped the selected folder.");
        for (var part = full; !String.IsNullOrEmpty(part); part = Path.GetDirectoryName(part))
            if ((File.Exists(part) || Directory.Exists(part)) && (File.GetAttributes(part) & FileAttributes.ReparsePoint) != 0)
                throw new IOException("Linked folders/files are not supported: " + part);
        return full;
    }
    public const string ModPrefix = "Mods/", RootPrefix = "GameRoot/";
    // Where a package path goes, relative to the game folder. Mod packs need Data files of any kind, and SKSE beside the game.
    public static string Target(string name)
    {
        if (name.StartsWith(RootPrefix, StringComparison.OrdinalIgnoreCase)) return name.Substring(RootPrefix.Length);
        if (name.StartsWith(ModPrefix, StringComparison.OrdinalIgnoreCase)) return "Data/" + name.Substring(ModPrefix.Length);
        return "Data/" + name;
    }
    // Also used by release.ps1 to keep only these files in the update ZIP
    public static bool Payload(string path)
    {
        return path.StartsWith(ModPrefix, StringComparison.OrdinalIgnoreCase) || path.StartsWith(RootPrefix, StringComparison.OrdinalIgnoreCase) ||
            path.Equals(AddressLibrary, StringComparison.OrdinalIgnoreCase) || path.StartsWith("SkyrimTogetherReborn/", StringComparison.OrdinalIgnoreCase) ||
            path.StartsWith("SkyrimTogetherRebornBehaviors/", StringComparison.OrdinalIgnoreCase) ||
            path.StartsWith("scripts/", StringComparison.OrdinalIgnoreCase) || path.StartsWith("meshes/", StringComparison.OrdinalIgnoreCase) ||
            path.StartsWith("Interface/SkyrimBetterTogether/", StringComparison.OrdinalIgnoreCase) ||
            path.Equals("SkyrimTogether.esp", StringComparison.OrdinalIgnoreCase) || path.Equals("SkyrimTogetherQuestPatches.esp", StringComparison.OrdinalIgnoreCase);
    }
    // Replaced by the checks, which run on a PC where the game may be open
    public static Func<string, bool> IsRunning = name => Process.GetProcessesByName(name).Length != 0;
    static void Idle(Place place)
    {
        foreach (var name in new[] { "SkyrimSE", "SkyrimTogether", "SkyrimTogetherServer", "TPProcess" })
            if (IsRunning(name)) throw new IOException("Close Skyrim and its multiplayer server before continuing. Still running: " + name + ".exe");
        // BetterNMM keeps its mod list in memory and writes it back when it closes, which would undo the change below
        if (place.Instance != null && IsRunning("betternmm"))
            throw new IOException("Close BetterNMM before continuing (its window, and its tray icon > Exit). It saves its mod list when it closes, which would undo this update.");
    }

    // Where setup puts files. Normally the chosen game folder. For a BetterNMM instance (its folder, or the game folder
    // inside it) it is the instance's Better Together mod: BetterNMM links its mods into the game folder at every launch,
    // so a file changed only in the game folder would still be the old build in the mod.
    public class Place { public string Root, Game, Instance, ModId, ModsJson; }
    public const string ModName = "Skyrim Better Together";
    public static Place Locate(string chosen)
    {
        var full = Path.GetFullPath(chosen).TrimEnd('\\');
        string instance = null;
        if (File.Exists(Path.Combine(full, "instance.cfg"))) instance = full;
        else if (Path.GetFileName(full).Equals("game", StringComparison.OrdinalIgnoreCase) && File.Exists(Path.Combine(Path.GetDirectoryName(full), "instance.cfg")))
            instance = Path.GetDirectoryName(full);
        if (instance == null) return new Place { Root = full, Game = full };
        var cfg = ReadCfg(Path.Combine(instance, "instance.cfg"));
        string type, kind, source;
        cfg.TryGetValue("InstanceType", out type); cfg.TryGetValue("InstanceKind", out kind); cfg.TryGetValue("SourceGameDir", out source);
        if (type != "Game") throw new IOException("This BetterNMM instance is not a game instance: " + instance);
        // an instance managing the Steam folder deploys into it; an isolated one into its own game folder
        var game = kind == "Main" && !String.IsNullOrEmpty(source) ? Path.GetFullPath(source.Replace('/', '\\')) : Path.Combine(instance, "game");
        var modsJson = Path.Combine(instance, "mods.json");
        var modId = BetterTogetherMod(instance, modsJson) ?? ModName;
        return new Place { Root = Path.Combine(instance, "mods", modId), Game = game, Instance = instance, ModId = modId, ModsJson = modsJson };
    }
    // BetterNMM's settings files: key=value, values with special characters in quotes with \" and \\ escapes
    static Dictionary<string, string> ReadCfg(string file)
    {
        var values = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        foreach (var raw in File.ReadAllLines(file))
        {
            var line = raw.Trim(); int eq = line.IndexOf('=');
            if (eq <= 0 || line.StartsWith("[")) continue;
            var value = line.Substring(eq + 1).Trim();
            if (value.Length >= 2 && value[0] == '"' && value[value.Length - 1] == '"') value = value.Substring(1, value.Length - 2).Replace("\\\"", "\"").Replace("\\\\", "\\");
            values[line.Substring(0, eq).Trim()] = value;
        }
        return values;
    }
    static List<object> Mods(Dictionary<string, object> list)
    {
        var mods = new List<object>(); object value;
        if (list.TryGetValue("mods", out value) && value is System.Collections.IEnumerable && !(value is string))
            foreach (var m in (System.Collections.IEnumerable)value) mods.Add(m);
        return mods;
    }
    static string ModId(object mod)
    {
        var d = mod as Dictionary<string, object>; object id;
        if (d == null || !d.TryGetValue("id", out id)) return null;
        var s = id as string;
        return String.IsNullOrEmpty(s) || s == "." || s == ".." || s.IndexOfAny(Path.GetInvalidFileNameChars()) >= 0 ? null : s;
    }
    // The mod holding Better Together; with several, the one later in the list, whose files win
    static string BetterTogetherMod(string instance, string modsJson)
    {
        if (!File.Exists(modsJson)) return null;
        string found = null;
        foreach (var mod in Mods(Json.Deserialize<Dictionary<string, object>>(File.ReadAllText(modsJson))))
        {
            var id = ModId(mod);
            if (id != null && File.Exists(Path.Combine(instance, "mods", id, "Data", "SkyrimTogetherReborn", "SkyrimTogether.exe"))) found = id;
        }
        return found;
    }
    // A new stamp makes BetterNMM take the mod's old files out of the game folder and link the new ones at the next launch
    static void MarkChanged(Place place, bool enable)
    {
        var list = File.Exists(place.ModsJson) ? Json.Deserialize<Dictionary<string, object>>(File.ReadAllText(place.ModsJson)) : new Dictionary<string, object>();
        var mods = Mods(list);
        Dictionary<string, object> entry = null;
        foreach (var mod in mods) if (String.Equals(ModId(mod), place.ModId, StringComparison.OrdinalIgnoreCase)) entry = (Dictionary<string, object>)mod;
        if (entry == null) { entry = new Dictionary<string, object> { { "id", place.ModId }, { "name", place.ModId }, { "enabled", true } }; mods.Add(entry); }
        if (enable) entry["enabled"] = true;
        entry["stamp"] = (long)(DateTime.UtcNow - new DateTime(1970, 1, 1, 0, 0, 0, DateTimeKind.Utc)).TotalMilliseconds;
        list["mods"] = mods;
        var temp = place.ModsJson + ".setup-new";
        File.WriteAllText(temp, Json.Serialize(list));
        if (File.Exists(place.ModsJson)) File.Replace(temp, place.ModsJson, null); else File.Move(temp, place.ModsJson);
    }
    // Folders BetterNMM keeps instances in: its own folder, extra folders from its settings, and the "BetterNMM Instances"
    // folder it makes next to a Steam library on another drive
    public static List<string> BetterNmmInstanceRoots()
    {
        var roots = new List<string>();
        var data = Environment.GetEnvironmentVariable("BETTERNMM_DATA_DIR");
        if (String.IsNullOrEmpty(data)) data = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "BetterNMM");
        string dir = "instances", extra = null;
        var cfg = Path.Combine(data, "betternmm.cfg");
        if (File.Exists(cfg)) { var v = ReadCfg(cfg); string s; if (v.TryGetValue("InstanceDir", out s) && s != "") dir = s; v.TryGetValue("AdditionalInstanceDirs", out extra); }
        roots.Add(Path.IsPathRooted(dir) ? dir : Path.Combine(data, dir));
        if (!String.IsNullOrEmpty(extra)) foreach (var e in extra.Split(',')) if (e.Trim().Trim('"') != "") roots.Add(e.Trim().Trim('"'));
        foreach (var drive in DriveInfo.GetDrives().Where(x => x.IsReady && x.DriveType == DriveType.Fixed))
            foreach (var library in new[] { "", "SteamClient", "SteamLibrary", "Steam", @"Program Files (x86)\Steam" })
                roots.Add(Path.Combine(drive.RootDirectory.FullName, library, "BetterNMM Instances"));
        return roots.Select(r => r.Replace('/', '\\')).Distinct(StringComparer.OrdinalIgnoreCase).ToList();
    }
    // BetterNMM instances with Better Together in them, most recently changed first
    public static List<string> BetterNmmInstances()
    {
        var found = new List<string>();
        foreach (var root in BetterNmmInstanceRoots())
        {
            if (!Directory.Exists(root)) continue;
            foreach (var folder in Directory.GetDirectories(root))
            {
                if (!File.Exists(Path.Combine(folder, "instance.cfg"))) continue;
                Place place;
                // an instance this can't read is simply not offered; choosing it by hand reports why
                try { place = Locate(folder); } catch (IOException) { continue; } catch (ArgumentException) { continue; } catch (InvalidOperationException) { continue; }
                if (File.Exists(Path.Combine(place.Root, "Data", "SkyrimTogetherReborn", "SkyrimTogether.exe")) && !found.Contains(folder, StringComparer.OrdinalIgnoreCase)) found.Add(folder);
            }
        }
        return found.OrderByDescending(x => File.GetLastWriteTimeUtc(Path.Combine(x, "mods.json"))).ToList();
    }
    public static string Describe(string chosen)
    {
        var place = Locate(chosen);
        return place.Instance == null ? "Game: " + place.Game : "BetterNMM instance: " + place.Instance + "\r\nMod: " + place.ModId;
    }
    public static void ValidateGame(string game, bool requireAddressLibrary = true, string root = null)
    {
        var exe = Safe(game, "SkyrimSE.exe");
        if (!File.Exists(exe)) throw new IOException("Choose the Skyrim Special Edition folder containing SkyrimSE.exe, or a BetterNMM instance.");
        var v = FileVersionInfo.GetVersionInfo(exe);
        if (v.FileMajorPart != 1 || v.FileMinorPart != 7 || v.FileBuildPart != 104 || v.FilePrivatePart != 0)
            throw new IOException("This build requires Skyrim 1.7.104.0. Selected game: " + v.FileVersion);
        if (requireAddressLibrary && !File.Exists(Safe(game, "Data/" + AddressLibrary)) && (root == null || !File.Exists(Safe(root, "Data/" + AddressLibrary))))
            throw new IOException("Install Address Library v13 for Skyrim 1.7.104.0 first, then run setup again. It is not bundled.");
    }
    public static string Installed(string game)
    {
        var path = Safe(Locate(game).Root, StateName + "/installed.json");
        return File.Exists(path) ? Read<Receipt>(path).Package : "Not managed by this setup yet";
    }
    static void Recover(string game, string state)
    {
        var txn = Safe(game, StateName + "/transaction");
        var journal = Safe(txn, "journal.json");
        if (File.Exists(journal))
        {
            foreach (var item in Read<Journal>(journal).Files)
            {
                var to = Safe(game, item.Key);
                if (item.Value) Copy(Safe(txn, "files/" + item.Key), to); else Remove(to);
            }
            // A complete journal is kept until every restoration succeeded.
            File.Delete(journal);
        }
        if (Directory.Exists(txn)) Directory.Delete(txn, true);
    }
    static void Snapshot(string game, string state, IEnumerable<string> paths)
    {
        var txn = Safe(game, StateName + "/transaction");
        Directory.CreateDirectory(txn);
        var j = new Journal();
        foreach (var path in paths.Distinct(StringComparer.OrdinalIgnoreCase))
        {
            var file = Safe(game, path); j.Files[path] = File.Exists(file);
            if (j.Files[path]) Copy(file, Safe(txn, "files/" + path));
        }
        Write(Safe(txn, "journal.json"), j);
    }
    static void PreserveChanged(string game, string state, string name, Entry entry, string recovery)
    {
        var file = Safe(game, Target(name));
        if (File.Exists(file) && Hash(file) != entry.Hash) Copy(file, Safe(state, recovery + "/" + name));
    }
    static void RestoreOriginal(string game, string state, string name, Entry entry)
    {
        var dest = Safe(game, Target(name));
        if (entry.Original) Copy(Safe(state, "original/" + name), dest); else Remove(dest);
    }
    // Local release ZIPs; Updater.cs fetches signed ones from the GitHub release feed and installs them through here.
    public static string Run(string chosen, string zip, bool uninstall, Action<string> progress, bool checkGame = true)
    {
        var place = Locate(chosen);
        Idle(place);
        if (checkGame && !uninstall) ValidateGame(place.Game, false);
        if (!File.Exists(Safe(place.Game, "SkyrimSE.exe"))) throw new IOException("SkyrimSE.exe not found.");
        // from here on "game" is where the files go: the game folder, or a BetterNMM instance's mod
        var game = place.Root;
        var state = Safe(game, StateName); Directory.CreateDirectory(state);
        using (var gate = new FileStream(Safe(state, "lock"), FileMode.OpenOrCreate, FileAccess.ReadWrite, FileShare.None))
        {
            Recover(game, state);
            var receiptFile = Safe(state, "installed.json");
            var old = File.Exists(receiptFile) ? Read<Receipt>(receiptFile) : new Receipt();
            foreach (var item in old.Files)
            {
                if (!Payload(item.Key)) throw new IOException("Invalid installed-file receipt.");
                Safe(game, Target(item.Key));
                if (item.Value.Original && !File.Exists(Safe(state, "original/" + item.Key))) throw new IOException("Original backup missing: " + item.Key);
            }
            if (uninstall && !File.Exists(receiptFile)) throw new IOException("No installation managed by this setup. Install this build first to track its files safely.");
            var stage = Safe(state, "stage-" + Guid.NewGuid().ToString("N"));
            var next = new Receipt { Package = uninstall ? null : Path.GetFileName(zip) };
            var recovery = "preserved/" + DateTime.UtcNow.ToString("yyyyMMdd-HHmmss") + "-" + Guid.NewGuid().ToString("N").Substring(0, 6);
            bool snapshotted = false;
            var kept = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            try
            {
                if (!uninstall)
                {
                    progress("Checking and preparing package...");
                    using (var archive = ZipFile.OpenRead(zip))
                    {
                        var manifest = archive.GetEntry("SHA256SUMS.txt");
                        if (manifest == null) throw new IOException("This is not a supported release ZIP: SHA256SUMS.txt is missing.");
                        var hashes = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
                        using (var reader = new StreamReader(manifest.Open()))
                            while (!reader.EndOfStream)
                            {
                                var line = reader.ReadLine(); var split = line.IndexOf("  ", StringComparison.Ordinal);
                                if (split != 64) throw new IOException("Invalid package manifest.");
                                var name = line.Substring(66); Safe(stage, name);
                                if (hashes.ContainsKey(name)) throw new IOException("Duplicate manifest path.");
                                hashes.Add(name, line.Substring(0, 64));
                            }
                        var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
                        long bytes = 0;
                        foreach (var item in archive.Entries)
                        {
                            var name = item.FullName.TrimEnd('/'); Safe(stage, name);
                            if (!seen.Add(name)) throw new IOException("Duplicate ZIP entry: " + name);
                            if (item.Name.Length != 0 && Payload(name)) bytes = checked(bytes + item.Length);
                        }
                        if (bytes > 4L * 1024 * 1024 * 1024) throw new IOException("Package exceeds the 4 GB setup limit.");
                        if (new DriveInfo(Path.GetPathRoot(game)).AvailableFreeSpace < bytes * 4 + 128L * 1024 * 1024)
                            throw new IOException("Not enough space on the game drive for installation and rollback backups.");
                        foreach (var item in archive.Entries.Where(x => x.Name.Length != 0 && Payload(x.FullName)))
                        {
                            var file = Safe(stage, item.FullName); Directory.CreateDirectory(Path.GetDirectoryName(file)); item.ExtractToFile(file);
                            if (!hashes.ContainsKey(item.FullName) || Hash(file) != hashes[item.FullName]) throw new IOException("Package checksum failed: " + item.FullName);
                            next.Files.Add(item.FullName, new Entry { Hash = hashes[item.FullName] });
                        }
                        if (archive.GetEntry(UpdateMarker) != null)
                        {
                            if (old.Files.Count == 0) throw new IOException("This is only an update. Run the full setup first.");
                            foreach (var item in old.Files.Where(x => !next.Files.ContainsKey(x.Key)))
                            {
                                if (!File.Exists(Safe(game, Target(item.Key)))) throw new IOException("Missing " + Target(item.Key) + ". Run the full setup to repair this install.");
                                kept.Add(item.Key);
                                next.Files.Add(item.Key, new Entry { Hash = item.Value.Hash });
                            }
                        }
                    }
                    // Retain a shared prerequisite when updating a friend install with the regular build.
                    if (!next.Files.ContainsKey(AddressLibrary) && old.Files.ContainsKey(AddressLibrary) && File.Exists(Safe(game, "Data/" + AddressLibrary)))
                    {
                        Copy(Safe(game, "Data/" + AddressLibrary), Safe(stage, AddressLibrary));
                        next.Files.Add(AddressLibrary, new Entry { Hash = Hash(Safe(stage, AddressLibrary)) });
                    }
                    if (checkGame && !next.Files.ContainsKey(AddressLibrary)) ValidateGame(place.Game, true, game);
                    foreach (var required in new[] { "SkyrimTogetherReborn/SkyrimTogether.exe", "SkyrimTogetherReborn/STServer.dll", "SkyrimTogether.esp" })
                        if (!next.Files.ContainsKey(required)) throw new IOException("Package is missing " + required);
                }
                var all = old.Files.Keys.Union(next.Files.Keys, StringComparer.OrdinalIgnoreCase).ToArray();
                foreach (var name in all) Safe(game, Target(name));
                foreach (var item in next.Files)
                {
                    Entry prior;
                    if (old.Files.TryGetValue(item.Key, out prior)) item.Value.Original = prior.Original;
                    else
                    {
                        var dest = Safe(game, Target(item.Key)); item.Value.Original = File.Exists(dest);
                        if (item.Value.Original) Copy(dest, Safe(state, "original/" + item.Key));
                    }
                }
                Idle(place);
                Snapshot(game, state, all.Where(x => !kept.Contains(x)).Select(Target).Concat(new[] { StateName + "/installed.json" })); snapshotted = true;
                progress(uninstall ? "Removing this build and restoring previous files..." : "Installing files...");
                foreach (var item in old.Files.Where(x => !kept.Contains(x.Key)))
                {
                    PreserveChanged(game, state, item.Key, item.Value, recovery);
                    if (!next.Files.ContainsKey(item.Key)) RestoreOriginal(game, state, item.Key, item.Value);
                }
                foreach (var item in next.Files.Where(x => !kept.Contains(x.Key))) Copy(Safe(stage, item.Key), Safe(game, Target(item.Key)));
                if (uninstall) Remove(receiptFile); else Write(receiptFile, next);
                // Delete the journal to commit. A crash before this point rolls back on next run.
                Remove(Safe(state, "transaction/journal.json")); snapshotted = false;
                Recover(game, state);
                if (place.Instance != null) MarkChanged(place, !uninstall);
                var done = place.Instance != null
                    ? (uninstall ? "Uninstalled this build from the BetterNMM mod \"" + place.ModId + "\". Its previous files were restored." : "Installed " + next.Package + " into the BetterNMM mod \"" + place.ModId + "\".") +
                      "\r\nBetterNMM puts it in the game folder the next time you press Launch there."
                    : uninstall ? "Uninstalled this build. Previously replaced files were restored." : "Installed " + next.Package + ".\r\nLaunch Data\\SkyrimTogetherReborn\\SkyrimTogether.exe.";
                return done + "\r\nSaves and game settings were not changed." + (Directory.Exists(Safe(state, recovery)) ? "\r\nLocally changed files were preserved in " + Safe(state, recovery) : "");
            }
            catch
            {
                if (snapshotted) Recover(game, state);
                throw;
            }
            finally { if (Directory.Exists(stage)) Directory.Delete(stage, true); }
        }
    }
}

public class Wizard : Form
{
    readonly TextBox game = new TextBox(), zip = new TextBox(), status = new TextBox();
    readonly ComboBox action = new ComboBox(); readonly Button run = new Button();
    readonly ProgressBar progress = new ProgressBar();
    readonly Button zipBrowse;
    readonly List<Control> inputs = new List<Control>(); bool busy; public bool SilentRender;
    protected override bool ShowWithoutActivation { get { return SilentRender; } }
    public Wizard()
    {
        Text = "Skyrim Better Together - Setup"; ClientSize = new Size(720, 570); Font = new Font("Segoe UI", 10);
        BackColor = Color.FromArgb(245, 246, 248); ForeColor = Color.FromArgb(32, 40, 49);
        FormBorderStyle = FormBorderStyle.FixedDialog; MaximizeBox = false; StartPosition = FormStartPosition.CenterScreen;
        var header = new Panel { BackColor = Color.FromArgb(30, 40, 52) }; header.SetBounds(0, 0, 720, 108); Controls.Add(header);
        var title = LabelAt("Skyrim Better Together", 28, 20, 660, 36); title.Font = new Font("Segoe UI", 20, FontStyle.Bold); title.ForeColor = Color.White; title.BackColor = header.BackColor; title.Parent = header;
        var subtitle = LabelAt("SETUP  /  INSTALL - UPDATE - UNINSTALL", 30, 66, 660, 25); subtitle.ForeColor = Color.FromArgb(205, 191, 151); subtitle.BackColor = header.BackColor; subtitle.Parent = header; title.Parent = header;
        LabelAt("1   Game folder or BetterNMM instance", 28, 128, 660, 24).Font = new Font(Font, FontStyle.Bold);
        game.SetBounds(28, 156, 550, 28); Controls.Add(game); Browse("Browse...", 590, 154, delegate { using (var d = new FolderBrowserDialog()) { d.Description = "Folder containing SkyrimSE.exe, or a BetterNMM instance folder"; if (d.ShowDialog() == DialogResult.OK) game.Text = d.SelectedPath; } });
        LabelAt("2   Release package", 28, 200, 660, 24).Font = new Font(Font, FontStyle.Bold);
        zip.SetBounds(28, 228, 550, 28); Controls.Add(zip); zipBrowse = Browse("Browse...", 590, 226, delegate { using (var d = new OpenFileDialog { Filter = "Skyrim Better Together release ZIP|*.zip" }) { if (d.ShowDialog() == DialogResult.OK) zip.Text = d.FileName; } });
        LabelAt("3   Choose an action", 28, 272, 660, 24).Font = new Font(Font, FontStyle.Bold);
        action.DropDownStyle = ComboBoxStyle.DropDownList; action.Items.AddRange(new object[] { "Install / Update", "Uninstall this build" }); action.SelectedIndex = 0; action.SetBounds(28, 300, 260, 28); Controls.Add(action);
        LabelAt("Your saves and settings stay in place. Replaced files are backed up.\r\nUninstall restores those originals. Close Skyrim before continuing.", 28, 346, 660, 48);
        status.Multiline = true; status.ReadOnly = true; status.ScrollBars = ScrollBars.Vertical; status.BackColor = Color.White; status.BorderStyle = BorderStyle.FixedSingle; status.SetBounds(28, 406, 664, 94); Controls.Add(status);
        progress.SetBounds(28, 514, 470, 5); progress.Style = ProgressBarStyle.Marquee; progress.Visible = false; Controls.Add(progress);
        run.Text = "Install / Update"; run.SetBounds(512, 516, 180, 36); run.BackColor = header.BackColor; run.ForeColor = Color.White; run.FlatStyle = FlatStyle.Flat; run.FlatAppearance.BorderSize = 0; run.Click += async delegate { await Execute(); }; Controls.Add(run); AcceptButton = run;
        action.SelectedIndexChanged += delegate { zip.Enabled = zipBrowse.Enabled = action.SelectedIndex == 0; run.Text = action.SelectedIndex == 1 ? "Uninstall" : "Install / Update"; };
        inputs.AddRange(new Control[] { game, zip, action, run });
        header.SendToBack();
        FormClosing += delegate(object sender, FormClosingEventArgs e) { if (busy) { e.Cancel = true; MessageBox.Show(this, "Please wait for the file operation to finish."); } };
        var known = Registry.GetValue(@"HKEY_LOCAL_MACHINE\SOFTWARE\WOW6432Node\Bethesda Softworks\Skyrim Special Edition", "Installed Path", "") as string;
        if (!String.IsNullOrEmpty(known) && File.Exists(Path.Combine(known, "SkyrimSE.exe"))) game.Text = known;
        if (game.Text == "") foreach (var drive in DriveInfo.GetDrives().Where(x => x.IsReady)) foreach (var folder in new[] { @"SteamClient\steamapps\common\Skyrim Special Edition", @"SteamLibrary\steamapps\common\Skyrim Special Edition", @"Program Files (x86)\Steam\steamapps\common\Skyrim Special Edition" })
            if (File.Exists(Path.Combine(drive.RootDirectory.FullName, folder, "SkyrimSE.exe"))) game.Text = Path.Combine(drive.RootDirectory.FullName, folder);
        var packages = Directory.GetFiles(AppDomain.CurrentDomain.BaseDirectory, "SkyrimTogether-*.zip");
        if (Install.HasBundle) zip.Text = Install.Bundled; else if (packages.Length == 1) zip.Text = packages[0];
#if UPDATE
        Text = "Skyrim Better Together - Modded Update";
        subtitle.Text = "MODDED UPDATE  /  KEEPS YOUR MODS";
        status.Text = "Updates an install made with the modded friend setup. The mods already on this PC stay in place.\r\nRequires Skyrim 1.7.104.0. Start the game from Data\\SkyrimTogetherReborn\\SkyrimTogether.exe.";
#elif MODDED
        Text = "Skyrim Better Together - Modded Friend Setup";
        subtitle.Text = "MODDED FRIEND SETUP  /  INSTALL - UPDATE - UNINSTALL";
        status.Text = "Custom fork of Skyrim Together Reborn, with the mod pack, SKSE and Address Library v13 included.\r\nRequires Skyrim 1.7.104.0. Plugins switch on by themselves when you start SkyrimTogether.exe.\r\nReplaced files are backed up and uninstall removes the pack.";
#elif FRIEND
        Text = "Skyrim Better Together - Friend Setup";
        subtitle.Text = "FRIEND SETUP  /  INSTALL - UPDATE - UNINSTALL";
        status.Text = "Custom fork of Skyrim Together Reborn.\r\nRequires Skyrim 1.7.104.0. Address Library v13 is included.\r\nFor shared Helgen, join at character creation and follow the same companion.";
#else
        status.Text = "Custom fork of Skyrim Together Reborn.\r\nRequires Skyrim 1.7.104.0 and its Address Library v13.\r\nFor shared Helgen, join at character creation and follow the same companion.";
#endif
        // Better Together already moved into BetterNMM is what gets updated, not the Steam folder it came from
        var instances = Install.BetterNmmInstances();
        if (instances.Count > 0)
        {
            game.Text = instances[0];
            status.Text += "\r\nFound Better Together in the BetterNMM instance \"" + Path.GetFileName(instances[0]) + "\"" +
                (instances.Count > 1 ? " (" + instances.Count + " instances have it; Browse to pick another)." : ".");
        }
    }
    Label LabelAt(string text, int x, int y, int w, int h) { var l = new Label { Text = text }; l.SetBounds(x, y, w, h); Controls.Add(l); return l; }
    Button Browse(string text, int x, int y, Action clicked) { var b = new Button { Text = text, FlatStyle = FlatStyle.System }; b.SetBounds(x, y, 102, 32); b.Click += delegate { clicked(); }; Controls.Add(b); inputs.Add(b); return b; }
    async Task Execute()
    {
        string folder = game.Text.Trim(), package = zip.Text.Trim(); bool remove = action.SelectedIndex == 1;
        try
        {
            string current = Install.Installed(folder);
            if (MessageBox.Show(this, (remove ? "Uninstall this build and restore files that it replaced?" : "Install this package? Existing managed files will be updated.") + "\r\n\r\n" + Install.Describe(folder) + "\r\nCurrent: " + current + (remove ? "" : "\r\nPackage: " + Path.GetFileName(package)), "Review changes", MessageBoxButtons.OKCancel, MessageBoxIcon.Information) != DialogResult.OK) return;
            busy = true; progress.Visible = true; foreach (var c in inputs) c.Enabled = false;
            status.Text = await Task.Run(() => Install.Apply(folder, package, remove, s => BeginInvoke(new Action(() => status.Text = s))));
        }
        catch (Exception e) { status.Text = "Setup could not finish: " + e.Message; MessageBox.Show(this, status.Text, "Setup", MessageBoxButtons.OK, MessageBoxIcon.Error); }
        finally { busy = false; progress.Visible = false; foreach (var c in inputs) c.Enabled = true; zip.Enabled = zipBrowse.Enabled = action.SelectedIndex == 0; }
    }
    [STAThread] public static void Main(string[] args)
    {
        Application.EnableVisualStyles(); Application.SetCompatibleTextRenderingDefault(false);
        if (args.Length == 2 && args[0] == "--render-test") { using (var w = new Wizard()) { w.SilentRender = true; w.ShowInTaskbar = false; w.Opacity = 0; w.Show(); Application.DoEvents(); using (var image = new Bitmap(w.Width, w.Height)) { w.DrawToBitmap(image, new Rectangle(0, 0, w.Width, w.Height)); image.Save(args[1]); } } return; }
        Application.Run(new Wizard());
    }
}
