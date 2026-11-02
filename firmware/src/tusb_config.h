// TinyUSB: one UAC2 speaker function with an async feedback endpoint, high speed on the RT1011.
#pragma once

#define CFG_TUSB_MCU OPT_MCU_MIMXRT1XXX
#define CFG_TUSB_OS OPT_OS_NONE
#define CFG_TUSB_DEBUG 0
#define CFG_TUD_ENABLED 1
#define CFG_TUD_MAX_SPEED OPT_MODE_HIGH_SPEED
#define BOARD_TUD_RHPORT 0
#define CFG_TUSB_MEM_ALIGN __attribute__((aligned(32)))
#define CFG_TUD_ENDPOINT0_SIZE 64

#define CFG_TUD_AUDIO 1
#define CFG_TUD_CDC 0
#define CFG_TUD_MSC 0
#define CFG_TUD_HID 0
#define CFG_TUD_MIDI 0
#define CFG_TUD_VENDOR 0

// 24-bit samples in 32-bit slots: passed to the DAC unchanged
#define CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX 2
#define CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_RX 4
#define CFG_TUD_AUDIO_FUNC_1_RESOLUTION_RX 24

// High speed: up to 192 kHz (the SAI's limit here). Full speed fits 96 kHz in a 1023-byte packet.
#define CFG_TUD_AUDIO_FUNC_1_MAX_SAMPLE_RATE_FS 96000
#define CFG_TUD_AUDIO_FUNC_1_MAX_SAMPLE_RATE_HS 192000
#define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_FS                                                             \
    TUD_AUDIO_EP_SIZE(false, CFG_TUD_AUDIO_FUNC_1_MAX_SAMPLE_RATE_FS, CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_RX, \
                      CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX)
#define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_HS                                                            \
    TUD_AUDIO_EP_SIZE(true, CFG_TUD_AUDIO_FUNC_1_MAX_SAMPLE_RATE_HS, CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_RX, \
                      CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX)
#define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_MAX TU_MAX(CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_FS, CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_HS)
// 8 KB: about 5 ms at 192 kHz. The feedback holds it half full.
#define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SW_BUF_SZ 8192
#define CFG_TUD_AUDIO_ENABLE_EP_OUT 1
#define CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP 1
