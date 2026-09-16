#ifndef COLEMAK_H
#define COLEMAK_H

#include <windows.h>

// QWERTY -> Colemak VK Remapping (Standard Colemak Layout)
static inline WORD GetColemakVK(WORD vk) {
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

#endif // COLEMAK_H
