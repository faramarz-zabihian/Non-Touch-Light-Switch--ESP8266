@echo off
cls
@@python %IDF_PATH%\tools\idf_monitor.py -p COM5 -b 115200 cmake-build-debug-xten\hello-world.elf
