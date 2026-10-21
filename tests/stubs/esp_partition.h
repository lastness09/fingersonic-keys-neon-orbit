#pragma once
#include <stdint.h>
#define ESP_PARTITION_TYPE_APP 0
#define ESP_PARTITION_SUBTYPE_APP_OTA_1 0x11
struct esp_partition_t { uint32_t address; uint32_t size; };
const esp_partition_t* esp_partition_find_first(int, int, const char*);
