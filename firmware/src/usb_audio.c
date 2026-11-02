// UAC2 control requests, the stream's state, and the samples the SAI asks for.
// The host's volume control maps onto the AK4493's attenuator (0.5 dB steps), which is bit-perfect
// at 0 dB; mute is the DAC's soft mute.
#include <string.h>
#include "audio_feed.h"
#include "output.h"
#include "tusb.h"
#include "usb_audio.h"
#include "usb_descriptors.h"

#define UAC2_ENTITY_CLOCK 0x04
#define UAC2_ENTITY_FEATURE_UNIT 0x02

#define VOL_MIN_DB256 (-127 * 256)  // AK4493 attenuator range
#define VOL_RES_DB256 128           // 0.5 dB

static const uint32_t rates[] = {44100, 48000, 88200, 96000, 176400, 192000};

static uint32_t rate = 44100;
static volatile bool streaming;
static volatile uint32_t underruns;
static bool mute_ch[3];
static int16_t vol_ch[3];  // 1/256 dB, 0 = full scale

static uint8_t attenuation(void)
{
    // Master and channel gains add (in dB). One value for both DAC channels, from the louder one:
    // hosts move the master.
    int32_t db256 = vol_ch[0] + (vol_ch[1] > vol_ch[2] ? vol_ch[1] : vol_ch[2]);
    if (mute_ch[0] || (mute_ch[1] && mute_ch[2])) return 0;
    if (db256 >= 0) return 0xFF;
    if (db256 < VOL_MIN_DB256) return 0x01;
    return (uint8_t)(0xFF + db256 / VOL_RES_DB256);
}

uint8_t usb_audio_attenuation(void) { return attenuation(); }
bool usb_audio_streaming(void) { return streaming; }
uint32_t usb_audio_rate(void) { return rate; }
uint32_t usb_audio_underruns(void) { return underruns; }

void audio_feed(uint32_t *dst, uint32_t frames)
{
    uint16_t want = (uint16_t)(frames * 2u * sizeof(uint32_t));
    uint16_t got = streaming ? tud_audio_read(dst, want) : 0;
    if (got < want) {
        if (streaming) underruns++;
        memset((uint8_t *)dst + got, 0, want - got);
    }
}

// ---- Clock source: sample rate ----

static bool clock_get(uint8_t rhport, const tusb_control_request_t *req)
{
    uint8_t sel = TU_U16_HIGH(req->wValue);
    if (sel == AUDIO20_CS_CTRL_SAM_FREQ && req->bRequest == AUDIO20_CS_REQ_CUR) {
        audio20_control_cur_4_t cur = {(int32_t)tu_htole32(rate)};
        return tud_audio_buffer_and_schedule_control_xfer(rhport, req, &cur, sizeof cur);
    }
    if (sel == AUDIO20_CS_CTRL_SAM_FREQ && req->bRequest == AUDIO20_CS_REQ_RANGE) {
        audio20_control_range_4_n_t(TU_ARRAY_SIZE(rates)) r = {.wNumSubRanges = tu_htole16(TU_ARRAY_SIZE(rates))};
        for (size_t i = 0; i < TU_ARRAY_SIZE(rates); i++) {
            r.subrange[i].bMin = (int32_t)rates[i];
            r.subrange[i].bMax = (int32_t)rates[i];
            r.subrange[i].bRes = 0;
        }
        return tud_audio_buffer_and_schedule_control_xfer(rhport, req, &r, sizeof r);
    }
    if (sel == AUDIO20_CS_CTRL_CLK_VALID && req->bRequest == AUDIO20_CS_REQ_CUR) {
        audio20_control_cur_1_t valid = {.bCur = 1};
        return tud_audio_buffer_and_schedule_control_xfer(rhport, req, &valid, sizeof valid);
    }
    return false;
}

static bool clock_set(const tusb_control_request_t *req, const uint8_t *buf)
{
    if (TU_U16_HIGH(req->wValue) != AUDIO20_CS_CTRL_SAM_FREQ || req->bRequest != AUDIO20_CS_REQ_CUR) return false;
    TU_VERIFY(req->wLength == sizeof(audio20_control_cur_4_t));
    uint32_t r = (uint32_t)((const audio20_control_cur_4_t *)buf)->bCur;
    TU_VERIFY(output_rate_supported(r));
    rate = r;
    output_want_rate(rate);
    return true;
}

// ---- Feature unit: mute and volume ----

static bool feature_get(uint8_t rhport, const tusb_control_request_t *req)
{
    uint8_t sel = TU_U16_HIGH(req->wValue), ch = TU_U16_LOW(req->wValue);
    TU_VERIFY(ch < 3);
    if (sel == AUDIO20_FU_CTRL_MUTE && req->bRequest == AUDIO20_CS_REQ_CUR) {
        audio20_control_cur_1_t m = {.bCur = mute_ch[ch]};
        return tud_audio_buffer_and_schedule_control_xfer(rhport, req, &m, sizeof m);
    }
    if (sel == AUDIO20_FU_CTRL_VOLUME && req->bRequest == AUDIO20_CS_REQ_RANGE) {
        audio20_control_range_2_n_t(1) r = {
            .wNumSubRanges = tu_htole16(1),
            .subrange[0] = {.bMin = tu_htole16(VOL_MIN_DB256), .bMax = tu_htole16(0), .bRes = tu_htole16(VOL_RES_DB256)},
        };
        return tud_audio_buffer_and_schedule_control_xfer(rhport, req, &r, sizeof r);
    }
    if (sel == AUDIO20_FU_CTRL_VOLUME && req->bRequest == AUDIO20_CS_REQ_CUR) {
        audio20_control_cur_2_t v = {.bCur = tu_htole16(vol_ch[ch])};
        return tud_audio_buffer_and_schedule_control_xfer(rhport, req, &v, sizeof v);
    }
    return false;
}

static bool feature_set(const tusb_control_request_t *req, const uint8_t *buf)
{
    uint8_t sel = TU_U16_HIGH(req->wValue), ch = TU_U16_LOW(req->wValue);
    TU_VERIFY(ch < 3 && req->bRequest == AUDIO20_CS_REQ_CUR);
    if (sel == AUDIO20_FU_CTRL_MUTE) {
        TU_VERIFY(req->wLength == sizeof(audio20_control_cur_1_t));
        mute_ch[ch] = ((const audio20_control_cur_1_t *)buf)->bCur != 0;
        return true;
    }
    if (sel == AUDIO20_FU_CTRL_VOLUME) {
        TU_VERIFY(req->wLength == sizeof(audio20_control_cur_2_t));
        int16_t v = (int16_t)((const audio20_control_cur_2_t *)buf)->bCur;
        vol_ch[ch] = v > 0 ? 0 : v < VOL_MIN_DB256 ? VOL_MIN_DB256 : v;
        return true;
    }
    return false;
}

bool tud_audio_get_req_entity_cb(uint8_t rhport, const tusb_control_request_t *req)
{
    uint8_t entity = TU_U16_HIGH(req->wIndex);
    if (entity == UAC2_ENTITY_CLOCK) return clock_get(rhport, req);
    if (entity == UAC2_ENTITY_FEATURE_UNIT) return feature_get(rhport, req);
    return false;
}

bool tud_audio_set_req_entity_cb(uint8_t rhport, const tusb_control_request_t *req, uint8_t *buf)
{
    (void)rhport;
    uint8_t entity = TU_U16_HIGH(req->wIndex);
    if (entity == UAC2_ENTITY_CLOCK) return clock_set(req, buf);
    if (entity == UAC2_ENTITY_FEATURE_UNIT) return feature_set(req, buf);
    return false;
}

// ---- Streaming interface ----

bool tud_audio_set_itf_cb(uint8_t rhport, const tusb_control_request_t *req)
{
    (void)rhport;
    if (tu_u16_low(req->wIndex) == ITF_NUM_AUDIO_STREAMING && tu_u16_low(req->wValue) != 0) {
        tud_audio_clear_ep_out_ff();
        streaming = true;
    }
    return true;
}

bool tud_audio_set_itf_close_ep_cb(uint8_t rhport, const tusb_control_request_t *req)
{
    (void)rhport;
    if (tu_u16_low(req->wIndex) == ITF_NUM_AUDIO_STREAMING && tu_u16_low(req->wValue) == 0) streaming = false;
    return true;
}

void tud_audio_feedback_params_cb(uint8_t func_id, uint8_t alt_itf, audio_feedback_params_t *p)
{
    (void)func_id;
    (void)alt_itf;
    // The SAI drains the FIFO at the oscillator's rate; feedback holds the FIFO half full
    p->method = AUDIO_FEEDBACK_METHOD_FIFO_COUNT;
    p->sample_freq = rate;
}

void tud_umount_cb(void) { streaming = false; }
void tud_suspend_cb(bool remote_wakeup_en)
{
    (void)remote_wakeup_en;
    streaming = false;
}
