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
static bool IsDriveStillConnected(void) {
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
static bool CheckDriveDisconnected(void) {
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
static LRESULT CALLBACK MonitorWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
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
static bool IsCurrentLayoutThai(void) {
    HWND foreground = GetForegroundWindow();
    if (!foreground) return false;

    DWORD threadId = GetWindowThreadProcessId(foreground, NULL);
    HKL hkl = GetKeyboardLayout(threadId);

    // Primary Language ID: 0x1E = Thai (LANG_THAI)
    WORD langId = PRIMARYLANGID(LOWORD((DWORD_PTR)hkl));
    return (langId == 0x1E);
}

// จำลองการส่ง Keystroke ใหม่พร้อม Flag กำกับ
static void SendSynthesizedVk(WORD vkCode, bool keyUp) {
    INPUT input = {0};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = vkCode;
    input.ki.wScan = (WORD)MapVirtualKeyW(vkCode, MAPVK_VK_TO_VSC);
    input.ki.dwFlags = keyUp ? KEYEVENTF_KEYUP : 0;
    input.ki.dwExtraInfo = INJECTED_KEY_FLAG;
    SendInput(1, &input, sizeof(INPUT));
}

// QWERTY -> Colemak VK Remapping (Standard Colemak Layout)
static WORD GetColemakVK(WORD vk) {
    switch (vk) {
        case 'E': return 'F';
        case 'R': return 'P';
        case 'T': return 'G';
        case 'Y': return 'J';
        case 'U': return 'L';
        case 'I': return 'U';
        case 'O': return 'Y';
        case 'P': return VK_OEM_1; // Semicolon (;)
        case 'S': return 'R';
        case 'D': return 'S';
        case 'F': return 'T';
        case 'G': return 'D';
        case 'J': return 'N';
        case 'K': return 'E';
        case 'L': return 'I';
        case VK_OEM_1: return 'O'; // Semicolon (;) -> O
        case 'N': return 'K';
        default:  return vk;
    }
}

// Callback ดักจับคีย์บอร์ดสำหรับ Colemak
static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT *p = (KBDLLHOOKSTRUCT *)lParam;

        // ข้าม Event ที่โปรแกรมส่งเอง เพื่อป้องกัน Infinite Loop
        if (p->dwExtraInfo == INJECTED_KEY_FLAG) {
            return CallNextHookEx(g_hHook, nCode, wParam, lParam);
        }

        bool isKeyDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
        bool isKeyUp   = (wParam == WM_KEYUP   || wParam == WM_SYSKEYUP);

        bool ctrlDown  = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
        bool altDown   = (GetAsyncKeyState(VK_MENU)    & 0x8000) != 0;
        bool shiftDown = (GetAsyncKeyState(VK_SHIFT)   & 0x8000) != 0;

        // ปุ่มลัดฉุกเฉินสำหรับปิดโปรแกรม: Ctrl + Alt + Shift + Q
        // ส่งต่อสัญญาณเพื่อให้ instance อื่นๆ ปิดตัวลงพร้อมกันด้วย
        if (ctrlDown && altDown && shiftDown && p->vkCode == 'Q') {
            PostQuitMessage(0);
            return CallNextHookEx(g_hHook, nCode, wParam, lParam);
        }

        // หากเป็นโหมดภาษาไทย ให้ปล่อยผ่าน ไม่ต้องแปลง Colemak
        if (IsCurrentLayoutThai()) {
            return CallNextHookEx(g_hHook, nCode, wParam, lParam);
        }

        // ปล่อยผ่านหากกดร่วมกับ Ctrl หรือ Alt เพื่อให้ Shortcut ของระบบ/แอปทำงานปกติ
        if (ctrlDown || altDown) {
            return CallNextHookEx(g_hHook, nCode, wParam, lParam);
        }

        // โหมดภาษาอังกฤษ: แปลงเป็น Colemak Layout
        WORD targetVk = GetColemakVK((WORD)p->vkCode);
        if (targetVk != p->vkCode) {
            SendSynthesizedVk(targetVk, isKeyUp);
            return 1; // บล็อกคีย์เดิม
        }
    }

    return CallNextHookEx(g_hHook, nCode, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // ตรวจสอบ Path ของไฟล์ Executable และ Drive Letter
    GetModuleFileNameW(NULL, g_exePath, MAX_PATH);
    if (g_exePath[1] == L':' && g_exePath[2] == L'\\') {
        g_driveLetter = towupper(g_exePath[0]);
        g_driveRoot[0] = g_driveLetter;
        g_driveRoot[1] = L':';
        g_driveRoot[2] = L'\\';
        g_driveRoot[3] = L'\0';
    }

    // ป้องกันการเปิดรันซ้ำซ้อนสำหรับ Colemak
    g_hMutex = CreateMutexW(NULL, TRUE, L"Global\\ColemakSwitcherMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(NULL, L"โปรแกรม Colemak Switcher ทำงานอยู่ในระบบแล้ว", L"Info", MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    // สร้าง Hidden Window เพื่อรับ System Broadcast Messages เช่น WM_DEVICECHANGE
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = MonitorWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"ColemakSwitchMonitorClass";
    RegisterClassW(&wc);

    g_hWndMonitor = CreateWindowExW(
        0,
        wc.lpszClassName,
        L"ColemakSwitchMonitor",
        0,
        0, 0, 0, 0,
        NULL, NULL, hInstance, NULL
    );

    if (g_hWndMonitor) {
        // ตั้งเวลา Heartbeat ทุก 1 วินาที เพื่อตรวจเช็กการมีอยู่ของไดรฟ์
        SetTimer(g_hWndMonitor, TIMER_HEARTBEAT_ID, 1000, NULL);
    }

    g_hHook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(NULL), 0);
    if (!g_hHook) {
        MessageBoxW(NULL, L"ไม่สามารถติดตั้ง Keyboard Hook ได้", L"Error", MB_OK | MB_ICONERROR);
        if (g_hWndMonitor) DestroyWindow(g_hWndMonitor);
        CloseHandle(g_hMutex);
        return 1;
    }

    // Windows Message Pump
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
