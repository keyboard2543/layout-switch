import time
import usb_hid
from adafruit_hid.keyboard import Keyboard
from adafruit_hid.keyboard_layout_us import KeyboardLayoutUS
from adafruit_hid.keycode import Keycode

# 1. รอให้ USB เชื่อมต่อสมบูรณ์
time.sleep(1.5)

kbd = Keyboard(usb_hid.devices)
layout = KeyboardLayoutUS(kbd)

def safe_type(text, delay=0.005):
    """พิมพ์ทีละตัวแบบมี micro-delay ป้องกัน Windows buffer ล้น"""
    for char in text:
        layout.write(char)
        time.sleep(delay)

# 2. เปิดหน้าต่าง Run (Win + R) แบบปล่อยปุ่มชัวร์ 100%
kbd.press(Keycode.GUI, Keycode.R)
time.sleep(0.08)
kbd.release_all()
time.sleep(0.35)  # รอให้หน้าต่าง Run โฟกัสเสร็จ

# 3. ล้างข้อความค้างเก่าใน Run Dialog ป้องกัน Autocomplete เพี้ยน
kbd.press(Keycode.CONTROL, Keycode.A)
time.sleep(0.02)
kbd.release_all()
kbd.send(Keycode.BACKSPACE)
time.sleep(0.05)

# 4. คำสั่งรัน run.bat (สั้นและปลอดภัย)
cmd = 'powershell "68..90|%{$d=[char]$_+\':\\run.bat\';if(test-path $d){&$d}}"'

# 5. พิมพ์และ Enter
safe_type(cmd)
time.sleep(0.08)
kbd.send(Keycode.ENTER)

# กันโปรแกรมจบ
while True:
    time.sleep(3600)
