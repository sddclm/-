// mc_clicker.h —— 点击引擎：独立线程 + 高精度定时 + SendInput + 运行中实时改参
#pragma once
#include "mc_common.h"
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <mmsystem.h>

namespace mc {

class Clicker {
public:
    struct Params {
        int  intervalMs  = 100;   // 点击间隔（毫秒）
        int  button      = 0;     // 0 左键 1 右键 2 中键
        bool dblClick    = false; // 双击模式
        bool useFixedPos = false; // 固定坐标
        int  x = 0, y = 0;
        int  repeatMode  = 0;     // 0 一直点击 1 指定次数
        long long repeatCount = 100;
        int  dblGapMs    = 30;    // 双击模式下两次点击之间的间隔
    };

    Clicker() = default;
    ~Clicker() { stop(); }
    Clicker(const Clicker&) = delete;
    Clicker& operator=(const Clicker&) = delete;

    // ---------------- 状态查询 ----------------
    bool running() const { return running_.load(std::memory_order_relaxed); }
    long long clicks() const { return clicks_.load(std::memory_order_relaxed); }
    long long target() const { return target_.load(std::memory_order_relaxed); }
    double cps() const { return cps_.load(std::memory_order_relaxed); }
    double elapsed() const {
        double t0 = startTime_.load(std::memory_order_relaxed);
        if (t0 <= 0.0) return 0.0;
        double end = running() ? nowSec() : endTime_.load(std::memory_order_relaxed);
        return end > t0 ? end - t0 : 0.0;
    }
    static double nominalCps(const Params& p) {
        double per = p.dblClick ? (p.dblGapMs * 2.0 + p.intervalMs) : (double)p.intervalMs;
        return per > 0.0 ? 1000.0 / per : 0.0;
    }

    // ---------------- 控制 ----------------
    void start(const Params& p) {
        if (running()) { updateParams(p); return; }
        if (worker_.joinable()) worker_.join();
        {
            std::lock_guard<std::mutex> lk(m_);
            params_ = p;
        }
        stopFlag_.store(false);
        finished_.store(false);
        clicks_.store(0);
        cps_.store(0.0);
        target_.store(p.repeatMode == 1 ? p.repeatCount : 0);
        startTime_.store(nowSec());
        endTime_.store(0.0);
        running_.store(true);
        worker_ = std::thread(&Clicker::worker, this);
    }

    void updateParams(const Params& p) {
        std::lock_guard<std::mutex> lk(m_);
        params_ = p;
    }
    Params params() {
        std::lock_guard<std::mutex> lk(m_);
        return params_;
    }

    void requestStop() {
        stopFlag_.store(true);
        cv_.notify_all();
    }
    void stop() {
        if (!worker_.joinable()) { running_.store(false); return; }
        requestStop();
        worker_.join();
        running_.store(false);
    }
    // 线程自然结束（点够次数）后回收资源
    bool reapIfFinished() {
        if (worker_.joinable() && !running_.load(std::memory_order_relaxed)) {
            worker_.join();
            return true;
        }
        return false;
    }

private:
    static void sendMouse(DWORD flags) {
        INPUT in{};
        in.type = INPUT_MOUSE;
        in.mi.dwFlags = flags;
        SendInput(1, &in, sizeof(INPUT));
    }
    static void moveTo(int x, int y) {
        const int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
        const int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
        const int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        const int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
        if (vw <= 1 || vh <= 1) return;
        INPUT in{};
        in.type = INPUT_MOUSE;
        in.mi.dx = (LONG)std::llround((double)(x - vx) * 65535.0 / (double)(vw - 1));
        in.mi.dy = (LONG)std::llround((double)(y - vy) * 65535.0 / (double)(vh - 1));
        in.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
        SendInput(1, &in, sizeof(INPUT));
    }
    static void buttonFlags(int button, DWORD& down, DWORD& up) {
        switch (button) {
            case 1:  down = MOUSEEVENTF_RIGHTDOWN;  up = MOUSEEVENTF_RIGHTUP;  break;
            case 2:  down = MOUSEEVENTF_MIDDLEDOWN; up = MOUSEEVENTF_MIDDLEUP; break;
            default: down = MOUSEEVENTF_LEFTDOWN;   up = MOUSEEVENTF_LEFTUP;   break;
        }
    }

    void doClick(const Params& p) {
        if (p.useFixedPos) moveTo(p.x, p.y);
        DWORD down = 0, up = 0;
        buttonFlags(p.button, down, up);
        sendMouse(down);
        sendMouse(up);
        clicks_.fetch_add(1, std::memory_order_relaxed);
        if (p.dblClick) {
            if (!interruptibleSleep(p.dblGapMs / 1000.0)) return;
            sendMouse(down);
            sendMouse(up);
            clicks_.fetch_add(1, std::memory_order_relaxed);
        }
    }

    // 可中断睡眠：粗睡用条件变量等待（最长 20ms 一段），末段自旋保证精度
    bool interruptibleSleep(double seconds) {
        if (seconds <= 0.0) return !stopFlag_.load();
        double target = nowSec() + seconds;
        for (;;) {
            if (stopFlag_.load(std::memory_order_relaxed)) return false;
            double remain = target - nowSec();
            if (remain <= 0.0) return true;
            if (remain > 0.003) {
                std::unique_lock<std::mutex> lk(waitMx_);
                double chunk = std::min(remain - 0.001, 0.020);
                if (chunk > 0.0) {
                    cv_.wait_for(lk, std::chrono::microseconds((long long)(chunk * 1e6)),
                                 [this] { return stopFlag_.load(std::memory_order_relaxed); });
                }
            } else {
                Sleep(0);
            }
        }
    }

    void worker() {
        timeBeginPeriod(1);
        Params p = params();
        double perClick = perClickSec(p);
        double t0 = nowSec();
        long long n = 0;
        double statStart = nowSec();
        long long statClicks = 0;

        while (!stopFlag_.load(std::memory_order_relaxed)) {
            Params cur = params();
            double newPer = perClickSec(cur);
            if (std::fabs(newPer - perClick) > 1e-9) {   // 运行中改了速度：平滑重排节奏
                perClick = newPer;
                t0 = nowSec();
                n = 0;
                p = cur;
            } else {
                p = cur;
            }

            double due = t0 + (double)n * perClick;
            double wait = due - nowSec();
            if (wait > 0.0 && !interruptibleSleep(wait)) break;
            if (stopFlag_.load()) break;

            doClick(p);
            ++n;
            statClicks += p.dblClick ? 2 : 1;

            double nowt = nowSec();
            if (nowt - statStart >= 0.5) {
                cps_.store((double)statClicks / (nowt - statStart));
                statStart = nowt;
                statClicks = 0;
            }

            if (p.repeatMode == 1 && clicks_.load() >= p.repeatCount) break;

            // 单次点击耗时已超过间隔：重置节奏，避免追赶式连发
            if (nowSec() - due > perClick * 2.0) { t0 = nowSec(); n = 0; }
        }

        cps_.store(0.0);
        endTime_.store(nowSec());
        running_.store(false);
        finished_.store(true);
        timeEndPeriod(1);
        cv_.notify_all();
    }

    static double perClickSec(const Params& p) {
        double v = p.dblClick ? (p.dblGapMs * 2.0 + p.intervalMs) / 1000.0 : p.intervalMs / 1000.0;
        return v > 0.0 ? v : 0.001;
    }

    Params params_;
    std::thread worker_;
    std::mutex m_;
    std::mutex waitMx_;
    std::condition_variable cv_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopFlag_{false};
    std::atomic<bool> finished_{false};
    std::atomic<long long> clicks_{0};
    std::atomic<long long> target_{0};
    std::atomic<double> cps_{0.0};
    std::atomic<double> startTime_{0.0};
    std::atomic<double> endTime_{0.0};
};

} // namespace mc
