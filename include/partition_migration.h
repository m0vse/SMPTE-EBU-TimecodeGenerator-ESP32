#pragma once

#include <stdint.h>

const char *partitionLayoutName();
const char *partitionRunningLabel();
uint32_t partitionRunningAddress();
uint32_t partitionRunningSize();
bool partitionMigrationAvailable();
const char *partitionMigrationPreflight();
bool schedulePartitionMigration();
void partitionMigrationLoop();
