#pragma once
#include "esp_partition.h"
#define ESP_OK 0
int esp_ota_set_boot_partition(const esp_partition_t*);
