#include "engine.h"
#include "manoonchai.h"

// Callback ดักจับคีย์บอร์ดสำหรับ Manoonchai
static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT *p = (KBDLLHOOKSTRUCT *)lParam;

        // ข้าม Event ที่โปรแกรมส่งเอง เพื่อป้องกัน Infinite Loop
        if (p->dwExtraInfo == INJECTED_KEY_FLAG) {
            return CallNextHookEx(g_hHook, nCode, wParam, lParam);
        }

        bool isKeyDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
        bool ctrlDown  = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
        bool altDown   = (GetAsyncKeyState(VK_MENU)    & 0x8000) != 0;
        bool shiftDown = (GetAsyncKeyState(VK_SHIFT)   & 0x8000) != 0;

        // ปุ่มลัดฉุกเฉิน Ctrl + Alt + Shift + Q
        if (HandleEmergencyExitShortcut(p, ctrlDown, altDown, shiftDown, nCode, wParam, lParam)) {
            return 1;
        }

        // ตรวจสอบภาษาปัจจุบัน ถ้าไม่ใช่ภาษาไทย ให้ปล่อยผ่านทันที
        if (!IsCurrentLayoutThai()) {
            return CallNextHookEx(g_hHook, nCode, wParam, lParam);
        }

        // ตรวจสอบ Right Alt (AltGr) สำหรับโหมดภาษาไทย
        bool rAltDown = (GetAsyncKeyState(VK_RMENU) & 0x8000) != 0;
        bool lCtrlDown = (GetAsyncKeyState(VK_LCONTROL) & 0x8000) != 0;

        if (rAltDown && !lCtrlDown) {
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

        // โหมดภาษาไทย: แปลงเป็น Manoonchai Layout
        bool capsLock = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
        bool isShifted = shiftDown ^ capsLock;
        WCHAR targetChar = GetManoonchaiChar((WORD)p->vkCode, isShifted);

        if (targetChar != 0) {
            if (isKeyDown) {
                SendUnicodeChar(targetChar);
            }
            return 1; // บล็อกคีย์เดิม
        }
    }

    return CallNextHookEx(g_hHook, nCode, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    return RunLayoutEngine(
        hInstance,
        L"Global\\ManoonchaiSwitcherMutex",
        L"ManoonchaiSwitchMonitorClass",
        L"ManoonchaiSwitchMonitor",
        LowLevelKeyboardProc
    );
}
