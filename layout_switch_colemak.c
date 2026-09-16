#include "engine.h"
#include "colemak.h"

// Callback ดักจับคีย์บอร์ดสำหรับ Colemak
static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT *p = (KBDLLHOOKSTRUCT *)lParam;

        // ข้าม Event ที่โปรแกรมส่งเอง เพื่อป้องกัน Infinite Loop
        if (p->dwExtraInfo == INJECTED_KEY_FLAG) {
            return CallNextHookEx(g_hHook, nCode, wParam, lParam);
        }

        bool isKeyUp   = (wParam == WM_KEYUP   || wParam == WM_SYSKEYUP);
        bool ctrlDown  = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
        bool altDown   = (GetAsyncKeyState(VK_MENU)    & 0x8000) != 0;
        bool shiftDown = (GetAsyncKeyState(VK_SHIFT)   & 0x8000) != 0;

        // ปุ่มลัดฉุกเฉิน Ctrl + Alt + Shift + Q
        if (HandleEmergencyExitShortcut(p, ctrlDown, altDown, shiftDown, nCode, wParam, lParam)) {
            return 1;
        }

        // หากเป็นโหมดภาษาไทย ให้ปล่อยผ่าน ไม่ต้องแปลง Colemak
        if (IsCurrentLayoutThai()) {
            return CallNextHookEx(g_hHook, nCode, wParam, lParam);
        }

        // ปล่อยผ่านหากกดร่วมกับ Ctrl หรือ Alt เพื่อให้ Shortcut ทำงานปกติ
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
    return RunLayoutEngine(
        hInstance,
        L"Global\\ColemakSwitcherMutex",
        L"ColemakSwitchMonitorClass",
        L"ColemakSwitchMonitor",
        LowLevelKeyboardProc
    );
}
