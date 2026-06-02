/*
 * ================================================================
 *  MCD News Interceptor - version.dll Proxy  (v7 - Simple)
 * ================================================================
 *
 * DLL ตัวนี้ทำแค่ 2 อย่าง:
 *   1. Proxy version.dll functions ไปยัง C:\Windows\System32\version.dll
 *   2. Hook DNS: launchercontent.mojang.com -> 127.0.0.1
 *
 * HTTPS server รันแยกผ่าน Python (server.py) แทนการฝังใน DLL
 * ทำให้โค้ด C++ ง่ายขึ้นมาก ไม่มี crypto/Schannel/cert
 *
 * Build (MSVC Developer Command Prompt x64):
 *   cl /nologo /O2 /LD /MT /DNDEBUG /D_WINSOCK_DEPRECATED_NO_WARNINGS ^
 *      /D SECURITY_WIN32 /EHsc /std:c++17 version_proxy.cpp ^
 *      /DEF:version.def ^
 *      /link ws2_32.lib user32.lib psapi.lib ^
 *      /OUT:version.dll
 *
 * Install:
 *   copy version.dll "...\Dungeons\Binaries\Win64\"
 *   copy news.json  "...\Dungeons\Binaries\Win64\"
 *   รัน server.py ก่อนเปิดเกม (ต้องรันเป็น Admin)
 *
 * ================================================================
 */

#ifndef _WIN64
#error "This must be compiled as x64!"
#endif

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <psapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "psapi.lib")

// ═══════════════════════════════════════════════════════════
//  CONFIG
// ═══════════════════════════════════════════════════════════

static const char TARGET_DOMAIN_A[] = "launchercontent.mojang.com";
static const int  SERVER_PORT        = 443;

// ═══════════════════════════════════════════════════════════
//  VERSION.DLL PROXY TYPEDEFS & GLOBALS
// ═══════════════════════════════════════════════════════════

typedef BOOL  (WINAPI *FnGetFileVersionInfoA)(LPCSTR, DWORD, DWORD, LPVOID);
typedef BOOL  (WINAPI *FnGetFileVersionInfoW)(LPCWSTR, DWORD, DWORD, LPVOID);
typedef BOOL  (WINAPI *FnGetFileVersionInfoByHandle)(HANDLE, DWORD, DWORD, LPVOID);
typedef BOOL  (WINAPI *FnGetFileVersionInfoExA)(DWORD, LPCSTR, DWORD, DWORD, LPVOID);
typedef BOOL  (WINAPI *FnGetFileVersionInfoExW)(DWORD, LPCWSTR, DWORD, DWORD, LPVOID);
typedef DWORD (WINAPI *FnGetFileVersionInfoSizeA)(LPCSTR, LPDWORD);
typedef DWORD (WINAPI *FnGetFileVersionInfoSizeW)(LPCWSTR, LPDWORD);
typedef DWORD (WINAPI *FnGetFileVersionInfoSizeExA)(DWORD, LPCSTR, LPDWORD);
typedef DWORD (WINAPI *FnGetFileVersionInfoSizeExW)(DWORD, LPCWSTR, LPDWORD);
typedef DWORD (WINAPI *FnVerFindFileA)(DWORD, LPCSTR, LPCSTR, LPCSTR, LPSTR, PUINT, LPSTR, PUINT);
typedef DWORD (WINAPI *FnVerFindFileW)(DWORD, LPCWSTR, LPCWSTR, LPCWSTR, LPWSTR, PUINT, LPWSTR, PUINT);
typedef DWORD (WINAPI *FnVerInstallFileA)(DWORD, LPCSTR, LPCSTR, LPCSTR, LPCSTR, LPCSTR, LPSTR, PUINT);
typedef DWORD (WINAPI *FnVerInstallFileW)(DWORD, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, LPWSTR, PUINT);
typedef DWORD (WINAPI *FnVerLanguageNameA)(DWORD, LPSTR, DWORD);
typedef DWORD (WINAPI *FnVerLanguageNameW)(DWORD, LPWSTR, DWORD);
typedef BOOL  (WINAPI *FnVerQueryValueA)(const LPVOID, LPCSTR, LPVOID*, PUINT);
typedef BOOL  (WINAPI *FnVerQueryValueW)(const LPVOID, LPCWSTR, LPVOID*, PUINT);

static FnGetFileVersionInfoA          g_GetFileVersionInfoA = NULL;
static FnGetFileVersionInfoW          g_GetFileVersionInfoW = NULL;
static FnGetFileVersionInfoByHandle   g_GetFileVersionInfoByHandle = NULL;
static FnGetFileVersionInfoExA        g_GetFileVersionInfoExA = NULL;
static FnGetFileVersionInfoExW        g_GetFileVersionInfoExW = NULL;
static FnGetFileVersionInfoSizeA      g_GetFileVersionInfoSizeA = NULL;
static FnGetFileVersionInfoSizeW      g_GetFileVersionInfoSizeW = NULL;
static FnGetFileVersionInfoSizeExA    g_GetFileVersionInfoSizeExA = NULL;
static FnGetFileVersionInfoSizeExW    g_GetFileVersionInfoSizeExW = NULL;
static FnVerFindFileA                 g_VerFindFileA = NULL;
static FnVerFindFileW                 g_VerFindFileW = NULL;
static FnVerInstallFileA              g_VerInstallFileA = NULL;
static FnVerInstallFileW              g_VerInstallFileW = NULL;
static FnVerLanguageNameA             g_VerLanguageNameA = NULL;
static FnVerLanguageNameW             g_VerLanguageNameW = NULL;
static FnVerQueryValueA               g_VerQueryValueA = NULL;
static FnVerQueryValueW               g_VerQueryValueW = NULL;

// ═══════════════════════════════════════════════════════════
//  DNS HOOK GLOBALS
// ═══════════════════════════════════════════════════════════

typedef int (WSAAPI *FnGetAddrInfo)(const char*, const char*, const ADDRINFOA*, ADDRINFOA**);
static FnGetAddrInfo g_origGetAddrInfo = NULL;

static HMODULE g_realDll = NULL;
static volatile LONG g_initialized = 0;
static wchar_t g_dllDir[MAX_PATH] = {0};

// ═══════════════════════════════════════════════════════════
//  LOGGING
// ═══════════════════════════════════════════════════════════

static void Log(const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    OutputDebugStringA("[MCD-News] ");
    OutputDebugStringA(buf);
    OutputDebugStringA("\n");
}

// ═══════════════════════════════════════════════════════════
//  DNS HOOK
// ═══════════════════════════════════════════════════════════

static int WSAAPI HookedGetAddrInfo(
    const char* node, const char* service,
    const ADDRINFOA* hints, ADDRINFOA** result)
{
    if (node && strstr(node, TARGET_DOMAIN_A)) {
        Log("DNS INTERCEPT: %s -> 127.0.0.1", node);

        ADDRINFOA* ai = (ADDRINFOA*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(ADDRINFOA));
        sockaddr_in* sa = (sockaddr_in*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(sockaddr_in));

        if (!ai || !sa) {
            if (ai) HeapFree(GetProcessHeap(), 0, ai);
            if (sa) HeapFree(GetProcessHeap(), 0, sa);
            return EAI_MEMORY;
        }

        sa->sin_family = AF_INET;
        sa->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        sa->sin_port = htons(SERVER_PORT);
        ai->ai_family = AF_INET;
        ai->ai_socktype = SOCK_STREAM;
        ai->ai_protocol = IPPROTO_TCP;
        ai->ai_addrlen = sizeof(sockaddr_in);
        ai->ai_addr = (sockaddr*)sa;
        ai->ai_canonname = _strdup(node);
        ai->ai_next = NULL;

        *result = ai;
        return 0;
    }

    return g_origGetAddrInfo(node, service, hints, result);
}

static bool PatchIAT(HMODULE hMod, const char* dll, const char* func, void* hook, void** orig) {
    if (!hMod) return false;
    auto dos = (IMAGE_DOS_HEADER*)hMod;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    auto nt = (IMAGE_NT_HEADERS64*)((BYTE*)hMod + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
    auto& impDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!impDir.VirtualAddress) return false;
    auto desc = (IMAGE_IMPORT_DESCRIPTOR*)((BYTE*)hMod + impDir.VirtualAddress);
    while (desc->Name) {
        const char* name = (const char*)((BYTE*)hMod + desc->Name);
        if (_stricmp(name, dll) == 0) {
            auto thunk = (IMAGE_THUNK_DATA64*)((BYTE*)hMod + desc->FirstThunk);
            auto origThunk = (IMAGE_THUNK_DATA64*)((BYTE*)hMod + desc->OriginalFirstThunk);
            while (thunk->u1.Function) {
                if (!(origThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG64)) {
                    auto ibn = (IMAGE_IMPORT_BY_NAME*)((BYTE*)hMod + origThunk->u1.AddressOfData);
                    if (strcmp(ibn->Name, func) == 0) {
                        DWORD oldProt;
                        VirtualProtect(&thunk->u1.Function, sizeof(ULONG_PTR), PAGE_READWRITE, &oldProt);
                        *orig = (void*)thunk->u1.Function;
                        thunk->u1.Function = (ULONG_PTR)hook;
                        VirtualProtect(&thunk->u1.Function, sizeof(ULONG_PTR), oldProt, &oldProt);
                        Log("Hooked %s!%s", dll, func);
                        return true;
                    }
                }
                thunk++;
                origThunk++;
            }
        }
        desc++;
    }
    return false;
}

static void InstallDnsHook() {
    Log("Installing DNS hook...");

    HMODULE ws2 = GetModuleHandleW(L"ws2_32.dll");
    if (ws2) {
        g_origGetAddrInfo = (FnGetAddrInfo)GetProcAddress(ws2, "getaddrinfo");
    }
    if (!g_origGetAddrInfo) {
        Log("WARNING: Cannot find getaddrinfo");
        return;
    }

    HMODULE hExe = GetModuleHandleW(NULL);
    PatchIAT(hExe, "ws2_32.dll", "getaddrinfo", (void*)HookedGetAddrInfo, (void**)&g_origGetAddrInfo);

    HMODULE hMods[1024];
    DWORD cbNeeded;
    if (EnumProcessModules(GetCurrentProcess(), hMods, sizeof(hMods), &cbNeeded)) {
        DWORD count = cbNeeded / sizeof(HMODULE);
        for (DWORD i = 0; i < count; i++) {
            if (hMods[i] == hExe) continue;
            FnGetAddrInfo dummy = NULL;
            PatchIAT(hMods[i], "ws2_32.dll", "getaddrinfo", (void*)HookedGetAddrInfo, (void**)&dummy);
        }
    }
    Log("DNS hook installed!");
}

// ═══════════════════════════════════════════════════════════
//  WAIT FOR INIT & INIT THREAD
// ═══════════════════════════════════════════════════════════

static bool WaitForInit(DWORD ms = 5000) {
    DWORD start = GetTickCount();
    while (!InterlockedCompareExchange(&g_initialized, 0, 0)) {
        if (GetTickCount() - start > ms) return false;
        Sleep(10);
    }
    return true;
}

static DWORD WINAPI InitThreadProc(LPVOID) {
    // Sleep เล็กน้อยเพื่อให้ DllMain return ก่อน (ป้องกัน loader lock)
    Sleep(200);

    Log("Init thread starting...");

    // Load real version.dll from System32
    wchar_t sysDir[MAX_PATH];
    GetSystemDirectoryW(sysDir, MAX_PATH);
    wcscat_s(sysDir, L"\\version.dll");
    g_realDll = LoadLibraryW(sysDir);
    if (!g_realDll) {
        Log("FATAL: Cannot load real version.dll from %ls", sysDir);
        return 1;
    }

    #define VLOAD(name) g_##name = (Fn##name)GetProcAddress(g_realDll, #name)
    VLOAD(GetFileVersionInfoA);
    VLOAD(GetFileVersionInfoW);
    VLOAD(GetFileVersionInfoByHandle);
    VLOAD(GetFileVersionInfoExA);
    VLOAD(GetFileVersionInfoExW);
    VLOAD(GetFileVersionInfoSizeA);
    VLOAD(GetFileVersionInfoSizeW);
    VLOAD(GetFileVersionInfoSizeExA);
    VLOAD(GetFileVersionInfoSizeExW);
    VLOAD(VerFindFileA);
    VLOAD(VerFindFileW);
    VLOAD(VerInstallFileA);
    VLOAD(VerInstallFileW);
    VLOAD(VerLanguageNameA);
    VLOAD(VerLanguageNameW);
    VLOAD(VerQueryValueA);
    VLOAD(VerQueryValueW);
    #undef VLOAD

    Log("Real version.dll loaded (17 functions)");

    InstallDnsHook();

    InterlockedExchange(&g_initialized, 1);
    Log("Init complete! DNS redirect active.");
    return 0;
}

// ═══════════════════════════════════════════════════════════
//  VERSION.DLL PROXY FUNCTIONS
// ═══════════════════════════════════════════════════════════

extern "C" {

BOOL WINAPI Proxy_GetFileVersionInfoA(LPCSTR a, DWORD b, DWORD c, LPVOID d) {
    if (!g_GetFileVersionInfoA) WaitForInit();
    return g_GetFileVersionInfoA ? g_GetFileVersionInfoA(a, b, c, d) : FALSE;
}
BOOL WINAPI Proxy_GetFileVersionInfoW(LPCWSTR a, DWORD b, DWORD c, LPVOID d) {
    if (!g_GetFileVersionInfoW) WaitForInit();
    return g_GetFileVersionInfoW ? g_GetFileVersionInfoW(a, b, c, d) : FALSE;
}
BOOL WINAPI Proxy_GetFileVersionInfoByHandle(HANDLE a, DWORD b, DWORD c, LPVOID d) {
    if (!g_GetFileVersionInfoByHandle) WaitForInit();
    return g_GetFileVersionInfoByHandle ? g_GetFileVersionInfoByHandle(a, b, c, d) : FALSE;
}
BOOL WINAPI Proxy_GetFileVersionInfoExA(DWORD a, LPCSTR b, DWORD c, DWORD d, LPVOID e) {
    if (!g_GetFileVersionInfoExA) WaitForInit();
    return g_GetFileVersionInfoExA ? g_GetFileVersionInfoExA(a, b, c, d, e) : FALSE;
}
BOOL WINAPI Proxy_GetFileVersionInfoExW(DWORD a, LPCWSTR b, DWORD c, DWORD d, LPVOID e) {
    if (!g_GetFileVersionInfoExW) WaitForInit();
    return g_GetFileVersionInfoExW ? g_GetFileVersionInfoExW(a, b, c, d, e) : FALSE;
}
DWORD WINAPI Proxy_GetFileVersionInfoSizeA(LPCSTR a, LPDWORD b) {
    if (!g_GetFileVersionInfoSizeA) WaitForInit();
    return g_GetFileVersionInfoSizeA ? g_GetFileVersionInfoSizeA(a, b) : 0;
}
DWORD WINAPI Proxy_GetFileVersionInfoSizeW(LPCWSTR a, LPDWORD b) {
    if (!g_GetFileVersionInfoSizeW) WaitForInit();
    return g_GetFileVersionInfoSizeW ? g_GetFileVersionInfoSizeW(a, b) : 0;
}
DWORD WINAPI Proxy_GetFileVersionInfoSizeExA(DWORD a, LPCSTR b, LPDWORD c) {
    if (!g_GetFileVersionInfoSizeExA) WaitForInit();
    return g_GetFileVersionInfoSizeExA ? g_GetFileVersionInfoSizeExA(a, b, c) : 0;
}
DWORD WINAPI Proxy_GetFileVersionInfoSizeExW(DWORD a, LPCWSTR b, LPDWORD c) {
    if (!g_GetFileVersionInfoSizeExW) WaitForInit();
    return g_GetFileVersionInfoSizeExW ? g_GetFileVersionInfoSizeExW(a, b, c) : 0;
}
DWORD WINAPI Proxy_VerFindFileA(DWORD a, LPCSTR b, LPCSTR c, LPCSTR d, LPSTR e, PUINT f, LPSTR g, PUINT h) {
    if (!g_VerFindFileA) WaitForInit();
    return g_VerFindFileA ? g_VerFindFileA(a, b, c, d, e, f, g, h) : 0;
}
DWORD WINAPI Proxy_VerFindFileW(DWORD a, LPCWSTR b, LPCWSTR c, LPCWSTR d, LPWSTR e, PUINT f, LPWSTR g, PUINT h) {
    if (!g_VerFindFileW) WaitForInit();
    return g_VerFindFileW ? g_VerFindFileW(a, b, c, d, e, f, g, h) : 0;
}
DWORD WINAPI Proxy_VerInstallFileA(DWORD a, LPCSTR b, LPCSTR c, LPCSTR d, LPCSTR e, LPCSTR f, LPSTR g, PUINT h) {
    if (!g_VerInstallFileA) WaitForInit();
    return g_VerInstallFileA ? g_VerInstallFileA(a, b, c, d, e, f, g, h) : 0;
}
DWORD WINAPI Proxy_VerInstallFileW(DWORD a, LPCWSTR b, LPCWSTR c, LPCWSTR d, LPCWSTR e, LPCWSTR f, LPSTR g, PUINT h) {
    if (!g_VerInstallFileW) WaitForInit();
    return g_VerInstallFileW ? g_VerInstallFileW(a, b, c, d, e, f, g, h) : 0;
}
DWORD WINAPI Proxy_VerLanguageNameA(DWORD a, LPSTR b, DWORD c) {
    if (!g_VerLanguageNameA) WaitForInit();
    return g_VerLanguageNameA ? g_VerLanguageNameA(a, b, c) : 0;
}
DWORD WINAPI Proxy_VerLanguageNameW(DWORD a, LPWSTR b, DWORD c) {
    if (!g_VerLanguageNameW) WaitForInit();
    return g_VerLanguageNameW ? g_VerLanguageNameW(a, b, c) : 0;
}
BOOL WINAPI Proxy_VerQueryValueA(const LPVOID a, LPCSTR b, LPVOID* c, PUINT d) {
    if (!g_VerQueryValueA) WaitForInit();
    return g_VerQueryValueA ? g_VerQueryValueA(a, b, c, d) : FALSE;
}
BOOL WINAPI Proxy_VerQueryValueW(const LPVOID a, LPCWSTR b, LPVOID* c, PUINT d) {
    if (!g_VerQueryValueW) WaitForInit();
    return g_VerQueryValueW ? g_VerQueryValueW(a, b, c, d) : FALSE;
}

} // extern "C"

// ═══════════════════════════════════════════════════════════
//  DLL ENTRY POINT
// ═══════════════════════════════════════════════════════════

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        GetModuleFileNameW(hModule, g_dllDir, MAX_PATH);
        wchar_t* slash = wcsrchr(g_dllDir, L'\\');
        if (slash) *(slash + 1) = L'\0';

        // สร้าง thread แยกเพื่อ init (หลีกเลี่ยง loader lock)
        HANDLE hInit = CreateThread(NULL, 0, InitThreadProc, NULL, 0, NULL);
        if (hInit) CloseHandle(hInit);
    }
    else if (reason == DLL_PROCESS_DETACH) {
        if (g_realDll) FreeLibrary(g_realDll);
    }
    return TRUE;
}
