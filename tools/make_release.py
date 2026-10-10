# 创建 GitHub Release 并上传编译产物
# 用法: $env:GH_TOKEN="ghp_xxx"; python tools/make_release.py
import urllib.request, json, base64, os, sys

TOKEN = os.environ.get("GH_TOKEN", "")
REPO = "sddclm/-"
TAG = "v1.0"
EXE = r"D:\工作区\MouseClicker\dist\AutoClicker.exe"
ASSET_NAME = "MouseClicker.exe"

if not TOKEN:
    print("缺少 GH_TOKEN 环境变量"); sys.exit(1)

def api(path, method="GET", body=None, base="https://api.github.com", raw=None):
    url = base + path
    headers = {"User-Agent": "dsh-release", "Accept": "application/vnd.github+json",
               "Authorization": "Bearer " + TOKEN}
    data = None
    if body is not None:
        data = json.dumps(body).encode("utf-8")
        headers["Content-Type"] = "application/json"
    if raw is not None:
        data = raw
        headers["Content-Type"] = "application/octet-stream"
    req = urllib.request.Request(url, data=data, method=method, headers=headers)
    try:
        r = urllib.request.urlopen(req, timeout=300)
        return r.status, json.load(r)
    except urllib.error.HTTPError as e:
        try:
            msg = json.load(e)
        except Exception:
            msg = e.read().decode("utf-8", "ignore")[:300]
        return e.code, msg
    except Exception as e:
        return -1, str(e)

# 1) 从仓库读取 Release 说明（避免本地中文路径问题）
print("[1] 读取 Release 说明 ...")
st, d = api("/repos/" + REPO + "/contents/RELEASE_NOTES_v1.0.md")
if st != 200 or not isinstance(d, dict):
    print("    读取失败:", st, d); sys.exit(1)
notes = base64.b64decode(d["content"]).decode("utf-8")
title = notes.split("\n")[0].lstrip("#").strip()
print("    标题:", title, "| 说明长度:", len(notes), "字符")

# 2) 创建 Release
print("[2] 创建 Release " + TAG + " ...")
st2, rel = api("/repos/" + REPO + "/releases", "POST", {
    "tag_name": TAG,
    "target_commitish": "main",
    "name": title,
    "body": notes,
    "draft": False,
    "prerelease": False
})
print("    HTTP", st2)
if st2 not in (200, 201) or not isinstance(rel, dict):
    print("    失败:", json.dumps(rel, ensure_ascii=False)[:400] if isinstance(rel, dict) else rel)
    sys.exit(1)
print("    release id:", rel["id"])
print("    网页:", rel["html_url"])

# 3) 上传 exe 资产
print("[3] 上传 " + ASSET_NAME + " ...")
with open(EXE, "rb") as f:
    data = f.read()
print("    本地大小:", round(len(data)/1024.0, 1), "KB")
upload_base = rel["upload_url"].split("{")[0]
st3, asset = api("?name=" + ASSET_NAME, "POST", base=upload_base, raw=data)
print("    HTTP", st3)
if isinstance(asset, dict) and asset.get("browser_download_url"):
    print("    下载地址:", asset["browser_download_url"])
    print("    大小:", round(asset["size"]/1024.0, 1), "KB")
else:
    print("    失败:", json.dumps(asset, ensure_ascii=False)[:400] if isinstance(asset, dict) else asset)

# 4) 验证
print()
print("[4] 验证")
st4, rels = api("/repos/" + REPO + "/releases")
if isinstance(rels, list):
    for x in rels:
        print("    Release:", x["tag_name"], "|", x["name"])
        print("    资产:", [a["name"] for a in x.get("assets", [])])
        print("    状态: draft=", x["draft"], " prerelease=", x["prerelease"])
