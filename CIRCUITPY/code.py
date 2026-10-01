import time
import supervisor
import usb_hid

# ==============================================================================
# Pure Native CircuitPython - Zero External Libraries Required
# ไม่ต้องใช้โฟลเดอร์ lib/ หรือ adafruit_hid ใดๆ ทั้งสิ้น (ใช้ Built-in usb_hid เพียวๆ)
# ==============================================================================

# HID Keycodes
MOD_LCTRL = 0x01
MOD_LSHIFT = 0x02
MOD_LALT = 0x04
MOD_LGUI = 0x08  # Windows Key

KEY_ENTER = 0x28
KEY_BACKSPACE = 0x2A
KEY_R = 0x15
KEY_A = 0x04

# Map ตัวอักษร ASCII พื้นฐาน -> (HID_KEY_CODE, REQUIRE_SHIFT)
ASCII_MAP = {
    'a': (0x04, False), 'b': (0x05, False), 'c': (0x06, False), 'd': (0x07, False),
    'e': (0x08, False), 'f': (0x09, False), 'g': (0x0A, False), 'h': (0x0B, False),
    'i': (0x0C, False), 'j': (0x0D, False), 'k': (0x0E, False), 'l': (0x0F, False),
    'm': (0x10, False), 'n': (0x11, False), 'o': (0x12, False), 'p': (0x13, False),
    'q': (0x14, False), 'r': (0x15, False), 's': (0x16, False), 't': (0x17, False),
    'u': (0x18, False), 'v': (0x19, False), 'w': (0x1A, False), 'x': (0x1B, False),
    'y': (0x1C, False), 'z': (0x1D, False),
    'A': (0x04, True),  'B': (0x05, True),  'C': (0x06, True),  'D': (0x07, True),
    'E': (0x08, True),  'F': (0x09, True),  'G': (0x0A, True),  'H': (0x0B, True),
    'I': (0x0C, True),  'J': (0x0D, True),  'K': (0x0E, True),  'L': (0x0F, True),
    'M': (0x10, True),  'N': (0x11, True),  'O': (0x12, True),  'P': (0x13, True),
    'Q': (0x14, True),  'R': (0x15, True),  'S': (0x16, True),  'T': (0x17, True),
    'U': (0x18, True),  'V': (0x19, True),  'W': (0x1A, True),  'X': (0x1B, True),
    'Y': (0x1C, True),  'Z': (0x1D, True),
    '1': (0x1E, False), '!': (0x1E, True),
    '2': (0x1F, False), '@': (0x1F, True),
    '3': (0x20, False), '#': (0x20, True),
    '4': (0x21, False), '$': (0x21, True),
    '5': (0x22, False), '%': (0x22, True),
    '6': (0x23, False), '^': (0x23, True),
    '7': (0x24, False), '&': (0x24, True),
    '8': (0x25, False), '*': (0x25, True),
    '9': (0x26, False), '(': (0x26, True),
    '0': (0x27, False), ')': (0x27, True),
    ' ': (0x2C, False),
    '-': (0x2D, False), '_': (0x2D, True),
    '=': (0x2E, False), '+': (0x2E, True),
    '[': (0x2F, False), '{': (0x2F, True),
    ']': (0x30, False), '}': (0x30, True),
    '\\': (0x31, False), '|': (0x31, True),
    ';': (0x33, False), ':': (0x33, True),
    "'": (0x34, False), '"': (0x34, True),
    '`': (0x35, False), '~': (0x35, True),
    ',': (0x36, False), '<': (0x36, True),
    '.': (0x37, False), '>': (0x37, True),
    '/': (0x38, False), '?': (0x38, True),
}

# 1. รอ USB Handshake
while not supervisor.runtime.usb_connected:
    time.sleep(0.05)

# รอ OS จัดสรร HID Driver ให้พร้อม
time.sleep(2.5)

# ค้นหา Keyboard HID Device
kbd = None
for dev in usb_hid.devices:
    if dev.usage == 0x06 and dev.usage_page == 0x01:
        kbd = dev
        break

if not kbd:
    while True:
        time.sleep(1.0)

report = bytearray(8)


def release_all():
    for i in range(8):
        report[i] = 0
    kbd.send_report(report)


def send_key(modifier, keycode, delay=0.015):
    release_all()
    report[0] = modifier
    report[2] = keycode
    kbd.send_report(report)
    time.sleep(delay)
    release_all()
    time.sleep(0.01)


def type_string(text, delay=0.015):
    for ch in text:
        if ch in ASCII_MAP:
            kc, req_shift = ASCII_MAP[ch]
            mod = MOD_LSHIFT if req_shift else 0
            send_key(mod, kc, delay)
        else:
            time.sleep(delay)


# ล้างสถานะปุ่ม
release_all()
time.sleep(0.5)

# 2. เปิดหน้าต่าง Run (Win + R)
send_key(MOD_LGUI, KEY_R, delay=0.08)
time.sleep(1.5)  # รอหน้าต่าง Run โผล่ขึ้นมา

# 3. ล้างข้อความเก่า (Ctrl + A แล้ว Backspace)
send_key(MOD_LCTRL, KEY_A, delay=0.03)
time.sleep(0.02)
send_key(0, KEY_BACKSPACE, delay=0.03)
time.sleep(0.1)

# 4. พิมพ์คำสั่งรันโปรแกรม Layout Switcher
cmd = 'cmd /c "for %i in (D E F G H I J K L M N O P Q R S T U V W X Y Z) do @if exist %i:\\layout-switch-colemak-manoonchai.exe start %i:\\layout-switch-colemak-manoonchai.exe"'
type_string(cmd, delay=0.012)
time.sleep(0.2)

# 5. กด Enter
send_key(0, KEY_ENTER, delay=0.05)

# วนลูปป้องกันโปรแกรมจบ
while True:
    time.sleep(3600)
