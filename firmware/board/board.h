#pragma once
#include <stdint.h>
#include "clock_config.h"
#include "fsl_common.h"

// fsl_flexspi_nor_boot.h sizes the boot image from this
#define BOARD_FLASH_SIZE (4u * 1024u * 1024u)

void board_init(void);
void board_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
