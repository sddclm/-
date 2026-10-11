package com.dsh.themeguard;

import android.animation.ArgbEvaluator;
import android.animation.ObjectAnimator;
import android.animation.ValueAnimator;
import android.app.Activity;
import android.content.Intent;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.StateListDrawable;
import android.os.Bundle;
import android.os.Handler;
import android.text.InputType;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.text.style.ForegroundColorSpan;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.animation.DecelerateInterpolator;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;

/** Card-based dark UI. Everything is built in code (no XML resources). */
public class MainActivity extends Activity {

    private static final int BG_TOP = 0xFF090D12;
    private static final int BG_BOTTOM = 0xFF141C26;
    private static final int SURFACE = 0xFF161E29;
    private static final int SURFACE_2 = 0xFF1E2836;
    private static final int STROKE = 0xFF26313F;
    private static final int FG = 0xFFE9EFF6;
    private static final int MUTED = 0xFF8B99AA;
    private static final int GREEN = 0xFF34D399;
    private static final int BLUE = 0xFF5AA9FF;
    private static final int RED = 0xFFF87171;
    private static final int AMBER = 0xFFFBBF24;

    private TextView statusText;
    private TextView statBlocked;
    private TextView statSeen;
    private TextView statRules;
    private TextView rishText;
    private TextView lastCmd;
    private LinearLayout logContainer;
    private final java.util.List<String> shownLines = new java.util.ArrayList<String>();
    private static final int MAX_LOG_LINES = 120;
    private EditText cmdInput;
    private ScrollView logScroll;
    private View statusDot;
    private TextView kaLine;
    private TextView guardToggle;
    private GradientDrawable statusCardBg;
    private ObjectAnimator dotPulse;
    private final java.util.List<View> animTargets = new java.util.ArrayList<View>();
    private boolean lastRunning;
    private int lastBlocked = -1;


    private final Handler handler = new Handler();
    private final Runnable ticker = new Runnable() {
        @Override
        public void run() {
            refresh();
            handler.postDelayed(this, 1200);
        }
    };

    // ---------- drawing helpers ----------

    private int dp(float v) {
        return Math.round(v * getResources().getDisplayMetrics().density);
    }

    private GradientDrawable shape(int fill, float radius, int strokeColor, float strokeWidth) {
        GradientDrawable g = new GradientDrawable();
        g.setShape(GradientDrawable.RECTANGLE);
        g.setColor(fill);
        g.setCornerRadius(dp(radius));
        if (strokeWidth > 0) {
            g.setStroke(dp(strokeWidth), strokeColor);
        }
        return g;
    }

    private StateListDrawable pressable(int normal, int pressed, float radius) {
        StateListDrawable s = new StateListDrawable();
        s.addState(new int[]{android.R.attr.state_pressed}, shape(pressed, radius, 0, 0));
        s.addState(new int[]{}, shape(normal, radius, 0, 0));
        return s;
    }

    private StateListDrawable outlinePressable(int fill, float radius, int strokeColor) {
        StateListDrawable s = new StateListDrawable();
        s.addState(new int[]{android.R.attr.state_pressed}, shape(fill, radius, strokeColor, 1));
        s.addState(new int[]{}, shape(SURFACE, radius, strokeColor, 1));
        return s;
    }

    private TextView label(String text, float size, int color, boolean bold) {
        TextView t = new TextView(this);
        t.setText(text);
        t.setTextSize(size);
        t.setTextColor(color);
        if (bold) {
            t.setTypeface(Typeface.DEFAULT_BOLD);
        }
        return t;
    }

    private LinearLayout card() {
        LinearLayout c = new LinearLayout(this);
        c.setOrientation(LinearLayout.VERTICAL);
        GradientDrawable g = shape(SURFACE, 20, STROKE, 1);
        c.setBackground(g);
        c.setPadding(dp(16), dp(14), dp(16), dp(14));
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.bottomMargin = dp(12);
        c.setLayoutParams(lp);
        c.setElevation(dp(2));
        return c;
    }

    private TextView actionButton(String text, int fill, int pressedFill, View.OnClickListener l) {
        TextView b = label(text, 14f, 0xFF0B1116, true);
        b.setGravity(Gravity.CENTER);
        b.setPadding(dp(10), dp(14), dp(10), dp(14));
        b.setBackground(pressable(fill, pressedFill, 16));
        b.setOnClickListener(l);
        attachPressFeedback(b);
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(0,
                ViewGroup.LayoutParams.WRAP_CONTENT, 1f);
        lp.setMargins(dp(3), 0, dp(3), 0);
        b.setLayoutParams(lp);
        return b;
    }

    private TextView ghostButton(String text, View.OnClickListener l) {
        TextView b = label(text, 14f, FG, true);
        b.setGravity(Gravity.CENTER);
        b.setPadding(dp(10), dp(14), dp(10), dp(14));
        b.setBackground(outlinePressable(SURFACE_2, 16, STROKE));
        b.setOnClickListener(l);
        attachPressFeedback(b);
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(0,
                ViewGroup.LayoutParams.WRAP_CONTENT, 1f);
        lp.setMargins(dp(3), 0, dp(3), 0);
        b.setLayoutParams(lp);
        return b;
    }

    private LinearLayout row() {
        LinearLayout r = new LinearLayout(this);
        r.setOrientation(LinearLayout.HORIZONTAL);
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.topMargin = dp(8);
        r.setLayoutParams(lp);
        return r;
    }

    private LinearLayout statPill(String caption) {
        LinearLayout box = new LinearLayout(this);
        box.setOrientation(LinearLayout.VERTICAL);
        box.setGravity(Gravity.CENTER);
        box.setBackground(shape(SURFACE_2, 14, STROKE, 1));
        box.setPadding(dp(6), dp(10), dp(6), dp(10));
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(0,
                ViewGroup.LayoutParams.WRAP_CONTENT, 1f);
        lp.setMargins(dp(4), 0, dp(4), 0);
        box.setLayoutParams(lp);
        TextView value = label("0", 20f, FG, true);
        value.setGravity(Gravity.CENTER);
        TextView cap = label(caption, 11f, MUTED, false);
        cap.setGravity(Gravity.CENTER);
        box.addView(value);
        box.addView(cap);
        box.setTag(value);
        return box;
    }

    private View dot(int color, int sizeDp) {
        View v = new View(this);
        GradientDrawable g = new GradientDrawable();
        g.setShape(GradientDrawable.OVAL);
        g.setColor(color);
        v.setBackground(g);
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(dp(sizeDp), dp(sizeDp));
        lp.setMargins(0, dp(4), dp(10), 0);
        v.setLayoutParams(lp);
        return v;
    }

    // ---------- activity ----------

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Guard.loadRishState(this);
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        try {
            overridePendingTransition(android.R.anim.fade_in, android.R.anim.fade_out);
        } catch (Throwable ignored) {
        }
        try {
            getWindow().setStatusBarColor(BG_TOP);
            getWindow().setNavigationBarColor(BG_TOP);
        } catch (Throwable ignored) {
        }

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        GradientDrawable bgGrad = new GradientDrawable(GradientDrawable.Orientation.TOP_BOTTOM,
                new int[]{BG_TOP, BG_BOTTOM});
        root.setBackground(bgGrad);
        int pad = dp(16);
        root.setPadding(pad, dp(40), pad, dp(16));

        // header
        TextView title = label("ThemeGuard", 27f, FG, true);
        root.addView(title);
        TextView sub = label("MIUI 主题恢复广播拦截 · 优先级 999", 12f, MUTED, false);
        sub.setPadding(0, dp(2), 0, dp(14));
        root.addView(sub);

        // status card
        LinearLayout statusCard = card();
        statusCardBg = (GradientDrawable) statusCard.getBackground();
        LinearLayout head = new LinearLayout(this);
        head.setOrientation(LinearLayout.HORIZONTAL);
        head.setGravity(Gravity.CENTER_VERTICAL);
        statusDot = dot(MUTED, 10);
        head.addView(statusDot);
        statusText = label("服务已停止", 16f, FG, true);
        head.addView(statusText);
        statusCard.addView(head);

        LinearLayout stats = new LinearLayout(this);
        stats.setOrientation(LinearLayout.HORIZONTAL);
        stats.setPadding(0, dp(12), 0, 0);
        LinearLayout pBlocked = statPill("已拦截");
        LinearLayout pSeen = statPill("已接收");
        LinearLayout pRules = statPill("监听规则");
        statBlocked = (TextView) pBlocked.getTag();
        statSeen = (TextView) pSeen.getTag();
        statRules = (TextView) pRules.getTag();
        stats.addView(pBlocked);
        stats.addView(pSeen);
        stats.addView(pRules);
        statusCard.addView(stats);

        rishText = label("Shizuku：未测试", 12f, MUTED, false);
        rishText.setPadding(0, dp(12), 0, 0);
        statusCard.addView(rishText);
        lastCmd = label("", 12f, AMBER, false);
        statusCard.addView(lastCmd);
        root.addView(statusCard);

        // keep-alive card
        LinearLayout kaCard = card();
        kaCard.addView(label("后台保活", 13f, FG, true));
        kaLine = label("检测中…", 12f, MUTED, false);
        kaLine.setPadding(0, dp(8), 0, dp(6));
        kaCard.addView(kaLine);
        LinearLayout kaRow = row();
        kaRow.addView(ghostButton("电池白名单", new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                try {
                    Intent i = new Intent(android.provider.Settings.ACTION_REQUEST_IGNORE_BATTERY_OPTIMIZATIONS);
                    i.setData(android.net.Uri.parse("package:" + getPackageName()));
                    startActivity(i);
                } catch (Throwable t) {
                    Guard.log(MainActivity.this, "错误", "打开电池优化设置失败：" + t);
                }
            }
        }));
        kaRow.addView(ghostButton("MIUI 自启动", new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                try {
                    Intent i = new Intent();
                    i.setComponent(new android.content.ComponentName("com.miui.securitycenter",
                            "com.miui.permcenter.autostart.AutoStartManagementActivity"));
                    startActivity(i);
                } catch (Throwable t) {
                    Guard.log(MainActivity.this, "错误", "打开 MIUI 自启动设置失败：" + t);
                }
            }
        }));
        kaCard.addView(kaRow);
        root.addView(kaCard);

        // controls
        LinearLayout row1 = row();
        guardToggle = actionButton("启动守护", GREEN, 0xFF2BB98A, new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (Guard.serviceRunning) {
                    Guard.setWanted(MainActivity.this, false);
                    KeepAlive.cancelAlarm(MainActivity.this);
                    stopService(new Intent(MainActivity.this, GuardService.class));
                    Guard.log(MainActivity.this, "操作", "手动停止守护服务，并关闭自动重启");
                } else {
                    Guard.setWanted(MainActivity.this, true);
                    startForegroundService(new Intent(MainActivity.this, GuardService.class));
                    Guard.log(MainActivity.this, "操作", "手动启动守护服务");
                }
            }
        });
        row1.addView(guardToggle);
        root.addView(row1);

        LinearLayout row2 = row();
        row2.addView(ghostButton("测试 Shizuku", new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Guard.log(MainActivity.this, "命令", "开始测试 Shizuku 权限…");
                new Thread(new Runnable() {
                    @Override
                    public void run() {
                        final String r = Guard.rishTest(getApplicationContext());
                        handler.post(new Runnable() {
                            @Override
                            public void run() {
                                Guard.log(MainActivity.this, "命令", "Shizuku 测试：" + Guard.rishState
                                        + "（" + Guard.summarize(r) + "）");
                                refresh();
                            }
                        });
                    }
                }).start();
            }
        }));
        row2.addView(ghostButton("自测广播", new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Intent t = new Intent("com.dsh.themeguard.TEST");
                t.setPackage(getPackageName());
                sendOrderedBroadcast(t, null);
                Guard.log(MainActivity.this, "自测", "已发送自测广播（有序广播，应被自己拦下）");
            }
        }));
        root.addView(row2);

        LinearLayout row3 = row();
        row3.addView(ghostButton("测试兜底命令", new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                final String c = cmdInput.getText().toString().trim();
                if (c.length() == 0) {
                    Toast.makeText(MainActivity.this, "请先填写兜底命令", Toast.LENGTH_SHORT).show();
                    return;
                }
                Guard.log(MainActivity.this, "命令", "手动测试兜底：" + c);
                new Thread(new Runnable() {
                    @Override
                    public void run() {
                        final String r = Guard.rish(getApplicationContext(), c);
                        Guard.lastResult = r;
                        handler.post(new Runnable() {
                            @Override
                            public void run() {
                                Guard.log(MainActivity.this, "命令", "兜底返回：" + r.replace("\n", " | ").trim());
                                refresh();
                            }
                        });
                    }
                }).start();
            }
        }));
        root.addView(row3);

        // command card
        LinearLayout cmdCard = card();
        cmdCard.addView(label("触发后执行的命令（经 Shizuku 以 shell 执行，可留空）", 12f, MUTED, false));
        cmdInput = new EditText(this);
        cmdInput.setHint("cp /sdcard/theme/x.mtz … && echo done");
        cmdInput.setTextColor(FG);
        cmdInput.setHintTextColor(0xFF5A6675);
        cmdInput.setTextSize(12.5f);
        cmdInput.setBackground(shape(SURFACE_2, 12, STROKE, 1));
        cmdInput.setPadding(dp(12), dp(10), dp(12), dp(10));
        cmdInput.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_MULTI_LINE);
        cmdInput.setText(getSharedPreferences("cfg", MODE_PRIVATE).getString("cmd", ""));
        LinearLayout.LayoutParams clp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        clp.topMargin = dp(10);
        cmdCard.addView(cmdInput, clp);
        LinearLayout cmdRow = row();
        cmdRow.addView(actionButton("保存命令", BLUE, 0xFF3E86D6, new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                getSharedPreferences("cfg", MODE_PRIVATE).edit()
                        .putString("cmd", cmdInput.getText().toString()).apply();
                Toast.makeText(MainActivity.this, "已保存", Toast.LENGTH_SHORT).show();
                Guard.log(MainActivity.this, "操作", "触发后要执行的命令已保存");
            }
        }));
        cmdCard.addView(cmdRow);
        root.addView(cmdCard);

        // log card: takes whatever vertical space is left, scrolls inside
        LinearLayout logCard = card();
        logCard.setLayoutParams(new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f));
        logCard.setMinimumHeight(dp(150));
        LinearLayout logHead = new LinearLayout(this);
        logHead.setOrientation(LinearLayout.HORIZONTAL);
        logHead.setGravity(Gravity.CENTER_VERTICAL);
        TextView logTitle = label("运行日志", 13f, FG, true);
        logHead.addView(logTitle, new LinearLayout.LayoutParams(0,
                ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        TextView clear = label("清空", 13f, RED, true);
        clear.setPadding(dp(10), dp(4), dp(2), dp(4));
        clear.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Guard.clearLog(MainActivity.this);
                logContainer.removeAllViews();
                shownLines.clear();
            }
        });
        logHead.addView(clear);
        logCard.addView(logHead);

        logScroll = new ScrollView(this);
        logContainer = new LinearLayout(this);
        logContainer.setOrientation(LinearLayout.VERTICAL);
        logContainer.setPadding(0, dp(8), 0, 0);
        logScroll.addView(logContainer);
        LinearLayout.LayoutParams llp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f);
        logScroll.setLayoutParams(llp);
        logCard.addView(logScroll);
        root.addView(logCard);

        for (int i = 0; i < root.getChildCount(); i++) {
            animTargets.add(root.getChildAt(i));
        }
        setContentView(root);
        Guard.log(this, "界面", "打开界面（累计接收 " + Guard.total + " 条，已拦截 " + Guard.blocked + " 条）");
        try {
            startForegroundService(new Intent(MainActivity.this, GuardService.class));
            Guard.log(this, "服务", "界面已请求自动启动守护");
        } catch (Throwable t) {
            Guard.log(this, "错误", "自动启动守护失败：" + t);
        }
        playEntrance();
        askNotificationPermission();
        seedFallback();
    }

    /** First run: remember the default fallback command and push the helper scripts. */
    private void seedFallback() {
        if (getSharedPreferences("cfg", MODE_PRIVATE).getString("cmd", "").isEmpty()) {
            getSharedPreferences("cfg", MODE_PRIVATE).edit()
                    .putString("cmd", Guard.DEFAULT_CMD).apply();
            cmdInput.setText(Guard.DEFAULT_CMD);
            Guard.log(this, "操作", "已预置兜底命令：" + Guard.DEFAULT_CMD);
        }
        if (getSharedPreferences("cfg", MODE_PRIVATE).getBoolean("scripts", false)) {
            return;
        }
        Guard.log(this, "命令", "正在通过 Shizuku 部署兜底脚本…");
        new Thread(new Runnable() {
            @Override
            public void run() {
                final String r = Guard.deployScripts(getApplicationContext());
                final boolean ok = r.contains("fallback.sh");
                getSharedPreferences("cfg", MODE_PRIVATE).edit().putBoolean("scripts", ok).apply();
                handler.post(new Runnable() {
                    @Override
                    public void run() {
                        Guard.log(MainActivity.this, "命令", ok
                                ? "兜底脚本已就绪：/sdcard/ThemeGuard/fallback.sh"
                                : "兜底脚本部署失败（需 Shizuku 授权）：" + r.replace("\n", " ").trim());
                        refresh();
                    }
                });
            }
        }).start();
    }

    private void askNotificationPermission() {
        try {
            if (android.os.Build.VERSION.SDK_INT >= 33
                    && checkSelfPermission("android.permission.POST_NOTIFICATIONS")
                    != android.content.pm.PackageManager.PERMISSION_GRANTED) {
                requestPermissions(new String[]{"android.permission.POST_NOTIFICATIONS"}, 1);
                Guard.log(this, "服务", "已申请通知权限（保活通知需要）");
            }
        } catch (Throwable ignored) {
        }
    }

    private void refresh() {
        boolean running = Guard.serviceRunning;
        statusText.setText(running ? "守护中 · 服务运行" : "已停止");
        if (running != lastRunning) {
            statusDot.setBackground(ovalBg(running ? GREEN : MUTED));
            if (running) {
                startDotPulse();
            } else {
                stopDotPulse();
            }
            lastRunning = running;
        }
        updateToggle(running);
        setStat(statBlocked, Guard.blocked);
        setStat(statSeen, Guard.total);
        statRules.setText(String.valueOf(Guard.WATCH.length));
        if (lastBlocked < 0) {
            lastBlocked = Guard.blocked;
        } else if (Guard.blocked > lastBlocked) {
            lastBlocked = Guard.blocked;
            flashStatusCard();
        }
        rishText.setText("Shizuku：" + Guard.rishState);
        rishText.setTextColor(Guard.rishState.contains("可用") ? GREEN : MUTED);
        String last = Guard.lastResult == null ? "-" : Guard.lastResult.replace("\n", " ").trim();
        if (last.length() > 90) {
            last = last.substring(0, 90) + "…";
        }
        lastCmd.setText(last.equals("-") ? "" : "上次命令：" + last);

        SpannableStringBuilder ka = new SpannableStringBuilder("保活：");
        boolean exempt;
        try {
            android.os.PowerManager pm = (android.os.PowerManager) getSystemService(POWER_SERVICE);
            exempt = pm != null && pm.isIgnoringBatteryOptimizations(getPackageName());
        } catch (Throwable t) {
            exempt = false;
        }
        appendColored(ka, exempt ? "电池优化已豁免" : "电池优化未豁免", exempt ? GREEN : AMBER);
        try {
            if (android.os.Build.VERSION.SDK_INT >= 28) {
                android.app.usage.UsageStatsManager usm =
                        (android.app.usage.UsageStatsManager) getSystemService("usagestats");
                if (usm != null) {
                    int bucket = usm.getAppStandbyBucket();
                    String name;
                    if (bucket == 5) {
                        name = "active";
                    } else if (bucket == 10) {
                        name = "working set";
                    } else if (bucket == 20) {
                        name = "frequent";
                    } else if (bucket == 30) {
                        name = "rare";
                    } else if (bucket == 40) {
                        name = "restricted";
                    } else {
                        name = "bucket " + bucket;
                    }
                    ka.append(" · ");
                    appendColored(ka, "待机分组 " + name, name.equals("active") ? GREEN
                            : name.equals("working set") ? 0xFF6FD8B6
                            : name.equals("frequent") ? BLUE
                            : name.equals("rare") ? AMBER : RED);
                }
            }
        } catch (Throwable ignored) {
        }
        try {
            android.app.job.JobScheduler js =
                    (android.app.job.JobScheduler) getSystemService(JOB_SCHEDULER_SERVICE);
            ka.append(" · ");
            boolean armed = js != null && js.getPendingJob(KeepAlive.JOB_ID) != null;
            appendColored(ka, armed ? "看门狗已排程" : "看门狗未排程", armed ? GREEN : RED);
        } catch (Throwable ignored) {
        }
        kaLine.setText(ka);

        syncLog(Guard.readLog(this));
    }

    /** Append only the freshly arrived lines; each new line slides in from the right. */
    private void syncLog(String all) {
        java.util.List<String> lines = new java.util.ArrayList<String>();
        for (String l : all.split("\n")) {
            if (l.trim().length() > 0) {
                lines.add(l);
            }
        }
        if (lines.size() == shownLines.size()
                && (lines.isEmpty() || lines.get(lines.size() - 1).equals(shownLines.get(shownLines.size() - 1)))) {
            return;
        }
        boolean appendOnly = lines.size() > shownLines.size()
                && lines.subList(0, shownLines.size()).equals(shownLines);
        if (!appendOnly) {
            logContainer.removeAllViews();
            shownLines.clear();
        }
        boolean nearBottom = true;
        View child = logScroll.getChildAt(0);
        if (child != null) {
            nearBottom = (child.getBottom() - logScroll.getScrollY() - logScroll.getHeight()) < dp(48);
        }
        int added = 0;
        for (int i = shownLines.size(); i < lines.size(); i++) {
            String line = lines.get(i);
            TextView tv = label(line, 11f, logColor(line), false);
            tv.setTypeface(Typeface.MONOSPACE);
            tv.setLineSpacing(dp(2), 1f);
            logContainer.addView(tv);
            shownLines.add(line);
            if (added < 12) {
                tv.setAlpha(0f);
                tv.setTranslationX(dp(36));
                tv.animate().alpha(1f).translationX(0f)
                        .setDuration(240)
                        .setStartDelay(added * 45L)
                        .setInterpolator(new DecelerateInterpolator())
                        .start();
            }
            added++;
        }
        while (shownLines.size() > MAX_LOG_LINES) {
            shownLines.remove(0);
            if (logContainer.getChildCount() > 0) {
                logContainer.removeViewAt(0);
            }
        }
        if (nearBottom) {
            logScroll.post(new Runnable() {
                @Override
                public void run() {
                    logScroll.fullScroll(View.FOCUS_DOWN);
                }
            });
        }
    }

    private void appendColored(SpannableStringBuilder sb, String s, int color) {
        int start = sb.length();
        sb.append(s);
        sb.setSpan(new ForegroundColorSpan(color), start, sb.length(),
                Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
    }

    /** Squash-and-release feedback while the finger is down. */
    private void attachPressFeedback(final View v) {
        v.setOnTouchListener(new View.OnTouchListener() {
            @Override
            public boolean onTouch(View view, MotionEvent e) {
                switch (e.getActionMasked()) {
                    case MotionEvent.ACTION_DOWN:
                        view.animate().scaleX(0.965f).scaleY(0.965f).setDuration(90)
                                .setInterpolator(new DecelerateInterpolator()).start();
                        break;
                    case MotionEvent.ACTION_UP:
                    case MotionEvent.ACTION_CANCEL:
                        view.animate().scaleX(1f).scaleY(1f).setDuration(160)
                                .setInterpolator(new DecelerateInterpolator()).start();
                        break;
                    default:
                        break;
                }
                return false;
            }
        });
    }

    /** Staggered fade + rise for every block on the page. */
    private void playEntrance() {
        for (int i = 0; i < animTargets.size(); i++) {
            View v = animTargets.get(i);
            v.setAlpha(0f);
            v.setTranslationY(dp(26));
            v.animate().alpha(1f).translationY(0f)
                    .setStartDelay(55L * i)
                    .setDuration(360)
                    .setInterpolator(new DecelerateInterpolator())
                    .start();
        }
    }

    /** One button, two jobs: 启动守护 <-> 停止守护. */
    private void updateToggle(boolean running) {
        if (guardToggle == null) {
            return;
        }
        String want = running ? "停止守护" : "启动守护";
        if (want.contentEquals(guardToggle.getText())) {
            return;
        }
        guardToggle.setText(want);
        guardToggle.setTextColor(0xFF0B1116);
        guardToggle.setBackground(running
                ? pressable(RED, 0xFFD45B5B, 16)
                : pressable(GREEN, 0xFF2BB98A, 16));
        guardToggle.animate().cancel();
        guardToggle.setScaleX(0.93f);
        guardToggle.setScaleY(0.93f);
        guardToggle.animate().scaleX(1f).scaleY(1f).setDuration(240)
                .setInterpolator(new DecelerateInterpolator()).start();
    }

    private void startDotPulse() {
        if (dotPulse != null) {
            return;
        }
        statusDot.setAlpha(1f);
        dotPulse = ObjectAnimator.ofFloat(statusDot, "alpha", 1f, 0.3f);
        dotPulse.setDuration(850);
        dotPulse.setRepeatCount(ValueAnimator.INFINITE);
        dotPulse.setRepeatMode(ValueAnimator.REVERSE);
        dotPulse.start();
    }

    private void stopDotPulse() {
        if (dotPulse != null) {
            dotPulse.cancel();
            dotPulse = null;
        }
        statusDot.setAlpha(1f);
    }

    /** Pop the number when it changes. */
    private void setStat(TextView v, int value) {
        String s = String.valueOf(value);
        if (s.equals(String.valueOf(v.getText()))) {
            return;
        }
        v.setText(s);
        v.animate().cancel();
        v.setScaleX(1f);
        v.setScaleY(1f);
        v.animate().scaleX(1.32f).scaleY(1.32f).setDuration(110)
                .setInterpolator(new DecelerateInterpolator())
                .withEndAction(new Runnable() {
                    @Override
                    public void run() {
                        v.animate().scaleX(1f).scaleY(1f).setDuration(180)
                                .setInterpolator(new DecelerateInterpolator()).start();
                    }
                }).start();
    }

    /** Green border flash when a broadcast gets blocked. */
    private void flashStatusCard() {
        if (statusCardBg == null) {
            return;
        }
        ValueAnimator va = ValueAnimator.ofObject(new ArgbEvaluator(), GREEN, STROKE);
        va.setDuration(900);
        va.addUpdateListener(new ValueAnimator.AnimatorUpdateListener() {
            @Override
            public void onAnimationUpdate(ValueAnimator a) {
                int c = (Integer) a.getAnimatedValue();
                float f = a.getAnimatedFraction();
                statusCardBg.setStroke(dp(f < 0.5f ? 2 : 1), c);
            }
        });
        va.start();
    }

    private int logColor(String line) {
        if (line.contains("[拦截]")) {
            return GREEN;
        }
        if (line.contains("[接收]")) {
            return 0xFF8FC2FF;
        }
        if (line.contains("[命令]")) {
            return AMBER;
        }
        if (line.contains("[错误]") || line.contains("[警告]")) {
            return RED;
        }
        if (line.contains("[保活]")) {
            return 0xFF6FD8B6;
        }
        if (line.contains("[服务]") || line.contains("[启动]")) {
            return 0xFFB9C7D6;
        }
        return 0xFF98A6B5;
    }

    private GradientDrawable ovalBg(int color) {
        GradientDrawable g = new GradientDrawable();
        g.setShape(GradientDrawable.OVAL);
        g.setColor(color);
        return g;
    }

    @Override
    protected void onResume() {
        super.onResume();
        handler.post(ticker);
    }

    @Override
    protected void onPause() {
        super.onPause();
        handler.removeCallbacks(ticker);
    }
}
