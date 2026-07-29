#include "partition_migration.h"

#include <Arduino.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

#ifdef ENABLE_PARTITION_MIGRATION
#include <esp_flash.h>
#include <esp_flash_internal.h>
#include <esp_flash_partitions.h>
#include <string.h>

#include "ltc_output.h"
#endif

namespace {

constexpr uint32_t kFlashSize = 0x400000;
constexpr uint32_t kOldAppSize = 0x140000;
constexpr uint32_t kNewAppSize = 0x1f0000;
constexpr uint32_t kApp0Address = 0x10000;
constexpr uint32_t kOldApp1Address = 0x150000;
constexpr uint32_t kNewApp1Address = 0x200000;
constexpr uint32_t kOldSpiffsAddress = 0x290000;
constexpr uint32_t kCoredumpAddress = 0x3f0000;

const esp_partition_t *findPartition(esp_partition_type_t type,
                                     esp_partition_subtype_t subtype,
                                     const char *label = nullptr) {
  return esp_partition_find_first(type, subtype, label);
}

bool partitionMatches(const esp_partition_t *partition, uint32_t address,
                      uint32_t size) {
  return partition != nullptr && partition->address == address &&
         partition->size == size;
}

bool hasExpectedSystemPartitions() {
  return partitionMatches(
             findPartition(ESP_PARTITION_TYPE_DATA,
                           ESP_PARTITION_SUBTYPE_DATA_NVS),
             0x9000, 0x5000) &&
         partitionMatches(
             findPartition(ESP_PARTITION_TYPE_DATA,
                           ESP_PARTITION_SUBTYPE_DATA_OTA),
             0xe000, 0x2000);
}

bool isLargeLayout() {
  return hasExpectedSystemPartitions() &&
         partitionMatches(
             findPartition(ESP_PARTITION_TYPE_APP,
                           ESP_PARTITION_SUBTYPE_APP_OTA_0),
             kApp0Address, kNewAppSize) &&
         partitionMatches(
             findPartition(ESP_PARTITION_TYPE_APP,
                           ESP_PARTITION_SUBTYPE_APP_OTA_1),
             kNewApp1Address, kNewAppSize) &&
         partitionMatches(
             findPartition(ESP_PARTITION_TYPE_DATA,
                           ESP_PARTITION_SUBTYPE_DATA_COREDUMP),
             kCoredumpAddress, 0x10000);
}

bool isCompatibleOldLayout() {
  return hasExpectedSystemPartitions() &&
         partitionMatches(
             findPartition(ESP_PARTITION_TYPE_APP,
                           ESP_PARTITION_SUBTYPE_APP_OTA_0),
             kApp0Address, kOldAppSize) &&
         partitionMatches(
             findPartition(ESP_PARTITION_TYPE_APP,
                           ESP_PARTITION_SUBTYPE_APP_OTA_1),
             kOldApp1Address, kOldAppSize);
}

bool isOldLayout() {
  return isCompatibleOldLayout() &&
         partitionMatches(
             findPartition(ESP_PARTITION_TYPE_DATA,
                           ESP_PARTITION_SUBTYPE_DATA_SPIFFS),
             kOldSpiffsAddress, 0x160000) &&
         partitionMatches(
             findPartition(ESP_PARTITION_TYPE_DATA,
                           ESP_PARTITION_SUBTYPE_DATA_COREDUMP),
             kCoredumpAddress, 0x10000);
}

#ifdef ENABLE_PARTITION_MIGRATION
bool migrationScheduled = false;
unsigned long migrationScheduledAt = 0;

void addEntry(uint8_t *table, size_t index, uint8_t type, uint8_t subtype,
              uint32_t address, uint32_t size, const char *label) {
  esp_partition_info_t entry{};
  entry.magic = ESP_PARTITION_MAGIC;
  entry.type = type;
  entry.subtype = subtype;
  entry.pos.offset = address;
  entry.pos.size = size;
  strncpy(reinterpret_cast<char *>(entry.label), label, sizeof(entry.label));
  memcpy(table + index * sizeof(entry), &entry, sizeof(entry));
}

void buildLargePartitionTable(uint8_t *table) {
  memset(table, 0xff, ESP_PARTITION_TABLE_SIZE);
  addEntry(table, 0, PART_TYPE_DATA, PART_SUBTYPE_DATA_WIFI, 0x9000, 0x5000,
           "nvs");
  addEntry(table, 1, PART_TYPE_DATA, PART_SUBTYPE_DATA_OTA, 0xe000, 0x2000,
           "otadata");
  addEntry(table, 2, PART_TYPE_APP, PART_SUBTYPE_OTA_FLAG, kApp0Address,
           kNewAppSize, "app0");
  addEntry(table, 3, PART_TYPE_APP, PART_SUBTYPE_OTA_FLAG + 1,
           kNewApp1Address, kNewAppSize, "app1");
  addEntry(table, 4, PART_TYPE_DATA, 0x03, kCoredumpAddress, 0x10000,
           "coredump");

  constexpr uint8_t digest[] = {
      0x6e, 0x63, 0xcd, 0x55, 0x79, 0x41, 0xf9, 0xab,
      0x0e, 0x0b, 0xb4, 0x8c, 0x02, 0x20, 0x59, 0x1b,
  };
  const size_t md5EntryOffset = 5 * sizeof(esp_partition_info_t);
  table[md5EntryOffset] = 0xeb;
  table[md5EntryOffset + 1] = 0xeb;
  memcpy(table + md5EntryOffset + ESP_PARTITION_MD5_OFFSET, digest,
         sizeof(digest));
}

bool writeAndVerifyTable(const uint8_t *table) {
  esp_err_t result =
      esp_flash_set_dangerous_write_protection(esp_flash_default_chip, false);
  if (result != ESP_OK) {
    Serial.printf("Partition migration unlock failed: %s\n",
                  esp_err_to_name(result));
    return false;
  }

  result = esp_flash_erase_region(esp_flash_default_chip,
                                  ESP_PARTITION_TABLE_OFFSET,
                                  ESP_PARTITION_TABLE_SIZE);
  if (result == ESP_OK) {
    result = esp_flash_write(esp_flash_default_chip, table,
                             ESP_PARTITION_TABLE_OFFSET,
                             ESP_PARTITION_TABLE_SIZE);
  }

  uint8_t *verification =
      static_cast<uint8_t *>(malloc(ESP_PARTITION_TABLE_SIZE));
  if (result == ESP_OK && verification != nullptr) {
    result = esp_flash_read(esp_flash_default_chip, verification,
                            ESP_PARTITION_TABLE_OFFSET,
                            ESP_PARTITION_TABLE_SIZE);
    if (result == ESP_OK &&
        memcmp(table, verification, ESP_PARTITION_TABLE_SIZE) != 0) {
      result = ESP_ERR_INVALID_CRC;
    }
  } else if (verification == nullptr) {
    result = ESP_ERR_NO_MEM;
  }
  free(verification);

  const esp_err_t protectResult =
      esp_flash_set_dangerous_write_protection(esp_flash_default_chip, true);
  if (protectResult != ESP_OK && result == ESP_OK) {
    result = protectResult;
  }
  if (result != ESP_OK) {
    Serial.printf("Partition migration write failed: %s\n",
                  esp_err_to_name(result));
    return false;
  }
  return true;
}
#endif

}  // namespace

const char *partitionLayoutName() {
  if (isLargeLayout()) {
    return "large-dual-ota";
  }
  if (isOldLayout()) {
    return "default-dual-ota";
  }
  if (isCompatibleOldLayout()) {
    return "compatible-dual-ota";
  }
  return "unknown";
}

const char *partitionRunningLabel() {
  const esp_partition_t *running = esp_ota_get_running_partition();
  return running == nullptr ? "unknown" : running->label;
}

uint32_t partitionRunningAddress() {
  const esp_partition_t *running = esp_ota_get_running_partition();
  return running == nullptr ? 0 : running->address;
}

uint32_t partitionRunningSize() {
  const esp_partition_t *running = esp_ota_get_running_partition();
  return running == nullptr ? 0 : running->size;
}

const char *partitionMigrationPreflight() {
#ifndef ENABLE_PARTITION_MIGRATION
  return "migration support is not enabled";
#else
  uint32_t flashSize = 0;
  if (esp_flash_get_size(esp_flash_default_chip, &flashSize) != ESP_OK ||
      flashSize != kFlashSize) {
    return "flash is not exactly 4 MB";
  }
  if (!isCompatibleOldLayout()) {
    return "current partition table is not a compatible original dual-OTA layout";
  }
  const esp_partition_t *running = esp_ota_get_running_partition();
  if (!partitionMatches(running, kApp0Address, kOldAppSize) ||
      running->subtype != ESP_PARTITION_SUBTYPE_APP_OTA_0) {
    return "migration must run from the original ota_0 partition";
  }

  uint8_t *table = static_cast<uint8_t *>(malloc(ESP_PARTITION_TABLE_SIZE));
  if (table == nullptr) {
    return "unable to allocate partition-table buffer";
  }
  buildLargePartitionTable(table);
  int partitionCount = 0;
  const esp_err_t verifyResult = esp_partition_table_verify(
      reinterpret_cast<const esp_partition_info_t *>(table), true,
      &partitionCount);
  free(table);
  if (verifyResult != ESP_OK || partitionCount != 5) {
    return "embedded partition table failed validation";
  }
  return nullptr;
#endif
}

bool partitionMigrationAvailable() {
  return partitionMigrationPreflight() == nullptr;
}

bool schedulePartitionMigration() {
#ifndef ENABLE_PARTITION_MIGRATION
  return false;
#else
  if (migrationScheduled || partitionMigrationPreflight() != nullptr) {
    return false;
  }
  migrationScheduled = true;
  migrationScheduledAt = millis();
  return true;
#endif
}

void partitionMigrationLoop() {
#ifdef ENABLE_PARTITION_MIGRATION
  if (!migrationScheduled || millis() - migrationScheduledAt < 1500) {
    return;
  }
  migrationScheduled = false;

  const char *preflight = partitionMigrationPreflight();
  if (preflight != nullptr) {
    Serial.printf("Partition migration cancelled: %s\n", preflight);
    return;
  }

  rmtSetOutputEnabled(false);
  delay(100);
  Serial.println("Starting verified partition-table migration");
  Serial.flush();

  uint8_t *table = static_cast<uint8_t *>(malloc(ESP_PARTITION_TABLE_SIZE));
  if (table == nullptr) {
    Serial.println("Partition migration allocation failed");
    return;
  }
  buildLargePartitionTable(table);
  const bool success = writeAndVerifyTable(table);
  free(table);
  if (!success) {
    Serial.println("Partition migration failed; do not power-cycle");
    return;
  }

  Serial.println("Partition migration verified; rebooting");
  Serial.flush();
  delay(250);
  ESP.restart();
#endif
}
