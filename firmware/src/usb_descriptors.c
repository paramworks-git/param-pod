#include <string.h>
#include "tusb.h"
#include "usb_descriptors.h"

// pid.codes test PID until the project has its own (https://pid.codes)
#define USB_VID 0x1209
#define USB_PID 0x0001

#define EPNUM_AUDIO 0x01
#define EPNUM_AUDIO_FB 0x81

static const tusb_desc_device_t desc_device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    // IAD: subclass common, protocol IAD (USB IAD ECN)
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = USB_VID,
    .idProduct = USB_PID,
    .bcdDevice = 0x0100,
    .iManufacturer = 1,
    .iProduct = 2,
    .iSerialNumber = 3,
    .bNumConfigurations = 1,
};

const uint8_t *tud_descriptor_device_cb(void) { return (const uint8_t *)&desc_device; }

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_AUDIO20_SPEAKER_STEREO_FB_DESC_LEN)

// Self-powered: the pod has its own cell and draws nothing from the host for audio
static const uint8_t desc_config_hs[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_SELF_POWERED, 100),
    TUD_AUDIO20_SPEAKER_STEREO_FB_DESCRIPTOR(ITF_NUM_AUDIO_CONTROL, 4, CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_RX,
                                             CFG_TUD_AUDIO_FUNC_1_RESOLUTION_RX, EPNUM_AUDIO,
                                             CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_HS, EPNUM_AUDIO_FB, 4),
};
static const uint8_t desc_config_fs[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_SELF_POWERED, 100),
    TUD_AUDIO20_SPEAKER_STEREO_FB_DESCRIPTOR(ITF_NUM_AUDIO_CONTROL, 4, CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_RX,
                                             CFG_TUD_AUDIO_FUNC_1_RESOLUTION_RX, EPNUM_AUDIO,
                                             CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_FS, EPNUM_AUDIO_FB, 4),
};
TU_VERIFY_STATIC(sizeof(desc_config_hs) == CONFIG_TOTAL_LEN, "config length");

const uint8_t *tud_descriptor_configuration_cb(uint8_t index)
{
    (void)index;
    return tud_speed_get() == TUSB_SPEED_HIGH ? desc_config_hs : desc_config_fs;
}

static const tusb_desc_device_qualifier_t desc_qualifier = {
    .bLength = sizeof(tusb_desc_device_qualifier_t),
    .bDescriptorType = TUSB_DESC_DEVICE_QUALIFIER,
    .bcdUSB = 0x0200,
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .bNumConfigurations = 1,
    .bReserved = 0,
};

const uint8_t *tud_descriptor_device_qualifier_cb(void) { return (const uint8_t *)&desc_qualifier; }

const uint8_t *tud_descriptor_other_speed_configuration_cb(uint8_t index)
{
    (void)index;
    return tud_speed_get() == TUSB_SPEED_HIGH ? desc_config_fs : desc_config_hs;
}

static const char *const strings[] = {
    NULL,               // 0: language, handled below
    "openmusicplayer",  // 1: manufacturer
    "openmusicplayer",  // 2: product
    NULL,               // 3: serial, from the chip's unique ID
    "Headphones",       // 4: audio function
};

static uint16_t desc_str[33];

static size_t serial(uint16_t *out)
{
    // OCOTP CFG0/CFG1 shadow registers: the chip's unique ID
    static const char hex[] = "0123456789ABCDEF";
    uint32_t id[2] = {OCOTP->CFG0, OCOTP->CFG1};
    size_t n = 0;
    for (int w = 1; w >= 0; w--)
        for (int s = 28; s >= 0; s -= 4) out[n++] = (uint16_t)hex[(id[w] >> s) & 0xFu];
    return n;
}

const uint16_t *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    (void)langid;
    size_t n;
    if (index == 0) {
        desc_str[1] = 0x0409;
        n = 1;
    } else if (index == 3) {
        n = serial(&desc_str[1]);
    } else if (index < sizeof strings / sizeof strings[0]) {
        const char *s = strings[index];
        n = strlen(s);
        if (n > 32) n = 32;
        for (size_t i = 0; i < n; i++) desc_str[1 + i] = (uint16_t)s[i];
    } else {
        return NULL;
    }
    desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2u * n + 2u));
    return desc_str;
}
