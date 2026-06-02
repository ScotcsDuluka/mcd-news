// mcd_news_hook.cpp v5
// Minecraft Dungeons News Mod - Transparent Proxy + Auto Server
//
// What this DLL does:
// 1. Forwards ALL 89 WinHTTP exports to winhttp_orig.dll (ZERO interception)
//    Game can't tell the difference - everything works normally
// 2. On DLL load: starts Python HTTPS server in background
//    - Server fetches news from scotcsduluka.github.io/mcd-news/news.json
//    - Server runs on 127.0.0.1:443 with self-signed cert
// 3. On DLL unload: kills server + cleans temp files
//
// The hosts entry is managed by install.bat / uninstall.bat

#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>

// ALL WinHTTP exports forwarded - NO interception, NO skipping
// This makes our DLL 100% transparent to the game
#include "winhttp_pragmas.h"

// ========== Embedded Python HTTPS Server ==========
static const char SERVER_SCRIPT[] =
"import http.server,ssl,os,subprocess,urllib.request\n"
"d=os.path.dirname(os.path.abspath(__file__))\n"
"c,k=os.path.join(d,'cert.pem'),os.path.join(d,'key.pem')\n"
"if not os.path.exists(c) or not os.path.exists(k):\n"
" subprocess.run(['openssl','req','-x509','-newkey','rsa:2048',\n"
"  '-keyout',k,'-out',c,'-days','365','-nodes',\n"
"  '-subj','/CN=launchercontent.mojang.com'],capture_output=True)\n"
"U='https://scotcsduluka.github.io/mcd-news/news.json'\n"
"class H(http.server.BaseHTTPRequestHandler):\n"
" def do_GET(self):\n"
"  try:\n"
"   r=urllib.request.Request(U)\n"
"   r.add_header('User-Agent','MCD-News/1.0')\n"
"   d=urllib.request.urlopen(r,timeout=10).read()\n"
"   self.send_response(200)\n"
"   self.send_header('Content-Type','application/json')\n"
"   self.send_header('Content-Length',str(len(d)))\n"
"   self.send_header('Cache-Control','no-cache')\n"
"   self.end_headers()\n"
"   self.wfile.write(d)\n"
"  except Exception as e:\n"
"   self.send_error(502,str(e))\n"
" def log_message(self,*a):pass\n"
"s=http.server.HTTPServer(('127.0.0.1',443),H)\n"
"ctx=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)\n"
"ctx.load_cert_chain(c,k)\n"
"s.socket=ctx.wrap_socket(s.socket,server_side=True)\n"
"s.serve_forever()\n"
;

// ========== Global State ==========
static HANDLE g_hJob  = NULL;
static HANDLE g_hProc = NULL;
static char   g_szDir[MAX_PATH]  = {0};
static char   g_szPy[MAX_PATH]   = {0};

// ========== Server Thread ==========
static DWORD WINAPI SrvThread(LPVOID) {
    Sleep(1000); // Wait for DLL loading to complete

    // Create Job Object - kills Python server when game process exits
    g_hJob = CreateJobObjectA(NULL, NULL);
    if (g_hJob) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = {0};
        jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(g_hJob, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
    }

    // Create temp directory
    GetTempPathA(MAX_PATH, g_szDir);
    strcat(g_szDir, "mcd_news");
    CreateDirectoryA(g_szDir, NULL);

    // Write server.py to temp dir
    strcpy(g_szPy, g_szDir);
    strcat(g_szPy, "\\srv.py");
    HANDLE hFile = CreateFileA(g_szPy, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD written;
        WriteFile(hFile, SERVER_SCRIPT, (DWORD)strlen(SERVER_SCRIPT), &written, NULL);
        CloseHandle(hFile);
    } else {
        return 1;
    }

    // Start Python server (try pythonw.exe first, then python.exe)
    char cmd[512];
    STARTUPINFOA si = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {0};

    BOOL ok = FALSE;

    sprintf(cmd, "pythonw.exe \"%s\"", g_szPy);
    ok = CreateProcessA(NULL, cmd, NULL, NULL, FALSE,
                        CREATE_NO_WINDOW, NULL, g_szDir, &si, &pi);

    if (!ok) {
        sprintf(cmd, "python.exe \"%s\"", g_szPy);
        ok = CreateProcessA(NULL, cmd, NULL, NULL, FALSE,
                            CREATE_NO_WINDOW, NULL, g_szDir, &si, &pi);
    }

    if (!ok) return 1;

    g_hProc = pi.hProcess;
    CloseHandle(pi.hThread);

    // Assign Python process to Job Object (auto-kill on game exit)
    if (g_hJob) {
        AssignProcessToJobObject(g_hJob, g_hProc);
    }

    return 0;
}

// ========== Cleanup ==========
static void Cleanup() {
    if (g_hProc) {
        TerminateProcess(g_hProc, 0);
        CloseHandle(g_hProc);
        g_hProc = NULL;
    }
    if (g_hJob) {
        CloseHandle(g_hJob);
        g_hJob = NULL;
    }
    if (g_szPy[0]) DeleteFileA(g_szPy);
    char szTemp[MAX_PATH];
    strcpy(szTemp, g_szDir); strcat(szTemp, "\\cert.pem");  DeleteFileA(szTemp);
    strcpy(szTemp, g_szDir); strcat(szTemp, "\\key.pem");   DeleteFileA(szTemp);
    if (g_szDir[0]) RemoveDirectoryA(g_szDir);
}

// ========== DLL Entry Point ==========
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        CreateThread(NULL, 0, SrvThread, NULL, 0, NULL);
    } else if (reason == DLL_PROCESS_DETACH) {
        Cleanup();
    }
    return TRUE;
}
