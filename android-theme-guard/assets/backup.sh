#!/system/bin/sh
# 备份 MIUI 本地主题包，供兜底脚本回注
BK=/sdcard/ThemeGuard/theme-backup
SRC=/sdcard/Android/data/com.android.thememanager/files/theme
mkdir -p "$BK"
c=0
for f in "$SRC"/*.mtz; do
  [ -f "$f" ] || continue
  cp -f "$f" "$BK"/ 2>/dev/null && c=$((c+1))
done
echo "[备份] 已备份主题包 $c 个 -> $BK"
ls -la "$BK" 2>/dev/null | head -12
