#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdbool.h>
#include <stdio.h>

#define INJECTED_KEY_FLAG 0xBEEF

static HHOOK g_hHook = NULL;

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

// QWERTY -> Colemak VK Remapping
static WORD GetColemakVK(WORD vk) {
    switch (vk) {
        case 'E': return 'K';
        case 'R': return 'P';
        case 'T': return 'G';
        case 'Y': return 'J';
        case 'U': return 'L';
        case 'I': return 'U';
        case 'O': return 'Y';
        case 'P': return VK_OEM_1; // Semicolon
        case 'S': return 'R';
        case 'D': return 'S';
        case 'F': return 'T';
        case 'G': return 'D';
        case 'J': return 'N';
        case 'K': return 'E';
        case 'L': return 'I';
        case VK_OEM_1: return 'O'; // Semicolon -> O
        case 'N': return 'K';
        default:  return vk;
    }
}

// QWERTY -> ผังมนูญชัย (Manoonchai)
static WCHAR GetManoonchaiChar(WORD vk, bool shift) {
    if (!shift) {
        switch (vk) {
            // Number row
            case VK_OEM_3: return L'`';
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
            case VK_OEM_MINUS: return L'-';
            case VK_OEM_PLUS:  return L'=';

            // Top row
            case 'Q': return 0x0E43; // ใ
            case 'W': return 0x0E15; // ต
            case 'E': return 0x0E22; // ย
            case 'R': return 0x0E2D; // อ
            case 'T': return 0x0E23; // ร
            case 'Y': return 0x0E48; // ่ (ไม้เอก)
            case 'U': return 0x0E14; // ด
            case 'I': return 0x0E21; // ม
            case 'O': return 0x0E27; // ว
            case 'P': return 0x0E41; // แ
            case VK_OEM_4: return 0x0E1C; // ผ
            case VK_OEM_6: return 0x0E0A; // ช
            case VK_OEM_5: return L'\\';

            // Home row
            case 'A': return 0x0E07; // ง
            case 'S': return 0x0E40; // เ
            case 'D': return 0x0E4 lawsuit; // า
            case 'D': return 0x0E32; // า
            case 'F': return 0x0E30; // ะ
            case 'G': return 0x0E34; // ิ
            case 'H': return 0x0E49; // ้ (ไม้โท)
            case 'J': return 0x0E01; // ก
            case 'K': return 0x0E19; // น
            case 'L': return 0x0E47; // ็ (ไม้ไต่คู้)
            case VK_OEM_1: return 0x0E44; // ไ
            case VK_OEM_7: return 0x0E02; // ข

            // Bottom row
            case 'Z': return 0x0E1A; // บ
            case 'X': return 0x0E1B; // ป
            case 'C': return 0x0E25; // ล
            case 'V': return 0x0E2ProgressBar; // ห
            case 'V': return 0x0E2B; // ห
            case 'B': return 0x0E38; // ุ
            case 'N': return 0x0E35; // ี
            case 'M': return 0x0E08; // จ
            case VK_OEM_COMMA:  return 0x0E04; // ค
            case VK_OEM_PERIOD: return 0x0E2E; // ฮ
            case VK_OEM_2:      return 0x0E37; // ื
            default: return 0;
        }
    } else {
        switch (vk) {
            // Number row (Shift)
            case VK_OEM_3: return L'~';
            case '1': return L'!';
            case '2': return L'@';
            case '3': return L'#';
            case '4': return L'$';
            case '5': return L'%';
            case '6': return L'^';
            case '7': return 0x0E3F; // ฿
            case '8': return L'*';
            case '9': return 0x0E51; // ๑
            case '0': return 0x0E52; // ๒
            case VK_OEM_MINUS: return L'_';
            case VK_OEM_PLUS:  return L'+';

            // Top row (Shift)
            case 'Q': return 0x0E53; // ๓
            case 'W': return 0x0E54; // ๔
            case 'E': return 0x0E39; // ู
            case 'R': return 0x0E4C; // ์ (การันต์)
            case 'T': return 0x0E36; // ึ
            case 'Y': return 0x0E18; // ธ
            case 'U': return 0x0E16; // ถ
            case 'I': return 0x0E10; // ฐ
            case 'O': return 0x0E09; // ฉ
            case 'P': return 0x0E20; // ภ
            case VK_OEM_4: return 0x0E4B; // ๋ (ไม้จัตวา)
            case VK_OEM_6: return 0x0E13; // ณ
            case VK_OEM_5: return L'|';

            // Home row (Shift)
            case 'A': return 0x0E1F; // ฟ
            case 'S': return 0x0E48; // ่
            case 'D': return 0x0E28; // ศ
            case 'F': return 0x0E2A; // ส
            case 'G': return 0x0E4A; // ๊ (ไม้ตรี)
            case 'H': return 0x0E26; // ฤ
            case 'J': return 0x0E0D; // ญ
            case 'K': return 0x0E1E; // พ
            case 'L': return 0x0E1D; // ฝ
            case VK_OEM_1: return 0x0E0B; // ซ
            case VK_OEM_7: return 0x0E42; // โ

            // Bottom row (Shift)
            case 'Z': return 0x0E1C; // ผ
            case 'X': return 0x0E12; // ฒ
            case 'C': return 0x0E06; // ฆ
            case 'V': return 0x0E46; // ๆ
            case 'B': return 0x0E2C; // ฬ
            case 'N': return 0x0E33; // ำ
            case 'M': return 0x0E0A; // ช
            case VK_OEM_COMMA:  return 0x0E24; // ฤ
            case VK_OEM_PERIOD: return 0x0E4D; // ํ (นิคหิต)
            case VK_OEM_2:      return 0x0E0F; // ฏ
            default: return 0;
        }
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

        // ปล่อยผ่านหากกดร่วมกับ Ctrl หรือ Alt เพื่อให้ Shortcut ทำงานปกติ
        bool ctrlDown = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
        bool altDown  = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;

        if (ctrlDown || altDown) {
            return CallNextHookEx(g_hHook, nCode, wParam, lParam);
        }

        bool isThai = IsCurrentLayoutThai();

        if (isThai) {
            // โหมดภาษาไทย: Manoonchai Layout
            bool shiftDown = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
            WCHAR targetChar = GetManoonchaiChar((WORD)p->vkCode, shiftDown);

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
    // ป้องกันการเปิดรันซ้ำซ้อน
    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"Global\\ManoonchaiColemakSwitcherMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(NULL, L"โปรแกรมทำงานอยู่ในระบบแล้ว", L"Info", MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    g_hHook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(NULL), 0);
    if (!g_hHook) {
        MessageBoxW(NULL, L"ไม่สามารถติดตั้ง Keyboard Hook ได้", L"Error", MB_OK | MB_ICONERROR);
        CloseHandle(hMutex);
        return 1;
    }

    // Windows Message Pump
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    UnhookWindowsHookEx(g_hHook);
    CloseHandle(hMutex);
    return 0;
}