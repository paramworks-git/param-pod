#include "board.h"
#include "hal.h"
#include "output.h"
#include "tusb.h"
#include "usb_audio.h"

// A cold start takes about 1.8 s (VREFH and VCM settling), which would eat the start of the first
// track. So the analog path warms up as soon as a host enumerates the pod, stays up (muted) between
// streams, and powers down once the host goes away or suspends the bus for this long. The pod has
// its own cell, so this costs the phone nothing.
#define IDLE_OFF_MS 60000u

static void policy(void)
{
    static uint32_t last_active;
    uint32_t now = hal_ms();
    bool streaming = usb_audio_streaming();
    if (streaming || (tud_mounted() && !tud_suspended())) last_active = now;
    output_want_on(last_active && now - last_active < IDLE_OFF_MS);
    output_want_rate(usb_audio_rate());
    output_want_playing(streaming);
    output_want_attenuation(usb_audio_attenuation());
}

static void led(void)
{
    // On while the drivers run, slow blink while powering up, fast blink on a fault
    uint32_t t = hal_ms();
    switch (output_state()) {
    case OUTPUT_OFF: hal_set(OUT_LED, false); break;
    case OUTPUT_FAULT: hal_set(OUT_LED, (t / 100u) & 1u); break;
    case OUTPUT_ANALOG_UP:
    case OUTPUT_AMP_RAILS:
    case OUTPUT_AMP_SETTLE: hal_set(OUT_LED, (t / 500u) & 1u); break;
    default: hal_set(OUT_LED, true); break;
    }
}

int main(void)
{
    board_init();
    output_init();
    tusb_rhport_init_t dev = {.role = TUSB_ROLE_DEVICE, .speed = TUSB_SPEED_HIGH};
    tusb_init(BOARD_TUD_RHPORT, &dev);
    board_log("openmusicplayer up\r\n");

    uint32_t last = hal_ms();
    output_fault_t logged = OUTPUT_FAULT_NONE;
    for (;;) {
        tud_task();
        if (hal_ms() != last) {  // the output timings are in ms
            last = hal_ms();
            policy();
            output_step();
            led();
            if (output_fault() != logged) {
                logged = output_fault();
                if (logged != OUTPUT_FAULT_NONE) board_log("output fault %d\r\n", (int)logged);
            }
        }
    }
}
