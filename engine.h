#ifndef ENGINE_H
#define ENGINE_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbt.h>
#include <stdbool.h>
#include <stdio.h>
#include <wctype.h>

#define INJECTED_KEY_FLAG 0xBEEF
#define TIMER_HEARTBEAT_ID 1001

#ifndef DBT_DEVICEQUERYREMOVE
#define DBT_DEVICEQUERYREMOVE 0x8001
#endif
#ifndef DBT_DEVICEREMOVEPENDING
#define DBT_DEVICEREMOVEPENDING 0x8003
#endif
#ifndef DBT_DEVICEREMOVECOMPLETE
#define DBT_DEVICEREMOVECOMPLETE 0x8004
#endif
#ifndef DBT_DEVTYP_VOLUME
#define DBT_DEVTYP_VOLUME 0x00000002
#endif

static HHOOK g_hHook = NULL;
static HANDLE g_hMutex = NULL;
static HWND g_hWndMonitor = NULL;
static WCHAR g_exePath[MAX_PATH] = {0};
static WCHAR g_driveRoot[4] = {0};
static WCHAR g_driveLetter = 0;

// ตรวจสอบว่าไดรฟ์และไฟล์โปรแกรมยังคงเชื่อมต่ออยู่หรือไม่
static inline bool IsDriveStillConnected(void) {
    if (g_driveLetter < L'A' || g_driveLetter > L'Z') {
        return true;
    }

    UINT driveType = GetDriveTypeW(g_driveRoot);
    if (driveType == DRIVE_NO_ROOT_DIR || driveType == DRIVE_UNKNOWN) {
        return false;
    }

    DWORD attr = GetFileAttributesW(g_exePath);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND ||
            err == ERROR_PATH_NOT_FOUND ||
            err == ERROR_BAD_NET_NAME ||
            err == ERROR_DEV_NOT_EXIST ||
            err == ERROR_DEVICE_NOT_CONNECTED ||
            err == ERROR_NOT_READY) {
            return false;
        }
    }

    return true;
}

// ตรวจสอบซ้ำอีกครั้งหลังรอ 50ms เพื่อป้องกัน False-positive จาก Transient I/O glitch
static inline bool CheckDriveDisconnected(void) {
    if (g_driveLetter < L'A' || g_driveLetter > L'Z') {
        return false;
    }
    if (IsDriveStillConnected()) {
        return false;
    }
    Sleep(50);
    return !IsDriveStillConnected();
}

// Window Procedure สำหรับ Hidden Window ที่คอยตรวจจับ Hardware Change / Timer
static inline LRESULT CALLBACK MonitorWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_DEVICECHANGE: {
            if (wParam == DBT_DEVICEQUERYREMOVE ||
                wParam == DBT_DEVICEREMOVECOMPLETE ||
                wParam == DBT_DEVICEREMOVEPENDING) {

                bool shouldExit = false;

                if (lParam != 0) {
                    PDEV_BROADCAST_HDR pHdr = (PDEV_BROADCAST_HDR)lParam;
                    if (pHdr->dbch_devicetype == DBT_DEVTYP_VOLUME) {
                        PDEV_BROADCAST_VOLUME pVol = (PDEV_BROADCAST_VOLUME)pHdr;
                        if (g_driveLetter >= L'A' && g_driveLetter <= L'Z') {
                            DWORD mask = 1 << (g_driveLetter - L'A');
                            if (pVol->dbcv_unitmask & mask) {
                                shouldExit = true;
                            }
                        }
                    }
                }

                if (!shouldExit && CheckDriveDisconnected()) {
                    shouldExit = true;
                }

                if (shouldExit) {
                    PostQuitMessage(0);
                    return TRUE;
                }
            }
            break;
        }

        case WM_TIMER: {
            if (wParam == TIMER_HEARTBEAT_ID) {
                if (CheckDriveDisconnected()) {
                    PostQuitMessage(0);
                }
            }
            break;
        }

        case WM_DESTROY: {
            KillTimer(hwnd, TIMER_HEARTBEAT_ID);
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

// ตรวจสอบภาษาปัจจุบันของ Active Window
static inline bool IsCurrentLayoutThai(void) {
    HWND foreground = GetForegroundWindow();
    if (!foreground) return false;

    DWORD threadId = GetWindowThreadProcessId(foreground, NULL);
    HKL hkl = GetKeyboardLayout(threadId);

    // Primary Language ID: 0x1E = Thai (LANG_THAI)
    WORD langId = PRIMARYLANGID(LOWORD((DWORD_PTR)hkl));
    return (langId == 0x1E);
}

// จำลองการส่ง Keystroke ใหม่พร้อม Flag กำกับ
static inline void SendSynthesizedVk(WORD vkCode, bool keyUp) {
    INPUT input = {0};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = vkCode;
    input.ki.wScan = (WORD)MapVirtualKeyW(vkCode, MAPVK_VK_TO_VSC);
    input.ki.dwFlags = keyUp ? KEYEVENTF_KEYUP : 0;
    input.ki.dwExtraInfo = INJECTED_KEY_FLAG;
    SendInput(1, &input, sizeof(INPUT));
}

// ส่งตัวอักษร Unicode UTF-16
static inline void SendUnicodeChar(WCHAR ch) {
    INPUT input[2] = {0};

    input[0].type = INPUT_KEYBOARD;
    input[0].ki.wScan = ch;
    input[0].ki.dwFlags = KEYEVENTF_UNICODE;
    input[0].ki.dwExtraInfo = INJECTED_KEY_FLAG;

    input[1].type = INPUT_KEYBOARD;
    input[1].ki.wScan = ch;
    input[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
    input[1].ki.dwExtraInfo = INJECTED_KEY_FLAG;

    SendInput(2, input, sizeof(INPUT));
}

// ปุ่มลัดฉุกเฉินสำหรับปิดโปรแกรม: Ctrl + Alt + Shift + Q
// ส่งต่อสัญญาณลง hook chain เพื่อให้โปรแกรมอื่นๆ ที่รันอยู่ปิดตัวพร้อมกัน
static inline bool HandleEmergencyExitShortcut(KBDLLHOOKSTRUCT *p, bool ctrlDown, bool altDown, bool shiftDown, int nCode, WPARAM wParam, LPARAM lParam) {
    if (ctrlDown && altDown && shiftDown && p->vkCode == 'Q') {
        PostQuitMessage(0);
        CallNextHookEx(g_hHook, nCode, wParam, lParam);
        return true;
    }
    return false;
}

// รัน Background Engine ครบวงจร (ตรวจจับ USB, Hidden Window, Mutex, Keyboard Hook, Message Pump)
static inline int RunLayoutEngine(HINSTANCE hInstance, const WCHAR *mutexName, const WCHAR *className, const WCHAR *windowTitle, HOOKPROC hookProc) {
    GetModuleFileNameW(NULL, g_exePath, MAX_PATH);
    if (g_exePath[1] == L':' && g_exePath[2] == L'\\') {
        g_driveLetter = towupper(g_exePath[0]);
        g_driveRoot[0] = g_driveLetter;
        g_driveRoot[1] = L':';
        g_driveRoot[2] = L'\\';
        g_driveRoot[3] = L'\0';
    }

    g_hMutex = CreateMutexW(NULL, TRUE, mutexName);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(NULL, L"โปรแกรมทำงานอยู่ในระบบแล้ว", L"Info", MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    WNDCLASSW wc = {0};
    wc.lpfnWndProc = MonitorWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = className;
    RegisterClassW(&wc);

    g_hWndMonitor = CreateWindowExW(
        0,
        wc.lpszClassName,
        windowTitle,
        0,
        0, 0, 0, 0,
        NULL, NULL, hInstance, NULL
    );

    if (g_hWndMonitor) {
        SetTimer(g_hWndMonitor, TIMER_HEARTBEAT_ID, 1000, NULL);
    }

    g_hHook = SetWindowsHookExW(WH_KEYBOARD_LL, hookProc, GetModuleHandle(NULL), 0);
    if (!g_hHook) {
        MessageBoxW(NULL, L"ไม่สามารถติดตั้ง Keyboard Hook ได้", L"Error", MB_OK | MB_ICONERROR);
        if (g_hWndMonitor) DestroyWindow(g_hWndMonitor);
        CloseHandle(g_hMutex);
        return 1;
    }

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (g_hHook) {
        UnhookWindowsHookEx(g_hHook);
        g_hHook = NULL;
    }
    if (g_hWndMonitor) {
        DestroyWindow(g_hWndMonitor);
        g_hWndMonitor = NULL;
    }
    if (g_hMutex) {
        CloseHandle(g_hMutex);
        g_hMutex = NULL;
    }

    return 0;
}

#endif // ENGINE_H
