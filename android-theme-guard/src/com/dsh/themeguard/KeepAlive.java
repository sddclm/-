package com.dsh.themeguard;

import android.app.AlarmManager;
import android.app.PendingIntent;
import android.app.job.JobInfo;
import android.app.job.JobScheduler;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.os.Build;

/** Keep-alive plumbing: JobScheduler watchdog + AlarmManager fallback + service revival. */
public final class KeepAlive {

    public static final int JOB_ID = 4711;
    public static final String ACTION_ALIVE = "com.dsh.themeguard.ALIVE";
    public static final long WATCHDOG_MS = 15 * 60 * 1000L;
    private static final int REQ_ALARM = 4712;

    private KeepAlive() {
    }

    public static void scheduleJob(Context c) {
        try {
            JobScheduler js = (JobScheduler) c.getSystemService(Context.JOB_SCHEDULER_SERVICE);
            if (js == null) {
                return;
            }
            if (js.getPendingJob(JOB_ID) != null) {
                return;   // already armed - re-scheduling would restart its period and fire it again
            }
            JobInfo.Builder b = new JobInfo.Builder(JOB_ID, new ComponentName(c, GuardJobService.class));
            b.setPersisted(true);
            b.setPeriodic(WATCHDOG_MS);
            b.setRequiredNetworkType(JobInfo.NETWORK_TYPE_NONE);
            js.schedule(b.build());
        } catch (Throwable t) {
            Guard.log(c, "保活", "排程系统定时任务失败：" + t);
        }
    }

    public static void scheduleAlarm(Context c, long delayMs) {
        try {
            AlarmManager am = (AlarmManager) c.getSystemService(Context.ALARM_SERVICE);
            if (am == null) {
                return;
            }
            PendingIntent pi = alarmIntent(c);
            long at = System.currentTimeMillis() + delayMs;
            if (Build.VERSION.SDK_INT >= 31) {
                if (am.canScheduleExactAlarms()) {
                    am.setExactAndAllowWhileIdle(AlarmManager.RTC_WAKEUP, at, pi);
                } else {
                    am.setAndAllowWhileIdle(AlarmManager.RTC_WAKEUP, at, pi);
                }
            } else if (Build.VERSION.SDK_INT >= 23) {
                am.setExactAndAllowWhileIdle(AlarmManager.RTC_WAKEUP, at, pi);
            } else {
                am.set(AlarmManager.RTC_WAKEUP, at, pi);
            }
        } catch (Throwable t) {
            Guard.log(c, "保活", "设置看门狗闹钟失败：" + t);
        }
    }

    public static void cancelAlarm(Context c) {
        try {
            AlarmManager am = (AlarmManager) c.getSystemService(Context.ALARM_SERVICE);
            if (am != null) {
                am.cancel(alarmIntent(c));
            }
        } catch (Throwable ignored) {
        }
    }

    private static PendingIntent alarmIntent(Context c) {
        Intent i = new Intent(c, WatchdogReceiver.class);
        i.setAction(ACTION_ALIVE);
        int flags = PendingIntent.FLAG_UPDATE_CURRENT;
        if (Build.VERSION.SDK_INT >= 23) {
            flags |= PendingIntent.FLAG_IMMUTABLE;
        }
        return PendingIntent.getBroadcast(c, REQ_ALARM, i, flags);
    }

    /** Bring the guard service back if the user still wants it running. */
    public static void ensureService(Context c) {
        if (!Guard.isWanted(c) || Guard.serviceRunning) {
            return;
        }
        try {
            Intent s = new Intent(c, GuardService.class);
            if (Build.VERSION.SDK_INT >= 26) {
                c.startForegroundService(s);
            } else {
                c.startService(s);
            }
        } catch (Throwable t) {
            Guard.log(c, "保活", "拉起守护服务失败：" + t);
        }
    }
}
