@echo off
@echo reference https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/index.html
@echo ..................................................
@SETLOCAL
set NVS_PARTGEN_PATH=%IDF_PATH%\components\nvs_flash\nvs_partition_generator
set ESPTOOL_PATH=%IDF_PATH%/components/esptool_py/esptool
python %NVS_PARTGEN_PATH%\nvs_partition_gen.py encrypt^
 fac_set.csv^
 encrypted_nvs.bin 0x3000^
 --keygen --keyfile nvs_keys.bin

@echo ======================
python %ESPTOOL_PATH%/esptool.py^
  --chip esp8266 --baud 115200 --port COM5^
  --before default_reset^
  --after soft_reset^
  write_flash^
  0x1f0000 encrypted_nvs.bin^
  0x1f3000 keys/nvs_keys.bin