package com.dsh.themeguard;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.Service;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Build;
import android.os.IBinder;

/** Foreground service that hooks MIUI theme-trial broadcasts at max priority and aborts them. */
public class GuardService extends Service {

    public static final String CH_ID = "guard";
    public static final String CH_WARN = "guard_warn";
    private BroadcastReceiver recv;
    private int tick;

    @Override
    public void onCreate() {
        super.onCreate();
        Guard.serviceRunning = true;
        Guard.log(this, "服务", "守护服务已创建，准备挂载监听");
        createChannel();

        recv = new BroadcastReceiver() {
            @Override
            public void onReceive(Context ctx, Intent intent) {
                handle(this, intent);
            }
        };
        int ok = 0;
        for (String a : Guard.WATCH) {
            try {
                IntentFilter f = new IntentFilter(a);
                f.setPriority(999);
                if (Build.VERSION.SDK_INT >= 33) {
                    registerReceiver(recv, f, Context.RECEIVER_EXPORTED);
                } else {
                    registerReceiver(recv, f);
                }
                ok++;
            } catch (Throwable t) {
                Guard.log(this, "错误", "监听 " + Guard.friendly(a) + " 失败：" + t);
            }
        }
        Guard.log(this, "服务", "已挂载 " + ok + "/" + Guard.WATCH.length + " 条规则（优先级 999，可抢在系统之前）");
    }

    private void createChannel() {
        try {
            NotificationManager nm = getSystemService(NotificationManager.class);
            if (nm != null && nm.getNotificationChannel(CH_ID) == null) {
                NotificationChannel ch = new NotificationChannel(CH_ID, "主题守护",
                        NotificationManager.IMPORTANCE_LOW);
                ch.setShowBadge(false);
                nm.createNotificationChannel(ch);
            }
        } catch (Throwable ignored) {
        }
    }

    private void handle(BroadcastReceiver receiver, Intent intent) {
        final String action = intent == null ? "null" : String.valueOf(intent.getAction());
        boolean ordered = false;
        try {
            ordered = receiver.isOrderedBroadcast();
        } catch (Throwable ignored) {
        }
        Guard.total++;
        boolean quiet = false;
        if ("android.intent.action.TIME_TICK".equals(action)) {
            tick++;
            quiet = (tick % 10) != 0;   // heartbeat: log every 10th tick only
        }
        boolean aborted;
        try {
            receiver.abortBroadcast();
            aborted = true;
        } catch (Throwable t) {
            aborted = false;
        }

        String name = Guard.friendly(action);
        if (!ordered) {
            if (!quiet) {
                Guard.log(this, "接收", name + "（普通广播，系统本来就不允许中断）");
            }
        } else if (aborted) {
            Guard.blocked++;
            Guard.log(this, "拦截", name + " —— 已中断，系统应用收不到这条广播了");
        } else {
            Guard.log(this, "警告", name + " —— 中断失败");
        }

        boolean dangerous = Guard.isDangerous(action);
        if (dangerous && !(ordered && aborted)) {
            warnCouldNotBlock(action);
        }
        if (!dangerous) {
            return;   // heartbeat / screen events never run the fallback
        }
        final Context app = getApplicationContext();
        final String cmd = getSharedPreferences("cfg", MODE_PRIVATE).getString("cmd", "");
        if (cmd != null && cmd.trim().length() > 0) {
            final String shown = name;
            new Thread(new Runnable() {
                @Override
                public void run() {
                    String r = Guard.rish(app, cmd);
                    Guard.lastResult = r;
                    Guard.log(app, "命令", "因「" + shown + "」执行：" + cmd
                            + "\n        结果：" + r.replace("\n", " | ").trim());
                }
            }).start();
        }
    }

    /** Loud notification so the user can react the moment a restore slips through. */
    private void warnCouldNotBlock(String action) {
        try {
            NotificationManager nm = getSystemService(NotificationManager.class);
            if (nm != null && nm.getNotificationChannel(CH_WARN) == null) {
                NotificationChannel ch = new NotificationChannel(CH_WARN, "主题恢复告警",
                        NotificationManager.IMPORTANCE_HIGH);
                nm.createNotificationChannel(ch);
            }
            if (nm != null) {
                Notification.Builder b = new Notification.Builder(this, CH_WARN)
                        .setContentTitle("MIUI 正在恢复主题")
                        .setContentText(Guard.friendly(action) + " —— 未能拦截，已执行兜底命令")
                        .setSmallIcon(android.R.drawable.ic_dialog_alert)
                        .setAutoCancel(true);
                nm.notify(2, b.build());
            }
        } catch (Throwable t) {
            Guard.log(this, "错误", "告警通知失败：" + t);
        }
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        try {
            Notification.Builder b = new Notification.Builder(this, CH_ID)
                    .setContentTitle("主题守护正在运行")
                    .setContentText("正在监听 MIUI 主题恢复广播")
                    .setSmallIcon(android.R.drawable.ic_lock_lock)
                    .setOngoing(true);
            startForeground(1, b.build());
        } catch (Throwable t) {
            Guard.log(this, "错误", "前台服务启动失败：" + t);
        }
        KeepAlive.scheduleJob(this);
        KeepAlive.scheduleAlarm(this, KeepAlive.WATCHDOG_MS);
        Guard.log(this, "保活", "看门狗已布防：系统定时任务 + " + (KeepAlive.WATCHDOG_MS / 60000) + " 分钟闹钟");
        return START_STICKY;
    }

    @Override
    public void onTaskRemoved(Intent rootIntent) {
        Guard.log(this, "保活", "应用被从后台划掉，服务继续运行");
        super.onTaskRemoved(rootIntent);
    }

    @Override
    public void onDestroy() {
        Guard.serviceRunning = false;
        if (Guard.isWanted(this)) {
            KeepAlive.scheduleAlarm(this, 10 * 1000L);
            Guard.log(this, "保活", "服务被系统销毁，已预约 10 秒后自动重启");
        } else {
            Guard.log(this, "服务", "守护服务已停止（用户手动关闭，不再自动重启）");
        }
        try {
            if (recv != null) {
                unregisterReceiver(recv);
            }
        } catch (Throwable ignored) {
        }
        super.onDestroy();
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }
}
