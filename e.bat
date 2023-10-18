@echo off
cls
set TOOL_PATH=D:\DEV_ESP8266\msys32\home\asus\ESP8266_RTOS_SDK\components\esptool_py\esptool
@@python %TOOL_PATH%\esptool.py -p COM5 erase_flash
