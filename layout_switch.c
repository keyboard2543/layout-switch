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

// ส่งตัวอักษร Unicode UTF-16
static void SendUnicodeChar(WCHAR ch) {
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

// QWERTY -> ผังมนูญชัย (Manoonchai Layout - Official kiimo)
static WCHAR GetManoonchaiChar(WORD vk, bool shift) {
    if (!shift) {
        switch (vk) {
            // Number row
            case '1': return L'1';
            case '2': return L'2';
            case '3': return L'3';
            case '4': return L'4';
            case '5': return L'5';
            case '6': return L'6';
            case '7': return L'7';
            case '8': return L'8';
            case '9': return L'9';
            case '0': return L'0';
            case VK_OEM_3: return L'`';
            case VK_OEM_MINUS: return L'-';
            case VK_OEM_PLUS:  return L'=';

            // Top row
            case 'Q': return 0x0E43; // ใ
            case 'W': return 0x0E15; // ต
            case 'E': return 0x0E2B; // ห
            case 'R': return 0x0E25; // ล
            case 'T': return 0x0E2A; // ส
            case 'Y': return 0x0E1B; // ป
            case 'U': return 0x0E31; // ั
            case 'I': return 0x0E01; // ก
            case 'O': return 0x0E34; // ิ
            case 'P': return 0x0E1A; // บ
            case VK_OEM_4: return 0x0E47; // ็
            case VK_OEM_6: return 0x0E2C; // ฬ
            case VK_OEM_5: return 0x0E2F; // ฯ

            // Home row
            case 'A': return 0x0E07; // ง
            case 'S': return 0x0E40; // เ
            case 'D': return 0x0E23; // ร
            case 'F': return 0x0E19; // น
            case 'G': return 0x0E21; // ม
            case 'H': return 0x0E2D; // อ
            case 'J': return 0x0E32; // า
            case 'K': return 0x0E48; // ่
            case 'L': return 0x0E49; // ้
            case VK_OEM_1: return 0x0E27; // ว
            case VK_OEM_7: return 0x0E37; // ื

            // Bottom row
            case 'Z': return 0x0E38; // ุ
            case 'X': return 0x0E44; // ไ
            case 'C': return 0x0E17; // ท
            case 'V': return 0x0E22; // ย
            case 'B': return 0x0E08; // จ
            case 'N': return 0x0E04; // ค
            case 'M': return 0x0E35; // ี
            case VK_OEM_COMMA:  return 0x0E14; // ด
            case VK_OEM_PERIOD: return 0x0E30; // ะ
            case VK_OEM_2:      return 0x0E39; // ู
            default: return 0;
        }
    } else {
        switch (vk) {
            // Number row (Shift)
            case '1': return L'!';
            case '2': return L'@';
            case '3': return L'#';
            case '4': return L'$';
            case '5': return L'%';
            case '6': return L'^';
            case '7': return L'&';
            case '8': return L'*';
            case '9': return L'(';
            case '0': return L')';
            case VK_OEM_3: return L'~';
            case VK_OEM_MINUS: return L'_';
            case VK_OEM_PLUS:  return L'+';

            // Top row (Shift)
            case 'Q': return 0x0E12; // ฒ
            case 'W': return 0x0E0F; // ฏ
            case 'E': return 0x0E0B; // ซ
            case 'R': return 0x0E0D; // ญ
            case 'T': return 0x0E1F; // ฟ
            case 'Y': return 0x0E09; // ฉ
            case 'U': return 0x0E36; // ึ
            case 'I': return 0x0E18; // ธ
            case 'O': return 0x0E10; // ฐ
            case 'P': return 0x0E0E; // ฎ
            case VK_OEM_4: return 0x0E06; // ฆ
            case VK_OEM_6: return 0x0E11; // ฑ
            case VK_OEM_5: return 0x0E0C; // ฌ

            // Home row (Shift)
            case 'A': return 0x0E29; // ษ
            case 'S': return 0x0E16; // ถ
            case 'D': return 0x0E41; // แ
            case 'F': return 0x0E0A; // ช
            case 'G': return 0x0E1E; // พ
            case 'H': return 0x0E1C; // ผ
            case 'J': return 0x0E33; // ำ
            case 'K': return 0x0E02; // ข
            case 'L': return 0x0E42; // โ
            case VK_OEM_1: return 0x0E20; // ภ
            case VK_OEM_7: return L'"';

            // Bottom row (Shift)
            case 'Z': return 0x0E24; // ฤ
            case 'X': return 0x0E1D; // ฝ
            case 'C': return 0x0E46; // ๆ
            case 'V': return 0x0E13; // ณ
            case 'B': return 0x0E4A; // ๊
            case 'N': return 0x0E4B; // ๋
            case 'M': return 0x0E4C; // ์
            case VK_OEM_COMMA:  return 0x0E28; // ศ
            case VK_OEM_PERIOD: return 0x0E2E; // ฮ
            case VK_OEM_2:      return L'?';
            default: return 0;
        }
    }
}

// Manoonchai AltGr (Right Alt) Layer สำหรับเลขไทยและสัญลักษณ์พิเศษ
static WCHAR GetManoonchaiAltGrChar(WORD vk) {
    switch (vk) {
        // เลขไทย (Thai Digits)
        case '0': return 0x0E50; // ๐
        case '1': return 0x0E51; // ๑
        case '2': return 0x0E52; // ๒
        case '3': return 0x0E53; // ๓
        case '4': return 0x0E54; // ๔
        case '5': return 0x0E55; // ๕
        case '6': return 0x0E56; // ๖
        case '7': return 0x0E57; // ๗
        case '8': return 0x0E58; // ๘
        case '9': return 0x0E59; // ๙

        // สัญลักษณ์คณิตศาสตร์และเครื่องหมายพิเศษ
        case VK_OEM_MINUS: return 0x00F7; // ÷
        case VK_OEM_PLUS:  return 0x00D7; // ×
        case 'U': return 0x0E3A; // ฺ (พินทุ)
        case 'A': return 0x25CC; // ◌
        case 'S': return 0x0E4F; // ๏ (ฟองมัน)
        case 'D': return 0x0E5B; // ๛ (โคมูตร)
        case 'F': return 0x0E3F; // ฿ (บาท)
        case 'H': return 0x0E4D; // ํ (นิคหิต)
        case 'J': return 0x0E45; // ๅ (ลากข้างยาว)
        case 'K': return 0x0E03; // ฃ (ขอขวด)
        case 'Z': return 0x0E26; // ฦ (ลือ)
        case 'C': return 0x0E5A; // ๚ (อังคั่นคู่)
        case 'N': return 0x0E05; // ฅ (คอคน)
        case 'M': return 0x0E4E; // ๎ (ยามักการ)
        default: return 0;
    }
}

// Callback ดักจับคีย์บอร์ด
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
        if (ctrlDown && altDown && shiftDown && p->vkCode == 'Q') {
            PostQuitMessage(0);
            return 1;
        }

        bool isThai = IsCurrentLayoutThai();

        // ตรวจสอบ Right Alt (AltGr) สำหรับโหมดภาษาไทย
        bool rAltDown = (GetAsyncKeyState(VK_RMENU) & 0x8000) != 0;
        bool lCtrlDown = (GetAsyncKeyState(VK_LCONTROL) & 0x8000) != 0;

        if (isThai && rAltDown && !lCtrlDown) {
            WCHAR altGrChar = GetManoonchaiAltGrChar((WORD)p->vkCode);
            if (altGrChar != 0) {
                if (isKeyDown) {
                    SendUnicodeChar(altGrChar);
                }
                return 1; // บล็อกคีย์เดิม
            }
        }

        // ปล่อยผ่านหากกดร่วมกับ Ctrl หรือ Alt เพื่อให้ Shortcut ทำงานปกติ
        if (ctrlDown || altDown) {
            return CallNextHookEx(g_hHook, nCode, wParam, lParam);
        }

        if (isThai) {
            // โหมดภาษาไทย: Manoonchai Layout
            bool capsLock = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
            bool isShifted = shiftDown ^ capsLock;
            WCHAR targetChar = GetManoonchaiChar((WORD)p->vkCode, isShifted);

            if (targetChar != 0) {
                if (isKeyDown) {
                    SendUnicodeChar(targetChar);
                }
                return 1; // บล็อกคีย์เดิม
            }
        } else {
            // โหมดภาษาอังกฤษ: Colemak Layout
            WORD targetVk = GetColemakVK((WORD)p->vkCode);
            if (targetVk != p->vkCode) {
                SendSynthesizedVk(targetVk, isKeyUp);
                return 1; // บล็อกคีย์เดิม
            }
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

    // ป้องกันการเปิดรันซ้ำซ้อน
    g_hMutex = CreateMutexW(NULL, TRUE, L"Global\\ManoonchaiColemakSwitcherMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(NULL, L"โปรแกรมทำงานอยู่ในระบบแล้ว", L"Info", MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    // สร้าง Hidden Window เพื่อรับ System Broadcast Messages เช่น WM_DEVICECHANGE
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = MonitorWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"LayoutSwitchMonitorClass";
    RegisterClassW(&wc);

    g_hWndMonitor = CreateWindowExW(
        0,
        wc.lpszClassName,
        L"LayoutSwitchMonitor",
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