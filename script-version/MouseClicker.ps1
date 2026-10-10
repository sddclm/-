# ============================================================
#  鼠标连点器（脚本版 / 无需管理员权限，不受智能应用控制拦截）
#  运行: 双击 启动连点器.bat  或
#        powershell -NoProfile -ExecutionPolicy Bypass -File MouseClicker.ps1
# ============================================================
$ErrorActionPreference = "Stop"

$cs = @'
using System;
using System.Drawing;
using System.Runtime.InteropServices;
using System.Threading;
using System.Windows.Forms;
using System.IO;
using System.Text;

namespace ScriptClicker
{
    public static class Native
    {
        [StructLayout(LayoutKind.Sequential)]
        public struct MOUSEINPUT { public int dx; public int dy; public uint mouseData; public uint dwFlags; public uint time; public IntPtr dwExtraInfo; }
        [StructLayout(LayoutKind.Sequential)]
        public struct INPUT { public uint type; public MOUSEINPUT mi; }
        [StructLayout(LayoutKind.Sequential)]
        public struct POINT { public int X; public int Y; }

        [DllImport("user32.dll", SetLastError = true)]
        public static extern uint SendInput(uint nInputs, INPUT[] pInputs, int cbSize);
        [DllImport("user32.dll")] public static extern bool RegisterHotKey(IntPtr hWnd, int id, uint fsModifiers, uint vk);
        [DllImport("user32.dll")] public static extern bool UnregisterHotKey(IntPtr hWnd, int id);
        [DllImport("user32.dll")] public static extern bool SetCursorPos(int X, int Y);
        [DllImport("user32.dll")] public static extern bool GetCursorPos(out POINT p);
        [DllImport("user32.dll")] public static extern int GetSystemMetrics(int nIndex);
        [DllImport("winmm.dll")] public static extern uint timeBeginPeriod(uint p);
        [DllImport("winmm.dll")] public static extern uint timeEndPeriod(uint p);
        [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
        [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);

        public const uint INPUT_MOUSE = 0;
        public const uint LEFTDOWN = 0x0002, LEFTUP = 0x0004;
        public const uint RIGHTDOWN = 0x0008, RIGHTUP = 0x0010;
        public const uint MIDDLEDOWN = 0x0020, MIDDLEUP = 0x0040;
        public const uint MOVE = 0x0001, ABSOLUTE = 0x8000, VIRTUALDESK = 0x4000;
        public const uint MOD_ALT = 1, MOD_CONTROL = 2, MOD_SHIFT = 4, MOD_WIN = 8, MOD_NOREPEAT = 0x4000;
        public const int SM_XVIRTUALSCREEN = 76, SM_YVIRTUALSCREEN = 77, SM_CXVIRTUALSCREEN = 78, SM_CYVIRTUALSCREEN = 79;

        public static void Mouse(uint flags)
        {
            INPUT[] a = new INPUT[1];
            a[0].type = INPUT_MOUSE;
            a[0].mi.dwFlags = flags;
            SendInput(1, a, Marshal.SizeOf(typeof(INPUT)));
        }
        public static void MoveTo(int x, int y)
        {
            int vx = GetSystemMetrics(SM_XVIRTUALSCREEN), vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
            int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN), vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
            if (vw <= 1 || vh <= 1) return;
            INPUT[] a = new INPUT[1];
            a[0].type = INPUT_MOUSE;
            a[0].mi.dx = (int)Math.Round((double)(x - vx) * 65535.0 / (double)(vw - 1));
            a[0].mi.dy = (int)Math.Round((double)(y - vy) * 65535.0 / (double)(vh - 1));
            a[0].mi.dwFlags = MOVE | ABSOLUTE | VIRTUALDESK;
            SendInput(1, a, Marshal.SizeOf(typeof(INPUT)));
        }
    }

    public class Config
    {
        public int IntervalMs = 100;
        public int Button = 0;        // 0 左 1 右 2 中
        public int ClickKind = 0;     // 0 单击 1 双击
        public int PosMode = 0;       // 0 跟随 1 固定
        public int FixedX = 0, FixedY = 0;
        public int RepeatMode = 0;    // 0 一直 1 指定次数
        public int RepeatCount = 100;
        public uint ToggleMods = 0, ToggleVk = 0x75;   // F6
        public bool StopEnabled = true;
        public uint StopMods = 0, StopVk = 0x76;       // F7

        static string Path()
        {
            string dir = System.Reflection.Assembly.GetExecutingAssembly().Location;
            if (string.IsNullOrEmpty(dir)) dir = AppDomain.CurrentDomain.BaseDirectory;
            dir = System.IO.Path.GetDirectoryName(dir);
            if (string.IsNullOrEmpty(dir)) dir = AppDomain.CurrentDomain.BaseDirectory;
            return System.IO.Path.Combine(dir, "ScriptClicker.ini");
        }
        public static Config Load()
        {
            Config c = new Config();
            try
            {
                if (File.Exists(Path()))
                {
                    string[] lines = File.ReadAllLines(Path(), Encoding.UTF8);
                    foreach (string ln in lines)
                    {
                        int eq = ln.IndexOf('=');
                        if (eq <= 0) continue;
                        string k = ln.Substring(0, eq).Trim(), v = ln.Substring(eq + 1).Trim();
                        int iv; if (!int.TryParse(v, out iv)) continue;
                        if (k == "IntervalMs") c.IntervalMs = Math.Max(1, Math.Min(3600000, iv));
                        else if (k == "Button") c.Button = iv;
                        else if (k == "ClickKind") c.ClickKind = iv;
                        else if (k == "PosMode") c.PosMode = iv;
                        else if (k == "FixedX") c.FixedX = iv;
                        else if (k == "FixedY") c.FixedY = iv;
                        else if (k == "RepeatMode") c.RepeatMode = iv;
                        else if (k == "RepeatCount") c.RepeatCount = Math.Max(1, iv);
                        else if (k == "ToggleMods") c.ToggleMods = (uint)iv;
                        else if (k == "ToggleVk") c.ToggleVk = (uint)iv;
                        else if (k == "StopEnabled") c.StopEnabled = iv != 0;
                        else if (k == "StopMods") c.StopMods = (uint)iv;
                        else if (k == "StopVk") c.StopVk = (uint)iv;
                    }
                }
            }
            catch { }
            return c;
        }
        public void Save()
        {
            try
            {
                StringBuilder sb = new StringBuilder();
                sb.AppendLine("IntervalMs=" + IntervalMs);
                sb.AppendLine("Button=" + Button);
                sb.AppendLine("ClickKind=" + ClickKind);
                sb.AppendLine("PosMode=" + PosMode);
                sb.AppendLine("FixedX=" + FixedX);
                sb.AppendLine("FixedY=" + FixedY);
                sb.AppendLine("RepeatMode=" + RepeatMode);
                sb.AppendLine("RepeatCount=" + RepeatCount);
                sb.AppendLine("ToggleMods=" + ToggleMods);
                sb.AppendLine("ToggleVk=" + ToggleVk);
                sb.AppendLine("StopEnabled=" + (StopEnabled ? 1 : 0));
                sb.AppendLine("StopMods=" + StopMods);
                sb.AppendLine("StopVk=" + StopVk);
                File.WriteAllText(Path(), sb.ToString(), Encoding.UTF8);
            }
            catch { }
        }
        public static string KeyText(uint mods, uint vk)
        {
            if (vk == 0) return "未设置";
            string s = "";
            if ((mods & Native.MOD_CONTROL) != 0) s += "Ctrl+";
            if ((mods & Native.MOD_ALT) != 0) s += "Alt+";
            if ((mods & Native.MOD_SHIFT) != 0) s += "Shift+";
            if ((mods & Native.MOD_WIN) != 0) s += "Win+";
            if (vk >= 0x70 && vk <= 0x87) return s + "F" + (vk - 0x6F);
            if (vk >= 0x30 && vk <= 0x39) return s + (char)vk;
            if (vk >= 0x41 && vk <= 0x5A) return s + (char)vk;
            Keys k = (Keys)vk;
            if (vk == 0x20) return s + "空格";
            return s + k.ToString();
        }
    }

    public class ClickerForm : Form
    {
        Config cfg = Config.Load();
        Thread worker;
        volatile bool running = false;
        volatile bool stopFlag = false;
        long clicks = 0;
        double cps = 0;
        DateTime startedAt;
        DateTime endedAt;
        int captureMode = 0;   // 0 无 1 启停 2 急停

        const int HK_TOGGLE = 0x2001;
        const int HK_STOP = 0x2002;

        NumericUpDown numInterval, numX, numY, numCount;
        ComboBox cmbButton, cmbKind, cmbPos, cmbRepeat;
        Button btnMain, btnPick, btnHkToggle, btnHkStop;
        CheckBox chkStop;
        Label lblStatus, lblStat;
        TrackBar track;
        System.Windows.Forms.Timer uiTimer;
        FlowLayoutPanel presets;

        public ClickerForm()
        {
            Text = "鼠标连点器（脚本版）";
            ClientSize = new Size(470, 560);
            FormBorderStyle = FormBorderStyle.FixedSingle;
            MaximizeBox = false;
            StartPosition = FormStartPosition.CenterScreen;
            BackColor = Color.FromArgb(24, 26, 32);
            ForeColor = Color.FromArgb(230, 234, 242);
            Font = new Font("Microsoft YaHei UI", 9F);
            KeyPreview = true;

            int y = 12;
            // ---- 主控 ----
            btnMain = new Button();
            btnMain.SetBounds(16, y, 438, 52);
            btnMain.FlatStyle = FlatStyle.Flat;
            btnMain.FlatAppearance.BorderSize = 0;
            btnMain.Font = new Font("Microsoft YaHei UI", 12F, FontStyle.Bold);
            btnMain.BackColor = Color.FromArgb(45, 170, 125);
            btnMain.ForeColor = Color.White;
            btnMain.Click += delegate { Toggle(); };
            Controls.Add(btnMain);
            y += 60;

            lblStatus = new Label();
            lblStatus.SetBounds(18, y, 200, 20);
            lblStatus.Text = "● 已停止";
            lblStatus.ForeColor = Color.FromArgb(150, 158, 172);
            Controls.Add(lblStatus);

            lblStat = new Label();
            lblStat.SetBounds(200, y, 254, 20);
            lblStat.TextAlign = ContentAlignment.MiddleRight;
            lblStat.ForeColor = Color.FromArgb(150, 158, 172);
            Controls.Add(lblStat);
            y += 30;

            // ---- 速度 ----
            GroupBox g1 = MakeGroup("点击速度", 16, y, 438, 116);
            Label l1 = MakeLabel("间隔(毫秒)", 14, 26);
            g1.Controls.Add(l1);
            numInterval = new NumericUpDown();
            numInterval.SetBounds(110, 24, 110, 26);
            numInterval.Minimum = 1; numInterval.Maximum = 3600000;
            numInterval.Value = cfg.IntervalMs;
            numInterval.BackColor = Color.FromArgb(38, 42, 52);
            numInterval.ForeColor = Color.White;
            numInterval.BorderStyle = BorderStyle.FixedSingle;
            numInterval.ValueChanged += delegate { if (!sync) { cfg.IntervalMs = (int)numInterval.Value; track.Value = Math.Min(track.Maximum, Math.Max(track.Minimum, MsToTrack(cfg.IntervalMs))); Sync(); } };
            g1.Controls.Add(numInterval);

            Label l1b = MakeLabel("约 " + (1000.0 / Math.Max(1, cfg.IntervalMs)).ToString("0.0") + " 次/秒", 236, 27);
            l1b.Name = "lblCps";
            l1b.Width = 180;
            g1.Controls.Add(l1b);

            presets = new FlowLayoutPanel();
            presets.SetBounds(10, 56, 418, 30);
            presets.FlowDirection = FlowDirection.LeftToRight;
            presets.WrapContents = false;
            int[] pv = new int[] { 1, 10, 50, 100, 500, 1000 };
            string[] pn = new string[] { "1ms", "10ms", "50ms", "100ms", "500ms", "1s" };
            for (int i = 0; i < pv.Length; i++)
            {
                int val = pv[i];
                Button b = new Button();
                b.Text = pn[i];
                b.Size = new Size(64, 26);
                b.FlatStyle = FlatStyle.Flat;
                b.FlatAppearance.BorderColor = Color.FromArgb(60, 66, 80);
                b.BackColor = Color.FromArgb(34, 38, 47);
                b.ForeColor = Color.FromArgb(210, 216, 226);
                b.Click += delegate { numInterval.Value = val; };
                presets.Controls.Add(b);
            }
            g1.Controls.Add(presets);

            track = new TrackBar();
            track.SetBounds(104, 84, 320, 28);
            track.Minimum = 0; track.Maximum = 1000; track.TickStyle = TickStyle.None;
            track.Value = MsToTrack(cfg.IntervalMs);
            track.ValueChanged += delegate { if (!sync) { int ms = TrackToMs(track.Value); numInterval.Value = Math.Max(1, Math.Min(3600000, ms)); } };
            g1.Controls.Add(track);
            Controls.Add(g1);
            y += 124;

            // ---- 点击设置 ----
            GroupBox g2 = MakeGroup("点击设置", 16, y, 438, 176);
            g2.Controls.Add(MakeLabel("鼠标按键", 14, 30));
            cmbButton = MakeCombo(110, 26, 130, new string[] { "左键", "右键", "中键" }, cfg.Button);
            g2.Controls.Add(cmbButton);

            g2.Controls.Add(MakeLabel("点击类型", 258, 30));
            cmbKind = MakeCombo(330, 26, 100, new string[] { "单击", "双击" }, cfg.ClickKind);
            g2.Controls.Add(cmbKind);

            g2.Controls.Add(MakeLabel("点击位置", 14, 66));
            cmbPos = MakeCombo(110, 62, 130, new string[] { "跟随光标", "固定坐标" }, cfg.PosMode);
            cmbPos.SelectedIndexChanged += delegate { cfg.PosMode = cmbPos.SelectedIndex; Sync(); };
            g2.Controls.Add(cmbPos);

            g2.Controls.Add(MakeLabel("X", 248, 66));
            numX = MakeNum(266, 62, 70, -100000, 100000, cfg.FixedX);
            numX.ValueChanged += delegate { cfg.FixedX = (int)numX.Value; };
            g2.Controls.Add(numX);
            g2.Controls.Add(MakeLabel("Y", 342, 66));
            numY = MakeNum(360, 62, 70, -100000, 100000, cfg.FixedY);
            numY.ValueChanged += delegate { cfg.FixedY = (int)numY.Value; };
            g2.Controls.Add(numY);

            btnPick = new Button();
            btnPick.Text = "拾取";
            btnPick.SetBounds(374, 96, 56, 26);
            btnPick.FlatStyle = FlatStyle.Flat;
            btnPick.FlatAppearance.BorderColor = Color.FromArgb(60, 66, 80);
            btnPick.BackColor = Color.FromArgb(34, 38, 47);
            btnPick.ForeColor = Color.FromArgb(210, 216, 226);
            btnPick.Click += delegate { StartPick(); };
            g2.Controls.Add(btnPick);

            g2.Controls.Add(MakeLabel("重复方式", 14, 100));
            cmbRepeat = MakeCombo(110, 96, 130, new string[] { "一直点击", "指定次数" }, cfg.RepeatMode);
            cmbRepeat.SelectedIndexChanged += delegate { cfg.RepeatMode = cmbRepeat.SelectedIndex; Sync(); };
            g2.Controls.Add(cmbRepeat);

            g2.Controls.Add(MakeLabel("次数", 254, 100));
            numCount = MakeNum(266, 96, 100, 1, 100000000, cfg.RepeatCount);
            numCount.ValueChanged += delegate { cfg.RepeatCount = (int)numCount.Value; };
            g2.Controls.Add(numCount);

            Label hint = MakeLabel("提示：固定坐标模式运行时会持续把光标移到目标点，停止请用热键", 14, 134);
            hint.Width = 410; hint.ForeColor = Color.FromArgb(120, 128, 142);
            g2.Controls.Add(hint);
            Controls.Add(g2);
            y += 184;

            // ---- 热键 ----
            GroupBox g3 = MakeGroup("全局热键", 16, y, 438, 96);
            g3.Controls.Add(MakeLabel("启停切换", 14, 30));
            btnHkToggle = MakeKeyButton(110, 26, 150);
            btnHkToggle.Click += delegate { BeginCapture(1); };
            g3.Controls.Add(btnHkToggle);

            g3.Controls.Add(MakeLabel("紧急停止", 14, 62));
            btnHkStop = MakeKeyButton(110, 58, 150);
            btnHkStop.Click += delegate { BeginCapture(2); };
            g3.Controls.Add(btnHkStop);

            chkStop = new CheckBox();
            chkStop.Text = "启用";
            chkStop.SetBounds(276, 60, 70, 24);
            chkStop.ForeColor = Color.FromArgb(210, 216, 226);
            chkStop.Checked = cfg.StopEnabled;
            chkStop.CheckedChanged += delegate { cfg.StopEnabled = chkStop.Checked; ApplyHotkeys(); Sync(); };
            g3.Controls.Add(chkStop);

            Label hkHint = MakeLabel("点击键位后直接按下新热键（Esc 取消）", 276, 30);
            hkHint.Width = 160; hkHint.ForeColor = Color.FromArgb(120, 128, 142);
            g3.Controls.Add(hkHint);
            Controls.Add(g3);

            uiTimer = new System.Windows.Forms.Timer();
            uiTimer.Interval = 100;
            uiTimer.Tick += delegate { Tick(); };
            uiTimer.Start();

            KeyDown += OnKeyDownHandler;
            FormClosing += delegate { stopFlag = true; SaveCfg(); };
            Load += delegate { ApplyHotkeys(); Sync(); };
            Shown += delegate { Native.ShowWindow(this.Handle, 5); Native.SetForegroundWindow(this.Handle); };
        }

        bool sync = false;

        GroupBox MakeGroup(string title, int x, int y, int w, int h)
        {
            GroupBox g = new GroupBox();
            g.Text = title;
            g.SetBounds(x, y, w, h);
            g.ForeColor = Color.FromArgb(150, 158, 172);
            return g;
        }
        Label MakeLabel(string text, int x, int y)
        {
            Label l = new Label();
            l.Text = text;
            l.SetBounds(x, y, 96, 22);
            l.ForeColor = Color.FromArgb(200, 206, 218);
            return l;
        }
        ComboBox MakeCombo(int x, int y, int w, string[] items, int sel)
        {
            ComboBox c = new ComboBox();
            c.DropDownStyle = ComboBoxStyle.DropDownList;
            c.SetBounds(x, y, w, 26);
            c.Items.AddRange(items);
            c.SelectedIndex = Math.Max(0, Math.Min(items.Length - 1, sel));
            c.BackColor = Color.FromArgb(38, 42, 52);
            c.ForeColor = Color.White;
            c.FlatStyle = FlatStyle.Flat;
            return c;
        }
        NumericUpDown MakeNum(int x, int y, int w, int min, int max, int val)
        {
            NumericUpDown n = new NumericUpDown();
            n.SetBounds(x, y, w, 26);
            n.Minimum = min; n.Maximum = max;
            n.Value = Math.Max(min, Math.Min(max, val));
            n.BackColor = Color.FromArgb(38, 42, 52);
            n.ForeColor = Color.White;
            n.BorderStyle = BorderStyle.FixedSingle;
            return n;
        }
        Button MakeKeyButton(int x, int y, int w)
        {
            Button b = new Button();
            b.SetBounds(x, y, w, 26);
            b.FlatStyle = FlatStyle.Flat;
            b.FlatAppearance.BorderColor = Color.FromArgb(70, 78, 94);
            b.BackColor = Color.FromArgb(34, 38, 47);
            b.ForeColor = Color.White;
            return b;
        }

        static int MsToTrack(int ms)
        {
            double t = (Math.Log(Math.Max(1, Math.Min(10000, ms))) - Math.Log(1)) / (Math.Log(10000) - Math.Log(1));
            return (int)Math.Round(t * 1000);
        }
        static int TrackToMs(int v)
        {
            double t = v / 1000.0;
            double ms = Math.Exp(Math.Log(1) + (Math.Log(10000) - Math.Log(1)) * t);
            return (int)Math.Round(ms);
        }

        void Sync()
        {
            sync = true;
            numX.Enabled = numY.Enabled = btnPick.Enabled = (cfg.PosMode == 1);
            numCount.Enabled = (cfg.RepeatMode == 1);
            btnHkStop.Enabled = cfg.StopEnabled;
            btnHkToggle.Text = captureMode == 1 ? "按下新键…" : Config.KeyText(cfg.ToggleMods, cfg.ToggleVk);
            btnHkStop.Text = captureMode == 2 ? "按下新键…" : (cfg.StopEnabled ? Config.KeyText(cfg.StopMods, cfg.StopVk) : "已停用");
            btnMain.Text = running ? ("停止点击  (" + Config.KeyText(cfg.ToggleMods, cfg.ToggleVk) + ")")
                                   : ("开始点击  (" + Config.KeyText(cfg.ToggleMods, cfg.ToggleVk) + ")");
            btnMain.BackColor = running ? Color.FromArgb(214, 74, 92) : Color.FromArgb(45, 170, 125);
            lblStatus.Text = running ? "● 运行中" : "● 已停止";
            lblStatus.ForeColor = running ? Color.FromArgb(60, 210, 155) : Color.FromArgb(150, 158, 172);
            foreach (Control c in Controls) { }
            foreach (Control c in Controls)
            {
                GroupBox gb = c as GroupBox;
                if (gb == null) continue;
                foreach (Control cc in gb.Controls)
                {
                    if (cc.Name == "lblCps") cc.Text = "约 " + (1000.0 / Math.Max(1, cfg.IntervalMs)).ToString("0.0") + " 次/秒";
                }
            }
            sync = false;
        }

        void SaveCfg() { cfg.Save(); }

        void ApplyHotkeys()
        {
            Native.UnregisterHotKey(Handle, HK_TOGGLE);
            Native.UnregisterHotKey(Handle, HK_STOP);
            if (cfg.ToggleVk != 0) Native.RegisterHotKey(Handle, HK_TOGGLE, cfg.ToggleMods | Native.MOD_NOREPEAT, cfg.ToggleVk);
            if (cfg.StopEnabled && cfg.StopVk != 0) Native.RegisterHotKey(Handle, HK_STOP, cfg.StopMods | Native.MOD_NOREPEAT, cfg.StopVk);
        }

        protected override void WndProc(ref Message m)
        {
            const int WM_HOTKEY = 0x0312;
            if (m.Msg == WM_HOTKEY)
            {
                int id = m.WParam.ToInt32();
                if (id == HK_TOGGLE) { Toggle(); return; }
                if (id == HK_STOP) { if (running) { stopFlag = true; } return; }
            }
            base.WndProc(ref m);
        }

        void BeginCapture(int mode)
        {
            if (captureMode == mode) { captureMode = 0; ApplyHotkeys(); Sync(); return; }
            captureMode = mode;
            Native.UnregisterHotKey(Handle, HK_TOGGLE);
            Native.UnregisterHotKey(Handle, HK_STOP);
            Activate();
            Sync();
        }

        void OnKeyDownHandler(object sender, KeyEventArgs e)
        {
            if (captureMode == 0) { if (e.KeyCode == Keys.Space) { Toggle(); e.Handled = true; } return; }
            if (e.KeyCode == Keys.Escape) { captureMode = 0; ApplyHotkeys(); Sync(); return; }
            Keys k = e.KeyCode;
            if (k == Keys.ShiftKey || k == Keys.LShiftKey || k == Keys.RShiftKey ||
                k == Keys.ControlKey || k == Keys.LControlKey || k == Keys.RControlKey ||
                k == Keys.Menu || k == Keys.LMenu || k == Keys.RMenu ||
                k == Keys.LWin || k == Keys.RWin) return;

            uint mods = 0;
            if (e.Control) mods |= Native.MOD_CONTROL;
            if (e.Alt) mods |= Native.MOD_ALT;
            if (e.Shift) mods |= Native.MOD_SHIFT;

            uint vk = (uint)k;
            if (captureMode == 1)
            {
                cfg.ToggleMods = mods; cfg.ToggleVk = vk;
                string err = RegisterOrError(HK_TOGGLE, mods, vk);
                if (err != null) { MessageBox.Show(this, err, "热键不可用", MessageBoxButtons.OK, MessageBoxIcon.Warning); }
            }
            else
            {
                cfg.StopMods = mods; cfg.StopVk = vk;
                string err = RegisterOrError(HK_STOP, mods, vk);
                if (err != null) { MessageBox.Show(this, err, "热键不可用", MessageBoxButtons.OK, MessageBoxIcon.Warning); }
            }
            captureMode = 0;
            ApplyHotkeys();
            Sync();
            SaveCfg();
            e.Handled = true;
            e.SuppressKeyPress = true;
        }

        string RegisterOrError(int id, uint mods, uint vk)
        {
            Native.UnregisterHotKey(Handle, id);
            if (!Native.RegisterHotKey(Handle, id, mods | Native.MOD_NOREPEAT, vk)) return "该热键已被其它程序占用，请换一个。";
            return null;
        }

        void StartPick()
        {
            cfg.PosMode = 1;
            cmbPos.SelectedIndex = 1;
            WindowState = FormWindowState.Minimized;
            System.Windows.Forms.Timer t = new System.Windows.Forms.Timer();
            t.Interval = 2500;
            t.Tick += delegate
            {
                t.Stop(); t.Dispose();
                Native.POINT p;
                if (Native.GetCursorPos(out p)) { cfg.FixedX = p.X; cfg.FixedY = p.Y; }
                WindowState = FormWindowState.Normal;
                Activate();
                sync = true; numX.Value = cfg.FixedX; numY.Value = cfg.FixedY; sync = false;
                Sync();
            };
            t.Start();
        }

        void Toggle()
        {
            if (running) { stopFlag = true; return; }
            if (cfg.PosMode == 1) { }
            clicks = 0; cps = 0; stopFlag = false;
            startedAt = DateTime.Now;
            running = true;
            worker = new Thread(Work);
            worker.IsBackground = true;
            worker.Start();
            Sync();
        }

        void Work()
        {
            Native.timeBeginPeriod(1);
            int interval = cfg.IntervalMs;
            double per = interval / 1000.0;
            double t0 = Now();
            long n = 0;
            long lastStat = 0;
            double statT = Now();
            while (!stopFlag)
            {
                if (cfg.IntervalMs != interval) { interval = cfg.IntervalMs; per = interval / 1000.0; t0 = Now(); n = 0; }
                double due = t0 + n * per;
                double wait = due - Now();
                while (wait > 0 && !stopFlag)
                {
                    if (wait > 0.004) Thread.Sleep((int)((wait - 0.002) * 1000));
                    else Thread.SpinWait(50);
                    wait = due - Now();
                }
                if (stopFlag) break;

                if (cfg.PosMode == 1) Native.MoveTo(cfg.FixedX, cfg.FixedY);
                uint down = Native.LEFTDOWN, up = Native.LEFTUP;
                if (cfg.Button == 1) { down = Native.RIGHTDOWN; up = Native.RIGHTUP; }
                else if (cfg.Button == 2) { down = Native.MIDDLEDOWN; up = Native.MIDDLEUP; }
                Native.Mouse(down); Native.Mouse(up);
                clicks++;
                if (cfg.ClickKind == 1)
                {
                    Thread.Sleep(30);
                    Native.Mouse(down); Native.Mouse(up);
                    clicks++;
                }
                n++;
                double nt = Now();
                if (nt - statT >= 0.5)
                {
                    cps = (clicks - lastStat) / (nt - statT);
                    lastStat = clicks; statT = nt;
                }
                if (cfg.RepeatMode == 1 && clicks >= cfg.RepeatCount) break;
                if (Now() - due > per * 2) { t0 = Now(); n = 0; }
            }
            endedAt = DateTime.Now;
            running = false; stopFlag = false;
            Native.timeEndPeriod(1);
        }

        static double Now() { return (double)Environment.TickCount / 1000.0; }

        void Tick()
        {
            if (running || clicks > 0)
            {
                double sec = ((running ? DateTime.Now : endedAt) - startedAt).TotalSeconds;
                lblStat.Text = string.Format("{0} 次 · 用时 {1:00}:{2:00} · {3:0.0} 次/秒",
                    clicks, (int)(sec / 60), (int)(sec % 60), cps);
            }
            else
            {
                lblStat.Text = "间隔 " + cfg.IntervalMs + " 毫秒 · 约 " + (1000.0 / Math.Max(1, cfg.IntervalMs)).ToString("0.0") + " 次/秒";
            }
            if (running != lastRunning) { lastRunning = running; Sync(); }
        }
        bool lastRunning = false;
    }

    public static class Program
    {
        [STAThread]
        public static void Main()
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new ClickerForm());
        }
    }
}
'@

Add-Type -TypeDefinition $cs -ReferencedAssemblies "System.Windows.Forms", "System.Drawing" -ErrorAction Stop
[ScriptClicker.Program]::Main()
