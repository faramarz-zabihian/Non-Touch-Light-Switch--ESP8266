@echo off
cls 
rem qio mode for nodemcu and dio for ESP-12S AI THINKER
@rem python %TOOL_PY% --port COM5 erase_flash
@rem 
python %IDF_PATH%\components\esptool_py\esptool\esptool.py -b 921600 --port COM5 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 40m --flash_size detect^
 0 cmake-build-debug-xten/bootloader/bootloader.bin^
 0x8000 cmake-build-debug-xten/partition_table/partition-table.bin^
 0x10000 cmake-build-debug-xten/hello-world.bin
@python %IDF_PATH%\tools\idf_monitor.py -p COM5 -b 115200 cmake-build-debug-xten\hello-world.elf