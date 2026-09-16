#ifndef MANOONCHAI_H
#define MANOONCHAI_H

#include <windows.h>
#include <stdbool.h>

// QWERTY -> ผังมนูญชัย (Manoonchai Layout - Official kiimo)
static inline WCHAR GetManoonchaiChar(WORD vk, bool shift) {
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
static inline WCHAR GetManoonchaiAltGrChar(WORD vk) {
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

#endif // MANOONCHAI_H
