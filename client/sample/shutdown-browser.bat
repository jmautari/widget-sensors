@echo off
setlocal

tasklist /fi "ImageName eq chrome.exe" /fo csv 2>NUL | find /I "chrome.exe">NUL
if "%ERRORLEVEL%"=="0" (
  rem set balanced power scheme to allow screen turning off
  power -b
  rem kill all running chrome instances
  taskkill /f /im chrome.exe
) else (
  echo Chrome not running
)
