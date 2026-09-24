#include "app_stream.h"
#include "app_config.h"
#include "channels.h"
#include "usb_cdc.h"
#include "protocol.h"
#include "cbor.h"
#include "esp_timer.h"
#include "esp_log.h"

static const char *TAG = "app_stream";
static esp_timer_handle_t s_timer;
static uint32_t s_seq = 0;

// docs/03-message-model.md: "counts up... wraps at 65535." Only this one
// task (the esp_timer callback) ever touches s_seq, so no lock is needed
// for the wrap itself.
#define STREAM_SEQ_MAX 65535

static uint32_t next_seq(void)
{
    uint32_t seq = s_seq;
    s_seq = (s_seq >= STREAM_SEQ_MAX) ? 0 : s_seq + 1;
    return seq;
}

// dist0's get() (ultrasonic_measure_mm(), via channels.c) blocks this
// esp_timer callback for up to tens of milliseconds when subscribed,
// unlike every other channel's near-instant read. esp_timer dispatches
// every periodic callback from one shared system task by default, so
// blocking it here would normally risk delaying unrelated timers
// elsewhere in the firmware. This firmware has no other periodic
// esp_timer callback -- no Wi-Fi, no BLE, nothing else registers one --
// so there is nothing else to delay, and keeping this design (instead of
// moving to a dedicated FreeRTOS task, as a board without that guarantee
// would need) avoids restructuring an already-working module. See
// docs/00-overview.md's design choices table.
static void send_sample(const channel_def_t *ch)
{
    uint32_t flags = usb_cdc_take_pending_drop() ? FLAG_DROP : 0;

    uint8_t buf[PROTO_MAX_PAYLOAD];
    CborEncoder enc, arr;
    cbor_encoder_init(&enc, buf, sizeof(buf), 0);
    cbor_encoder_create_array(&enc, &arr, 6);
    cbor_encode_uint(&arr, FRAME_KIND_DATA);
    cbor_encode_uint(&arr, next_seq());
    cbor_encode_uint(&arr, (uint64_t)esp_timer_get_time());
    cbor_encode_text_stringz(&arr, ch->name);
    ch->get(&arr);
    cbor_encode_uint(&arr, flags);
    cbor_encoder_close_container(&enc, &arr);

    size_t len = cbor_encoder_get_buffer_size(&enc, buf);
    usb_cdc_send_sample_frame(buf, len);
}

static void timer_cb(void *arg)
{
    (void)arg;
    if (!app_config_is_streaming()) {
        return;
    }
    size_t n = app_config_sub_count();
    for (size_t i = 0; i < n; i++) {
        const channel_def_t *ch = channels_find(app_config_sub_at(i));
        if (ch != NULL) {
            send_sample(ch);
        }
    }
}

esp_err_t app_stream_init(void)
{
    const esp_timer_create_args_t args = {
        .callback = timer_cb,
        .name = "app_stream",
    };
    esp_err_t err = esp_timer_create(&args, &s_timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_timer_create failed: %d", err);
        return err;
    }
    return esp_timer_start_periodic(s_timer, app_config_get_period_us());
}

void app_stream_apply_period(void)
{
    esp_timer_restart(s_timer, app_config_get_period_us());
}
