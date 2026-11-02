// Board interface for the logic in src/: the target implements it in board/board.c, the host tests
// in test/fake_board.c, so the sequencing can be checked without hardware.
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    OUT_BOOST_EN,  // TPS61023 5.25 V boost (V5P4): USB PHY, DAC LDOs, charge pump
    OUT_AN_EN,     // TPS7A2050 (VDDL/R) and LT3042 (VREFH)
    OUT_DAC_PDN,   // AK4493 power-down, low = off
    OUT_OSC44_EN,  // 22.5792 MHz MCLK (44.1 kHz family)
    OUT_OSC48_EN,  // 24.576 MHz MCLK (48 kHz family)
    OUT_AMP_EN,    // LM27762 +4.44 / -3.90 V rails
    OUT_AMP_EN_D,  // OPA1622 enables
    OUT_GAIN_HI,   // TS5A22362: low = -12 dB (IEMs)
    OUT_LED,
    OUT_COUNT
} hal_out_t;

typedef enum {
    IN_AMP_PG,   // LM27762 power good, high = rails up
    IN_PD_INT,   // TPS25750 interrupt, active low
    IN_GG_ALRT,  // MAX17055 alert, active low
    IN_BTN_UP,   // buttons, active low
    IN_BTN_SEL,
    IN_BTN_DN,
    IN_BTN_PWR,  // power button (BQ25792 QON), active low: 1 s wakes from ship mode, 10 s power-cycles
    IN_JACK44,   // 4.4 mm insert switch (which level means inserted: set at bring-up)
    IN_COUNT
} hal_in_t;

void hal_set(hal_out_t pin, bool level);
bool hal_get(hal_in_t pin);
uint32_t hal_ms(void);

// 7-bit address, register-addressed transfers on LPI2C1. False on NAK or bus error.
bool hal_i2c_write(uint8_t addr, uint8_t reg, const uint8_t *data, size_t len);
bool hal_i2c_read(uint8_t addr, uint8_t reg, uint8_t *data, size_t len);

// SAI1 transmitter: start bit and word clocks from MCLK for this rate, or stop them.
void hal_sai_start(uint32_t rate);
void hal_sai_stop(void);
