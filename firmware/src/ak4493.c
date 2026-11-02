#include "ak4493.h"
#include "hal.h"

enum {
    REG_CTRL1 = 0x00,  // ACKS EXDF ECS 0 DIF2-0 RSTN
    REG_CTRL2 = 0x01,  // DZFE DZFM SD DFS1-0 DEM1-0 SMUTE
    REG_CTRL3 = 0x02,  // DP ADP DCKS DCKB MONO DZFB SELLR SLOW
    REG_LATT = 0x03,
    REG_RATT = 0x04,
    REG_CTRL4 = 0x05,  // INVL INVR 0 0 0 0 DFS2 SSLOW
    REG_CTRL5 = 0x07,  // MSTBN 0 0 0 GC2-0 SYNCE
    REG_CTRL7 = 0x0B,  // ATS1-0 0 SDS0 0 0 0 TEST
};

#define CTRL1_DIF_I2S32 (7u << 1)  // mode 7: 32-bit I2S, BICK 64fs
#define CTRL1_RSTN (1u << 0)
#define CTRL2_SD (1u << 5)
#define CTRL2_DFS10_MASK (3u << 3)
#define CTRL2_DEM_OFF (1u << 1)
#define CTRL2_SMUTE (1u << 0)
#define CTRL3_SLOW (1u << 0)
#define CTRL4_DFS2 (1u << 1)
#define CTRL4_SSLOW (1u << 0)
#define CTRL7_ATS_510 (2u << 6)  // 0 dB to mute in 510/fs

// Shadows of the registers this driver changes; the chip resets them when PDN goes low.
static uint8_t ctrl1, ctrl2, ctrl3, ctrl4;

static bool wr(uint8_t reg, uint8_t val) { return hal_i2c_write(AK4493_ADDR, reg, &val, 1); }

static void filter_bits(ak_filter_t f, uint8_t *c2, uint8_t *c3, uint8_t *c4)
{
    static const uint8_t sd[] = {0, 0, 1, 1, 0, 1}, slow[] = {0, 1, 0, 1, 0, 0}, sslow[] = {0, 0, 0, 0, 1, 1};
    if (f >= AK_FILTER_COUNT) f = AK_FILTER_SHORT_DELAY_SHARP;
    *c2 = (uint8_t)((*c2 & ~CTRL2_SD) | (sd[f] ? CTRL2_SD : 0));
    *c3 = (uint8_t)((*c3 & ~CTRL3_SLOW) | (slow[f] ? CTRL3_SLOW : 0));
    *c4 = (uint8_t)((*c4 & ~CTRL4_SSLOW) | (sslow[f] ? CTRL4_SSLOW : 0));
}

bool ak4493_init(ak_filter_t filter)
{
    // Manual speed setting (ACKS = 0): auto mode lists 256fs under both Normal and Double speed
    // (Table 14), which is 88.2/96 kHz here. Manual mode takes every rate the SAI can clock here (44.1 to
    // 192 kHz) at the fixed 22.5792/24.576 MHz MCLK (Tables 12 and 13).
    ctrl1 = CTRL1_DIF_I2S32;
    ctrl2 = CTRL2_DEM_OFF | CTRL2_SMUTE;
    ctrl3 = 0;
    ctrl4 = 0;
    filter_bits(filter, &ctrl2, &ctrl3, &ctrl4);
    return wr(REG_CTRL1, ctrl1) && wr(REG_CTRL2, ctrl2) && wr(REG_CTRL3, ctrl3) && wr(REG_CTRL4, ctrl4) &&
           wr(REG_CTRL7, CTRL7_ATS_510) && wr(REG_LATT, 0xFF) && wr(REG_RATT, 0xFF);
}

bool ak4493_run(bool run)
{
    ctrl1 = (uint8_t)((ctrl1 & ~CTRL1_RSTN) | (run ? CTRL1_RSTN : 0));
    return wr(REG_CTRL1, ctrl1);
}

bool ak4493_rate(uint32_t rate)
{
    // DFS2-0, Table 11
    uint8_t dfs = rate <= 54000 ? 0 : rate <= 108000 ? 1 : rate <= 216000 ? 2 : rate <= 388000 ? 4 : 5;
    ctrl2 = (uint8_t)((ctrl2 & ~CTRL2_DFS10_MASK) | ((dfs & 3u) << 3));
    ctrl4 = (uint8_t)((ctrl4 & ~CTRL4_DFS2) | ((dfs & 4u) ? CTRL4_DFS2 : 0));
    return wr(REG_CTRL2, ctrl2) && wr(REG_CTRL4, ctrl4);
}

bool ak4493_mute(bool mute)
{
    ctrl2 = (uint8_t)((ctrl2 & ~CTRL2_SMUTE) | (mute ? CTRL2_SMUTE : 0));
    return wr(REG_CTRL2, ctrl2);
}

bool ak4493_filter(ak_filter_t filter)
{
    filter_bits(filter, &ctrl2, &ctrl3, &ctrl4);
    return wr(REG_CTRL2, ctrl2) && wr(REG_CTRL3, ctrl3) && wr(REG_CTRL4, ctrl4);
}

bool ak4493_attenuation(uint8_t att) { return wr(REG_LATT, att) && wr(REG_RATT, att); }

uint32_t ak4493_mute_ms(uint32_t rate)
{
    if (rate == 0) rate = 44100;
    // 510/fs transition (ATS mode 2) plus up to about 40/fs of filter group delay, rounded up
    return (550u * 1000u + rate - 1) / rate + 1;
}
