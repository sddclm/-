package com.dsh.themeguard;

import android.app.job.JobParameters;
import android.app.job.JobService;

/** Periodic watchdog: the system re-schedules this job even after the app is killed. */
public class GuardJobService extends JobService {

    @Override
    public boolean onStartJob(JobParameters params) {
        Guard.log(this, "保活", "系统定时任务触发（服务运行中=" + (Guard.serviceRunning ? "是" : "否")
                + "，用户期望运行=" + (Guard.isWanted(this) ? "是" : "否") + "）");
        KeepAlive.scheduleAlarm(this, KeepAlive.WATCHDOG_MS);
        KeepAlive.ensureService(this);
        return false;
    }

    @Override
    public boolean onStopJob(JobParameters params) {
        return true;
    }
}
