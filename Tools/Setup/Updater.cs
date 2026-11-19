using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Net;
using System.Reflection;
using System.Security.Cryptography;
using System.Text;
using System.Threading.Tasks;
using System.Web.Script.Serialization;
using System.Windows.Forms;

// SkyrimTogether.exe starts this before the game. --check asks the GitHub release feed for a newer version; on "Update and
// restart" a copy of this program in %TEMP% runs --apply, since installing replaces this very file, and installs the signed
// release ZIP with Install.Run.
public static class Updater
{
    public const string Feed = "https://api.github.com/repos/Dylan3124567/SkyrimBetterTogether/releases/latest";
    // RSA 3072, PKCS#1 v1.5 over SHA-256 of the ZIP. The private half is made by new-signing-key.ps1 and never leaves its file.
    public const string PublicKey = "<RSAKeyValue><Modulus>uxgxFWiMwXOHMfVvWJ9ilg6ndY5l3oZRzsou1ed8pEVaR0sad2aifdbS+c32qZRL+03fPGDXnXLsCc5ZVU1+UEPcuO++6Vtrel2N1e9Zw7whGWJNvyC2BkvkPiZj8gU5mhOLOjE7dde6L58MmmGFEg7zYXCwRJEstbcoj8ewA/2vRC9HzsPGFI5Ds5hZNDAvSwzxLpDQii62XYjdN4EsmZT+kuaVjtV19DWhYJ/mxY1Zj3L6I20F/QrX0abpDFgxvDC94qN8UJ6RDvzNmwB430TuluzRh/C/DPq1ycJ2GcZ9DA7A5oHLzyUwkTAwRep/di8j4VBs/rQNEOxJq7Sr9CxVv9llKNK5faQBYzCcvxt5O1RP2rkyCcA0/3q0O58sn41uxxbPmpAJ1Nj1NJfVFAtL7//1E4cZoOkaykMQpCTBC3dGVDEdjSxoj3mFdmo3xjv90VIEOCgjbUuwY6giuiN0Eoe4qi7S22mCD/YyQsAlHp3iMftwgyEx8Fn+LvGx</Modulus><Exponent>AQAB</Exponent></RSAKeyValue>";
    public const string VersionFile = "SkyrimTogetherReborn/BetterTogetherVersion.txt";
    public const string Title = "Skyrim Better Together update";
    const long MaxZip = 1L << 30, MaxSignature = 16 << 10;
    // Tells the launcher to quit at once, so the update can replace its files
    const int Restarting = 2;

    class Release { public Version Version; public string Name, Body, ZipUrl, SignatureUrl; public long ZipSize, SignatureSize; }
    class Options { public string Mode, Game, Relaunch, RelaunchArgs, Feed = Updater.Feed, Render, Zip, Key; public int Pid, ModeIndex; public bool Yes; public string[] Args; }

    [STAThread] public static int Main(string[] args)
    {
        Application.EnableVisualStyles(); Application.SetCompatibleTextRenderingDefault(false);
        if (args.Length == 0) { MessageBox.Show("Start the game with SkyrimTogether.exe: it checks for updates by itself.", Title); return 1; }
        return Run(args);
    }
    public static int Run(string[] args)
    {
        try
        {
            // TLS 1.2, which GitHub requires and .NET 4 leaves off by default
            ServicePointManager.SecurityProtocol |= (SecurityProtocolType)3072;
            Log("start " + String.Join(" ", args.Select(Quote)));
            var o = Parse(args);
            if (o.Mode == "--check") return Check(o);
            if (o.Mode == "--apply") return Apply(o);
            if (o.Mode == "--sign") { Console.WriteLine(Sign(o.Zip, o.Key)); return 0; }
            throw new ArgumentException("Choose --check, --apply or --sign.");
        }
        catch (Exception e)
        {
            Log("failed: " + e);
            // whatever goes wrong in a check, the launcher goes on to start the game
            if (args.Contains("--check")) return 0;
            Console.Error.WriteLine(e.Message); return 1;
        }
    }
    static string Value(string[] args, ref int i)
    {
        if (++i >= args.Length) throw new ArgumentException("Missing value after " + args[i - 1]);
        return args[i];
    }
    static Options Parse(string[] args)
    {
        var o = new Options { Args = args };
        for (int i = 0; i < args.Length; i++)
            switch (args[i])
            {
                case "--check": case "--apply": o.Mode = args[i]; o.ModeIndex = i; break;
                case "--sign": o.Mode = args[i]; o.Zip = Value(args, ref i); o.Key = Value(args, ref i); break;
                case "--yes": o.Yes = true; break;
                case "--game": o.Game = Path.GetFullPath(Value(args, ref i)).TrimEnd('\\'); break;
                case "--launcher-pid": o.Pid = Int32.Parse(Value(args, ref i)); break;
                case "--relaunch": o.Relaunch = Value(args, ref i); break;
                case "--relaunch-args": o.RelaunchArgs = Value(args, ref i); break;
                case "--feed": o.Feed = Value(args, ref i); break;
                case "--render-test": o.Render = Value(args, ref i); break;
                default: throw new ArgumentException("Unknown argument " + args[i]);
            }
        if (o.Mode != "--sign" && o.Game == null) throw new ArgumentException("--game is required.");
        return o;
    }

    static int Check(Options o)
    {
        var installed = InstalledVersion(o.Game);
        var fetch = Task.Run(() => Latest(o.Feed, 4000));
        // bounds the whole check, DNS and proxy discovery included, which a request timeout alone does not cover
        try { if (!fetch.Wait(4000)) { Log("no answer from " + o.Feed + " within 4 s"); return 0; } }
        catch (AggregateException e) { Log("update check failed: " + e.GetBaseException().Message); return 0; }
        var release = fetch.Result;
        if (release == null || release.Version <= installed) { Log("up to date: installed " + installed + ", latest " + (release == null ? "none" : release.Version.ToString())); return 0; }
        Log("update available: " + installed + " -> " + release.Version);
        if (o.Render != null) { using (var prompt = Offer(release, installed)) Render(prompt, o.Render); return 0; }
        if (!o.Yes) using (var prompt = Offer(release, installed)) if (prompt.ShowDialog() != DialogResult.OK) { Log("not now"); return 0; }
        var folder = Path.Combine(UpdateRoot(), Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(folder);
        var self = Assembly.GetExecutingAssembly().Location;
        var copy = Path.Combine(folder, Path.GetFileName(self));
        File.Copy(self, copy);
        var args = String.Join(" ", o.Args.Select((a, i) => Quote(i == o.ModeIndex ? "--apply" : a)));
        Log("starting " + copy + " " + args);
        Process.Start(new ProcessStartInfo(copy, args) { UseShellExecute = false, WorkingDirectory = folder });
        return Restarting;
    }
    static Prompt Offer(Release release, Version installed)
    {
        var lines = (release.Body ?? "").Replace("\r", "").Trim().Split('\n');
        var notes = String.Join("\r\n", lines.Take(12)) + (lines.Length > 12 ? "\r\n..." : "");
        return new Prompt("Skyrim Better Together " + release.Version + " is available. Installed: " + (installed == new Version(0, 0) ? "unknown" : installed.ToString()) + ".\r\n" +
            "Everyone in a session needs the same version, so update together with the people you play with.", notes.Length == 0 ? null : notes, "Update and restart", "Not now");
    }
    static void Render(Prompt prompt, string png)
    {
        prompt.SilentRender = true; prompt.ShowInTaskbar = false; prompt.Opacity = 0; prompt.Show(); Application.DoEvents();
        using (var image = new Bitmap(prompt.Width, prompt.Height)) { prompt.DrawToBitmap(image, new Rectangle(0, 0, prompt.Width, prompt.Height)); image.Save(png); }
    }

    static int Apply(Options o)
    {
        if (o.Yes) return Work(o, null);
        int code = 1;
        using (var window = new Progress())
        {
            window.Shown += async delegate { code = await Task.Run(() => Work(o, window)); window.Finished = true; window.Close(); };
            Application.Run(window);
        }
        return code;
    }
    static int Work(Options o, Progress window)
    {
        Action<string> show = s => { if (window != null) window.BeginInvoke(new Action(() => window.Status.Text = s)); };
        Action<string> status = s => { Log(s); show(s); };
        Install.Place place = null; string downloads = null;
        try
        {
            status("Waiting for Skyrim Together to close...");
            WaitForLauncher(o.Pid);
            var target = Target(o.Game);
            place = Install.Locate(target);
            status("Looking for the latest version...");
            var release = Latest(o.Feed, 30000);
            if (release == null) throw new IOException("No release is available right now.");
            string betterNmm = null;
            if (release.Version > InstalledVersion(place.Root))
            {
                downloads = Path.Combine(UpdateRoot(), Guid.NewGuid().ToString("N"));
                Directory.CreateDirectory(downloads);
                var zip = Path.Combine(downloads, release.Name);
                status("Downloading Skyrim Better Together " + release.Version + "...");
                Download(release.SignatureUrl, zip + ".sig", release.SignatureSize, MaxSignature, null);
                long shown = -1, total = release.ZipSize >> 20;
                Download(release.ZipUrl, zip, release.ZipSize, MaxZip, bytes => { if (bytes >> 20 != shown) { shown = bytes >> 20; show("Downloading Skyrim Better Together " + release.Version + ": " + shown + " of " + total + " MB"); } });
                // held open, so the verified bytes are the installed bytes
                using (var held = new FileStream(zip, FileMode.Open, FileAccess.Read, FileShare.Read))
                {
                    status("Checking the signature...");
                    Verify(held, zip + ".sig", release.Version);
                    while (place.Instance != null && Install.IsRunning("betternmm"))
                    {
                        if (betterNmm == null) betterNmm = RunningBetterNmm();
                        const string close = "Close BetterNMM to finish the update: its window, and its tray icon > Exit.\r\nIt saves its mod list when it closes, which would undo the update.";
                        if (window == null) throw new IOException(close);
                        if (!Ask(window, close, null, "Retry", "Cancel")) throw new OperationCanceledException("The update was cancelled. Nothing was changed.");
                    }
                    // the game was checked when it was set up, and Address Library may live in a mod manager's own folder
                    Install.Run(target, zip, false, status, false);
                }
                var now = InstalledVersion(place.Root);
                if (now != release.Version) throw new IOException("After the update the installed version is " + now + " instead of " + release.Version + ".");
                status("Updated to " + release.Version + ".");
            }
            else Log("already " + InstalledVersion(place.Root) + " in " + place.Root);
            if (place.Instance == null) { Relaunch(o); return 0; }
            if (betterNmm != null) { Log("starting " + betterNmm); Process.Start(betterNmm); }
            Tell(window, "Updated to " + release.Version + ". Press Launch in BetterNMM to play.");
            return 0;
        }
        catch (Exception e)
        {
            Log("update failed: " + e);
            var message = "The update could not finish: " + e.Message;
            if (window == null) { Console.Error.WriteLine(message); return 1; }
            if ((place == null || place.Instance == null) && o.Relaunch != null) { if (Ask(window, message, null, "Start the game anyway", "Close")) Relaunch(o); }
            else Tell(window, message);
            return 1;
        }
        finally
        {
            if (downloads != null)
                try { Directory.Delete(downloads, true); } catch (IOException e) { Log("could not remove " + downloads + ": " + e.Message); }
        }
    }
    static void WaitForLauncher(int pid)
    {
        if (pid == 0) return;
        Process launcher;
        try { launcher = Process.GetProcessById(pid); } catch (ArgumentException) { return; }
        using (launcher) if (!launcher.WaitForExit(60000)) throw new IOException("Skyrim Together is still running. Close it, then start it again to update.");
    }
    // A BetterNMM instance deploying into this game folder owns its files: update its mod, or BetterNMM puts the old build back
    static string Target(string game)
    {
        if (Install.Locate(game).Instance != null) return game;
        foreach (var instance in Install.BetterNmmInstances())
            if (String.Equals(Install.Locate(instance).Game.TrimEnd('\\'), game, StringComparison.OrdinalIgnoreCase)) return instance;
        return game;
    }
    static string RunningBetterNmm()
    {
        foreach (var process in Process.GetProcessesByName("betternmm"))
            using (process)
                try { return process.MainModule.FileName; }
                catch (System.ComponentModel.Win32Exception e) { Log("BetterNMM's path is unreadable: " + e.Message); }
        return null;
    }
    static void Relaunch(Options o)
    {
        if (o.Relaunch == null) return;
        Log("starting " + o.Relaunch + " " + o.RelaunchArgs);
        Process.Start(new ProcessStartInfo(o.Relaunch, o.RelaunchArgs ?? "") { UseShellExecute = true, WorkingDirectory = Path.GetDirectoryName(o.Relaunch) });
    }
    static bool Ask(Form owner, string text, string notes, string yes, string no)
    {
        Func<bool> ask = () => { using (var prompt = new Prompt(text, notes, yes, no)) return prompt.ShowDialog(owner) == DialogResult.OK; };
        return (bool)owner.Invoke(ask);
    }
    static void Tell(Form owner, string text)
    {
        Log(text);
        if (owner == null) Console.WriteLine(text); else Ask(owner, text, null, "OK", null);
    }

    static Version InstalledVersion(string root)
    {
        var file = Path.Combine(root, "Data", VersionFile.Replace('/', '\\'));
        Version version;
        return File.Exists(file) && Version.TryParse(File.ReadAllText(file).Trim(), out version) ? version : new Version(0, 0);
    }
    static string UpdateRoot()
    {
        var root = Path.Combine(Path.GetTempPath(), "SkyrimBetterTogetherUpdate");
        // copies of the updater from earlier updates; a day old, so none is still running
        if (Directory.Exists(root))
            foreach (var old in Directory.GetDirectories(root).Where(d => Directory.GetCreationTimeUtc(d) < DateTime.UtcNow.AddDays(-1)))
                try { Directory.Delete(old, true); }
                catch (IOException e) { Log("could not remove " + old + ": " + e.Message); }
                catch (UnauthorizedAccessException e) { Log("could not remove " + old + ": " + e.Message); }
        return root;
    }
    static HttpWebRequest Request(string url, int timeout)
    {
        var request = (HttpWebRequest)WebRequest.Create(url);
        request.UserAgent = "SkyrimBetterTogether-Updater"; request.Timeout = timeout; request.ReadWriteTimeout = timeout;
        return request;
    }
    static Release Latest(string feed, int timeout)
    {
        var request = Request(feed, timeout); request.Accept = "application/vnd.github+json";
        string text;
        using (var response = request.GetResponse()) using (var reader = new StreamReader(response.GetResponseStream())) text = reader.ReadToEnd();
        var json = new JavaScriptSerializer().Deserialize<Dictionary<string, object>>(text);
        object value;
        if (json.TryGetValue("draft", out value) && true.Equals(value) || json.TryGetValue("prerelease", out value) && true.Equals(value)) return null;
        var tag = json.TryGetValue("tag_name", out value) ? value as string : null; Version version;
        if (tag == null || !tag.StartsWith("v") || !Version.TryParse(tag.Substring(1), out version)) throw new IOException("Unexpected release tag " + tag);
        var release = new Release { Version = version, Name = "SkyrimBetterTogether-" + tag.Substring(1) + ".zip", Body = json.TryGetValue("body", out value) ? value as string : null };
        if (json.TryGetValue("assets", out value) && value is IEnumerable)
            foreach (var asset in ((IEnumerable)value).OfType<Dictionary<string, object>>())
            {
                object name, url, size;
                if (!asset.TryGetValue("name", out name) || !asset.TryGetValue("browser_download_url", out url) || !asset.TryGetValue("size", out size)) continue;
                if (release.Name.Equals(name)) { release.ZipUrl = (string)url; release.ZipSize = Convert.ToInt64(size); }
                if ((release.Name + ".sig").Equals(name)) { release.SignatureUrl = (string)url; release.SignatureSize = Convert.ToInt64(size); }
            }
        if (release.ZipUrl == null || release.SignatureUrl == null) throw new IOException("Release " + tag + " has no " + release.Name + " with its signature.");
        return release;
    }
    static void Download(string url, string file, long size, long limit, Action<long> progress)
    {
        if (size <= 0 || size > limit) throw new IOException("The release lists a file of " + size + " bytes, outside the accepted size.");
        using (var response = Request(url, 30000).GetResponse())
        {
            if (response.ContentLength >= 0 && response.ContentLength != size) throw new IOException("The server sent a different file than the release lists. Try again later.");
            long total = 0; var buffer = new byte[1 << 16];
            using (var input = response.GetResponseStream()) using (var output = File.Create(file))
                for (int read; (read = input.Read(buffer, 0, buffer.Length)) > 0;)
                {
                    total += read;
                    if (total > size) throw new IOException("The download is larger than the release lists.");
                    output.Write(buffer, 0, read);
                    if (progress != null) progress(total);
                }
            if (total != size) throw new IOException("The download stopped early. Check your connection and try again.");
        }
    }
    static void Verify(FileStream zip, string signatureFile, Version version)
    {
        byte[] hash;
        using (var sha = SHA256.Create()) hash = sha.ComputeHash(zip);
        if (!Verified(hash, File.ReadAllText(signatureFile))) throw new IOException("The download is not signed by Skyrim Better Together, so it was not installed.");
        zip.Position = 0;
        Version inside = null;
        using (var archive = new ZipArchive(zip, ZipArchiveMode.Read, true))
        {
            var entry = archive.GetEntry(VersionFile);
            if (entry != null) using (var reader = new StreamReader(entry.Open())) Version.TryParse(reader.ReadToEnd().Trim(), out inside);
        }
        // an older signed package served under a newer tag would otherwise downgrade
        if (inside != version) throw new IOException("The package is version " + inside + ", not the " + version + " the release lists.");
    }
    public static bool Verified(byte[] hash, string signature)
    {
        byte[] bytes;
        try { bytes = Convert.FromBase64String(signature.Trim()); } catch (FormatException) { return false; }
        using (var rsa = new RSACryptoServiceProvider())
        {
            rsa.FromXmlString(PublicKey);
            return rsa.VerifyHash(hash, CryptoConfig.MapNameToOID("SHA256"), bytes);
        }
    }
    // Writes <zip>.sig. Used by release.ps1 and --sign.
    public static string Sign(string zip, string keyFile)
    {
        byte[] hash;
        using (var sha = SHA256.Create()) using (var file = File.OpenRead(zip)) hash = sha.ComputeHash(file);
        string signature;
        using (var rsa = new RSACryptoServiceProvider())
        {
            rsa.FromXmlString(File.ReadAllText(keyFile));
            signature = Convert.ToBase64String(rsa.SignHash(hash, CryptoConfig.MapNameToOID("SHA256")));
        }
        // every player's updater would refuse a release signed with any other key
        if (!Verified(hash, signature)) throw new IOException(keyFile + " is not the key this updater trusts.");
        File.WriteAllText(zip + ".sig", signature);
        return zip + ".sig";
    }

    // Windows command-line quoting, so paths with spaces and quotes reach the copy unchanged
    static string Quote(string arg)
    {
        if (arg.Length > 0 && arg.IndexOfAny(new[] { ' ', '\t', '"' }) < 0) return arg;
        var quoted = new StringBuilder("\"");
        for (int i = 0; ; i++)
        {
            int slashes = 0;
            while (i < arg.Length && arg[i] == '\\') { i++; slashes++; }
            if (i == arg.Length) { quoted.Append('\\', slashes * 2); break; }
            if (arg[i] == '"') quoted.Append('\\', slashes * 2 + 1).Append('"'); else quoted.Append('\\', slashes).Append(arg[i]);
        }
        return quoted.Append('"').ToString();
    }
    static void Log(string line)
    {
        try
        {
            var folder = Path.Combine(Environment.GetEnvironmentVariable("LOCALAPPDATA") ?? Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "SkyrimBetterTogether");
            Directory.CreateDirectory(folder);
            var file = Path.Combine(folder, "updater.log");
            // stays small: one older log is kept beside it
            if (File.Exists(file) && new FileInfo(file).Length > 256 * 1024) { File.Delete(file + ".old"); File.Move(file, file + ".old"); }
            File.AppendAllText(file, DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss") + " [" + Process.GetCurrentProcess().Id + "] " + line + "\r\n");
        }
        // the log is only for diagnosis and must never keep the game from starting
        catch (IOException e) { Console.Error.WriteLine("log: " + e.Message); }
        catch (UnauthorizedAccessException e) { Console.Error.WriteLine("log: " + e.Message); }
    }
}

class Prompt : Form
{
    public bool SilentRender;
    protected override bool ShowWithoutActivation { get { return SilentRender; } }
    public Prompt(string text, string notes, string yes, string no)
    {
        Text = Updater.Title; Font = new Font("Segoe UI", 10); BackColor = Color.FromArgb(245, 246, 248); ForeColor = Color.FromArgb(32, 40, 49);
        FormBorderStyle = FormBorderStyle.FixedDialog; MaximizeBox = MinimizeBox = false; StartPosition = FormStartPosition.CenterScreen; TopMost = true;
        var label = new Label { Text = text };
        label.SetBounds(20, 20, 500, TextRenderer.MeasureText(text, Font, new Size(500, 0), TextFormatFlags.WordBreak).Height + 4); Controls.Add(label);
        int y = label.Bottom + 12;
        if (notes != null)
        {
            var box = new TextBox { Text = notes, Multiline = true, ReadOnly = true, ScrollBars = ScrollBars.Vertical, BackColor = Color.White, BorderStyle = BorderStyle.FixedSingle };
            box.SetBounds(20, y, 500, 226); Controls.Add(box); y = box.Bottom + 16;
        }
        var accept = new Button { Text = yes, DialogResult = DialogResult.OK, BackColor = Color.FromArgb(30, 40, 52), ForeColor = Color.White, FlatStyle = FlatStyle.Flat };
        accept.FlatAppearance.BorderSize = 0; accept.SetBounds(no == null ? 350 : 170, y, 170, 34); Controls.Add(accept); AcceptButton = accept; ActiveControl = accept;
        if (no != null) { var cancel = new Button { Text = no, DialogResult = DialogResult.Cancel, FlatStyle = FlatStyle.System }; cancel.SetBounds(350, y, 170, 34); Controls.Add(cancel); CancelButton = cancel; }
        ClientSize = new Size(540, y + 52);
    }
}

class Progress : Form
{
    public readonly Label Status = new Label(); public bool Finished;
    public Progress()
    {
        Text = Updater.Title; Font = new Font("Segoe UI", 10); ClientSize = new Size(480, 110); BackColor = Color.FromArgb(245, 246, 248);
        FormBorderStyle = FormBorderStyle.FixedDialog; ControlBox = false; StartPosition = FormStartPosition.CenterScreen;
        // closing it (Alt+F4 too) would end the program mid-install, leaving the game half updated until the next install
        FormClosing += delegate(object sender, FormClosingEventArgs e) { e.Cancel = !Finished; };
        Status.SetBounds(20, 20, 440, 44); Controls.Add(Status);
        var bar = new ProgressBar { Style = ProgressBarStyle.Marquee }; bar.SetBounds(20, 76, 440, 8); Controls.Add(bar);
    }
}
