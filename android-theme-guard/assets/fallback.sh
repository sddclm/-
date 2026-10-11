#!/system/bin/sh
# ThemeGuard 兜底脚本：MIUI 主题恢复广播拦不住时自动执行
BK=/sdcard/ThemeGuard/theme-backup
DST=/sdcard/Android/data/com.android.thememanager/files/theme
echo "[兜底] $(date '+%m-%d %H:%M:%S') 开始"
n=0
if [ -d "$BK" ]; then
  mkdir -p "$DST" 2>/dev/null
  for f in "$BK"/*.mtz; do
    [ -f "$f" ] || continue
    cp -f "$f" "$DST"/ 2>/dev/null && n=$((n+1))
  done
fi
echo "[兜底] 已回注主题包 $n 个"
if [ "$n" -gt 0 ]; then
  am start -n com.android.thememanager/.activity.ThemeTabActivity >/dev/null 2>&1
  echo "[兜底] 已拉起主题管理器，点一下应用即可"
else
  echo "[兜底] 没有可回注的主题包（把 .mtz 放进 $BK 即可）"
fi
