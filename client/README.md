# Client-related stuff

WORK IN PROGRESS

See .bat files in sample directory for scripts used when the remote WebSocket server gets up and down
- start-browser.bat
  - Starts browser instances to show WebSocket server data and set power scheme to Ultimate Performance to avoid screen(s) turning off while the WebSocket server is running
- shutdown-browser.bat
  - Shut down all browser instances when the WebSocket server is gone. Note that this example kills all chrome.exe instances, if Chromium is used for anything else you'll need to come up with a different solution to kill only the chrome.exe processes responsible to display WebSocket server data!

* Will add sample scripts in a future update.
