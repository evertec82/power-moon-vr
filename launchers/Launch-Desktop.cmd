@echo off
setlocal
cd /d "%~dp0"
set "ODYSSEY_VR=0"
set "ODYSSEY_PREPARE=0"
set "ODYSSEY_HIDDEN="
set "ODYSSEY_NO_USER_INPUT="
set "ODYSSEY_INPUT="
set "ODYSSEY_SHOT_AT="
set "ODYSSEY_SCREENSHOT_EVERY="
set "ODYSSEY_EXIT_AFTER_SHOT="
start "" /D "%~dp0" "%~dp0PowerMoonRecomp.exe"
endlocal
