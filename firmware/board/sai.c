// SAI1 transmitter to the AK4493: 32-bit I2S, BCLK = 64fs, BCLK and LRCK generated here from the
// MCLK the active oscillator drives into GPIO_08. eDMA channel 0 loops over a two-half ring and
// interrupts at each half; the ISR refills the half just sent.
#include "board.h"
#include "audio_feed.h"
#include "fsl_device_registers.h"
#include "hal.h"

#define SAI_DMA_CH 0u
#define HALF_FRAMES 96u  // 0.5 ms at 192 kHz, 2.2 ms at 44.1 kHz
#define WORDS (2u * HALF_FRAMES * 2u)

// DTCM is not cached, so the DMA reads what the CPU wrote.
static uint32_t ring[WORDS] __attribute__((aligned(32), section("NonCacheable")));

void hal_sai_stop(void)
{
    DMA0->CERQ = SAI_DMA_CH;
    SAI1->TCSR = 0;  // TE = 0: stops at the end of the frame
    while (SAI1->TCSR & I2S_TCSR_TE_MASK) {
    }
    SAI1->TCSR = I2S_TCSR_FR_MASK;
    DMA0->CINT = SAI_DMA_CH;
    NVIC_ClearPendingIRQ(DMA0_IRQn);
}

void hal_sai_start(uint32_t rate)
{
    uint32_t mclk = rate % 11025u == 0 ? 22579200u : 24576000u;
    uint32_t div = mclk / (64u * rate);  // 8, 4 or 2

    CLOCK_EnableClock(kCLOCK_Sai1);
    CLOCK_EnableClock(kCLOCK_Dma);
    // MCLK1 is the GPIO_08 pad, an input (IOMUXC_GPR GPR1)
    IOMUXC_GPR->GPR1 = (IOMUXC_GPR->GPR1 & ~(IOMUXC_GPR_GPR1_SAI1_MCLK_DIR_MASK | IOMUXC_GPR_GPR1_SAI1_MCLK1_SEL_MASK)) |
                       IOMUXC_GPR_GPR1_SAI1_MCLK1_SEL(3);

    hal_sai_stop();
    SAI1->TCSR = I2S_TCSR_SR_MASK;
    SAI1->TCSR = 0;
    SAI1->TCR1 = I2S_TCR1_TFW(16);
    SAI1->TCR2 = I2S_TCR2_SYNC(0) | I2S_TCR2_MSEL(1) | I2S_TCR2_BCP(1) | I2S_TCR2_BCD(1) | I2S_TCR2_DIV(div / 2u - 1u);
    SAI1->TCR3 = I2S_TCR3_TCE(1);
    SAI1->TCR4 = I2S_TCR4_FRSZ(1) | I2S_TCR4_SYWD(31) | I2S_TCR4_MF(1) | I2S_TCR4_FSE(1) | I2S_TCR4_FSP(1) |
                 I2S_TCR4_FSD(1);
    SAI1->TCR5 = I2S_TCR5_WNW(31) | I2S_TCR5_W0W(31) | I2S_TCR5_FBT(31);
    SAI1->TMR = 0;

    for (uint32_t i = 0; i < WORDS; i++) ring[i] = 0;

    DMAMUX->CHCFG[SAI_DMA_CH] = 0;
    DMAMUX->CHCFG[SAI_DMA_CH] = DMAMUX_CHCFG_SOURCE(kDmaRequestMuxSai1Tx & 0xFFu) | DMAMUX_CHCFG_ENBL(1);
    DMA0->TCD[SAI_DMA_CH].SADDR = (uint32_t)ring;
    DMA0->TCD[SAI_DMA_CH].SOFF = 4;
    DMA0->TCD[SAI_DMA_CH].ATTR = DMA_ATTR_SSIZE(2) | DMA_ATTR_DSIZE(2);
    DMA0->TCD[SAI_DMA_CH].NBYTES_MLNO = 4;
    DMA0->TCD[SAI_DMA_CH].SLAST = -(int32_t)sizeof ring;
    DMA0->TCD[SAI_DMA_CH].DADDR = (uint32_t)&SAI1->TDR[0];
    DMA0->TCD[SAI_DMA_CH].DOFF = 0;
    DMA0->TCD[SAI_DMA_CH].CITER_ELINKNO = WORDS;
    DMA0->TCD[SAI_DMA_CH].BITER_ELINKNO = WORDS;
    DMA0->TCD[SAI_DMA_CH].DLAST_SGA = 0;
    DMA0->TCD[SAI_DMA_CH].CSR = DMA_CSR_INTHALF_MASK | DMA_CSR_INTMAJOR_MASK;
    NVIC_SetPriority(DMA0_IRQn, 2);  // under USB (1), so the FIFO's writer never waits on its reader
    NVIC_EnableIRQ(DMA0_IRQn);
    DMA0->SERQ = SAI_DMA_CH;

    SAI1->TCSR = I2S_TCSR_FRDE_MASK | I2S_TCSR_BCE_MASK | I2S_TCSR_TE_MASK;
}

void DMA0_IRQHandler(void)
{
    DMA0->CINT = SAI_DMA_CH;
    // CITER counts down: past the halfway mark, the first half is free; at wrap, the second
    uint32_t left = DMA0->TCD[SAI_DMA_CH].CITER_ELINKNO & DMA_CITER_ELINKNO_CITER_MASK;
    uint32_t *half = left > WORDS / 2u ? &ring[WORDS / 2u] : &ring[0];
    audio_feed(half, HALF_FRAMES);
    __DSB();
}
