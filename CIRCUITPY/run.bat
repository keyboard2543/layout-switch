@echo off
:: 1. ล้างประวัติคำสั่งล่าสุดในกล่อง Win + R
reg delete "HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\RunMRU" /va /f >nul 2>&1


:: 2. เปิดโปรแกรม Layout Switcher
if exist "%~dp0layout-switch-colemak-manoonchai.exe" (
    start "" "%~dp0layout-switch-colemak-manoonchai.exe"
)

:: 3. ปิดหน้าต่าง CMD ทันที ไม่เปิดค้างไว้
exit
