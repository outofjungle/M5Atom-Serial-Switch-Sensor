#include "proto_cmd.h"
#include "protocol.h"
#include "err_codes.h"
#include "channels.h"
#include "app_config.h"
#include "app_stream.h"
#include "usb_cdc.h"
#include "cbor.h"
#include "esp_timer.h"
#include "esp_app_desc.h"
#include "esp_system.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#define DEVICE_NAME "M5Atom-Serial-Switch-Sensor"

static uint8_t s_txbuf[PROTO_MAX_PAYLOAD];

// Starts a response array of total_items items: kind, seq, verb, then
// (total_items - 3) more values the caller encodes into *arr.
static void resp_begin(CborEncoder *enc, CborEncoder *arr, int kind, uint32_t seq,
                        const char *verb, size_t total_items)
{
    cbor_encoder_init(enc, s_txbuf, sizeof(s_txbuf), 0);
    cbor_encoder_create_array(enc, arr, total_items);
    cbor_encode_int(arr, kind);
    cbor_encode_uint(arr, seq);
    cbor_encode_text_stringz(arr, verb);
}

static void resp_send(CborEncoder *enc, CborEncoder *arr)
{
    cbor_encoder_close_container(enc, arr);
    size_t len = cbor_encoder_get_buffer_size(enc, s_txbuf);
    usb_cdc_send_response_frame(s_txbuf, len);
}

static void send_ok0(uint32_t seq, const char *verb)
{
    CborEncoder enc, arr;
    resp_begin(&enc, &arr, FRAME_KIND_OK, seq, verb, 3);
    resp_send(&enc, &arr);
}

static void send_ok_uint(uint32_t seq, const char *verb, uint64_t value)
{
    CborEncoder enc, arr;
    resp_begin(&enc, &arr, FRAME_KIND_OK, seq, verb, 4);
    cbor_encode_uint(&arr, value);
    resp_send(&enc, &arr);
}

static void send_ok_text(uint32_t seq, const char *verb, const char *value)
{
    CborEncoder enc, arr;
    resp_begin(&enc, &arr, FRAME_KIND_OK, seq, verb, 4);
    cbor_encode_text_stringz(&arr, value);
    resp_send(&enc, &arr);
}

static void send_err(uint32_t seq, const char *verb, const char *code)
{
    CborEncoder enc, arr;
    resp_begin(&enc, &arr, FRAME_KIND_ERR, seq, verb, 4);
    cbor_encode_text_stringz(&arr, code);
    resp_send(&enc, &arr);
}

// ---------------------------------------------------------------------------
// System commands
// ---------------------------------------------------------------------------

static void handle_ping(uint32_t seq, CborValue *it)
{
    if (!cbor_value_at_end(it)) {
        send_err(seq, "PING", "EARG");
        return;
    }
    send_ok0(seq, "PING");
}

static void handle_id(uint32_t seq, CborValue *it)
{
    if (!cbor_value_at_end(it)) {
        send_err(seq, "ID", "EARG");
        return;
    }
    const esp_app_desc_t *desc = esp_app_get_description();
    CborEncoder enc, arr;
    resp_begin(&enc, &arr, FRAME_KIND_OK, seq, "ID", 6);
    cbor_encode_text_stringz(&arr, DEVICE_NAME);
    cbor_encode_text_stringz(&arr, desc->version);
    cbor_encode_text_stringz(&arr, usb_cdc_get_serial());
    resp_send(&enc, &arr);
}

static void handle_caps(uint32_t seq, CborValue *it)
{
    if (!cbor_value_at_end(it)) {
        send_err(seq, "CAPS", "EARG");
        return;
    }
    size_t count = channels_count();
    for (size_t i = 0; i < count; i++) {
        const channel_def_t *ch = channels_at(i);
        CborEncoder enc, arr;
        resp_begin(&enc, &arr, FRAME_KIND_ROW, seq, "CAPS", 6);
        cbor_encode_text_stringz(&arr, ch->name);
        cbor_encode_text_stringz(&arr, ch->type_name);
        cbor_encode_text_stringz(&arr, ch->access);
        resp_send(&enc, &arr);
    }
    send_ok_uint(seq, "CAPS", count);
}

static void handle_stat(uint32_t seq, CborValue *it)
{
    if (!cbor_value_at_end(it)) {
        send_err(seq, "STAT", "EARG");
        return;
    }
    usb_cdc_stats_t s;
    usb_cdc_get_stats(&s);
    CborEncoder enc, arr;
    resp_begin(&enc, &arr, FRAME_KIND_OK, seq, "STAT", 10);
    cbor_encode_uint(&arr, (uint64_t)esp_timer_get_time());
    cbor_encode_uint(&arr, s.frames_rx);
    cbor_encode_uint(&arr, s.frames_tx);
    cbor_encode_uint(&arr, s.crc_errors);
    cbor_encode_uint(&arr, s.samples_dropped);
    cbor_encode_uint(&arr, s.log_lines_dropped);
    cbor_encode_uint(&arr, s.ranging_timeouts);
    resp_send(&enc, &arr);
}

static void handle_rst(uint32_t seq, CborValue *it)
{
    if (!cbor_value_at_end(it)) {
        send_err(seq, "RST", "EARG");
        return;
    }
    send_ok0(seq, "RST");
    // Give CDC0 a moment to actually flush this response before the reset
    // tears the USB link down.
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_restart();
}

static void handle_echo(uint32_t seq, CborValue *it)
{
    if (cbor_value_at_end(it) || !cbor_value_is_text_string(it)) {
        send_err(seq, "ECHO", "EARG");
        return;
    }
    char text[128];
    size_t text_len = sizeof(text);
    CborValue after = *it;
    if (cbor_value_copy_text_string(it, text, &text_len, &after) != CborNoError) {
        send_err(seq, "ECHO", "EARG");
        return;
    }
    if (!cbor_value_at_end(&after)) {
        send_err(seq, "ECHO", "EARG");
        return;
    }
    send_ok_text(seq, "ECHO", text);
}

// ---------------------------------------------------------------------------
// Data commands
// ---------------------------------------------------------------------------

static void handle_get(uint32_t seq, CborValue *it)
{
    if (cbor_value_at_end(it) || !cbor_value_is_text_string(it)) {
        send_err(seq, "GET", "EARG");
        return;
    }
    char name[32];
    size_t name_len = sizeof(name);
    CborValue after;
    if (cbor_value_copy_text_string(it, name, &name_len, &after) != CborNoError) {
        send_err(seq, "GET", "EARG");
        return;
    }
    if (!cbor_value_at_end(&after)) {
        send_err(seq, "GET", "EARG");
        return;
    }

    if (strcmp(name, "*") == 0) {
        size_t count = channels_count();
        for (size_t i = 0; i < count; i++) {
            const channel_def_t *ch = channels_at(i);
            CborEncoder enc, arr;
            resp_begin(&enc, &arr, FRAME_KIND_ROW, seq, "GET", 5);
            cbor_encode_text_stringz(&arr, ch->name);
            ch->get(&arr);
            resp_send(&enc, &arr);
        }
        send_ok_uint(seq, "GET", count);
        return;
    }

    const channel_def_t *ch = channels_find(name);
    if (ch == NULL) {
        send_err(seq, "GET", "ENOCH");
        return;
    }
    CborEncoder enc, arr;
    resp_begin(&enc, &arr, FRAME_KIND_OK, seq, "GET", 4);
    ch->get(&arr);
    resp_send(&enc, &arr);
}

static void handle_set(uint32_t seq, CborValue *it)
{
    if (cbor_value_at_end(it) || !cbor_value_is_text_string(it)) {
        send_err(seq, "SET", "EARG");
        return;
    }
    char name[32];
    size_t name_len = sizeof(name);
    CborValue val;
    if (cbor_value_copy_text_string(it, name, &name_len, &val) != CborNoError) {
        send_err(seq, "SET", "EARG");
        return;
    }
    if (cbor_value_at_end(&val)) {
        send_err(seq, "SET", "EARG");
        return;
    }
    CborValue after = val;
    if (cbor_value_advance(&after) != CborNoError || !cbor_value_at_end(&after)) {
        send_err(seq, "SET", "EARG");
        return;
    }

    const channel_def_t *ch = channels_find(name);
    if (ch == NULL) {
        send_err(seq, "SET", "ENOCH");
        return;
    }
    if (ch->set == NULL) {
        send_err(seq, "SET", "EACCESS");
        return;
    }

    int rc = ch->set(&val);
    if (rc == ERR_RANGE) {
        send_err(seq, "SET", "ERANGE");
        return;
    }
    if (rc != 0) {
        send_err(seq, "SET", "EINTERNAL");
        return;
    }
    send_ok0(seq, "SET");
}

// ---------------------------------------------------------------------------
// Streaming commands
// ---------------------------------------------------------------------------

static void handle_cfg(uint32_t seq, CborValue *it)
{
    if (cbor_value_at_end(it) || !cbor_value_is_text_string(it)) {
        send_err(seq, "CFG", "EARG");
        return;
    }
    char key[16];
    size_t key_len = sizeof(key);
    CborValue after_key;
    if (cbor_value_copy_text_string(it, key, &key_len, &after_key) != CborNoError) {
        send_err(seq, "CFG", "EARG");
        return;
    }

    bool has_value = !cbor_value_at_end(&after_key);
    CborValue val = after_key;
    if (has_value) {
        CborValue after_val = val;
        if (cbor_value_advance(&after_val) != CborNoError || !cbor_value_at_end(&after_val)) {
            send_err(seq, "CFG", "EARG");
            return;
        }
    }

    if (strcmp(key, "PERIOD_US") == 0) {
        // 1 hour. A mistaken huge value (an extra zero typed into a host
        // script) should be rejected clearly, rather than making
        // streaming look broken for a very long time with no error to
        // explain why.
        #define PERIOD_US_MAX 3600000000ULL
        if (has_value) {
            int64_t v;
            if (!cbor_value_is_integer(&val) || cbor_value_get_int64(&val, &v) != CborNoError ||
                v < 0 || (uint64_t)v > PERIOD_US_MAX) {
                send_err(seq, "CFG", "EARG");
                return;
            }
            if (app_config_set_period_us((uint32_t)v) == ERR_RANGE) {
                send_err(seq, "CFG", "ERANGE");
                return;
            }
            app_stream_apply_period();
        }
        send_ok_uint(seq, "CFG", app_config_get_period_us());
        return;
    }

    if (strcmp(key, "BATCH") == 0) {
        if (has_value) {
            int64_t v;
            if (!cbor_value_is_integer(&val) || cbor_value_get_int64(&val, &v) != CborNoError ||
                v < 0 || v > 0xFFFF) {
                send_err(seq, "CFG", "EARG");
                return;
            }
            if (app_config_set_batch((uint16_t)v) == ERR_RANGE) {
                send_err(seq, "CFG", "ERANGE");
                return;
            }
        }
        send_ok_uint(seq, "CFG", app_config_get_batch());
        return;
    }

    if (strcmp(key, "MODE") == 0) {
        if (has_value) {
            if (!cbor_value_is_text_string(&val)) {
                send_err(seq, "CFG", "EARG");
                return;
            }
            char mode[16];
            size_t mode_len = sizeof(mode);
            CborValue tmp = val;
            if (cbor_value_copy_text_string(&val, mode, &mode_len, &tmp) != CborNoError) {
                send_err(seq, "CFG", "EARG");
                return;
            }
            if (app_config_set_mode(mode) == ERR_RANGE) {
                send_err(seq, "CFG", "ERANGE");
                return;
            }
        }
        send_ok_text(seq, "CFG", app_config_get_mode());
        return;
    }

    if (strcmp(key, "ENCODING") == 0) {
        if (has_value) {
            if (!cbor_value_is_text_string(&val)) {
                send_err(seq, "CFG", "EARG");
                return;
            }
            char enc_val[16];
            size_t enc_len = sizeof(enc_val);
            CborValue tmp = val;
            if (cbor_value_copy_text_string(&val, enc_val, &enc_len, &tmp) != CborNoError) {
                send_err(seq, "CFG", "EARG");
                return;
            }
            if (strcmp(enc_val, "CBOR") != 0) {
                send_err(seq, "CFG", "ERANGE");
                return;
            }
        }
        send_ok_text(seq, "CFG", app_config_get_encoding());
        return;
    }

    send_err(seq, "CFG", "EARG");
}

static void handle_sub(uint32_t seq, CborValue *it)
{
    if (cbor_value_at_end(it) || !cbor_value_is_text_string(it)) {
        send_err(seq, "SUB", "EARG");
        return;
    }
    char name[32];
    size_t name_len = sizeof(name);
    CborValue after;
    if (cbor_value_copy_text_string(it, name, &name_len, &after) != CborNoError) {
        send_err(seq, "SUB", "EARG");
        return;
    }
    if (!cbor_value_at_end(&after)) {
        send_err(seq, "SUB", "EARG");
        return;
    }
    int rc = app_config_subscribe(name);
    if (rc == ERR_NOCH) {
        send_err(seq, "SUB", "ENOCH");
        return;
    }
    if (rc == ERR_RANGE) {
        send_err(seq, "SUB", "ERANGE");
        return;
    }
    send_ok0(seq, "SUB");
}

static void handle_unsub(uint32_t seq, CborValue *it)
{
    if (cbor_value_at_end(it) || !cbor_value_is_text_string(it)) {
        send_err(seq, "UNSUB", "EARG");
        return;
    }
    char name[32];
    size_t name_len = sizeof(name);
    CborValue after;
    if (cbor_value_copy_text_string(it, name, &name_len, &after) != CborNoError) {
        send_err(seq, "UNSUB", "EARG");
        return;
    }
    if (!cbor_value_at_end(&after)) {
        send_err(seq, "UNSUB", "EARG");
        return;
    }
    if (app_config_unsubscribe(name) == ERR_STATE) {
        send_err(seq, "UNSUB", "ESTATE");
        return;
    }
    send_ok0(seq, "UNSUB");
}

static void handle_start(uint32_t seq, CborValue *it)
{
    if (!cbor_value_at_end(it)) {
        send_err(seq, "START", "EARG");
        return;
    }
    if (app_config_start_streaming() == ERR_STATE) {
        send_err(seq, "START", "ESTATE");
        return;
    }
    send_ok0(seq, "START");
}

static void handle_stop(uint32_t seq, CborValue *it)
{
    if (!cbor_value_at_end(it)) {
        send_err(seq, "STOP", "EARG");
        return;
    }
    if (app_config_stop_streaming() == ERR_STATE) {
        send_err(seq, "STOP", "ESTATE");
        return;
    }
    send_ok0(seq, "STOP");
}

// ---------------------------------------------------------------------------
// Storage commands
// ---------------------------------------------------------------------------

static void handle_save(uint32_t seq, CborValue *it)
{
    if (!cbor_value_at_end(it)) {
        send_err(seq, "SAVE", "EARG");
        return;
    }
    if (app_config_save() != ESP_OK) {
        send_err(seq, "SAVE", "EINTERNAL");
        return;
    }
    send_ok0(seq, "SAVE");
}

static void handle_load(uint32_t seq, CborValue *it)
{
    if (!cbor_value_at_end(it)) {
        send_err(seq, "LOAD", "EARG");
        return;
    }
    esp_err_t err = app_config_load();
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        send_err(seq, "LOAD", "ESTATE");
        return;
    }
    if (err != ESP_OK) {
        send_err(seq, "LOAD", "EINTERNAL");
        return;
    }
    send_ok0(seq, "LOAD");
}

// ---------------------------------------------------------------------------
// Dispatch
// ---------------------------------------------------------------------------

typedef void (*cmd_handler_t)(uint32_t seq, CborValue *it);

typedef struct {
    const char *verb;
    cmd_handler_t handler;
} cmd_entry_t;

static const cmd_entry_t s_commands[] = {
    { "PING", handle_ping },
    { "ID", handle_id },
    { "CAPS", handle_caps },
    { "STAT", handle_stat },
    { "RST", handle_rst },
    { "ECHO", handle_echo },
    { "GET", handle_get },
    { "SET", handle_set },
    { "CFG", handle_cfg },
    { "SUB", handle_sub },
    { "UNSUB", handle_unsub },
    { "START", handle_start },
    { "STOP", handle_stop },
    { "SAVE", handle_save },
    { "LOAD", handle_load },
};
#define COMMAND_COUNT (sizeof(s_commands) / sizeof(s_commands[0]))

void proto_cmd_dispatch(const uint8_t *payload, size_t len)
{
    CborParser parser;
    CborValue top;

    if (cbor_parser_init(payload, len, 0, &parser, &top) != CborNoError || !cbor_value_is_array(&top)) {
        usb_cdc_note_crc_error();
        return;
    }

    size_t arr_len = 0;
    if (cbor_value_get_array_length(&top, &arr_len) != CborNoError || arr_len < 3) {
        usb_cdc_note_crc_error();
        return;
    }

    CborValue it;
    if (cbor_value_enter_container(&top, &it) != CborNoError) {
        usb_cdc_note_crc_error();
        return;
    }

    int64_t kind = 0;
    if (!cbor_value_is_integer(&it) || cbor_value_get_int64(&it, &kind) != CborNoError) {
        usb_cdc_note_crc_error();
        return;
    }
    if (cbor_value_advance(&it) != CborNoError) {
        usb_cdc_note_crc_error();
        return;
    }

    int64_t seq64 = 0;
    if (!cbor_value_is_integer(&it) || cbor_value_get_int64(&it, &seq64) != CborNoError) {
        usb_cdc_note_crc_error();
        return;
    }
    uint32_t seq = (uint32_t)seq64;
    if (cbor_value_advance(&it) != CborNoError) {
        usb_cdc_note_crc_error();
        return;
    }

    // A trustworthy sequence number has been found. Every problem from
    // here on gets a matching ERR response. See docs/07-error-codes.md.

    char verb[16];
    size_t verb_len = sizeof(verb);
    if (!cbor_value_is_text_string(&it) ||
        cbor_value_copy_text_string(&it, verb, &verb_len, &it) != CborNoError) {
        send_err(seq, "", "EFRAME");
        return;
    }

    if (kind != FRAME_KIND_CMD) {
        send_err(seq, verb, "EFRAME");
        return;
    }

    for (size_t i = 0; i < COMMAND_COUNT; i++) {
        if (strcmp(verb, s_commands[i].verb) == 0) {
            s_commands[i].handler(seq, &it);
            return;
        }
    }
    send_err(seq, verb, "ECMD");
}
