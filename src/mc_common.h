// mc_common.h —— 基础层：主题、DPI、GDI+ 绘图辅助、字体、计时工具
#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <windowsx.h>
#include <objidl.h>
#include <gdiplus.h>
#include <string>
#include <vector>
#include <functional>
#include <cmath>
#include <cstdio>
#include <cstdarg>
#include <cstdint>
#include <algorithm>

namespace mc {

using Gdiplus::ARGB;
using Gdiplus::Color;
using Gdiplus::RectF;
using Gdiplus::PointF;
using Gdiplus::Graphics;
using Gdiplus::GraphicsPath;
using Gdiplus::Font;
using Gdiplus::FontFamily;
using Gdiplus::StringFormat;
using Gdiplus::StringAlignment;
using Gdiplus::StringAlignmentNear;
using Gdiplus::StringAlignmentCenter;
using Gdiplus::StringAlignmentFar;
using Gdiplus::StringFormatFlags;
using Gdiplus::StringFormatFlagsNoWrap;
using Gdiplus::StringFormatFlagsMeasureTrailingSpaces;
using Gdiplus::StringTrimming;
using Gdiplus::StringTrimmingEllipsisCharacter;
using Gdiplus::SolidBrush;
using Gdiplus::Pen;
using Gdiplus::Bitmap;
using Gdiplus::LinearGradientBrush;
using Gdiplus::LinearGradientMode;
using Gdiplus::LinearGradientModeVertical;
using Gdiplus::GdiplusStartupInput;
using Gdiplus::GdiplusStartup;
using Gdiplus::GdiplusShutdown;
using Gdiplus::UnitPixel;
using Gdiplus::FontStyleRegular;
using Gdiplus::FontStyleBold;
using Gdiplus::SmoothingModeAntiAlias;
using Gdiplus::TextRenderingHintAntiAliasGridFit;

// ---------------------------------------------------------------- 配色
namespace col {
constexpr ARGB Win        = 0xFF0F1116; // 窗口底色
constexpr ARGB WinEdge    = 0xFF272C37; // 窗口描边
constexpr ARGB Card       = 0xFF171A21; // 卡片底
constexpr ARGB CardEdge   = 0xFF232833;
constexpr ARGB Field      = 0xFF1E222B; // 输入框/次级块
constexpr ARGB FieldEdge  = 0xFF2A303C;
constexpr ARGB FieldHover = 0xFF242935;
constexpr ARGB Text       = 0xFFE9EDF5;
constexpr ARGB TextDim    = 0xFF98A1B2;
constexpr ARGB TextFaint  = 0xFF646D7E;
constexpr ARGB Accent     = 0xFF4C8DFF;
constexpr ARGB AccentSoft = 0xFF23375E;
constexpr ARGB AccentEdge = 0xFF5C99FF;
constexpr ARGB Green      = 0xFF35D6A0;
constexpr ARGB GreenDeep  = 0xFF13382C;
constexpr ARGB Red        = 0xFFF2556B;
constexpr ARGB RedDeep    = 0xFF3A1720;
constexpr ARGB Amber      = 0xFFF5A524;
constexpr ARGB Track      = 0xFF272D39;
constexpr ARGB White      = 0xFFFFFFFF;
} // namespace col

// ---------------------------------------------------------------- DPI
inline float g_dpi  = 96.0f;
inline float g_scale = 1.0f;
inline void  setDpi(float dpi) { g_dpi = dpi; g_scale = dpi / 96.0f; }
inline float S(float v) { return v * g_scale; }         // 逻辑像素 -> 物理像素
inline float US(float v) { return v / g_scale; }        // 反向
inline int   SI(float v) { return (int)std::lround(v * g_scale); }

// ---------------------------------------------------------------- 绘图辅助
// 返回堆分配的 GraphicsPath（其拷贝构造为私有，不可按值返回）
inline GraphicsPath* makeRoundRect(const RectF& r, float radius) {
    float d = radius * 2.0f;
    d = std::min(d, std::min(r.Width, r.Height));
    GraphicsPath* p = new GraphicsPath();
    if (d <= 0.5f) {
        p->AddRectangle(r);
        return p;
    }
    const float x = r.X, y = r.Y, w = r.Width, h = r.Height;
    p->AddArc(x,           y,           d, d, 180.0f, 90.0f);
    p->AddArc(x + w - d,   y,           d, d, 270.0f, 90.0f);
    p->AddArc(x + w - d,   y + h - d,   d, d,   0.0f, 90.0f);
    p->AddArc(x,           y + h - d,   d, d,  90.0f, 90.0f);
    p->CloseFigure();
    return p;
}

inline void fillRound(Graphics& g, const RectF& r, float radius, const Color& c) {
    GraphicsPath* p = makeRoundRect(r, radius);
    SolidBrush b(c);
    g.FillPath(&b, p);
    delete p;
}

inline void strokeRound(Graphics& g, const RectF& r, float radius, const Color& c, float w) {
    GraphicsPath* p = makeRoundRect(r, radius);
    Pen pen(c, w);
    g.DrawPath(&pen, p);
    delete p;
}

inline void fillRoundBorder(Graphics& g, const RectF& r, float radius, const Color& fill, const Color& edge, float w) {
    fillRound(g, r, radius, fill);
    strokeRound(g, r, radius, edge, w);
}

enum class Align { Left, Center, Right };
enum class VAlign { Top, Middle, Bottom };

inline void drawText(Graphics& g, const std::wstring& s, const RectF& r, Font& f, const Color& c,
                     Align ha = Align::Left, VAlign va = VAlign::Middle) {
    if (s.empty()) return;
    StringFormat fmt;
    fmt.SetAlignment(ha == Align::Left ? StringAlignmentNear : (ha == Align::Center ? StringAlignmentCenter : StringAlignmentFar));
    fmt.SetLineAlignment(va == VAlign::Top ? StringAlignmentNear : (va == VAlign::Middle ? StringAlignmentCenter : StringAlignmentFar));
    fmt.SetFormatFlags(StringFormatFlagsNoWrap);
    fmt.SetTrimming(StringTrimmingEllipsisCharacter);
    SolidBrush b(c);
    g.DrawString(s.c_str(), (INT)s.size(), &f, r, &fmt, &b);
}

inline float textWidth(Graphics& g, const std::wstring& s, Font& f) {
    RectF out;
    StringFormat fmt;
    fmt.SetFormatFlags(StringFormatFlagsNoWrap | StringFormatFlagsMeasureTrailingSpaces);
    g.MeasureString(s.c_str(), (INT)s.size(), &f, RectF(0, 0, 4000.0f, 4000.0f), &fmt, &out);
    return out.Width;
}

// ---------------------------------------------------------------- 字体
struct FontSet {
    FontFamily* family = nullptr;
    Font* h1      = nullptr; // 标题
    Font* body    = nullptr; // 正文
    Font* bodyB   = nullptr; // 正文加粗
    Font* small   = nullptr; // 小字说明
    Font* tiny    = nullptr; // 极小字
    Font* num     = nullptr; // 数字（大）
    Font* numBig  = nullptr; // 特大数字
    Font* btn     = nullptr; // 按钮文字
    Font* btnBig  = nullptr; // 主按钮
    std::wstring familyName;

    void init() {
        const wchar_t* candidates[] = { L"Microsoft YaHei UI", L"Microsoft YaHei", L"Segoe UI", L"SimSun" };
        for (const wchar_t* name : candidates) {
            FontFamily* f = new FontFamily(name);
            if (f->IsAvailable()) { family = f; familyName = name; break; }
            delete f;
        }
        if (!family) { family = FontFamily::GenericSansSerif()->Clone(); familyName = L"sans"; }
        rebuild();
    }
    void rebuild() {
        release(false);
        h1     = new Font(family, S(15.0f), FontStyleBold,    UnitPixel);
        body   = new Font(family, S(12.5f), FontStyleRegular, UnitPixel);
        bodyB  = new Font(family, S(12.5f), FontStyleBold,    UnitPixel);
        small  = new Font(family, S(11.0f), FontStyleRegular, UnitPixel);
        tiny   = new Font(family, S(10.0f), FontStyleRegular, UnitPixel);
        num    = new Font(family, S(15.0f), FontStyleBold,    UnitPixel);
        numBig = new Font(family, S(26.0f), FontStyleBold,    UnitPixel);
        btn    = new Font(family, S(12.5f), FontStyleRegular, UnitPixel);
        btnBig = new Font(family, S(16.0f), FontStyleBold,    UnitPixel);
    }
    void release(bool full = true) {
        auto del = [](Font*& f) { if (f) { delete f; f = nullptr; } };
        del(h1); del(body); del(bodyB); del(small); del(tiny); del(num); del(numBig); del(btn); del(btnBig);
        if (full && family) { delete family; family = nullptr; }
    }
};

inline FontSet g_font;

// ---------------------------------------------------------------- 计时
inline double nowSec() {
    static LARGE_INTEGER freq = [] { LARGE_INTEGER f{}; QueryPerformanceFrequency(&f); return f; }();
    LARGE_INTEGER c{};
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart / (double)freq.QuadPart;
}

// ---------------------------------------------------------------- 字符串
inline std::wstring wfmt(const wchar_t* fmt, ...) {
    wchar_t buf[512];
    va_list ap;
    va_start(ap, fmt);
    int n = _vsnwprintf(buf, 511, fmt, ap);
    va_end(ap);
    buf[511] = L'\0';
    if (n < 0) return std::wstring(buf);
    return std::wstring(buf, (size_t)std::min(n, 511));
}

inline std::wstring toW(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring out(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], n);
    return out;
}
inline std::string toU8(const std::wstring& s) {
    if (s.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0, nullptr, nullptr);
    std::string out(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], n, nullptr, nullptr);
    return out;
}

inline bool inRect(const RectF& r, const PointF& p) {
    return p.X >= r.X && p.X < r.X + r.Width && p.Y >= r.Y && p.Y < r.Y + r.Height;
}

// 把整数格式化为带千分位的字符串
inline std::wstring groupNum(long long v) {
    std::wstring s = std::to_wstring(v);
    std::wstring out;
    int cnt = 0;
    for (int i = (int)s.size() - 1; i >= 0; --i) {
        out.push_back(s[i]);
        if (++cnt % 3 == 0 && i > 0) out.push_back(L',');
    }
    std::reverse(out.begin(), out.end());
    return out;
}

// 时长 -> mm:ss / h:mm:ss
inline std::wstring fmtDuration(double sec) {
    long long t = (long long)sec;
    long long h = t / 3600, m = (t % 3600) / 60, s = t % 60;
    if (h > 0) return wfmt(L"%lld:%02lld:%02lld", h, m, s);
    return wfmt(L"%02lld:%02lld", m, s);
}

} // namespace mc
