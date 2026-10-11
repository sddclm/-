package com.dsh.themeguard;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.os.Build;

/** Restarts the guard after reboot. */
public class BootReceiver extends BroadcastReceiver {

    @Override
    public void onReceive(Context context, Intent intent) {
        Guard.log(context, "启动", "收到开机广播，准备自启动");
        if (!Guard.isWanted(context)) {
            Guard.log(context, "启动", "用户已手动关闭守护，本次开机不启动");
            return;
        }
        KeepAlive.scheduleJob(context);
        KeepAlive.scheduleAlarm(context, KeepAlive.WATCHDOG_MS);
        KeepAlive.ensureService(context);
    }
}
