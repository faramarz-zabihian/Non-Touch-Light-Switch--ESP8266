#include <string.h>
#include <assert.h>
#include <freertos.h>
#include <freertos/task.h>
#include "esp_partition.h"
#include "esp_log.h"

static const char *TAG = "example";


void find_partitions(void)
{
    ESP_LOGI(TAG, "App partitions...");
    esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, NULL);
    for (; it != NULL; it = esp_partition_next(it)) {
        const esp_partition_t *part = esp_partition_get(it);
        ESP_LOGI(TAG, "\tpartition '%s' at offset 0x%x with size 0x%x", part->label, part->address, part->size);
    }
    esp_partition_iterator_release(it);

    ESP_LOGI(TAG, "Data partitions...");
    it = esp_partition_find(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, NULL);
    for (; it != NULL; it = esp_partition_next(it)) {
        const esp_partition_t *part = esp_partition_get(it);
        ESP_LOGI(TAG, "\tfound partition '%s' at offset 0x%x with size 0x%x", part->label, part->address, part->size);
    }
    esp_partition_iterator_release(it);
}