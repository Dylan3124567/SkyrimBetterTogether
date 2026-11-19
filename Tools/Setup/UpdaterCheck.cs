using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Security.Cryptography;
using System.Text;
using System.Threading;
using System.Web.Script.Serialization;

#if IDLE_UPDATER
// The updater with running programs simulated as in Check.cs, for the check -> copy -> apply chain, which runs as real
// processes: Skyrim or BetterNMM may well be open on this PC.
static class IdleUpdater { [STAThread] static int Main(string[] args) { Install.IsRunning = name => false; return Updater.Main(args); } }
#else
// The updater against a local stand-in for the GitHub release API. Check mode, --sign, the dialog and the whole
// check -> copy -> apply chain run as real processes; the other install cases run in this process.
class UpdaterCheck
{
    static int count;
    static string Root, Exe, Key, Feed, Temp, Idle;
    static readonly JavaScriptSerializer Json = new JavaScriptSerializer();
    static void Assert(bool ok, string name) { if (!ok) throw new Exception(name + LogTail()); Console.WriteLine("PASS " + name); count++; }
    static string LogTail()
    {
        var log = Path.Combine(Root, "localappdata", "SkyrimBetterTogether", "updater.log");
        return File.Exists(log) ? "\r\nupdater.log:\r\n" + String.Join("\r\n", File.ReadAllLines(log).Reverse().Take(30).Reverse()) : "";
    }
    static string Q(string s) { return "\"" + s + "\""; }
    static void Put(string path, string content) { Directory.CreateDirectory(Path.GetDirectoryName(path)); File.WriteAllText(path, content); }
    static bool Wait(Func<bool> done, int ms) { var w = Stopwatch.StartNew(); while (!done()) { if (w.ElapsedMilliseconds > ms) return false; Thread.Sleep(200); } return true; }

    class Reply { public int Status = 200; public string Location; public byte[] Body = new byte[0]; public bool Hang; }
    static readonly Dictionary<string, Reply> Routes = new Dictionary<string, Reply>();
    static readonly List<string> Requests = new List<string>();
    static int Port;
    static string Url(string path) { return "http://127.0.0.1:" + Port + path; }
    // Raw sockets: HttpListener needs a URL reservation unless run as administrator
    static void Serve()
    {
        var listener = new TcpListener(IPAddress.Loopback, 0); listener.Start();
        Port = ((IPEndPoint)listener.LocalEndpoint).Port;
        new Thread(() => { while (true) { var client = listener.AcceptTcpClient(); new Thread(() => Answer(client)) { IsBackground = true }.Start(); } }) { IsBackground = true }.Start();
    }
    static void Answer(TcpClient client)
    {
        using (client)
        {
            var stream = client.GetStream(); var head = new StringBuilder();
            while (!head.ToString().EndsWith("\r\n\r\n")) { int b = stream.ReadByte(); if (b < 0) return; head.Append((char)b); }
            lock (Requests) Requests.Add(head.ToString());
            Reply reply; lock (Routes) Routes.TryGetValue(head.ToString().Split(' ')[1], out reply);
            if (reply == null) reply = new Reply { Status = 404 };
            // a feed that takes the connection and never answers
            if (reply.Hang) Thread.Sleep(Timeout.Infinite);
            var header = Encoding.ASCII.GetBytes("HTTP/1.1 " + reply.Status + " Fixture\r\nContent-Length: " + reply.Body.Length + "\r\nConnection: close\r\n" + (reply.Location == null ? "" : "Location: " + reply.Location + "\r\n") + "\r\n");
            stream.Write(header, 0, header.Length); stream.Write(reply.Body, 0, reply.Body.Length);
        }
    }
    // Serves a release the way GitHub does: the API answer, then each asset behind a redirect to its file host
    static void Publish(string tag, string zip, string signature = null, string flag = null, long declared = -1)
    {
        var name = "SkyrimBetterTogether-" + tag.Substring(1) + ".zip";
        var bytes = File.ReadAllBytes(zip); var sig = File.ReadAllBytes(signature ?? zip + ".sig");
        Func<string, long, Dictionary<string, object>> asset = (n, size) => new Dictionary<string, object> { { "name", n }, { "browser_download_url", Url("/download/" + n) }, { "size", size } };
        var body = String.Join("\n", Enumerable.Range(1, 14).Select(i => "Release note line " + i));
        var release = new Dictionary<string, object> { { "tag_name", tag }, { "draft", flag == "draft" }, { "prerelease", flag == "prerelease" }, { "body", body },
            { "assets", new object[] { asset(name, declared < 0 ? bytes.Length : declared), asset(name + ".sig", sig.Length) } } };
        lock (Routes)
        {
            Routes.Clear();
            Routes["/repos/latest"] = new Reply { Body = Encoding.UTF8.GetBytes(Json.Serialize(release)) };
            Routes["/download/" + name] = new Reply { Status = 302, Location = Url("/files/" + name) };
            Routes["/download/" + name + ".sig"] = new Reply { Status = 302, Location = Url("/files/" + name + ".sig") };
            Routes["/files/" + name] = new Reply { Body = bytes };
            Routes["/files/" + name + ".sig"] = new Reply { Body = sig };
        }
    }

    const string ExeName = "SkyrimTogetherReborn/SkyrimTogether.exe";
    // A package as release.ps1 makes one: the required files, the version file and the updater
    static string Package(string version, string versionInside = null)
    {
        var files = new Dictionary<string, string> { { ExeName, "exe " + version }, { "SkyrimTogetherReborn/STServer.dll", "dll " + version }, { "SkyrimTogether.esp", "esp " + version },
            { Updater.VersionFile, versionInside ?? version }, { "SkyrimTogetherReborn/BetterTogetherUpdater.exe", "updater " + version } };
        var zip = Path.Combine(Root, "packages", Guid.NewGuid().ToString("N").Substring(0, 8), "SkyrimBetterTogether-" + version + ".zip");
        Directory.CreateDirectory(Path.GetDirectoryName(zip));
        using (var archive = ZipFile.Open(zip, ZipArchiveMode.Create))
        {
            var manifest = new StringBuilder();
            foreach (var f in files)
            {
                using (var w = new StreamWriter(archive.CreateEntry(f.Key).Open())) w.Write(f.Value);
                using (var sha = SHA256.Create()) manifest.Append(BitConverter.ToString(sha.ComputeHash(Encoding.UTF8.GetBytes(f.Value))).Replace("-", "").ToLowerInvariant() + "  " + f.Key + "\n");
            }
            using (var w = new StreamWriter(archive.CreateEntry("SHA256SUMS.txt").Open())) w.Write(manifest);
        }
        return zip;
    }
    // Captured, so a refusal's message stays out of the check output (a PowerShell caller may treat stderr as an error)
    static Result SignWith(string zip, string key)
    {
        using (var p = Process.Start(new ProcessStartInfo(Exe, "--sign " + Q(zip) + " " + Q(key)) { UseShellExecute = false, RedirectStandardOutput = true, RedirectStandardError = true }))
        {
            var output = p.StandardOutput.ReadToEndAsync(); var error = p.StandardError.ReadToEnd(); p.WaitForExit();
            return new Result { Code = p.ExitCode, Output = output.Result, Error = error };
        }
    }
    static string Signed(string zip)
    {
        var result = SignWith(zip, Key);
        if (result.Code != 0 || !File.Exists(zip + ".sig")) throw new Exception("Could not sign " + zip + ": " + result.Error);
        return zip;
    }
    static string Game(string folder, string version)
    {
        Put(Path.Combine(folder, "SkyrimSE.exe"), "fixture");
        Install.Run(folder, Package(version), false, s => { }, false);
        return folder;
    }
    static string Tree(string dir)
    {
        return String.Join("\n", Directory.GetFiles(dir, "*", SearchOption.AllDirectories).OrderBy(f => f, StringComparer.OrdinalIgnoreCase).Select(f => f.Substring(dir.Length) + " " + Install.Hash(f)));
    }
    static int Copies() { var root = Path.Combine(Temp, "SkyrimBetterTogetherUpdate"); return Directory.Exists(root) ? Directory.GetDirectories(root).Length : 0; }
    static long Stamp(string instance)
    {
        var list = Json.Deserialize<Dictionary<string, object>>(File.ReadAllText(Path.Combine(instance, "mods.json")));
        return ((System.Collections.ArrayList)list["mods"]).Cast<Dictionary<string, object>>().Where(m => (string)m["id"] == Install.ModName).Select(m => Convert.ToInt64(m["stamp"])).Single();
    }

    static int Start(string args, int timeout, string exe = null)
    {
        using (var p = Process.Start(new ProcessStartInfo(exe ?? Exe, args) { UseShellExecute = false }))
        {
            if (!p.WaitForExit(timeout)) { p.Kill(); throw new Exception("The updater did not finish within " + timeout + " ms: " + args + LogTail()); }
            return p.ExitCode;
        }
    }
    static string CheckArgs(string game, string feed) { return "--check --yes --game " + Q(game) + " --feed " + Q(feed); }
    class Result { public int Code; public string Output, Error; }
    static Result Apply(string game, params string[] extra)
    {
        TextWriter output = new StringWriter(), error = new StringWriter(), oldOut = Console.Out, oldError = Console.Error;
        Console.SetOut(output); Console.SetError(error);
        int code;
        try { code = Updater.Run(new[] { "--apply", "--yes", "--game", game, "--feed", Feed }.Concat(extra).ToArray()); }
        finally { Console.SetOut(oldOut); Console.SetError(oldError); }
        return new Result { Code = code, Output = output.ToString(), Error = error.ToString() };
    }

    public static int Main(string[] args)
    {
        Root = args[0]; Exe = args[1]; Key = args[2]; Idle = args[3];
        // the updater's log, its %TEMP% copies and the BetterNMM instances it finds all stay inside the fixture
        Temp = Path.Combine(Root, "temp"); Directory.CreateDirectory(Temp);
        Environment.SetEnvironmentVariable("TEMP", Temp); Environment.SetEnvironmentVariable("TMP", Temp);
        Environment.SetEnvironmentVariable("LOCALAPPDATA", Path.Combine(Root, "localappdata"));
        Environment.SetEnvironmentVariable("BETTERNMM_DATA_DIR", Path.Combine(Root, "nmm-data"));
        Put(Path.Combine(Root, "nmm-data", "betternmm.cfg"), "[General]\nInstanceDir=" + Path.Combine(Root, "instances").Replace('\\', '/') + "\n");
        Install.IsRunning = name => false;
        Serve(); Feed = Url("/repos/latest");

        var v1 = Signed(Package("2026.1.1.1"));
        var v2 = Signed(Package("2026.11.19.2"));
        Assert(Updater.Verified(SHA256.Create().ComputeHash(File.ReadAllBytes(v2)), File.ReadAllText(v2 + ".sig")), "--sign signs with the release key, which the built-in public key accepts");
        var otherKey = Path.Combine(Root, "other-key.xml"); string wrongSig = Path.Combine(Root, "wrong.sig");
        using (var rsa = new RSACryptoServiceProvider(3072))
        {
            File.WriteAllText(otherKey, rsa.ToXmlString(true));
            File.WriteAllText(wrongSig, Convert.ToBase64String(rsa.SignData(File.ReadAllBytes(v2), "SHA256")));
        }
        var unsigned = Package("2026.11.19.3");
        var refused = SignWith(unsigned, otherKey);
        Assert(refused.Code == 1 && refused.Error.Contains("not the key this updater trusts") && !File.Exists(unsigned + ".sig"), "--sign refuses a key the updater does not trust");

        // The whole chain, as the launcher runs it: check, copy to %TEMP%, apply after the launcher exits, relaunch
        var steam = Game(Path.Combine(Root, "Steam Library", "Skyrim Special Edition"), "2026.1.1.1");
        Publish("v2026.11.19.2", v2);
        var relaunch = Path.Combine(steam, "Data", "SkyrimTogetherReborn", "relaunch fixture.cmd");
        Put(relaunch, "@>\"%~dp0relaunched.txt\" echo %*\r\n");
        var marker = Path.Combine(Path.GetDirectoryName(relaunch), "relaunched.txt");
        using (var launcher = Process.Start(new ProcessStartInfo("ping", "-n 120 127.0.0.1") { UseShellExecute = false, CreateNoWindow = true }))
        {
            int code = Start("--check --yes --game " + Q(steam) + " --launcher-pid " + launcher.Id + " --relaunch " + Q(relaunch) + " --relaunch-args --check --feed " + Q(Feed), 30000, Idle);
            Assert(code == 2, "newer release: check accepts it and tells the launcher to exit (code 2)");
            lock (Requests) Assert(Requests.Any(r => r.StartsWith("GET /repos/latest ") && r.Contains("User-Agent: SkyrimBetterTogether-Updater") && r.Contains("Accept: application/vnd.github+json")), "feed request sends GitHub's User-Agent and Accept headers");
            Thread.Sleep(1500);
            Assert(File.ReadAllText(Path.Combine(steam, "Data", ExeName)) == "exe 2026.1.1.1" && !File.Exists(marker), "apply waits while the launcher is still running");
            launcher.Kill();
        }
        Assert(Wait(() => File.Exists(marker), 90000), "after the launcher exits the update installs and starts the launcher again");
        Assert(File.ReadAllText(marker).Trim() == "--check", "the launcher is started again with its arguments, even one spelled like the mode");
        Assert(File.ReadAllText(Path.Combine(steam, "Data", ExeName)) == "exe 2026.11.19.2" && File.ReadAllText(Path.Combine(steam, "Data", Updater.VersionFile)) == "2026.11.19.2" &&
            File.ReadAllText(Path.Combine(steam, "Data", "SkyrimTogetherReborn", "BetterTogetherUpdater.exe")) == "updater 2026.11.19.2", "newer release installed: files, version file and updater replaced");
        var receipt = Json.Deserialize<Receipt>(File.ReadAllText(Path.Combine(steam, ".str-setup", "installed.json")));
        Assert(receipt.Package == "SkyrimBetterTogether-2026.11.19.2.zip" && receipt.Files.Count == 5 && receipt.Files.All(f => Install.Hash(Path.Combine(steam, Install.Target(f.Key).Replace('/', '\\'))) == f.Value.Hash), "receipt names the release and matches every installed file");
        // the copy's folder stays (it ran from there); the download folder goes once apply finishes
        Assert(Wait(() => Copies() == 1, 10000) && Directory.GetFiles(Path.Combine(Temp, "SkyrimBetterTogetherUpdate"), Path.GetFileName(Idle), SearchOption.AllDirectories).Length == 1, "apply ran from a copy in %TEMP% and its download is removed afterwards");

        // Nothing newer to offer
        var before = Tree(steam); int copies = Copies();
        Assert(Start(CheckArgs(steam, Feed), 15000) == 0, "same version: check exits 0");
        Publish("v2026.1.1.1", v1);
        Assert(Start(CheckArgs(steam, Feed), 15000) == 0, "older version: check exits 0");
        Publish("v2027.1.1.1", v2, null, "prerelease");
        Assert(Start(CheckArgs(steam, Feed), 15000) == 0, "newer prerelease: ignored, check exits 0");
        Publish("v2027.1.1.1", v2, null, "draft");
        Assert(Start(CheckArgs(steam, Feed), 15000) == 0, "newer draft: ignored, check exits 0");
        Assert(Tree(steam) == before && Copies() == copies, "same, older, prerelease and draft releases change nothing and start no update");

        // Offline: a closed port, and a feed that never answers
        int closed; { var l = new TcpListener(IPAddress.Loopback, 0); l.Start(); closed = ((IPEndPoint)l.LocalEndpoint).Port; l.Stop(); }
        var watch = Stopwatch.StartNew();
        Assert(Start(CheckArgs(steam, "http://127.0.0.1:" + closed + "/repos/latest"), 15000) == 0 && watch.ElapsedMilliseconds < 5000, "unreachable feed: check exits 0 in " + watch.ElapsedMilliseconds + " ms");
        lock (Routes) Routes["/hang"] = new Reply { Hang = true };
        watch.Restart();
        Assert(Start(CheckArgs(steam, Url("/hang")), 15000) == 0 && watch.ElapsedMilliseconds < 5000, "feed that never answers: check gives up and exits 0 in " + watch.ElapsedMilliseconds + " ms");
        Assert(Tree(steam) == before && Copies() == copies, "offline checks change nothing");

        // The dialog, rendered by the real program from a real check
        var old = Game(Path.Combine(Root, "render"), "2026.1.1.1");
        Publish("v2026.11.19.2", v2);
        var png = Path.Combine(Root, "update-dialog.png");
        Assert(Start("--check --game " + Q(old) + " --feed " + Q(Feed) + " --render-test " + Q(png), 30000) == 0 && File.Exists(png) && new FileInfo(png).Length > 1000, "check-mode update dialog renders (" + png + ")");

        // Refused downloads leave the install as it was
        var plain = Game(Path.Combine(Root, "plain"), "2026.1.1.1");
        before = Tree(plain);
        var tampered = Path.Combine(Root, "tampered.zip"); var bytes = File.ReadAllBytes(v2); bytes[bytes.Length / 2] ^= 0xFF; File.WriteAllBytes(tampered, bytes);
        Publish("v2026.11.19.2", tampered, v2 + ".sig");
        var result = Apply(plain);
        Assert(result.Code == 1 && result.Error.Contains("not signed") && Tree(plain) == before, "tampered ZIP is refused and nothing changes");
        Publish("v2026.11.19.2", v2, wrongSig);
        result = Apply(plain);
        Assert(result.Code == 1 && result.Error.Contains("not signed") && Tree(plain) == before, "ZIP signed with another key is refused and nothing changes");
        Publish("v2026.11.19.2", v2, null, null, new FileInfo(v2).Length + 10);
        result = Apply(plain);
        Assert(result.Code == 1 && result.Error.Contains("different file") && Tree(plain) == before, "download whose length differs from the release is refused");
        Publish("v2026.11.19.2", v2, null, null, (1L << 30) + 1);
        result = Apply(plain);
        Assert(result.Code == 1 && result.Error.Contains("outside the accepted size") && Tree(plain) == before, "release over 1 GB is refused before downloading");
        Publish("v2026.12.1.1", v2);
        result = Apply(plain);
        Assert(result.Code == 1 && result.Error.Contains("not the 2026.12.1.1") && Tree(plain) == before, "older signed package under a newer tag is refused (no downgrade)");
        Publish("v2026.1.1.1", v1);
        result = Apply(plain);
        Assert(result.Code == 0 && Tree(plain) == before, "apply with nothing newer changes nothing");
        Assert(Copies() == copies, "refused downloads are removed");

        // A BetterNMM instance managing a game folder: found among the instances, its mod updated and marked for redeploy
        var managed = Path.Combine(Root, "managed game"); Put(Path.Combine(managed, "SkyrimSE.exe"), "fixture");
        Put(Path.Combine(managed, "Data", "SkyrimTogetherReborn", "SkyrimTogether.exe"), "deployed by BetterNMM");
        var main = Path.Combine(Root, "instances", "Main Better Together");
        Put(Path.Combine(main, "instance.cfg"), "[General]\nInstanceKind=Main\nInstanceType=Game\nSourceGameDir=" + managed.Replace('\\', '/') + "\n");
        Put(Path.Combine(main, "mods.json"), "{\"mods\":[{\"enabled\":true,\"id\":\"" + Install.ModName + "\",\"name\":\"" + Install.ModName + "\",\"stamp\":1}]}");
        Install.Run(main, Package("2026.1.1.1"), false, s => { }, false);
        var mod = Path.Combine(main, "mods", Install.ModName, "Data");
        long stamp = Stamp(main); before = Tree(main); var deployed = Tree(managed);
        Publish("v2026.11.19.2", v2);
        Install.IsRunning = name => name == "betternmm";
        result = Apply(managed);
        Assert(result.Code == 1 && result.Error.Contains("Close BetterNMM") && Tree(main) == before && Tree(managed) == deployed, "BetterNMM open: update of its instance is refused and nothing changes");
        Install.IsRunning = name => false;
        var instanceMarker = Path.Combine(Root, "instance-relaunched.txt");
        Put(Path.Combine(Root, "instance relaunch.cmd"), "@>\"" + instanceMarker + "\" echo %*\r\n");
        result = Apply(managed, "--relaunch", Path.Combine(Root, "instance relaunch.cmd"));
        Assert(result.Code == 0 && File.ReadAllText(Path.Combine(mod, ExeName)) == "exe 2026.11.19.2" && File.ReadAllText(Path.Combine(mod, Updater.VersionFile)) == "2026.11.19.2", "BetterNMM instance deploying into the game folder: its mod is updated");
        Assert(Stamp(main) > stamp && Tree(managed) == deployed, "BetterNMM instance: mod stamp bumped for redeploy, game folder left to BetterNMM");
        Thread.Sleep(1000);
        Assert(result.Output.Contains("Press Launch in BetterNMM") && !File.Exists(instanceMarker), "BetterNMM instance: player is told to press Launch in BetterNMM, the game is not started directly");
        // ... and an isolated instance, chosen by its own game folder
        var isolated = Path.Combine(Root, "instances", "Isolated");
        Put(Path.Combine(isolated, "instance.cfg"), "[General]\nInstanceKind=Isolated\nInstanceType=Game\n"); Put(Path.Combine(isolated, "game", "SkyrimSE.exe"), "fixture");
        Put(Path.Combine(isolated, "mods.json"), "{\"mods\":[]}");
        Install.Run(isolated, Package("2026.1.1.1"), false, s => { }, false);
        stamp = Stamp(isolated);
        result = Apply(Path.Combine(isolated, "game"));
        Assert(result.Code == 0 && File.ReadAllText(Path.Combine(isolated, "mods", Install.ModName, "Data", ExeName)) == "exe 2026.11.19.2" && Stamp(isolated) > stamp, "BetterNMM instance chosen by its game folder: its mod is updated and stamped");

        Console.WriteLine(count + " updater checks passed"); return 0;
    }
}
#endif
