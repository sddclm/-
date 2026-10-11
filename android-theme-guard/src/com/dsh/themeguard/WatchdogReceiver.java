package com.dsh.themeguard;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

/** Alarm fallback: fires when the system wakes up for our pending alarm. */
public class WatchdogReceiver extends BroadcastReceiver {

    @Override
    public void onReceive(Context context, Intent intent) {
        Guard.log(context, "保活", "看门狗闹钟触发（服务运行中=" + (Guard.serviceRunning ? "是" : "否") + "）");
        KeepAlive.ensureService(context);
        KeepAlive.scheduleJob(context);
        KeepAlive.scheduleAlarm(context, KeepAlive.WATCHDOG_MS);
    }
}
