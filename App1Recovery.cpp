// SPDX-License-Identifier: MIT
#include "App1Recovery.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"

bool enterApp1() {
  const esp_partition_t* app1 = esp_partition_find_first(
      ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, nullptr);
  if (!app1 || app1->address != 0x210000 || app1->size != 0x100000) return false;
  if (esp_ota_set_boot_partition(app1) != ESP_OK) return false;
  esp_restart();
  return true;
}
