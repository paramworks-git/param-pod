#pragma once
#include <stdbool.h>
#include <stdint.h>

bool usb_audio_streaming(void);    // host has the streaming interface open
uint32_t usb_audio_rate(void);     // rate the host asked for
uint8_t usb_audio_attenuation(void);  // AK4493 ATT value for the host's volume and mute
uint32_t usb_audio_underruns(void);   // SAI blocks the FIFO couldn't fill
