import time
import supervisor
import usb_hid
from adafruit_hid.keyboard import Keyboard
from adafruit_hid.keyboard_layout_us import KeyboardLayoutUS
from adafruit_hid.keycode import Keycode

# 1. รอ USB Handshake กับ Host OS ให้พร้อม
while not supervisor.runtime.usb_connected:
    time.sleep(0.05)

# หน่วงเวลาให้ OS จัดสรร HID Driver เสร็จสมบูรณ์
time.sleep(2.5)

kbd = Keyboard(usb_hid.devices)
layout = KeyboardLayoutUS(kbd)
kbd.release_all()  # ล้างสถานะปุ่มค้างก่อนเริ่มเสมอ

def safe_type(text, delay=0.015):
    """หน่วง 15ms ต่อตัวอักษร พอดีกับรอบ USB Polling 125Hz ไม่ตกหล่น 100%"""
    for char in text:
        layout.write(char)
        time.sleep(delay)

# 2. เปิดหน้าต่าง Run (Win + R)
kbd.press(Keycode.GUI, Keycode.R)
time.sleep(0.08)
kbd.release_all()
time.sleep(1.5)  # ปรับเพิ่มเป็น 1.5s เผื่อเครื่องช้าเปิดหน้าต่าง Run

# 3. ล้างข้อความค้างเก่าใน Run Dialog ป้องกัน Autocomplete เพี้ยน
kbd.press(Keycode.CONTROL, Keycode.A)
time.sleep(0.03)
kbd.release_all()
time.sleep(0.02)
kbd.send(Keycode.BACKSPACE)
time.sleep(0.1)  # รอให้ช่อง Input ล้างข้อความเสร็จสนิท

# 4. คำสั่งรัน run.bat
cmd = 'powershell "68..90|%{$d=[char]$_+\':\\run.bat\';if(test-path $d){&$d}}"'

# 5. พิมพ์และ Enter
safe_type(cmd)
time.sleep(0.2)  # รอให้ข้อความลงช่อง Input ครบถ้วน
kbd.send(Keycode.ENTER)

# กันโปรแกรมจบ
while True:
    time.sleep(3600)
