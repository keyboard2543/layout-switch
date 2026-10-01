import storage
import usb_cdc
import usb_hid

# 1. ปิด USB Mass Storage Drive (ไม่ให้คอมเห็นเป็นแฟลชไดรฟ์ ลดโหลดและ Antivirus scan)
storage.disable_usb_drive()

# 2. ปิด Serial Console (COM Port) เพื่อให้เหลือเป็น HID Keyboard เพียวๆ
usb_cdc.disable()

# 3. เปิดเฉพาะ USB HID (Keyboard/Mouse) เท่านั้น
usb_hid.enable((usb_hid.Device.KEYBOARD,))
