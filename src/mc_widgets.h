// mc_widgets.h —— 轻量自绘控件库（深色主题，全部 GDI+ 绘制）
#pragma once
#include "mc_common.h"

namespace mc {

// 简易动画插值
inline float lerp(float a, float b, float t) { return a + (b - a) * t; }

class Widget {
public:
    RectF r;
    bool hover = false, pressed = false, enabled = true, visible = true;
    virtual ~Widget() = default;
    virtual void paint(Graphics& g) = 0;
    virtual void onMove(PointF p) { hover = enabled && visible && inRect(r, p); }
    virtual bool onDown(PointF p) { if (enabled && visible && inRect(r, p)) { pressed = true; return true; } return false; }
    virtual void onUp(PointF p) { bool fire = pressed && inRect(r, p); pressed = false; if (fire) onClick(); }
    virtual void onDrag(PointF) {}
    virtual void onClick() {}
    virtual void onWheel(int delta) { (void)delta; }
    // 键盘
    virtual bool takesKeyboard() const { return false; }
    virtual void onChar(wchar_t) {}
    virtual void onKeyDown(UINT, bool) {}
    virtual void onBlur() {}
    virtual HCURSOR cursor() const { return nullptr; }
};

// ---------------------------------------------------------------- 卡片 / 标签
inline void drawCard(Graphics& g, const RectF& r, float radius = 12.0f) {
    fillRoundBorder(g, r, radius, Color(col::Card), Color(col::CardEdge), 1.0f);
}
inline void drawCardTitle(Graphics& g, const std::wstring& text, const RectF& r) {
    drawText(g, text, r, *g_font.small, Color(col::TextDim), Align::Left, VAlign::Middle);
}

// ---------------------------------------------------------------- 按钮
enum class BtnStyle { Primary, Success, Danger, Secondary, Ghost, KeyCap };

class Button : public Widget {
public:
    std::wstring text;
    BtnStyle style = BtnStyle::Secondary;
    bool big = false;
    float radius = 10.0f;
    std::function<void()> cb;   // 注意：不可命名为 onClick，否则会隐藏基类虚函数
    void onClick() override { if (cb) cb(); }

    void paint(Graphics& g) override {
        if (!visible) return;
        float rad = big ? radius * 1.2f : radius;
        bool dis = !enabled;
        float hoverT = hover ? 1.0f : 0.0f;
        float pressT = pressed ? 1.0f : 0.0f;

        Color top, bottom, edge, txt;
        switch (style) {
            case BtnStyle::Primary:
                top    = Color(dis ? 0xFF2A3550 : (hover ? 0xFF5E9BFF : col::Accent));
                bottom = Color(dis ? 0xFF233047 : (hover ? 0xFF3F82F5 : 0xFF3B7BE8));
                edge   = Color(0x00000000);
                txt    = Color(col::White);
                break;
            case BtnStyle::Success:
                top    = Color(hover ? 0xFF3FE3AC : 0xFF2FCB98);
                bottom = Color(hover ? 0xFF21BC8C : 0xFF17A87B);
                edge   = Color(0x00000000);
                txt    = Color(0xFF07130F);
                break;
            case BtnStyle::Danger:
                top    = Color(hover ? 0xFFFF6478 : 0xFFEE4E64);
                bottom = Color(hover ? 0xFFE8435A : 0xFFD93A50);
                edge   = Color(0x00000000);
                txt    = Color(col::White);
                break;
            case BtnStyle::Ghost:
                top = bottom = Color(hover ? 0x14FFFFFF : 0x00000000);
                if (pressed) top = bottom = Color(0x22FFFFFF);
                edge = Color(0x00000000);
                txt  = Color(dis ? col::TextFaint : (hover ? col::Text : col::TextDim));
                break;
            case BtnStyle::KeyCap: {
                top    = Color(hover ? 0xFF262C38 : 0xFF1F242E);
                bottom = Color(hover ? 0xFF20252F : 0xFF1A1E27);
                edge   = Color(hover ? 0xFF3A4557 : col::FieldEdge);
                txt    = Color(col::Text);
                break;
            }
            case BtnStyle::Secondary:
            default:
                top    = Color(hover ? 0xFF2A3040 : 0xFF232833);
                bottom = Color(hover ? 0xFF242936 : 0xFF1E222B);
                edge   = Color(hover ? 0xFF3A4356 : col::FieldEdge);
                txt    = Color(dis ? col::TextFaint : col::Text);
                break;
        }

        if (pressT > 0.0f && style != BtnStyle::Ghost) {
            auto shade = [](const Color& c, float k) {
                return Color(c.GetA(), (BYTE)(c.GetR() * k), (BYTE)(c.GetG() * k), (BYTE)(c.GetB() * k));
            };
            top = shade(top, 0.84f);
            bottom = shade(bottom, 0.72f);
        }
        (void)hoverT;

        // 主按钮的柔和外发光
        if (big && (style == BtnStyle::Success || style == BtnStyle::Danger || style == BtnStyle::Primary) && enabled) {
            for (int i = 3; i >= 1; --i) {
                float e = S(i * 1.6f);
                ARGB base = style == BtnStyle::Danger ? col::Red : (style == BtnStyle::Success ? col::Green : col::Accent);
                BYTE a = (BYTE)(style == BtnStyle::Success ? 10 : 14);
                Color glow((ARGB)(a << 24) | (base & 0x00FFFFFF));
                fillRound(g, RectF(r.X - e, r.Y - e, r.Width + e * 2, r.Height + e * 2), rad + e, glow);
            }
        }

        GraphicsPath* path = makeRoundRect(r, rad);
        if (style == BtnStyle::Ghost && top.GetA() == 0) {
            // 无背景
        } else {
            LinearGradientBrush br(r, top, bottom, LinearGradientModeVertical);
            g.FillPath(&br, path);
        }
        if (edge.GetA() > 0) {
            Pen pen(edge, 1.0f);
            g.DrawPath(&pen, path);
        }
        delete path;
        if (style == BtnStyle::KeyCap) {
            // 键帽底部高光
            Pen pen(Color(0x18FFFFFF), 1.0f);
            RectF inner(r.X + 1.0f, r.Y + 1.0f, r.Width - 2.0f, r.Height - 2.0f);
            GraphicsPath* ip = makeRoundRect(inner, rad - 1.0f);
            g.DrawPath(&pen, ip);
            delete ip;
        }
        Font* f = big ? g_font.btnBig : g_font.btn;
        RectF tr = r;
        if (pressed) tr.Y += S(1.0f);
        drawText(g, text, tr, *f, txt, Align::Center, VAlign::Middle);
    }
};

// ---------------------------------------------------------------- 分段选择
class Segmented : public Widget {
public:
    std::vector<std::wstring> items;
    int index = 0;
    float radius = 9.0f;
    std::function<void(int)> onChange;
    float anim = -1.0f; // 滑块动画位置（以项为单位）

    void paint(Graphics& g) override {
        if (!visible || items.empty()) return;
        float rad = radius;
        fillRoundBorder(g, r, rad, Color(col::Field), Color(col::FieldEdge), 1.0f);

        float n = (float)items.size();
        float pad = S(3.0f);
        float w = (r.Width - pad * 2) / n;
        if (anim < 0.0f) anim = (float)index;
        anim = lerp(anim, (float)index, 0.35f);
        if (std::fabs(anim - (float)index) < 0.01f) anim = (float)index;

        RectF sel(r.X + pad + anim * w, r.Y + pad, w, r.Height - pad * 2);
        fillRound(g, sel, std::max(2.0f, rad - pad * 0.6f), Color(col::AccentSoft));
        strokeRound(g, sel, std::max(2.0f, rad - pad * 0.6f), Color(0x664C8DFF), 1.0f);

        for (size_t i = 0; i < items.size(); ++i) {
            RectF ir(r.X + pad + (float)i * w, r.Y + pad, w, r.Height - pad * 2);
            bool sel_i = ((int)i == index);
            drawText(g, items[i], ir, *(sel_i ? g_font.bodyB : g_font.body),
                     Color(sel_i ? col::White : (hover ? col::Text : col::TextDim)),
                     Align::Center, VAlign::Middle);
        }
    }
    void onClick() override {
        if (items.empty()) return;
        float pad = S(3.0f);
        float n = (float)items.size();
        float w = (r.Width - pad * 2) / n;
        int idx = (int)std::floor((lastPoint.X - r.X - pad) / w);
        idx = std::clamp(idx, 0, (int)items.size() - 1);
        if (idx != index) { index = idx; if (onChange) onChange(index); }
    }
    bool onDown(PointF p) override {
        lastPoint = p;
        return Widget::onDown(p);
    }
    PointF lastPoint{};

    void paintHoverCursor(Graphics& g) { (void)g; }
};

// ---------------------------------------------------------------- 数字输入框
class NumberBox : public Widget {
public:
    int value = 0, minV = 0, maxV = 1000000, step = 1;
    std::wstring suffix;
    bool showSteppers = true;
    bool focused = false;
    std::function<void(int)> onChange;
    RectF stepperUpRect() const { return RectF(r.X + r.Width - S(20), r.Y + S(2), S(18), r.Height / 2 - S(2)); }
    RectF stepperDnRect() const { return RectF(r.X + r.Width - S(20), r.Y + r.Height / 2, S(18), r.Height / 2 - S(2)); }

    void setValue(int v, bool notify = false) {
        v = std::clamp(v, minV, maxV);
        if (v == value) return;
        value = v;
        if (notify && onChange) onChange(value);
    }
    void bump(int d) { setValue(value + d, true); }

    void paint(Graphics& g) override {
        if (!visible) return;
        float rad = S(9.0f);
        Color bg = Color(focused ? col::FieldHover : (hover ? col::FieldHover : col::Field));
        Color edge = Color(focused ? col::Accent : (hover ? 0xFF39435A : col::FieldEdge));
        fillRoundBorder(g, r, rad, bg, edge, focused ? 1.6f : 1.0f);

        RectF txtR = r;
        txtR.X += S(11);
        txtR.Width -= S(showSteppers ? 30 : 15);
        std::wstring shown = editing_ ? buf_ : std::to_wstring(value);
        if (!suffix.empty() && !editing_) shown += L" " + suffix;
        drawText(g, shown, txtR, *g_font.body, Color(enabled ? col::Text : col::TextFaint), Align::Left, VAlign::Middle);

        if (editing_) {
            // 光标
            std::wstring before = buf_.substr(0, caret_);
            float w = textWidth(g, before, *g_font.body);
            float cx = txtR.X + w + S(1);
            Pen pen(Color(col::Accent), S(1.5f));
            g.DrawLine(&pen, cx, r.Y + S(7), cx, r.Y + r.Height - S(7));
        }

        if (showSteppers) {
            RectF up = stepperUpRect(), dn = stepperDnRect();
            SolidBrush bb(Color(col::TextFaint));
            auto tri = [&](RectF box, bool upward, bool hot) {
                float cx = box.X + box.Width / 2, cy = box.Y + box.Height / 2;
                float s = S(3.4f);
                PointF pts[3];
                if (upward) { pts[0] = PointF(cx, cy - s); pts[1] = PointF(cx - s, cy + s * 0.8f); pts[2] = PointF(cx + s, cy + s * 0.8f); }
                else        { pts[0] = PointF(cx, cy + s); pts[1] = PointF(cx - s, cy - s * 0.8f); pts[2] = PointF(cx + s, cy - s * 0.8f); }
                SolidBrush b(Color(hot ? col::Text : col::TextDim));
                g.FillPolygon(&b, pts, 3);
            };
            tri(up, true,  hoverUp_);
            tri(dn, false, hoverDn_);
            Pen sep(Color(col::FieldEdge), 1.0f);
            g.DrawLine(&sep, r.X + r.Width - S(24), r.Y + S(5), r.X + r.Width - S(24), r.Y + r.Height - S(5));
        }
    }

    void onMove(PointF p) override {
        Widget::onMove(p);
        hoverUp_ = hover && inRect(stepperUpRect(), p);
        hoverDn_ = hover && inRect(stepperDnRect(), p);
    }
    bool onDown(PointF p) override {
        if (!enabled || !visible || !inRect(r, p)) return false;
        if (showSteppers && inRect(stepperUpRect(), p)) { bump(step); pressed = true; mode_ = 1; return true; }
        if (showSteppers && inRect(stepperDnRect(), p)) { bump(-step); pressed = true; mode_ = 2; return true; }
        beginEdit();
        mode_ = 0;
        pressed = true;
        return true;
    }
    void onUp(PointF p) override {
        (void)p;
        pressed = false;
        mode_ = 0;
    }
    void onWheel(int delta) override {
        if (editing_) return;
        bump(delta > 0 ? step : -step);
    }
    void beginEdit() {
        focused = true;
        editing_ = true;
        buf_ = std::to_wstring(value);
        caret_ = (int)buf_.size();
        selectAll_ = true;
    }
    void endEdit(bool commit) {
        if (!editing_) { focused = false; return; }
        if (commit && !buf_.empty()) {
            try {
                long long v = std::stoll(buf_);
                int nv = (int)std::clamp<long long>(v, minV, maxV);
                if (nv != value) { value = nv; if (onChange) onChange(value); }
            } catch (...) {}
        }
        editing_ = false;
        focused = false;
        buf_.clear();
        selectAll_ = false;
    }
    bool takesKeyboard() const override { return editing_; }
    void onBlur() override { endEdit(true); }
    void onChar(wchar_t c) override {
        if (!editing_) return;
        if (c >= L'0' && c <= L'9') {
            if (selectAll_) { buf_.clear(); caret_ = 0; selectAll_ = false; }
            if (buf_.size() < 12) { buf_.insert(buf_.begin() + caret_, c); ++caret_; }
        }
    }
    void onKeyDown(UINT vk, bool ctrl) override {
        if (!editing_) return;
        if (vk == VK_BACK) {
            if (selectAll_) { buf_.clear(); caret_ = 0; selectAll_ = false; }
            else if (caret_ > 0) { buf_.erase(buf_.begin() + caret_ - 1); --caret_; }
        } else if (vk == VK_DELETE) {
            if (selectAll_) { buf_.clear(); caret_ = 0; selectAll_ = false; }
            else if (caret_ < (int)buf_.size()) buf_.erase(buf_.begin() + caret_);
        } else if (vk == VK_LEFT)  { caret_ = std::max(0, caret_ - 1); selectAll_ = false; }
        else if (vk == VK_RIGHT)   { caret_ = std::min((int)buf_.size(), caret_ + 1); selectAll_ = false; }
        else if (vk == VK_UP)      { bump(step); }
        else if (vk == VK_DOWN)    { bump(-step); }
        else if (vk == VK_RETURN)  { endEdit(true); }
        else if (vk == VK_ESCAPE)  { endEdit(false); }
        else if (vk == 'A' && ctrl) { selectAll_ = true; caret_ = (int)buf_.size(); }
    }
    void onDrag(PointF) override {}

    bool editing() const { return editing_; }
    const std::wstring& editBuf() const { return buf_; }

private:
    bool editing_ = false, selectAll_ = false, hoverUp_ = false, hoverDn_ = false;
    int caret_ = 0, mode_ = 0;
    std::wstring buf_;
};

// ---------------------------------------------------------------- 滑块
class Slider : public Widget {
public:
    float t = 0.0f;          // 0..1
    std::function<void(float)> onChange;
    bool dragging = false;
    std::wstring tooltip;

    void paint(Graphics& g) override {
        float cy = r.Y + r.Height / 2;
        float th = S(6.0f);
        RectF track(r.X, cy - th / 2, r.Width, th);
        fillRound(g, track, th / 2, Color(col::Track));

        float kx = r.X + S(9.0f) + t * (r.Width - S(18.0f));
        RectF fillR(track.X, track.Y, std::max(th, kx - track.X), th);
        fillRound(g, fillR, th / 2, Color(hover || dragging ? col::AccentEdge : col::Accent));

        float kr = S(hover || dragging ? 8.5f : 7.0f);
        RectF knob(kx - kr, cy - kr, kr * 2, kr * 2);
        fillRound(g, RectF(knob.X - S(1.5f), knob.Y - S(1.5f), knob.Width + S(3), knob.Height + S(3)), kr + S(1.5f), Color(0x331E2530));
        SolidBrush kb(Color(col::White));
        g.FillEllipse(&kb, knob);
    }
    void setFromPoint(PointF p, bool notify) {
        float x0 = r.X + S(9.0f), x1 = r.X + r.Width - S(9.0f);
        float nt = (p.X - x0) / std::max(1.0f, (x1 - x0));
        nt = std::clamp(nt, 0.0f, 1.0f);
        if (std::fabs(nt - t) > 0.0005f || !notify) {
            t = nt;
            if (notify && onChange) onChange(t);
        }
    }
    bool onDown(PointF p) override {
        if (!enabled || !visible || !inRect(r, p)) return false;
        dragging = true;
        setFromPoint(p, true);
        return true;
    }
    void onDrag(PointF p) override { if (dragging) setFromPoint(p, true); }
    void onUp(PointF p) override { (void)p; dragging = false; pressed = false; }
    void onWheel(int delta) override {
        float nt = std::clamp(t + (delta > 0 ? 0.02f : -0.02f), 0.0f, 1.0f);
        t = nt;
        if (onChange) onChange(t);
    }
    HCURSOR cursor() const override { return LoadCursorW(nullptr, (LPCWSTR)(ULONG_PTR)IDC_HAND); }
};

// ---------------------------------------------------------------- 开关
class Toggle : public Widget {
public:
    bool on = false;
    std::function<void(bool)> onChange;
    void paint(Graphics& g) override {
        RectF box(r.X, r.Y + (r.Height - S(22)) / 2, S(40), S(22));
        float rad = box.Height / 2;
        fillRound(g, box, rad, Color(on ? (hover ? 0xFF5E9BFF : col::Accent) : (hover ? 0xFF2E3543 : col::Track)));
        float kr = S(8.0f);
        float kx = on ? box.X + box.Width - rad : box.X + rad;
        RectF knob(kx - kr, box.Y + rad - kr, kr * 2, kr * 2);
        SolidBrush kb(Color(on ? col::White : 0xFF8B94A6));
        g.FillEllipse(&kb, knob);
    }
    void onClick() override { on = !on; if (onChange) onChange(on); }
};

// ---------------------------------------------------------------- 数值 <-> 滑块位置（对数映射 1ms..10s）
inline float msToT(int ms) {
    double lo = std::log(1.0), hi = std::log(10000.0);
    double v = std::log((double)std::clamp(ms, 1, 10000));
    return (float)std::clamp((v - lo) / (hi - lo), 0.0, 1.0);
}
inline int tToMs(float t) {
    double lo = std::log(1.0), hi = std::log(10000.0);
    double v = std::exp(lo + (hi - lo) * (double)std::clamp(t, 0.0f, 1.0f));
    return (int)std::lround(v);
}

} // namespace mc
