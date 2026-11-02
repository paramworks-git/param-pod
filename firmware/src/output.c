#include "output.h"
#include "hal.h"

// LT3042 soft start: SET cap 4.7 uF x 47.5k = 223 ms, and fast start-up is off with PGFB tied to
// V5P4, so VREFH needs 5 tau to settle within 1 %.
#define T_ANALOG_MS 1200u
// AK4493 VCML/VCMR (10 uF each) charge after PDN goes high; no time in the datasheet, so generous.
#define T_VCM_MS 500u
#define T_PG_TIMEOUT_MS 50u    // LM27762 soft start is about 1 ms
#define T_PG_STABLE_MS 5u
#define T_AMP_SETTLE_MS 20u    // OPA1622 enable, output DC settling
#define T_OSC_START_MS 5u      // SSW022579E3CH / OT3225 start-up, 5 ms max
#define T_GAIN_SETTLE_MS 5u
#define T_AMP_OFF_MS 10u

static struct {
    bool on, playing, gain_hi;
    uint32_t rate;
    uint8_t att;
    ak_filter_t filter;
} want;

static struct {
    output_state_t state;
    output_fault_t fault;
    uint32_t t, t_pdn, t_pg;
    uint32_t settle_ms;  // wait in OUTPUT_GAIN_SETTLE
    uint32_t rate;      // clocked rate
    bool gain_hi;
    bool unmuted;
    uint8_t att;
    ak_filter_t filter;
} cur;

static bool family44(uint32_t rate) { return rate % 11025u == 0; }

bool output_rate_supported(uint32_t rate)
{
    // BCLK = 64fs, divided from MCLK = 22.5792 / 24.576 MHz by (DIV + 1) x 2. The RT1011's SAI has no
    // divider bypass (FSL_FEATURE_SAI_HAS_BCLK_BYPASS = 0), so BCLK <= MCLK / 2: 176.4 / 192 kHz at most.
    switch (rate) {
    case 44100: case 48000: case 88200: case 96000: case 176400: case 192000:
        return true;
    default:
        return false;
    }
}

static void go(output_state_t s)
{
    cur.state = s;
    cur.t = hal_ms();
}

static uint32_t since(uint32_t t) { return hal_ms() - t; }

static void oscillator(uint32_t rate)
{
    // Both outputs drive the MCLK net (through 22R each): never on together.
    hal_set(OUT_OSC44_EN, false);
    hal_set(OUT_OSC48_EN, false);
    if (rate) hal_set(family44(rate) ? OUT_OSC44_EN : OUT_OSC48_EN, true);
}

static void all_off(void)
{
    hal_set(OUT_AMP_EN_D, false);
    hal_set(OUT_AMP_EN, false);
    hal_sai_stop();
    oscillator(0);  // AK4493: no clocks while PDN is low (Figure 61 note 9)
    hal_set(OUT_DAC_PDN, false);
    hal_set(OUT_AN_EN, false);
    hal_set(OUT_GAIN_HI, false);
    cur.rate = 0;
    cur.gain_hi = false;
    cur.unmuted = false;
}

static void fail(output_fault_t f)
{
    all_off();
    cur.fault = f;
    go(OUTPUT_FAULT);
}

static void mute(void)
{
    ak4493_mute(true);
    cur.unmuted = false;
}

static bool start_clocks(uint32_t rate)
{
    hal_sai_start(rate);
    if (!ak4493_rate(rate) || !ak4493_run(true)) return false;
    cur.rate = rate;
    return true;
}

void output_init(void)
{
    want.on = false;
    want.playing = false;
    want.gain_hi = false;
    want.rate = 44100;
    want.filter = AK_FILTER_SHORT_DELAY_SHARP;
    want.att = 0xFF;
    cur.fault = OUTPUT_FAULT_NONE;
    all_off();
    go(OUTPUT_OFF);
}

void output_step(void)
{
    switch (cur.state) {
    case OUTPUT_OFF:
        if (want.on) {
            hal_set(OUT_AN_EN, true);
            go(OUTPUT_ANALOG_UP);
        }
        break;

    case OUTPUT_ANALOG_UP:
        if (!want.on) {
            all_off();
            go(OUTPUT_OFF);
        } else if (since(cur.t) >= T_ANALOG_MS) {
            hal_set(OUT_DAC_PDN, true);
            cur.t_pdn = hal_ms();
            cur.filter = want.filter;
            cur.att = 0xFF;  // ak4493_init's level
            if (!ak4493_init(cur.filter)) {
                fail(OUTPUT_FAULT_DAC_I2C);
                break;
            }
            oscillator(want.rate);
            if (!start_clocks(want.rate)) {
                fail(OUTPUT_FAULT_DAC_I2C);
                break;
            }
            hal_set(OUT_AMP_EN, true);
            go(OUTPUT_AMP_RAILS);
        }
        break;

    case OUTPUT_AMP_RAILS:
        if (hal_get(IN_AMP_PG)) {
            cur.t_pg = hal_ms();
            go(OUTPUT_AMP_SETTLE);
        } else if (since(cur.t) > T_PG_TIMEOUT_MS) {
            fail(OUTPUT_FAULT_AMP_PG);
        }
        break;

    case OUTPUT_AMP_SETTLE:
        if (!hal_get(IN_AMP_PG)) {
            fail(OUTPUT_FAULT_AMP_PG);
        } else if (since(cur.t_pdn) >= T_VCM_MS && since(cur.t_pg) >= T_PG_STABLE_MS &&
                   since(cur.t) >= T_PG_STABLE_MS) {
            hal_set(OUT_GAIN_HI, want.gain_hi);
            cur.gain_hi = want.gain_hi;
            hal_set(OUT_AMP_EN_D, true);
            cur.settle_ms = T_AMP_SETTLE_MS;  // drivers settle before anything unmutes
            go(OUTPUT_GAIN_SETTLE);
        }
        break;

    case OUTPUT_READY:
        if (!hal_get(IN_AMP_PG)) {
            fail(OUTPUT_FAULT_AMP_LOST);
        } else if (!want.on) {
            mute();
            go(OUTPUT_DOWN_MUTE);
        } else if (want.rate != cur.rate) {
            mute();
            go(OUTPUT_RECLOCK);
        } else if (want.gain_hi != cur.gain_hi) {
            mute();
            go(OUTPUT_GAIN);
        } else if (want.filter != cur.filter) {
            // Filter bits change on the fly, but not under a playing signal
            if (cur.unmuted) {
                mute();
                go(OUTPUT_GAIN);  // reuse the muted wait; gain is unchanged
            } else {
                cur.filter = want.filter;
                ak4493_filter(cur.filter);
            }
        } else if (want.att != cur.att) {
            ak4493_attenuation(want.att);  // the DAC ramps between levels itself
            cur.att = want.att;
        } else if (want.playing != cur.unmuted) {
            ak4493_mute(!want.playing);
            cur.unmuted = want.playing;
        }
        break;

    case OUTPUT_RECLOCK:
        if (since(cur.t) >= ak4493_mute_ms(cur.rate)) {
            ak4493_run(false);
            hal_sai_stop();
            if (family44(want.rate) != family44(cur.rate)) {
                oscillator(want.rate);
                go(OUTPUT_OSC_START);
            } else if (start_clocks(want.rate)) {
                go(OUTPUT_READY);
            } else {
                fail(OUTPUT_FAULT_DAC_I2C);
            }
        }
        break;

    case OUTPUT_OSC_START:
        if (since(cur.t) >= T_OSC_START_MS) {
            if (start_clocks(want.rate)) go(OUTPUT_READY);
            else fail(OUTPUT_FAULT_DAC_I2C);
        }
        break;

    case OUTPUT_GAIN:
        if (since(cur.t) >= ak4493_mute_ms(cur.rate)) {
            hal_set(OUT_GAIN_HI, want.gain_hi);
            cur.gain_hi = want.gain_hi;
            if (want.filter != cur.filter) {
                cur.filter = want.filter;
                ak4493_filter(cur.filter);
            }
            cur.settle_ms = T_GAIN_SETTLE_MS;
            go(OUTPUT_GAIN_SETTLE);
        }
        break;

    case OUTPUT_GAIN_SETTLE:
        if (since(cur.t) >= cur.settle_ms) go(OUTPUT_READY);
        break;

    case OUTPUT_DOWN_MUTE:
        if (since(cur.t) >= ak4493_mute_ms(cur.rate)) {
            hal_set(OUT_AMP_EN_D, false);
            go(OUTPUT_DOWN_AMP);
        }
        break;

    case OUTPUT_DOWN_AMP:
        if (since(cur.t) >= T_AMP_OFF_MS) {
            all_off();
            go(OUTPUT_OFF);
        }
        break;

    case OUTPUT_FAULT:
        if (!want.on) {
            cur.fault = OUTPUT_FAULT_NONE;
            go(OUTPUT_OFF);
        }
        break;
    }
}

void output_want_on(bool on) { want.on = on; }
void output_want_rate(uint32_t rate)
{
    if (output_rate_supported(rate)) want.rate = rate;
}
void output_want_gain_hi(bool hi) { want.gain_hi = hi; }
void output_want_playing(bool playing) { want.playing = playing; }
void output_set_filter(ak_filter_t filter)
{
    if (filter < AK_FILTER_COUNT) want.filter = filter;
}

void output_want_attenuation(uint8_t att) { want.att = att; }

output_state_t output_state(void) { return cur.state; }
output_fault_t output_fault(void) { return cur.fault; }
uint32_t output_rate(void) { return cur.rate; }
