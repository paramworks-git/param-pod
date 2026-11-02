// The analog output path: DAC supplies, MCLK oscillators, AK4493, amp rails, OPA1622s, gain switch.
// Callers say what they want; output_step() gets there in an order that doesn't pop:
//
//   up:    AN_EN -> (VREFH settles) -> PDN high -> MCLK, SAI, DAC registers -> AMP_EN -> AMP_PG
//          -> (VCM settled) -> AMP_EN_D -> unmute when audio flows
//   rate:  mute -> RSTN 0 -> SAI off -> (other oscillator) -> SAI on -> DFS -> RSTN 1
//   gain:  mute -> GAIN_HI -> settle
//   down:  mute -> AMP_EN_D off -> AMP_EN off -> clocks off -> PDN low -> AN_EN off
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "ak4493.h"

typedef enum {
    OUTPUT_OFF,
    OUTPUT_ANALOG_UP,
    OUTPUT_AMP_RAILS,
    OUTPUT_AMP_SETTLE,
    OUTPUT_READY,       // drivers on; muted unless playing
    OUTPUT_RECLOCK,
    OUTPUT_OSC_START,
    OUTPUT_GAIN,
    OUTPUT_GAIN_SETTLE,
    OUTPUT_DOWN_MUTE,
    OUTPUT_DOWN_AMP,
    OUTPUT_FAULT,
} output_state_t;

typedef enum {
    OUTPUT_FAULT_NONE,
    OUTPUT_FAULT_DAC_I2C,   // AK4493 didn't answer
    OUTPUT_FAULT_AMP_PG,    // LM27762 power good never came
    OUTPUT_FAULT_AMP_LOST,  // power good dropped while running
} output_fault_t;

void output_init(void);
void output_step(void);

void output_want_on(bool on);        // power the path (stream open, or kept warm)
void output_want_rate(uint32_t rate);
void output_want_gain_hi(bool hi);
void output_want_playing(bool playing);  // audio is flowing: unmute
void output_set_filter(ak_filter_t filter);
void output_want_attenuation(uint8_t att);  // AK4493 ATT: 0xFF = 0 dB (bit-perfect), 0 = mute

bool output_rate_supported(uint32_t rate);
output_state_t output_state(void);
output_fault_t output_fault(void);
uint32_t output_rate(void);  // rate the DAC and SAI are clocked for, 0 when off
