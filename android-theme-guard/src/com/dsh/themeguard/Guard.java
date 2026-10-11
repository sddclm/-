package com.dsh.themeguard;

import android.content.Context;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Date;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.TimeUnit;

/** Shared helpers: watch list, logging, rish (Shizuku remote shell) bridge. */
public final class Guard {

    /** Broadcasts we hook. MIUI theme trial/restore uses CHECK_TIME_UP. */
    public static final String[] WATCH = new String[]{
            "miui.intent.action.CHECK_TIME_UP",
            "miui.intent.action.CLEAR_THEME_RUNTIME_DATA",
            "com.android.thememanager.action.GET_MIBI",
            "android.intent.action.TIME_SET",
            "android.intent.action.TIME_TICK",
            "android.intent.action.SCREEN_ON",
            "android.intent.action.SCREEN_OFF",
            "com.dsh.themeguard.TEST",
            "com.dsh.themeguard.TEST_ORDERED"
    };

    public static final List<String> LINES = Collections.synchronizedList(new ArrayList<String>());
    public static volatile int total = 0;
    public static volatile int blocked = 0;
    public static volatile boolean serviceRunning = false;
    public static volatile String lastResult = "-";
    public static volatile String rishState = "未测试";

    private Guard() {
    }

    /** Actions that actually threaten the applied theme. */
    public static final String DEFAULT_CMD = "sh /sdcard/ThemeGuard/fallback.sh";
    public static final String SCRIPTS_DIR = "/sdcard/ThemeGuard";

    public static boolean isDangerous(String action) {
        return "miui.intent.action.CHECK_TIME_UP".equals(action)
                || "miui.intent.action.CLEAR_THEME_RUNTIME_DATA".equals(action)
                || "com.android.thememanager.action.GET_MIBI".equals(action);
    }

    /** Push the fallback/backup shell scripts into shared storage through rish. */
    public static String deployScripts(Context c) {
        return writeRemote(c, "fallback.sh") + writeRemote(c, "backup.sh");
    }

    private static String writeRemote(Context c, String name) {
        try {
            java.io.InputStream in = c.getAssets().open(name);
            byte[] buf = new byte[Math.max(1024, in.available())];
            int n = in.read(buf);
            in.close();
            String b64 = android.util.Base64.encodeToString(buf, 0, n, android.util.Base64.NO_WRAP);
            String cmd = "mkdir -p " + SCRIPTS_DIR
                    + " && echo " + b64 + " | base64 -d > " + SCRIPTS_DIR + "/" + name
                    + " && chmod 755 " + SCRIPTS_DIR + "/" + name
                    + " && ls -l " + SCRIPTS_DIR + "/" + name;
            return rish(c, cmd);
        } catch (Throwable t) {
            return name + " 部署失败：" + t;
        }
    }

    /** Human readable name for a watched action. */
    public static String friendly(String action) {
        if (action == null) {
            return "空广播";
        }
        if ("miui.intent.action.CHECK_TIME_UP".equals(action)) {
            return "主题试用到期检查";
        }
        if ("miui.intent.action.CLEAR_THEME_RUNTIME_DATA".equals(action)) {
            return "清除主题运行数据（恢复默认主题）";
        }
        if ("com.android.thememanager.action.GET_MIBI".equals(action)) {
            return "主题试用信息查询";
        }
        if ("android.intent.action.TIME_SET".equals(action)) {
            return "系统时间被修改";
        }
        if ("android.intent.action.TIME_TICK".equals(action)) {
            return "时间心跳";
        }
        if ("android.intent.action.SCREEN_ON".equals(action)) {
            return "屏幕点亮";
        }
        if ("android.intent.action.SCREEN_OFF".equals(action)) {
            return "屏幕熄灭";
        }
        if ("com.dsh.themeguard.TEST".equals(action)) {
            return "自测广播";
        }
        if ("com.dsh.themeguard.TEST_ORDERED".equals(action)) {
            return "自测有序广播";
        }
        return action;
    }

    /** Append one log line: "HH:mm:ss  [标签] 内容". */
    public static void log(Context c, String tag, String detail) {
        log(c, "[" + tag + "] " + detail);
    }

    public static synchronized void log(Context c, String s) {
        String line = new SimpleDateFormat("HH:mm:ss", Locale.US).format(new Date()) + "  " + s;
        LINES.add(line);
        while (LINES.size() > 300) {
            LINES.remove(0);
        }
        if (c == null) {
            return;
        }
        try {
            FileOutputStream fos = new FileOutputStream(new File(c.getFilesDir(), "guard.log"), true);
            fos.write((line + "\n").getBytes("UTF-8"));
            fos.close();
        } catch (Throwable ignored) {
        }
    }

    public static synchronized String readLog(Context c) {
        try {
            File f = new File(c.getFilesDir(), "guard.log");
            if (!f.exists()) {
                return "";
            }
            InputStream in = new java.io.FileInputStream(f);
            byte[] buf = new byte[(int) f.length()];
            int n = in.read(buf);
            in.close();
            return n > 0 ? new String(buf, 0, n, "UTF-8") : "";
        } catch (Throwable t) {
            return "";
        }
    }

    public static synchronized void clearLog(Context c) {
        LINES.clear();
        try {
            new File(c.getFilesDir(), "guard.log").delete();
        } catch (Throwable ignored) {
        }
    }

    /** Extract an asset into files dir (once). */
    public static File asset(Context c, String name) {
        File f = new File(c.getFilesDir(), name);
        try {
            if (!f.exists()) {
                InputStream in = c.getAssets().open(name);
                OutputStream out = new FileOutputStream(f);
                byte[] buf = new byte[8192];
                int n;
                while ((n = in.read(buf)) > 0) {
                    out.write(buf, 0, n);
                }
                out.close();
                in.close();
            }
            return f;
        } catch (Throwable t) {
            log(c, "错误", "释放内置文件 " + name + " 失败：" + t);
            return null;
        }
    }

    private static String drain(java.lang.Process p) throws Exception {
        java.io.BufferedReader r = new java.io.BufferedReader(
                new java.io.InputStreamReader(p.getInputStream(), "UTF-8"));
        StringBuilder sb = new StringBuilder();
        String l;
        while ((l = r.readLine()) != null) {
            sb.append(l).append('\n');
        }
        return sb.toString();
    }

    /** Run a command with shell privileges through Shizuku's rish bridge. */
    public static String rish(Context c, String command) {
        File script = asset(c, "rish");
        File dex = asset(c, "rish_shizuku.dex");
        if (script == null || dex == null) {
            return "内置 rish 文件缺失";
        }
        // Android 14+ refuses to load a writable dex via app_process.
        try {
            Runtime.getRuntime().exec(new String[]{"chmod", "400", dex.getAbsolutePath()}).waitFor();
        } catch (Throwable ignored) {
        }
        try {
            ProcessBuilder pb = new ProcessBuilder("/system/bin/sh", script.getAbsolutePath(), "-c", command);
            pb.environment().put("RISH_APPLICATION_ID", c.getPackageName());
            pb.redirectErrorStream(true);
            Process p = pb.start();
            String raw = drain(p);
            StringBuilder clean = new StringBuilder();
            for (String line : raw.split("\n")) {
                if (line.startsWith("chmod failed")) {
                    continue;   // noise from the Shizuku loader on Android 14+
                }
                clean.append(line).append('\n');
            }
            String out = clean.toString();
            boolean done = p.waitFor(25, TimeUnit.SECONDS);
            if (!done) {
                p.destroy();
                return out + "\n[超时未结束]";
            }
            return out + "[退出码 " + p.exitValue() + "]";
        } catch (Throwable t) {
            return "rish 执行失败：" + t;
        }
    }

    /** rish self-test: expects uid=2000(shell). */
    public static String rishTest(Context c) {
        String out = rish(c, "id; getprop ro.product.model");
        if (out.contains("uid=2000")) {
            setRishState(c, "已授权 · shell 可用");
        } else if (out.toLowerCase().contains("denied") || out.toLowerCase().contains("permission")
                || out.toLowerCase().contains("no permission")) {
            setRishState(c, "未授权 · 请在 Shizuku 中允许本应用");
        } else {
            setRishState(c, "异常 · 详见日志");
        }
        return out;
    }

    /** Turn raw rish output into a one-line summary for the log. */
    public static String summarize(String out) {
        String uid = "";
        String model = "";
        java.util.regex.Matcher m = java.util.regex.Pattern
                .compile("uid=\\d+\\([a-z_]+\\)").matcher(out);
        if (m.find()) {
            uid = m.group();
        }
        for (String raw : out.split("\\n")) {
            String l = raw.trim();
            if (l.length() >= 4 && l.length() <= 24 && l.matches("[A-Za-z0-9_\\-]+")
                    && !l.startsWith("chmod") && !l.startsWith("uid") && !l.contains("exitCode")) {
                model = l;
            }
        }
        StringBuilder sb = new StringBuilder();
        sb.append(uid.isEmpty() ? "未取得 shell 身份" : uid);
        if (!model.isEmpty()) {
            sb.append(" · 设备 ").append(model);
        }
        if (out.contains("[退出码 0]") || out.contains("[exit 0]")) {
            sb.append(" · 命令正常返回");
        }
        return sb.toString();
    }

    public static void setRishState(Context c, String v) {
        rishState = v;
        try {
            c.getSharedPreferences("cfg", Context.MODE_PRIVATE).edit()
                    .putString("rishState", v).apply();
        } catch (Throwable ignored) {
        }
    }

    public static void loadRishState(Context c) {
        try {
            rishState = c.getSharedPreferences("cfg", Context.MODE_PRIVATE)
                    .getString("rishState", "未测试");
        } catch (Throwable ignored) {
        }
    }

    /** Whether the user wants the guard running (kept across reboots/restarts). */
    public static boolean isWanted(Context c) {
        try {
            return c.getSharedPreferences("cfg", Context.MODE_PRIVATE).getBoolean("want", true);
        } catch (Throwable t) {
            return true;
        }
    }

    public static void setWanted(Context c, boolean v) {
        try {
            c.getSharedPreferences("cfg", Context.MODE_PRIVATE).edit().putBoolean("want", v).apply();
        } catch (Throwable ignored) {
        }
    }
}
