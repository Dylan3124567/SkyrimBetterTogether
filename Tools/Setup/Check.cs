using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Runtime.InteropServices;
using System.Web.Script.Serialization;

class Check
{
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)] static extern bool CreateHardLink(string link, string existing, IntPtr attributes);
    static int count;
    static void Assert(bool ok, string name) { if (!ok) throw new Exception(name); Console.WriteLine("PASS " + name); count++; }
    static string Root, Game;
    static string FileAt(string name) { return Path.Combine(Game, "Data", name.Replace('/', '\\')); }
    static string Placed(string name) { return Path.Combine(Game, Install.Target(name).Replace('/', '\\')); }
    static void Put(string path, string content) { Directory.CreateDirectory(Path.GetDirectoryName(path)); File.WriteAllText(path, content); }
    static string Package(string name, Dictionary<string, string> files, bool badHash = false)
    {
        var zip = Path.Combine(Root, name + ".zip");
        using (var a = ZipFile.Open(zip, ZipArchiveMode.Create))
        {
            string manifest = "";
            foreach (var f in files)
            {
                var temp = Path.Combine(Root, "hash-input"); File.WriteAllText(temp, f.Value);
                manifest += (badHash ? new string('0', 64) : Install.Hash(temp)) + "  " + f.Key + "\n";
                using (var w = new StreamWriter(a.CreateEntry(f.Key).Open())) w.Write(f.Value);
            }
            using (var w = new StreamWriter(a.CreateEntry("SHA256SUMS.txt").Open())) w.Write(manifest);
        }
        return zip;
    }
    static void Rejected(Action action, string message) { bool failed = false; try { action(); } catch { failed = true; } Assert(failed, message); }
    public static int Main(string[] args)
    {
        // the real game may be open on this PC; what setup does about running programs is checked below with simulated ones
        Install.IsRunning = name => false;
        Root = args[0]; Directory.CreateDirectory(Root); Game = Path.Combine(Root, "game"); Directory.CreateDirectory(Game); Put(Path.Combine(Game, "SkyrimSE.exe"), "fixture");
        string exe = "SkyrimTogetherReborn/SkyrimTogether.exe", dll = "SkyrimTogetherReborn/STServer.dll", esp = "SkyrimTogether.esp", obsolete = "scripts/old.pex";
        Put(FileAt(exe), "previous mod"); Put(FileAt("another-mod.esp"), "untouched"); Put(Path.Combine(Game, "Saves/personal.ess"), "save");
        var v1 = new Dictionary<string, string> { { exe, "one" }, { dll, "one dll" }, { esp, "one esp" }, { obsolete, "old script" } };
        var v2 = new Dictionary<string, string> { { exe, "two" }, { dll, "two dll" }, { esp, "two esp" } };
        string p1 = Package("v1", v1), p2 = Package("v2", v2);
        Action<string> quiet = s => { };
        Install.Run(Game, p1, false, quiet, false);
        Assert(File.ReadAllText(FileAt(exe)) == "one", "install copies verified payload");
        Assert(File.ReadAllText(Path.Combine(Game, ".str-setup/original", exe)) == "previous mod", "existing installation backed up");
        Put(FileAt(esp), "user edit");
        Install.Run(Game, p2, false, quiet, false);
        Assert(File.ReadAllText(FileAt(exe)) == "two" && !File.Exists(FileAt(obsolete)), "update replaces files and removes obsolete owned file");
        Assert(Directory.GetFiles(Path.Combine(Game, ".str-setup/preserved"), "*.esp", SearchOption.AllDirectories).Any(p => File.ReadAllText(p) == "user edit"), "update preserves local modifications");
        Install.Run(Game, p2, false, quiet, false);
        Assert(File.ReadAllText(FileAt(exe)) == "two", "same-package repair is repeatable");
        Rejected(() => Install.Run(Game, Package("corrupt", v1, true), false, quiet, false), "bad hash rejected before changing installation");
        Assert(File.ReadAllText(FileAt(exe)) == "two", "bad package leaves current installation intact");
        var unsafeFiles = new Dictionary<string, string>(v1); unsafeFiles.Add("../escaped.txt", "escape");
        Rejected(() => Install.Run(Game, Package("traversal", unsafeFiles), false, quiet, false), "ZIP traversal rejected");
        foreach (var path in new[] { "../escape", "C:/escape", "x\\y", "/absolute", "folder./file" }) Rejected(() => Install.Safe(Game, path), "unsafe path rejected: " + path);
        string before = Install.Hash(Path.Combine(Game, ".str-setup/installed.json"));
        FileStream locked = null;
        try
        {
            Rejected(() => Install.Run(Game, p1, false, s => { if (s == "Installing files...") locked = new FileStream(FileAt(dll), FileMode.Open, FileAccess.Read, FileShare.Read); }, false), "write failure triggers transaction rollback");
        }
        finally { if (locked != null) locked.Dispose(); }
        // Recovery can itself be blocked by the same lock; the next invocation must complete it.
        Install.Run(Game, p2, false, quiet, false);
        Assert(File.ReadAllText(FileAt(exe)) == "two" && !File.Exists(FileAt(obsolete)) && Install.Hash(Path.Combine(Game, ".str-setup/installed.json")) == before, "failed update recovers original files and receipt");
        Install.Run(Game, null, true, quiet, false);
        Assert(File.ReadAllText(FileAt(exe)) == "previous mod" && !File.Exists(FileAt(dll)) && !File.Exists(FileAt(esp)), "uninstall restores previous files and removes new owned files");
        Assert(File.ReadAllText(FileAt("another-mod.esp")) == "untouched" && File.ReadAllText(Path.Combine(Game, "Saves/personal.ess")) == "save", "unrelated mods and saves preserved");
        Rejected(() => Install.Run(Game, null, true, quiet, false), "unmanaged uninstall refused");
        Assert(!File.Exists(Path.Combine(Game, ".str-setup/transaction/journal.json")), "no unfinished transaction remains");
        var friendFiles = new Dictionary<string, string>(v1); friendFiles.Add(Install.AddressLibrary, "included addresses");
        var friend = Package("friend", friendFiles);
        Install.Run(Game, friend, false, quiet, false);
        Assert(File.ReadAllText(FileAt(Install.AddressLibrary)) == "included addresses", "friend install includes matching Address Library");
        Install.Run(Game, p2, false, quiet, false);
        Assert(File.ReadAllText(FileAt(Install.AddressLibrary)) == "included addresses", "regular update retains managed Address Library");
        Install.Run(Game, null, true, quiet, false);
        Assert(!File.Exists(FileAt(Install.AddressLibrary)), "uninstall removes newly installed dependency");
        Put(FileAt(Install.AddressLibrary), "previous addresses");
        Install.Run(Game, friend, false, quiet, false); Install.Run(Game, null, true, quiet, false);
        Assert(File.ReadAllText(FileAt(Install.AddressLibrary)) == "previous addresses", "friend uninstall restores pre-existing Address Library");
        File.Delete(FileAt(Install.AddressLibrary));
        var outside = Path.Combine(Root, "mod-manager-copy.esp"); File.WriteAllText(outside, "mod manager copy");
        if (!CreateHardLink(FileAt("linked.esp"), outside, IntPtr.Zero)) throw new Exception("Could not create the hard link fixture");
        var pack = new Dictionary<string, string>(v1);
        pack.Add("Mods/textures/mod.dds", "mod texture"); pack.Add("Mods/another-mod.esp", "pack version"); pack.Add("Mods/linked.esp", "pack version");
        pack.Add("Mods/SKSE/Plugins/mod.dll", "mod plugin"); pack.Add("GameRoot/skse64_1_7_104.dll", "skse");
        var modded = Package("modded", pack);
        Install.Run(Game, modded, false, quiet, false);
        Assert(File.ReadAllText(FileAt("textures/mod.dds")) == "mod texture" && File.ReadAllText(FileAt("SKSE/Plugins/mod.dll")) == "mod plugin", "mod pack installs into Data");
        Assert(File.ReadAllText(Path.Combine(Game, "skse64_1_7_104.dll")) == "skse" && !File.Exists(FileAt("GameRoot/skse64_1_7_104.dll")), "mod pack installs SKSE beside the game");
        Assert(File.ReadAllText(FileAt("another-mod.esp")) == "pack version", "mod pack replaces a file the player already had");
        Assert(File.ReadAllText(outside) == "mod manager copy", "hard linked mod manager file is not written through");
        Install.Run(Game, modded, false, quiet, false);
        Assert(File.ReadAllText(Path.Combine(Game, "skse64_1_7_104.dll")) == "skse", "mod pack update is repeatable");
        Install.Run(Game, p2, false, quiet, false);
        Assert(!File.Exists(Path.Combine(Game, "skse64_1_7_104.dll")) && !File.Exists(FileAt("textures/mod.dds")) && File.ReadAllText(FileAt("another-mod.esp")) == "untouched", "plain build removes the mod pack and restores originals");
        Install.Run(Game, modded, false, quiet, false); Install.Run(Game, null, true, quiet, false);
        Assert(!File.Exists(Path.Combine(Game, "skse64_1_7_104.dll")) && !File.Exists(FileAt("SKSE/Plugins/mod.dll")) && File.ReadAllText(FileAt("another-mod.esp")) == "untouched" && File.ReadAllText(FileAt("linked.esp")) == "mod manager copy", "uninstall removes the mod pack and restores originals");
        File.Delete(FileAt("linked.esp"));
        var changes = new Dictionary<string, string> { { exe, "update exe" }, { "Mods/armor.esp", "new plugin" }, { Install.UpdateMarker, "update" } };
        var update = Package("update", changes);
        Rejected(() => Install.Run(Game, update, false, quiet, false), "update refuses a game without a full install");
        Install.Run(Game, modded, false, quiet, false);
        Install.Run(Game, update, false, quiet, false);
        Assert(File.ReadAllText(FileAt(exe)) == "update exe" && File.ReadAllText(FileAt("armor.esp")) == "new plugin", "update replaces and adds its files");
        Assert(File.ReadAllText(FileAt("textures/mod.dds")) == "mod texture" && File.ReadAllText(Path.Combine(Game, "skse64_1_7_104.dll")) == "skse", "update keeps the mods it does not carry");
        Install.Run(Game, update, false, quiet, false);
        Assert(File.ReadAllText(FileAt("SKSE/Plugins/mod.dll")) == "mod plugin", "update is repeatable");
        File.Delete(FileAt("textures/mod.dds"));
        Rejected(() => Install.Run(Game, update, false, quiet, false), "update refuses an install with a missing mod file");
        Install.Run(Game, modded, false, quiet, false);
        Install.Run(Game, update, false, quiet, false);
        Install.Run(Game, null, true, quiet, false);
        Assert(!File.Exists(FileAt("textures/mod.dds")) && !File.Exists(FileAt("armor.esp")) && !File.Exists(Path.Combine(Game, "skse64_1_7_104.dll")) && File.ReadAllText(FileAt("another-mod.esp")) == "untouched", "uninstall after an update removes everything");
        // A BetterNMM instance: setup updates the instance's Better Together mod and marks it changed for BetterNMM to deploy
        var json = new JavaScriptSerializer();
        Func<string, string, Dictionary<string, object>> modEntry = (list, id) =>
            ((System.Collections.ArrayList)json.Deserialize<Dictionary<string, object>>(File.ReadAllText(list))["mods"]).Cast<Dictionary<string, object>>().FirstOrDefault(m => (string)m["id"] == id);
        var inst = Path.Combine(Root, "instances", "Better Together"); var instGame = Path.Combine(inst, "game");
        Put(Path.Combine(inst, "instance.cfg"), "[General]\nInstanceKind=Isolated\nInstanceType=Game\nname=\"Better, Together\"\n");
        Put(Path.Combine(instGame, "SkyrimSE.exe"), "fixture");
        var modData = Path.Combine(inst, "mods", Install.ModName, "Data");
        var modExe = Path.Combine(modData, "SkyrimTogetherReborn", "SkyrimTogether.exe"); Put(modExe, "sleep1");
        var deployedExe = Path.Combine(instGame, "Data", "SkyrimTogetherReborn", "SkyrimTogether.exe"); Directory.CreateDirectory(Path.GetDirectoryName(deployedExe));
        if (!CreateHardLink(deployedExe, modExe, IntPtr.Zero)) throw new Exception("Could not create the deployed hard link fixture");
        Put(Path.Combine(inst, "mods", "Other", "Data", "other.esp"), "other");
        var modsJson = Path.Combine(inst, "mods.json");
        Put(modsJson, "{\"mods\":[{\"enabled\":true,\"id\":\"" + Install.ModName + "\",\"name\":\"" + Install.ModName + "\",\"stamp\":1,\"nexus\":{\"modId\":5}},{\"enabled\":false,\"id\":\"Other\",\"name\":\"Other\",\"stamp\":2}]}");
        Install.Run(instGame, p1, false, quiet, false);
        Assert(File.ReadAllText(modExe) == "one" && File.ReadAllText(Path.Combine(modData, "SkyrimTogether.esp")) == "one esp", "BetterNMM instance (game folder chosen): its Better Together mod is updated");
        Assert(File.ReadAllText(deployedExe) == "sleep1" && !Directory.Exists(Path.Combine(instGame, ".str-setup")), "BetterNMM instance: the game folder is left for BetterNMM to deploy");
        var bt = modEntry(modsJson, Install.ModName); var other = modEntry(modsJson, "Other");
        Assert(Convert.ToInt64(bt["stamp"]) > 1 && (bool)bt["enabled"] && ((Dictionary<string, object>)bt["nexus"])["modId"].Equals(5) && Convert.ToInt64(other["stamp"]) == 2 && !(bool)other["enabled"],
            "BetterNMM instance: the mod is marked changed, its other details and the other mods are untouched");
        Install.Run(inst, p2, false, quiet, false);
        Assert(File.ReadAllText(modExe) == "two" && !File.Exists(Path.Combine(modData, "scripts", "old.pex")), "BetterNMM instance (instance folder chosen): update replaces the mod's files");
        Install.Run(inst, null, true, quiet, false);
        Assert(File.ReadAllText(modExe) == "sleep1" && !File.Exists(Path.Combine(modData, "SkyrimTogether.esp")), "BetterNMM instance: uninstall restores the mod's previous build");
        var bare = Path.Combine(Root, "instances", "Fresh");
        Put(Path.Combine(bare, "instance.cfg"), "[General]\nInstanceKind=Isolated\nInstanceType=Game\n"); Put(Path.Combine(bare, "game", "SkyrimSE.exe"), "fixture");
        Put(Path.Combine(bare, "mods.json"), "{\"mods\":[]}");
        Install.Run(bare, p1, false, quiet, false);
        var added = modEntry(Path.Combine(bare, "mods.json"), Install.ModName);
        Assert(File.ReadAllText(Path.Combine(bare, "mods", Install.ModName, "Data", "SkyrimTogetherReborn", "SkyrimTogether.exe")) == "one" && added != null && (bool)added["enabled"],
            "BetterNMM instance without Better Together: setup adds it as an enabled mod");
        Put(Path.Combine(Root, "data", "betternmm.cfg"), "[General]\nInstanceDir=" + Path.Combine(Root, "instances").Replace('\\', '/') + "\n");
        Environment.SetEnvironmentVariable("BETTERNMM_DATA_DIR", Path.Combine(Root, "data"));
        // (instances elsewhere on this PC may be found too)
        var foundInstances = Install.BetterNmmInstances();
        Assert(foundInstances.Contains(inst, StringComparer.OrdinalIgnoreCase) && foundInstances.Contains(bare, StringComparer.OrdinalIgnoreCase), "BetterNMM instances with Better Together are found");
        Install.IsRunning = name => name == "betternmm";
        Rejected(() => Install.Run(inst, p1, false, quiet, false), "refused while BetterNMM is open");
        Assert(File.ReadAllText(modExe) == "sleep1", "refusal changed nothing");
        Install.Run(Game, p1, false, quiet, false);
        Assert(File.ReadAllText(FileAt(exe)) == "one", "an open BetterNMM doesn't block a plain game folder");
        Install.Run(Game, null, true, quiet, false);
        Install.IsRunning = name => name == "SkyrimTogether";
        Rejected(() => Install.Run(inst, p1, false, quiet, false), "refused while Better Together is running");
        Install.IsRunning = name => false;
        Environment.SetEnvironmentVariable("BETTERNMM_DATA_DIR", null);
        if (args.Length > 1)
        {
            string real = Install.HasBundle ? Install.Bundled : args[1];
            Install.Apply(Game, real, false, quiet, false);
            var installed = new JavaScriptSerializer { MaxJsonLength = 32 * 1024 * 1024 }.Deserialize<Receipt>(File.ReadAllText(Path.Combine(Game, ".str-setup/installed.json")));
            Assert(installed.Files.Count > 100 && installed.Files.All(x => Install.Hash(Placed(x.Key)) == x.Value.Hash), "actual release installs with every payload hash matching");
            Install.Apply(Game, real, false, quiet, false);
            Assert(installed.Files.All(x => Install.Hash(Placed(x.Key)) == x.Value.Hash), "actual release update preserves exact payload");
            Install.Run(Game, null, true, quiet, false);
            Assert(File.ReadAllText(FileAt(exe)) == "previous mod" && installed.Files.Keys.Where(x => x != exe).All(x => !File.Exists(Placed(x))), "actual release uninstall removes only owned payload and restores original");
        }
        Console.WriteLine(count + " checks passed"); return 0;
    }
}
