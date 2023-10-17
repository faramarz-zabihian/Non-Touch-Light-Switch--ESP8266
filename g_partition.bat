@echo usage g_partition partitions
@echo rem https://docs.espressif.com/projects/esp8266-rtos-sdk/en/latest/api-guides/partition-tables.html
@echo --------------------------------
@ python D:\AI\DEV_ESP8266\msys32\home\asus\ESP8266_RTOS_SDK\components\partition_table\gen_esp32part.py --verify %1.csv %1.bin