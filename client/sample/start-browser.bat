@echo off
setlocal

rem set ultimate power scheme to avoid screen turning off
power -u

cd "C:\Program Files\Chromium\Application"

rem remove chrome data to avoid restoring old tabs - the browser used to display sensor data don't need state
rmdir c:\pi\chrome-data /s /q

rem start chrome instances

rem for instances that don't run in kiosk mode, start the window a bit off screen to not show the window chrome (i.e. title bar, browser navigation buttons etc.)
start chrome.exe --disable-resizable-window --hide-crash-restore-bubble --restore-last-session --incognito --new-window --window-position=2880,-100 --window-size=960,1200 --app "http://localhost:30000/index.html?screen=4" --user-data-dir=C:\pi\chrome-data\s4

rem for full screen/kiosk browser instances, just need to set the starting position that matches the monitor offset positions
rem can start as many instances as needed - 2 extra monitors were used in this example
start chrome.exe --kiosk --incognito --new-window --window-position=3840,0 --app "http://localhost:30000/index.html?screen=3" --user-data-dir=C:\pi\chrome-data\s3
start chrome.exe --kiosk --incognito --new-window --window-position=4920,0 --app "http://localhost:30000/index.html?screen=1" --user-data-dir=C:\pi\chrome-data\s1
