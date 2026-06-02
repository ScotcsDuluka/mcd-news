# MCD News Mod - Build & Install Guide

## วิธีนี้ง่ายกว่าเดิมมาก!

เดิม: ฝัง HTTPS server ใน DLL (ซับซ้อน 800+ บรรทัด C++)
ใหม่: DLL ทำแค่ DNS hook, HTTPS server รันแยกด้วย Python

---

## Step 1: Build version.dll

เปิด **Visual Studio Developer Command Prompt** (x64) แล้วรัน:

```
cd C:\Users\ScotcsDuluka\Downloads\mcd_news_dll

cl /nologo /O2 /LD /MT /DNDEBUG /D_WINSOCK_DEPRECATED_NO_WARNINGS ^
   /D SECURITY_WIN32 /EHsc /std:c++17 version_proxy.cpp ^
   /DEF:version.def ^
   /link ws2_32.lib user32.lib psapi.lib ^
   /OUT:version.dll
```

ถ้า build สำเร็จจะได้ `version.dll`

---

## Step 2: ติดตั้ง Python (ถ้ายังไม่มี)

ดาวน์โหลดจาก https://www.python.org/downloads/
ตอนติดตั้ง ให้เลือก **"Add Python to PATH"**

---

## Step 3: ติดตั้งไฟล์ลงเกม

Copy ไฟล์เหล่านี้ไปยังโฟลเดอร์เกม:

```
copy version.dll  "D:\SteamLibrary\Steamapps\Common\Minecraft Dungeons\Dungeons\Binaries\Win64\"
copy news.json    "D:\SteamLibrary\Steamapps\Common\Minecraft Dungeons\Dungeons\Binaries\Win64\"
```

---

## Step 4: รัน HTTPS Server

**คลิกขวา `launch_server.bat` → Run as Administrator**

หรือรันเองใน Admin Command Prompt:
```
cd C:\Users\ScotcsDuluka\Downloads\mcd_news_dll
python server.py
```

รอจนเห็นข้อความ:
```
[MCD-News] HTTPS server running on https://127.0.0.1:443
```

---

## Step 5: เปิดเกม

เปิด Minecraft Dungeons ตามปกติ — news จะเปลี่ยนเป็นของเรา!

---

## หยุดเซิร์ฟเวอร์

กด **Ctrl+C** ในหน้าต่าง server.py — certificate จะถูกลบอัตโนมัติ

---

## แก้ไข News

แก้ไขไฟล์ `news.json` ด้วย text editor แล้ว refresh ในเกม (รีสตาร์ทเกม)

---

## ถ้าเกมเปิดไม่ได้

1. ลองลบ version.dll ออกก่อน → ถ้าเกมเปิดได้ = DLL มีปัญหา
2. ตรวจสอบว่า version.dll อยู่ข้าง .exe ถูกที่
3. ตรวจสอบ Debug View หาข้อความ `[MCD-News]`

---

## Troubleshooting

| ปัญหา | แก้ยังไง |
|---|---|
| `pip install cryptography` ไม่ได้ | รัน `python -m pip install --upgrade pip` ก่อน |
| `certutil` ล้มเหลว | รันเป็น Administrator |
| Port 443 ถูกใช้ | ปิดโปรแกรมอื่นที่ใช้ port 443 (เช่น IIS, Skype) |
| Certificate warning | รัน `certutil -addstore -user Root mcd_news_cert.der` เอง |
| เกมไม่แสดง news | เช็ค DebugView ดู `[MCD-News] DNS INTERCEPT` ขึ้นไหม |

---

## ไฟล์ทั้งหมด

| ไฟล์ | หน้าที่ |
|---|---|
| `version_proxy.cpp` | โค้ด C++ DLL (proxy + DNS hook) |
| `version.def` | Export definitions สำหรับ DLL |
| `server.py` | Python HTTPS server (สร้าง cert, ส่ง news.json) |
| `launch_server.bat` | สคริปต์รัน server (คลิกขวา Run as Admin) |
| `news.json` | ข้อมูล news ที่จะแสดงในเกม |
