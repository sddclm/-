// mc_config.h —— 配置项定义与持久化（INI，写入 %APPDATA%\\MouseClicker）
#pragma once
#include "mc_common.h"
#include <shlobj.h>

namespace mc {

struct Settings {
    int  intervalMs  = 100;   // 点击间隔（毫秒）
    int  button      = 0;     // 0 左键 1 右键 2 中键
    int  clickKind   = 0;     // 0 单击 1 双击
    int  posMode     = 0;     // 0 跟随光标 1 固定坐标
    int  fixedX      = 0;
    int  fixedY      = 0;
    int  repeatMode  = 0;     // 0 一直点击 1 指定次数
    int  repeatCount = 100;

    UINT toggleMods  = 0;
    UINT toggleVk    = VK_F6; // 默认 F6 启停
    bool stopEnabled = true;
    UINT stopMods    = 0;
    UINT stopVk      = VK_F7; // 默认 F7 紧急停止

    int  winX = -1;
    int  winY = -1;

    // ---------------- 热键文本 ----------------
    static std::wstring modsText(UINT mods) {
        std::wstring s;
        if (mods & MOD_CONTROL) s += L"Ctrl+";
        if (mods & MOD_ALT)     s += L"Alt+";
        if (mods & MOD_SHIFT)   s += L"Shift+";
        if (mods & MOD_WIN)     s += L"Win+";
        return s;
    }
    static std::wstring vkText(UINT vk) {
        if (vk == 0) return L"未设置";
        if (vk >= VK_F1 && vk <= VK_F24) return wfmt(L"F%u", vk - VK_F1 + 1);
        if (vk >= '0' && vk <= '9') return std::wstring(1, (wchar_t)vk);
        if (vk >= 'A' && vk <= 'Z') return std::wstring(1, (wchar_t)vk);
        switch (vk) {
            case VK_SPACE:   return L"空格";
            case VK_TAB:     return L"Tab";
            case VK_RETURN:  return L"回车";
            case VK_ESCAPE:  return L"Esc";
            case VK_BACK:    return L"退格";
            case VK_INSERT:  return L"Insert";
            case VK_DELETE:  return L"Delete";
            case VK_HOME:    return L"Home";
            case VK_END:     return L"End";
            case VK_PRIOR:   return L"PageUp";
            case VK_NEXT:    return L"PageDown";
            case VK_UP:      return L"↑";
            case VK_DOWN:    return L"↓";
            case VK_LEFT:    return L"←";
            case VK_RIGHT:   return L"→";
            case VK_OEM_3:   return L"`";
            case VK_OEM_MINUS: return L"-";
            case VK_OEM_PLUS:  return L"=";
            case VK_OEM_4:   return L"[";
            case VK_OEM_5:   return L"\\";
            case VK_OEM_6:   return L"]";
            case VK_OEM_1:   return L";";
            case VK_OEM_7:   return L"'";
            case VK_OEM_COMMA: return L",";
            case VK_OEM_PERIOD:return L".";
            case VK_OEM_2:   return L"/";
            case VK_NUMPAD0: case VK_NUMPAD1: case VK_NUMPAD2: case VK_NUMPAD3: case VK_NUMPAD4:
            case VK_NUMPAD5: case VK_NUMPAD6: case VK_NUMPAD7: case VK_NUMPAD8: case VK_NUMPAD9:
                return wfmt(L"小键盘%u", vk - VK_NUMPAD0);
            case VK_MULTIPLY: return L"小键盘*";
            case VK_ADD:      return L"小键盘+";
            case VK_SUBTRACT: return L"小键盘-";
            case VK_DECIMAL:  return L"小键盘.";
            case VK_DIVIDE:   return L"小键盘/";
            case VK_CAPITAL:  return L"CapsLock";
            case VK_SCROLL:   return L"ScrollLock";
            case VK_PAUSE:    return L"Pause";
            case VK_SNAPSHOT: return L"PrintScreen";
            default: break;
        }
        UINT sc = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
        wchar_t name[64] = L"";
        if (GetKeyNameTextW((LONG)(sc << 16), name, 64) > 0) return name;
        return wfmt(L"0x%02X", vk);
    }
    static bool isModifierKey(UINT vk) {
        return vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT ||
               vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL ||
               vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU ||
               vk == VK_LWIN || vk == VK_RWIN;
    }

    std::wstring toggleText() const { return modsText(toggleMods) + vkText(toggleVk); }
    std::wstring stopText()   const { return modsText(stopMods) + vkText(stopVk); }

    // ---------------- 文件路径 ----------------
    // 优先与 exe 同目录（便携，配置随程序走）；目录不可写时回退到 %APPDATA%
    static std::wstring configPath() {
        std::wstring portable = exeDir() + L"\\MouseClicker.ini";
        HANDLE h = CreateFileW(portable.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                               OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h != INVALID_HANDLE_VALUE) {
            CloseHandle(h);
            return portable;
        }
        wchar_t dir[MAX_PATH] = L"";
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, dir))) {
            std::wstring folder = std::wstring(dir) + L"\\MouseClicker";
            CreateDirectoryW(folder.c_str(), nullptr);
            return folder + L"\\settings.ini";
        }
        return portable;
    }
    static std::wstring exeDir() {
        wchar_t buf[MAX_PATH] = L"";
        GetModuleFileNameW(nullptr, buf, MAX_PATH);
        std::wstring s = buf;
        size_t p = s.find_last_of(L'\\');
        return p == std::wstring::npos ? s : s.substr(0, p);
    }

    void save() const {
        std::wstring path = configPath();
        const wchar_t* sec = L"clicker";
        auto wi = [&](const wchar_t* k, int v) { WritePrivateProfileStringW(sec, k, std::to_wstring(v).c_str(), path.c_str()); };
        wi(L"intervalMs", intervalMs);
        wi(L"button", button);
        wi(L"clickKind", clickKind);
        wi(L"posMode", posMode);
        wi(L"fixedX", fixedX);
        wi(L"fixedY", fixedY);
        wi(L"repeatMode", repeatMode);
        wi(L"repeatCount", repeatCount);
        wi(L"toggleMods", (int)toggleMods);
        wi(L"toggleVk", (int)toggleVk);
        wi(L"stopEnabled", stopEnabled ? 1 : 0);
        wi(L"stopMods", (int)stopMods);
        wi(L"stopVk", (int)stopVk);
        wi(L"winX", winX);
        wi(L"winY", winY);
    }

    void load() {
        std::wstring path = configPath();
        const wchar_t* sec = L"clicker";
        auto ri = [&](const wchar_t* k, int def) { return (int)GetPrivateProfileIntW(sec, k, def, path.c_str()); };
        intervalMs  = std::clamp(ri(L"intervalMs", intervalMs), 1, 3600000);
        button      = std::clamp(ri(L"button", button), 0, 2);
        clickKind   = std::clamp(ri(L"clickKind", clickKind), 0, 1);
        posMode     = std::clamp(ri(L"posMode", posMode), 0, 1);
        fixedX      = ri(L"fixedX", fixedX);
        fixedY      = ri(L"fixedY", fixedY);
        repeatMode  = std::clamp(ri(L"repeatMode", repeatMode), 0, 1);
        repeatCount = std::clamp(ri(L"repeatCount", repeatCount), 1, 100000000);
        toggleMods  = (UINT)ri(L"toggleMods", (int)toggleMods) & (MOD_ALT | MOD_CONTROL | MOD_SHIFT | MOD_WIN);
        toggleVk    = (UINT)ri(L"toggleVk", (int)toggleVk);
        stopEnabled = ri(L"stopEnabled", stopEnabled ? 1 : 0) != 0;
        stopMods    = (UINT)ri(L"stopMods", (int)stopMods) & (MOD_ALT | MOD_CONTROL | MOD_SHIFT | MOD_WIN);
        stopVk      = (UINT)ri(L"stopVk", (int)stopVk);
        winX        = ri(L"winX", winX);
        winY        = ri(L"winY", winY);
        if (toggleVk == 0) toggleVk = VK_F6;
        if (stopVk == 0) stopVk = VK_F7;
    }
};

} // namespace mc
