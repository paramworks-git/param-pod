// AK4493SEQ register driver (datasheet 211100143-E-02, register map p. 87).
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define AK4493_ADDR 0x10  // CAD1 = CAD0 = 0

// PCM filters, Table 25
typedef enum {
    AK_FILTER_SHARP,
    AK_FILTER_SLOW,
    AK_FILTER_SHORT_DELAY_SHARP,  // chip default
    AK_FILTER_SHORT_DELAY_SLOW,
    AK_FILTER_SUPER_SLOW,
    AK_FILTER_LOW_DISPERSION,
    AK_FILTER_COUNT
} ak_filter_t;

// After PDN goes high: registers for 32-bit I2S, soft-muted, still in reset (RSTN = 0).
// False if the chip doesn't answer.
bool ak4493_init(ak_filter_t filter);
bool ak4493_rate(uint32_t rate);   // speed mode (DFS) for this rate; only while RSTN = 0
bool ak4493_run(bool run);         // RSTN: release or hold the digital block
bool ak4493_mute(bool mute);       // SMUTE: soft mute over the transition time
bool ak4493_filter(ak_filter_t filter);
bool ak4493_attenuation(uint8_t att);  // both channels: 0xFF = 0 dB, 0.5 dB steps, 0 = mute

// Time for a soft mute to finish at this rate, with the filter's group delay, in ms.
uint32_t ak4493_mute_ms(uint32_t rate);
