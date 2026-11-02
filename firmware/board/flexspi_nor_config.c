// FlexSPI configuration block the boot ROM reads at 0x60000400: Winbond W25Q32JVSSIQ (U11), 4 MB.
// Single-pad fast read (0x0B, 24-bit address, 8 dummy clocks) at 100 MHz, which works whatever the
// flash's QE bit says. Quad reads (0xEB) need QE = 1; switch once a read of status register 2 on the
// first board confirms it. Read strobe loops back from the DQS pad (GPIO_SD_12, unconnected).
#include "flexspi_nor_config.h"

__attribute__((section(".boot_hdr.conf"), used))
const flexspi_nor_config_t qspiflash_config = {
    .memConfig = {
        .tag = FLEXSPI_CFG_BLK_TAG,
        .version = FLEXSPI_CFG_BLK_VERSION,
        .readSampleClkSrc = kFlexSPIReadSampleClk_LoopbackFromDqsPad,
        .csHoldTime = 3u,
        .csSetupTime = 3u,
        .sflashPadType = kSerialFlash_1Pad,
        .serialClkFreq = kFlexSpiSerialClk_100MHz,
        .sflashA1Size = 4u * 1024u * 1024u,
        .lookupTable = {
            FLEXSPI_LUT_SEQ(CMD_SDR, FLEXSPI_1PAD, 0x0B, RADDR_SDR, FLEXSPI_1PAD, 24),
            FLEXSPI_LUT_SEQ(DUMMY_SDR, FLEXSPI_1PAD, 0x08, READ_SDR, FLEXSPI_1PAD, 0x04),
        },
    },
    .pageSize = 256u,
    .sectorSize = 4u * 1024u,
    .blockSize = 64u * 1024u,
    .isUniformBlockSize = false,
};
