// mc_app.cpp —— 主程序：窗口、自绘界面、交互、全局热键、配置持久化
#include "mc_common.h"
#include "mc_config.h"
#include "mc_clicker.h"
#include "mc_widgets.h"
#include <memory>
#include <vector>

namespace {

using namespace mc;

constexpr wchar_t kClassName[] = L"MouseClickerMainWnd";
constexpr wchar_t kAppName[]   = L"鼠标连点器";

constexpr UINT_PTR IDT_UI      = 1;
constexpr UINT_PTR IDT_PICK    = 2;
constexpr UINT_PTR IDT_PICKPRE = 3;

constexpr int HK_TOGGLE = 0xA001;
constexpr int HK_STOP   = 0xA002;

// ---------------------------------------------------------------- 布局（逻辑像素）
constexpr float kWinW    = 486.0f;
constexpr float kPad     = 12.0f;
constexpr float kCardPad = 14.0f;
constexpr float kTitleH  = 46.0f;
constexpr float kGap     = 10.0f;
constexpr float kRowH    = 30.0f;
constexpr float kRowGap  = 6.0f;
constexpr float kCard1H  = 104.0f;
constexpr float kCard2H  = 154.0f;
constexpr float kCard3H  = 228.0f;
constexpr float kCard4H  = 120.0f;
constexpr float kFooterH = 22.0f;
constexpr float kWinH    = kTitleH + kGap + kCard1H + kGap + kCard2H + kGap + kCard3H + kGap + kCard4H + kGap + kFooterH;

typedef UINT     (WINAPI* PFN_GetDpiForWindow)(HWND);
typedef HRESULT  (WINAPI* PFN_DwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD);
typedef BOOL     (WINAPI* PFN_SetProcessDpiAwarenessContext)(HANDLE);
PFN_GetDpiForWindow       pGetDpiForWindow = nullptr;
PFN_DwmSetWindowAttribute pDwmSetAttr      = nullptr;

// ---------------------------------------------------------------- 应用
class App {
public:
    HWND     hwnd = nullptr;
    Settings cfg;
    Clicker  clicker;

    std::vector<std::unique_ptr<Widget>> widgets;
    Widget*  mouseCapture = nullptr;
    Widget*  kbdFocus     = nullptr;

    Button*    btnMin       = nullptr;
    Button*    btnClose     = nullptr;
    Button*    btnMain      = nullptr;
    Button*    btnPick      = nullptr;
    Button*    btnToggleKey = nullptr;
    Button*    btnStopKey   = nullptr;
    Segmented* segButton    = nullptr;
    Segmented* segKind      = nullptr;
    Segmented* segPos       = nullptr;
    Segmented* segRepeat    = nullptr;
    NumberBox* numInterval  = nullptr;
    NumberBox* numX         = nullptr;
    NumberBox* numY         = nullptr;
    NumberBox* numCount     = nullptr;
    Slider*    slider       = nullptr;
    Toggle*    tgStop       = nullptr;
    std::vector<Button*> presets;

    RectF rcTitle, rcCard1, rcCard2, rcCard3, rcCard4, rcFooter;

    int          captureMode = 0;    // 0 无 / 1 启停键 / 2 急停键
    std::wstring toast;
    double       toastUntil  = 0.0;
    bool         wasRunning  = false;
    bool         tracking    = false;

    // ---------------- 生命周期 ----------------
    void attach(HWND h) {
        hwnd = h;
        cfg.load();
        setUiScale();
        buildWidgets();
        layout();
        refreshDerived();
        applyHotkeys();
        SetTimer(hwnd, IDT_UI, 100, nullptr);
        placeWindow();
        applyRoundedCorners();
    }

    void shutdown() {
        clicker.stop();
        saveWinPos();
        cfg.save();
        KillTimer(hwnd, IDT_UI);
        KillTimer(hwnd, IDT_PICK);
        KillTimer(hwnd, IDT_PICKPRE);
        UnregisterHotKey(hwnd, HK_TOGGLE);
        UnregisterHotKey(hwnd, HK_STOP);
    }

    // ---------------- DPI 与窗口 ----------------
    void setUiScale() {
        UINT dpi = 96;
        if (pGetDpiForWindow) dpi = pGetDpiForWindow(hwnd);
        if (dpi == 0) dpi = 96;
        float scale = dpi / 96.0f;
        RECT wa{};
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
        float availH = (float)(wa.bottom - wa.top) - 24.0f;
        if (availH > 200.0f && kWinH * scale > availH) scale = availH / kWinH;
        setDpi(scale * 96.0f);
        g_font.rebuild();
    }

    void placeWindow() {
        RECT wa{};
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
        int w = SI(kWinW), h = SI(kWinH);
        int x = cfg.winX, y = cfg.winY;
        if (x < wa.left || y < wa.top || x + w > wa.right || y + h > wa.bottom) {
            x = wa.left + ((wa.right - wa.left) - w) / 2;
            y = wa.top + ((wa.bottom - wa.top) - h) / 2;
        }
        SetWindowPos(hwnd, nullptr, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
    }

    void saveWinPos() {
        // 用 rcNormalPosition 取窗口的正常位置：窗口处于最小化时 GetWindowRect 返回 -32000
        WINDOWPLACEMENT wp{};
        wp.length = sizeof(wp);
        if (GetWindowPlacement(hwnd, &wp)) {
            cfg.winX = wp.rcNormalPosition.left;
            cfg.winY = wp.rcNormalPosition.top;
            return;
        }
        if (!IsIconic(hwnd)) {
            RECT rc{};
            if (GetWindowRect(hwnd, &rc)) { cfg.winX = rc.left; cfg.winY = rc.top; }
        }
    }

    void applyRoundedCorners() {
        if (pDwmSetAttr) {
            int pref = 2; // DWMWCP_ROUND
            if (SUCCEEDED(pDwmSetAttr(hwnd, 33 /*DWMWA_WINDOW_CORNER_PREFERENCE*/, &pref, sizeof(pref)))) return;
        }
        RECT rc{};
        GetWindowRect(hwnd, &rc);
        int w = rc.right - rc.left, h = rc.bottom - rc.top;
        HRGN rgn = CreateRoundRectRgn(0, 0, w + 1, h + 1, SI(16), SI(16));
        SetWindowRgn(hwnd, rgn, TRUE);
    }

    // ---------------- 控件构建 ----------------
    void buildWidgets() {
        auto keep = [&](Widget* w) { widgets.emplace_back(w); return w; };

        btnClose = (Button*)keep(new Button());
        btnClose->text = L"✕";
        btnClose->style = BtnStyle::Ghost;
        btnClose->radius = 8.0f;
        btnClose->cb = [this] { PostMessageW(hwnd, WM_CLOSE, 0, 0); };

        btnMin = (Button*)keep(new Button());
        btnMin->text = L"─";
        btnMin->style = BtnStyle::Ghost;
        btnMin->radius = 8.0f;
        btnMin->cb = [this] { ShowWindow(hwnd, SW_MINIMIZE); };

        btnMain = (Button*)keep(new Button());
        btnMain->big = true;
        btnMain->radius = 12.0f;
        btnMain->style = BtnStyle::Success;
        btnMain->cb = [this] { toggleRun(); };

        numInterval = (NumberBox*)keep(new NumberBox());
        numInterval->minV = 1;
        numInterval->maxV = 3600000;
        numInterval->suffix = L"毫秒";
        numInterval->onChange = [this](int v) { setInterval(v, false); };

        slider = (Slider*)keep(new Slider());
        slider->onChange = [this](float t) { setInterval(tToMs(t), true); };

        struct Preset { int ms; const wchar_t* label; };
        static const Preset presetDefs[] = {
            {1, L"1ms"}, {10, L"10ms"}, {50, L"50ms"}, {100, L"100ms"}, {500, L"500ms"}, {1000, L"1s"},
        };
        for (const Preset& pd : presetDefs) {
            Button* b = (Button*)keep(new Button());
            b->text = pd.label;
            b->style = BtnStyle::Secondary;
            b->radius = 7.0f;
            b->cb = [this, ms = pd.ms] { setInterval(ms, false); };
            presets.push_back(b);
        }

        segButton = (Segmented*)keep(new Segmented());
        segButton->items = { L"左键", L"右键", L"中键" };
        segButton->onChange = [this](int i) { cfg.button = i; syncEngine(); invalidate(); };

        segKind = (Segmented*)keep(new Segmented());
        segKind->items = { L"单击", L"双击" };
        segKind->onChange = [this](int i) { cfg.clickKind = i; syncEngine(); invalidate(); };

        segPos = (Segmented*)keep(new Segmented());
        segPos->items = { L"跟随光标", L"固定坐标" };
        segPos->onChange = [this](int i) { cfg.posMode = i; refreshDerived(); syncEngine(); invalidate(); };

        numX = (NumberBox*)keep(new NumberBox());
        numX->minV = -100000; numX->maxV = 100000;
        numX->showSteppers = false;
        numX->onChange = [this](int v) { cfg.fixedX = v; syncEngine(); };

        numY = (NumberBox*)keep(new NumberBox());
        numY->minV = -100000; numY->maxV = 100000;
        numY->showSteppers = false;
        numY->onChange = [this](int v) { cfg.fixedY = v; syncEngine(); };

        btnPick = (Button*)keep(new Button());
        btnPick->text = L"拾取";
        btnPick->style = BtnStyle::Secondary;
        btnPick->radius = 7.0f;
        btnPick->cb = [this] { beginPick(); };

        segRepeat = (Segmented*)keep(new Segmented());
        segRepeat->items = { L"一直点击", L"指定次数" };
        segRepeat->onChange = [this](int i) {
            cfg.repeatMode = i;
            refreshDerived();
            syncEngine();
            invalidate();
        };

        numCount = (NumberBox*)keep(new NumberBox());
        numCount->minV = 1; numCount->maxV = 100000000;
        numCount->showSteppers = true;
        numCount->onChange = [this](int v) { cfg.repeatCount = v; syncEngine(); };

        btnToggleKey = (Button*)keep(new Button());
        btnToggleKey->style = BtnStyle::KeyCap;
        btnToggleKey->radius = 8.0f;
        btnToggleKey->cb = [this] { beginCapture(1); };

        btnStopKey = (Button*)keep(new Button());
        btnStopKey->style = BtnStyle::KeyCap;
        btnStopKey->radius = 8.0f;
        btnStopKey->cb = [this] { beginCapture(2); };

        tgStop = (Toggle*)keep(new Toggle());
        tgStop->onChange = [this](bool on) {
            cfg.stopEnabled = on;
            UnregisterHotKey(hwnd, HK_STOP);
            if (on) {
                if (!RegisterHotKey(hwnd, HK_STOP, cfg.stopMods | MOD_NOREPEAT, cfg.stopVk))
                    showToast(L"急停热键注册失败（可能被占用）", 3.0);
            }
            refreshDerived();
            cfg.save();
            invalidate();
        };
    }

    void layout() {
        const float w = S(kWinW);
        float y = S(kTitleH + kGap);
        const float cardX = S(kPad);
        const float cardW = w - S(kPad) * 2;
        rcTitle  = RectF(0, 0, w, S(kTitleH));
        rcCard1  = RectF(cardX, y, cardW, S(kCard1H)); y += S(kCard1H + kGap);
        rcCard2  = RectF(cardX, y, cardW, S(kCard2H)); y += S(kCard2H + kGap);
        rcCard3  = RectF(cardX, y, cardW, S(kCard3H)); y += S(kCard3H + kGap);
        rcCard4  = RectF(cardX, y, cardW, S(kCard4H)); y += S(kCard4H + kGap);
        rcFooter = RectF(cardX, y, cardW, S(kFooterH));

        // 标题栏按钮
        const float bs = S(30);
        btnClose->r = RectF(w - S(kPad) - bs + S(2), S((kTitleH - 30) / 2.0f), bs, bs);
        btnMin->r   = RectF(btnClose->r.X - bs - S(2), btnClose->r.Y, bs, bs);

        // 卡片 1
        const float cx = rcCard1.X + S(kCardPad);
        const float cw = rcCard1.Width - S(kCardPad) * 2;
        btnMain->r = RectF(cx, rcCard1.Y + S(14), cw, S(52));

        // 卡片 2
        {
            const float yy = rcCard2.Y + S(14 + 18 + 8);
            numInterval->r = RectF(cx, yy, S(140), S(34));
            slider->r = RectF(cx, yy + S(34 + 8), cw, S(22));
            const float py = yy + S(34 + 8 + 22 + 8);
            const float pw = (cw - S(6) * 5) / 6.0f;
            for (size_t i = 0; i < presets.size(); ++i)
                presets[i]->r = RectF(cx + (float)i * (pw + S(6)), py, pw, S(28));
        }

        // 卡片 3
        {
            const float labelW = S(64);
            const float ctrlX = cx + labelW + S(8);
            const float ctrlW = cw - labelW - S(8);
            const float yy = rcCard3.Y + S(14 + 18 + 8);
            auto rowY = [&](int i) { return yy + (float)i * S(kRowH + kRowGap); };

            segButton->r = RectF(ctrlX, rowY(0), ctrlW, S(kRowH));
            segKind->r   = RectF(ctrlX, rowY(1), ctrlW, S(kRowH));
            segPos->r    = RectF(ctrlX, rowY(2), ctrlW, S(kRowH));

            numX->r   = RectF(ctrlX + S(16), rowY(3), S(96), S(kRowH));
            numY->r   = RectF(numX->r.X + numX->r.Width + S(26), rowY(3), S(96), S(kRowH));
            btnPick->r = RectF(numY->r.X + numY->r.Width + S(12), rowY(3), ctrlX + ctrlW - (numY->r.X + numY->r.Width + S(12)), S(kRowH));

            segRepeat->r = RectF(ctrlX, rowY(4), ctrlW - S(140) - S(10), S(kRowH));
            numCount->r  = RectF(ctrlX + ctrlW - S(140), rowY(4), S(140), S(kRowH));
        }

        // 卡片 4
        {
            const float labelW = S(74);
            const float yy = rcCard4.Y + S(14 + 18 + 8);
            const float bx = cx + labelW + S(8);
            const float bw = S(172);
            btnToggleKey->r = RectF(bx, yy, bw, S(kRowH));
            btnStopKey->r   = RectF(bx, yy + S(kRowH + kRowGap), bw, S(kRowH));
            tgStop->r = RectF(rcCard4.X + rcCard4.Width - S(kCardPad) - S(46), yy + S(kRowH + kRowGap), S(46), S(kRowH));
        }
    }

    // ---------------- 状态刷新 ----------------
    Clicker::Params buildParams() const {
        Clicker::Params p;
        p.intervalMs  = cfg.intervalMs;
        p.button      = cfg.button;
        p.dblClick    = cfg.clickKind == 1;
        p.useFixedPos = cfg.posMode == 1;
        p.x = cfg.fixedX; p.y = cfg.fixedY;
        p.repeatMode  = cfg.repeatMode;
        p.repeatCount = cfg.repeatCount;
        return p;
    }

    void refreshDerived() {
        numX->enabled = numY->enabled = btnPick->enabled = (cfg.posMode == 1);
        numCount->enabled = (cfg.repeatMode == 1);
        btnStopKey->enabled = cfg.stopEnabled;

        segButton->index = cfg.button;
        segKind->index   = cfg.clickKind;
        segPos->index    = cfg.posMode;
        segRepeat->index = cfg.repeatMode;
        numInterval->setValue(cfg.intervalMs);
        numX->setValue(cfg.fixedX);
        numY->setValue(cfg.fixedY);
        numCount->setValue(cfg.repeatCount);
        slider->t = msToT(cfg.intervalMs);
        tgStop->on = cfg.stopEnabled;

        std::wstring toggle = captureMode == 1 ? L"按下新键…" : cfg.toggleText();
        std::wstring stop   = captureMode == 2 ? L"按下新键…" : (cfg.stopEnabled ? cfg.stopText() : L"已停用");
        btnToggleKey->text = toggle;
        btnStopKey->text   = stop;
        btnToggleKey->style = captureMode == 1 ? BtnStyle::Primary : BtnStyle::KeyCap;
        btnStopKey->style   = captureMode == 2 ? BtnStyle::Primary : BtnStyle::KeyCap;

        bool run = clicker.running();
        btnMain->text  = run ? L"停止点击  (" + cfg.toggleText() + L")" : L"开始点击  (" + cfg.toggleText() + L")";
        btnMain->style = run ? BtnStyle::Danger : BtnStyle::Success;
    }

    void setInterval(int ms, bool fromSlider) {
        ms = std::clamp(ms, 1, 3600000);
        cfg.intervalMs = ms;
        if (!fromSlider) numInterval->setValue(ms);
        slider->t = msToT(ms);
        syncEngine();
        invalidate();
        (void)fromSlider;
    }

    void syncEngine() {
        if (clicker.running()) clicker.updateParams(buildParams());
    }

    void showToast(const std::wstring& s, double sec = 2.4) {
        toast = s;
        toastUntil = nowSec() + sec;
        invalidate();
    }

    void invalidate() { if (hwnd) InvalidateRect(hwnd, nullptr, FALSE); }

    // ---------------- 运行控制 ----------------
    void toggleRun() {
        if (clicker.running()) {
            clicker.requestStop();
            showToast(L"已停止");
        } else {
            clicker.start(buildParams());
            showToast(wfmt(L"开始点击 · 间隔 %d 毫秒", cfg.intervalMs));
        }
        wasRunning = clicker.running();
        refreshDerived();
        invalidate();
    }

    // ---------------- 热键 ----------------
    void applyHotkeys() {
        UnregisterHotKey(hwnd, HK_TOGGLE);
        UnregisterHotKey(hwnd, HK_STOP);
        if (!RegisterHotKey(hwnd, HK_TOGGLE, cfg.toggleMods | MOD_NOREPEAT, cfg.toggleVk))
            showToast(L"启停热键注册失败（可能被占用）", 3.5);
        if (cfg.stopEnabled)
            if (!RegisterHotKey(hwnd, HK_STOP, cfg.stopMods | MOD_NOREPEAT, cfg.stopVk))
                showToast(L"急停热键注册失败（可能被占用）", 3.5);
    }

    void beginCapture(int mode) {
        if (captureMode == mode) { cancelCapture(); return; }
        // 先注销热键，让按键能正常到达窗口
        UnregisterHotKey(hwnd, HK_TOGGLE);
        UnregisterHotKey(hwnd, HK_STOP);
        captureMode = mode;
        if (kbdFocus) { kbdFocus->onBlur(); kbdFocus = nullptr; }
        SetFocus(hwnd);
        showToast(mode == 1 ? L"请按下新的【启停】热键…（Esc 取消）" : L"请按下新的【急停】热键…（Esc 取消）", 6.0);
        refreshDerived();
        invalidate();
    }

    void cancelCapture() {
        captureMode = 0;
        applyHotkeys();
        showToast(L"已取消修改热键");
        refreshDerived();
        invalidate();
    }

    bool handleCaptureKey(UINT vk) {
        if (captureMode == 0) return false;
        if (vk == VK_ESCAPE) { cancelCapture(); return true; }
        if (Settings::isModifierKey(vk)) return true;

        UINT mods = 0;
        if (GetKeyState(VK_CONTROL) & 0x8000) mods |= MOD_CONTROL;
        if (GetKeyState(VK_MENU)    & 0x8000) mods |= MOD_ALT;
        if (GetKeyState(VK_SHIFT)   & 0x8000) mods |= MOD_SHIFT;
        if ((GetKeyState(VK_LWIN) & 0x8000) || (GetKeyState(VK_RWIN) & 0x8000)) mods |= MOD_WIN;

        const bool isToggle = captureMode == 1;
        UINT& mRef = isToggle ? cfg.toggleMods : cfg.stopMods;
        UINT& vRef = isToggle ? cfg.toggleVk   : cfg.stopVk;
        const UINT oldM = mRef, oldV = vRef;
        mRef = mods;
        vRef = vk;

        const int id = isToggle ? HK_TOGGLE : HK_STOP;
        UnregisterHotKey(hwnd, id);
        bool ok = RegisterHotKey(hwnd, id, mods | MOD_NOREPEAT, vk) != 0;
        if (!ok) {
            mRef = oldM; vRef = oldV;
            RegisterHotKey(hwnd, id, oldM | MOD_NOREPEAT, oldV);
            captureMode = 0;
            applyHotkeys();
            showToast(L"该热键不可用或已被占用，请换一个", 3.5);
            refreshDerived();
            invalidate();
            return true;
        }
        captureMode = 0;
        if (isToggle) UnregisterHotKey(hwnd, HK_STOP);
        applyHotkeys();
        cfg.save();
        showToast(isToggle ? (L"启停热键已设为 " + cfg.toggleText()) : (L"急停热键已设为 " + cfg.stopText()));
        refreshDerived();
        invalidate();
        return true;
    }

    void onHotkey(int id) {
        if (id == HK_TOGGLE) toggleRun();
        else if (id == HK_STOP) {
            if (clicker.running()) {
                clicker.requestStop();
                showToast(L"已停止");
                wasRunning = false;
                refreshDerived();
                invalidate();
            }
        }
    }

    // ---------------- 坐标拾取 ----------------
    void beginPick() {
        cfg.posMode = 1;
        refreshDerived();
        showToast(L"3 秒后将拾取鼠标位置，请准备…", 6.0);
        SetTimer(hwnd, IDT_PICKPRE, 1400, nullptr);
    }

    // ---------------- 定时器 ----------------
    void onTimer(UINT_PTR id) {
        if (id == IDT_UI) tickUi();
        else if (id == IDT_PICKPRE) {
            KillTimer(hwnd, IDT_PICKPRE);
            ShowWindow(hwnd, SW_MINIMIZE);
            SetTimer(hwnd, IDT_PICK, 1600, nullptr);
        } else if (id == IDT_PICK) {
            KillTimer(hwnd, IDT_PICK);
            POINT pt{};
            GetCursorPos(&pt);
            cfg.fixedX = pt.x;
            cfg.fixedY = pt.y;
            numX->setValue(pt.x);
            numY->setValue(pt.y);
            ShowWindow(hwnd, SW_RESTORE);
            SetForegroundWindow(hwnd);
            refreshDerived();
            showToast(wfmt(L"已拾取坐标 (%d, %d)", pt.x, pt.y));
        }
    }

    void tickUi() {
        clicker.reapIfFinished();
        bool run = clicker.running();
        if (run != wasRunning) {
            wasRunning = run;
            refreshDerived();
            if (!run && clicker.target() > 0 && clicker.clicks() >= clicker.target())
                showToast(wfmt(L"已完成 %s 次点击", groupNum(clicker.clicks()).c_str()));
            invalidate();
            return;
        }
        if (run || nowSec() < toastUntil || captureMode) invalidate();
    }

    // ---------------- 命中测试 ----------------
    Widget* hitTest(PointF p) {
        for (auto it = widgets.rbegin(); it != widgets.rend(); ++it) {
            Widget* w = it->get();
            if (w->enabled && w->visible && inRect(w->r, p)) return w;
        }
        return nullptr;
    }

    // ---------------- 鼠标 ----------------
    void onMouseMove(PointF p) {
        if (!tracking) {
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);
            tracking = true;
        }
        if (mouseCapture) { mouseCapture->onDrag(p); }
        for (auto& w : widgets) w->onMove(p);
        invalidate();
    }

    void onMouseLeave() {
        tracking = false;
        for (auto& w : widgets) { w->hover = false; }
        invalidate();
    }

    void onMouseDown(PointF p) {
        SetFocus(hwnd);
        Widget* hit = hitTest(p);
        if (kbdFocus && kbdFocus != hit) {
            if (kbdFocus->takesKeyboard()) kbdFocus->onBlur();
            kbdFocus = nullptr;
        }
        if (!hit) { invalidate(); return; }
        if (hit->onDown(p)) mouseCapture = hit;
        if (hit->takesKeyboard()) kbdFocus = hit;
        invalidate();
    }

    void onMouseUp(PointF p) {
        if (mouseCapture) { mouseCapture->onUp(p); mouseCapture = nullptr; }
        invalidate();
    }

    void onWheel(PointF p, int delta) {
        Widget* hit = hitTest(p);
        if (hit) hit->onWheel(delta);
    }

    // ---------------- 键盘 ----------------
    void onKeyDown(UINT vk, bool ctrl) {
        if (captureMode != 0) { handleCaptureKey(vk); return; }
        if (kbdFocus && kbdFocus->takesKeyboard()) { kbdFocus->onKeyDown(vk, ctrl); invalidate(); return; }
        if (vk == VK_SPACE) { toggleRun(); }
    }

    void onChar(wchar_t c) {
        if (captureMode != 0) return;
        if (kbdFocus && kbdFocus->takesKeyboard()) { kbdFocus->onChar(c); invalidate(); }
    }

    // ---------------- 绘制 ----------------
    void paint(Graphics& g) {
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
        g.Clear(Color(0xFF000000));

        const float w = S(kWinW), h = S(kWinH);
        fillRound(g, RectF(0, 0, w, h), S(14), Color(col::Win));
        strokeRound(g, RectF(0.5f, 0.5f, w - 1, h - 1), S(14), Color(col::WinEdge), 1.0f);

        paintTitle(g);
        paintCard1(g);
        paintCard2(g);
        paintCard3(g);
        paintCard4(g);
        paintFooter(g);
        for (auto& wdg : widgets) wdg->paint(g);
    }

    void paintTitle(Graphics& g) {
        RectF icon(S(kPad + 2), S((kTitleH - 22) / 2.0f), S(22), S(22));
        {
            GraphicsPath* path = makeRoundRect(icon, S(6.5f));
            LinearGradientBrush br(icon, Color(0xFF5E9BFF), Color(0xFF3B7BE8), LinearGradientModeVertical);
            g.FillPath(&br, path);
            delete path;
            const float bw = icon.Width * 0.34f, bh = icon.Height * 0.54f;
            RectF body(icon.X + (icon.Width - bw) / 2, icon.Y + icon.Height * 0.24f, bw, bh);
            fillRound(g, body, bw / 2, Color(0xFFFFFFFF));
            Pen pen(Color(0x55203050), S(1.0f));
            g.DrawLine(&pen, body.X + S(0.6f), body.Y + body.Height * 0.44f, body.X + body.Width - S(0.6f), body.Y + body.Height * 0.44f);
            // 点击圆点
            SolidBrush dot(Color(0xFFFFFFFF));
            g.FillEllipse(&dot, RectF(icon.X + icon.Width * 0.30f, icon.Y + icon.Height * 0.12f, S(4), S(4)));
        }
        drawText(g, L"鼠标连点器", RectF(icon.X + icon.Width + S(9), 0, S(200), S(kTitleH)), *g_font.h1, Color(col::Text), Align::Left, VAlign::Middle);
        std::wstring state = clicker.running() ? L"运行中" : L"";
        drawText(g, state, RectF(S(180), 0, S(kWinW) - S(280), S(kTitleH)), *g_font.tiny, Color(col::Green), Align::Right, VAlign::Middle);
    }

    void paintCard1(Graphics& g) {
        drawCard(g, rcCard1);
        const float cx = rcCard1.X + S(kCardPad);
        const float cw = rcCard1.Width - S(kCardPad) * 2;
        const float sy = rcCard1.Y + S(14 + 52 + 6);
        RectF row(cx, sy, cw, S(18));
        const bool run = clicker.running();

        const float r0 = S(4.0f);
        const PointF c(row.X + r0, row.Y + row.Height / 2);
        if (run) {
            const float pulse = 0.5f + 0.5f * (float)std::sin(nowSec() * 5.0);
            ARGB glow = col::Green & 0x00FFFFFF;
            fillRound(g, RectF(c.X - r0 * 2.2f, c.Y - r0 * 2.2f, r0 * 4.4f, r0 * 4.4f), r0 * 2.2f, Color(((ARGB)(30 + (int)(60 * pulse)) << 24) | glow));
        }
        SolidBrush lb(Color(run ? col::Green : col::TextFaint));
        g.FillEllipse(&lb, RectF(c.X - r0, c.Y - r0, r0 * 2, r0 * 2));
        drawText(g, run ? L"运行中" : L"已停止",
                 RectF(row.X + r0 * 2 + S(8), row.Y, S(90), row.Height),
                 *g_font.bodyB, Color(run ? col::Green : col::TextDim), Align::Left, VAlign::Middle);

        std::wstring stat;
        if (run || clicker.clicks() > 0)
            stat = wfmt(L"%s 次 · 用时 %s · %.1f 次/秒",
                        groupNum(clicker.clicks()).c_str(), fmtDuration(clicker.elapsed()).c_str(), clicker.cps());
        else
            stat = wfmt(L"间隔 %d 毫秒 · 约 %.1f 次/秒", cfg.intervalMs, Clicker::nominalCps(buildParams()));
        drawText(g, stat, row, *g_font.small, Color(col::TextDim), Align::Right, VAlign::Middle);
    }

    void paintCard2(Graphics& g) {
        drawCard(g, rcCard2);
        const float x = rcCard2.X + S(kCardPad);
        const float w = rcCard2.Width - S(kCardPad) * 2;
        const float y = rcCard2.Y + S(14);
        drawText(g, L"点击速度", RectF(x, y, w, S(18)), *g_font.small, Color(col::TextDim), Align::Left, VAlign::Middle);
        drawText(g, wfmt(L"≈ %.1f 次/秒", Clicker::nominalCps(buildParams())),
                 RectF(x, y, w, S(18)), *g_font.small, Color(col::Accent), Align::Right, VAlign::Middle);

        const float iy = y + S(18 + 8);
        std::wstring hint = wfmt(L"每 %d 毫秒点击一次", cfg.intervalMs);
        if (cfg.clickKind == 1) hint += L"（双击）";
        drawText(g, hint, RectF(x + S(150), iy, w - S(150), S(34)), *g_font.small, Color(col::TextDim), Align::Left, VAlign::Middle);
    }

    void paintCard3(Graphics& g) {
        drawCard(g, rcCard3);
        const float x = rcCard3.X + S(kCardPad);
        const float w = rcCard3.Width - S(kCardPad) * 2;
        const float y = rcCard3.Y + S(14);
        drawText(g, L"点击设置", RectF(x, y, w, S(18)), *g_font.small, Color(col::TextDim), Align::Left, VAlign::Middle);

        const float labelW = S(64);
        const float ctrlX = x + labelW + S(8);
        const float yy = y + S(18 + 8);
        const wchar_t* labels[5] = { L"鼠标按键", L"点击类型", L"点击位置", L"固定坐标", L"重复方式" };
        for (int i = 0; i < 5; ++i) {
            float ry = yy + (float)i * S(kRowH + kRowGap);
            drawText(g, labels[i], RectF(x, ry, labelW, S(kRowH)), *g_font.body, Color(col::TextDim), Align::Left, VAlign::Middle);
        }
        // 坐标行的 X / Y 前缀
        const float ry3 = yy + 3 * S(kRowH + kRowGap);
        Color prefCol = cfg.posMode == 1 ? col::TextDim : col::TextFaint;
        drawText(g, L"X", RectF(ctrlX, ry3, S(16), S(kRowH)), *g_font.body, prefCol, Align::Left, VAlign::Middle);
        drawText(g, L"Y", RectF(numY->r.X - S(16), ry3, S(16), S(kRowH)), *g_font.body, prefCol, Align::Left, VAlign::Middle);
    }

    void paintCard4(Graphics& g) {
        drawCard(g, rcCard4);
        const float x = rcCard4.X + S(kCardPad);
        const float w = rcCard4.Width - S(kCardPad) * 2;
        const float y = rcCard4.Y + S(14);
        drawText(g, L"全局热键", RectF(x, y, w, S(18)), *g_font.small, Color(col::TextDim), Align::Left, VAlign::Middle);

        const float labelW = S(74);
        const float yy = y + S(18 + 8);
        drawText(g, L"启停切换", RectF(x, yy, labelW, S(kRowH)), *g_font.body, Color(col::TextDim), Align::Left, VAlign::Middle);
        drawText(g, L"紧急停止", RectF(x, yy + S(kRowH + kRowGap), labelW, S(kRowH)), *g_font.body, Color(col::TextDim), Align::Left, VAlign::Middle);
        drawText(g, L"点击键位后按下新热键", RectF(btnToggleKey->r.X + btnToggleKey->r.Width + S(12), yy, S(200), S(kRowH)), *g_font.tiny, Color(col::TextFaint), Align::Left, VAlign::Middle);
        drawText(g, L"启用", RectF(tgStop->r.X - S(34), yy + S(kRowH + kRowGap), S(30), S(kRowH)), *g_font.tiny, Color(col::TextDim), Align::Right, VAlign::Middle);
    }

    void paintFooter(Graphics& g) {
        std::wstring msg;
        if (nowSec() < toastUntil && !toast.empty()) msg = toast;
        else if (captureMode != 0) msg = captureMode == 1 ? L"按下组合键设置启停热键，Esc 取消" : L"按下组合键设置急停热键，Esc 取消";
        else msg = L"热键全局生效 · 设置自动保存 · 运行中可实时调速";
        bool isToast = nowSec() < toastUntil && !toast.empty();
        drawText(g, msg, rcFooter, *g_font.tiny, Color(isToast ? col::TextDim : col::TextFaint), Align::Center, VAlign::Middle);
    }
};

App g_app;

// ---------------------------------------------------------------- 窗口过程
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    App* app = (App*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if (msg == WM_CREATE) {
        CREATESTRUCTW* cs = (CREATESTRUCTW*)lp;
        app = (App*)cs->lpCreateParams;
        app->hwnd = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)app);
        app->attach(hwnd);
        return 0;
    }
    if (!app) return DefWindowProcW(hwnd, msg, wp, lp);

    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP bmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
            HBITMAP old = (HBITMAP)SelectObject(memDC, bmp);
            {
                Graphics g(memDC);
                app->paint(g);
            }
            BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);
            SelectObject(memDC, old);
            DeleteObject(bmp);
            DeleteDC(memDC);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_MOUSEMOVE: {
            POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            app->onMouseMove(PointF((float)pt.x, (float)pt.y));
            return 0;
        }
        case WM_MOUSELEAVE:
            app->onMouseLeave();
            return 0;
        case WM_LBUTTONDOWN: {
            POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            app->onMouseDown(PointF((float)pt.x, (float)pt.y));
            return 0;
        }
        case WM_LBUTTONUP: {
            POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            app->onMouseUp(PointF((float)pt.x, (float)pt.y));
            return 0;
        }
        case WM_MOUSEWHEEL: {
            POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            ScreenToClient(hwnd, &pt);
            app->onWheel(PointF((float)pt.x, (float)pt.y), GET_WHEEL_DELTA_WPARAM(wp));
            return 0;
        }
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            if (msg == WM_SYSKEYDOWN && app->captureMode != 0) { app->onKeyDown((UINT)wp, ctrl); return 0; }
            if (msg == WM_KEYDOWN) { app->onKeyDown((UINT)wp, ctrl); return 0; }
            break;
        }
        case WM_CHAR: {
            app->onChar((wchar_t)wp);
            return 0;
        }
        case WM_HOTKEY:
            app->onHotkey((int)wp);
            return 0;
        case WM_TIMER:
            app->onTimer((UINT_PTR)wp);
            return 0;
        case WM_NCHITTEST: {
            POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            ScreenToClient(hwnd, &pt);
            PointF p((float)pt.x, (float)pt.y);
            if (inRect(app->rcTitle, p) && !inRect(app->btnMin->r, p) && !inRect(app->btnClose->r, p))
                return HTCAPTION;
            return HTCLIENT;
        }
        case WM_SETCURSOR: {
            if (LOWORD(lp) == HTCLIENT) {
                POINT pt;
                GetCursorPos(&pt);
                ScreenToClient(hwnd, &pt);
                Widget* w = app->hitTest(PointF((float)pt.x, (float)pt.y));
                SetCursor(LoadCursorW(nullptr, (LPCWSTR)(ULONG_PTR)(w ? IDC_HAND : IDC_ARROW)));
                return TRUE;
            }
            break;
        }
        case WM_ACTIVATE: {
            if (LOWORD(wp) == WA_INACTIVE && app->captureMode != 0) app->cancelCapture();
            break;
        }
        case WM_DPICHANGED: {
            app->setUiScale();
            app->layout();
            RECT* sug = (RECT*)lp;
            SetWindowPos(hwnd, nullptr, sug->left, sug->top, sug->right - sug->left, sug->bottom - sug->top, SWP_NOZORDER | SWP_NOACTIVATE);
            app->invalidate();
            return 0;
        }
        case WM_CLOSE:
            app->shutdown();
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ---------------------------------------------------------------- WinMain
// 匿名命名空间内的函数是内部链接，crt0 需要外部链接的 WinMain 符号，
// 因此这里用 runMain 实现，在命名空间外提供 WinMain 入口。
static int runMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nShow) {
    HMODULE u32 = GetModuleHandleW(L"user32.dll");
    if (u32) {
        auto setDpiAware = (PFN_SetProcessDpiAwarenessContext)GetProcAddress(u32, "SetProcessDpiAwarenessContext");
        if (setDpiAware) setDpiAware((HANDLE)-4); // PER_MONITOR_AWARE_V2
        pGetDpiForWindow = (PFN_GetDpiForWindow)GetProcAddress(u32, "GetDpiForWindow");
    }
    HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
    if (dwm) pDwmSetAttr = (PFN_DwmSetWindowAttribute)GetProcAddress(dwm, "DwmSetWindowAttribute");

    GdiplusStartupInput gsi;
    ULONG_PTR token = 0;
    if (GdiplusStartup(&token, &gsi, nullptr) != Gdiplus::Ok) return 2;

    setDpi(96.0f);
    g_font.init();

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.style = CS_DROPSHADOW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(1));
    wc.hIconSm = wc.hIcon;
    wc.hCursor = LoadCursorW(nullptr, (LPCWSTR)(ULONG_PTR)IDC_ARROW);
    wc.lpszClassName = kClassName;
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(
        WS_EX_APPWINDOW, kClassName, kAppName,
        WS_POPUP | WS_MINIMIZEBOX | WS_SYSMENU | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, (int)S(kWinW), (int)S(kWinH),
        nullptr, nullptr, hInst, &g_app);
    if (!hwnd) return 3;

    ShowWindow(hwnd, nShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    GdiplusShutdown(token);
    return (int)msg.wParam;
}

} // namespace

// 全局入口（外部链接）
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow) {
    return runMain(hInst, hPrev, lpCmd, nShow);
}
