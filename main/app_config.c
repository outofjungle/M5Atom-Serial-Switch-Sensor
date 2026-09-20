#include "app_config.h"
#include "channels.h"
#include "err_codes.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include <string.h>

#define NVS_NAMESPACE "app_cfg"
#define NVS_KEY "cfg"
#define MAX_SUBS 8
#define MODE_MAXLEN 16
#define CHANNEL_NAME_MAXLEN 32

static const char *TAG = "app_config";

typedef struct {
    uint32_t period_us;
    char mode[MODE_MAXLEN];
    uint16_t batch;
    char subs[MAX_SUBS][CHANNEL_NAME_MAXLEN];
    uint32_t sub_count;
    bool streaming;
} config_state_t;

static config_state_t s_cfg;

// s_cfg is written by the USB RX task (handling CFG/SUB/UNSUB/START/STOP)
// and read every sample period by the esp_timer callback task in
// app_stream.c. Neither caller is ever an ISR, so a plain critical
// section is the correct, sufficient tool here.
static portMUX_TYPE s_cfg_mux = portMUX_INITIALIZER_UNLOCKED;

static void set_defaults(void)
{
    memset(&s_cfg, 0, sizeof(s_cfg));
    s_cfg.period_us = 100000; // 10 Hz
    strncpy(s_cfg.mode, "PERIODIC", sizeof(s_cfg.mode) - 1);
    s_cfg.batch = 1;
    s_cfg.sub_count = 0;
    s_cfg.streaming = false;
}

esp_err_t app_config_init(void)
{
    set_defaults();
    esp_err_t err = app_config_load();
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "app_config_load failed: %d, using defaults", err);
    }
    return ESP_OK;
}

uint32_t app_config_get_period_us(void)
{
    portENTER_CRITICAL(&s_cfg_mux);
    uint32_t v = s_cfg.period_us;
    portEXIT_CRITICAL(&s_cfg_mux);
    return v;
}

int app_config_set_period_us(uint32_t value)
{
    if (value < 1000) {
        return ERR_RANGE;
    }
    portENTER_CRITICAL(&s_cfg_mux);
    s_cfg.period_us = value;
    portEXIT_CRITICAL(&s_cfg_mux);
    return 0;
}

const char *app_config_get_mode(void)
{
    // Safe without a lock: callers only ever read this into a CBOR encoder
    // synchronously on the task that called this function, and the buffer
    // is only ever written whole (strncpy + explicit terminator) under
    // the lock in app_config_set_mode, never partially.
    return s_cfg.mode;
}

int app_config_set_mode(const char *value)
{
    if (strcmp(value, "PERIODIC") != 0 && strcmp(value, "ONCHANGE") != 0 && strcmp(value, "BOTH") != 0) {
        return ERR_RANGE;
    }
    portENTER_CRITICAL(&s_cfg_mux);
    strncpy(s_cfg.mode, value, sizeof(s_cfg.mode) - 1);
    s_cfg.mode[sizeof(s_cfg.mode) - 1] = '\0';
    portEXIT_CRITICAL(&s_cfg_mux);
    return 0;
}

uint16_t app_config_get_batch(void)
{
    portENTER_CRITICAL(&s_cfg_mux);
    uint16_t v = s_cfg.batch;
    portEXIT_CRITICAL(&s_cfg_mux);
    return v;
}

int app_config_set_batch(uint16_t value)
{
    if (value < 1 || value > 32) {
        return ERR_RANGE;
    }
    portENTER_CRITICAL(&s_cfg_mux);
    s_cfg.batch = value;
    portEXIT_CRITICAL(&s_cfg_mux);
    return 0;
}

const char *app_config_get_encoding(void)
{
    return "CBOR";
}

int app_config_subscribe(const char *channel_name)
{
    if (channels_find(channel_name) == NULL) {
        return ERR_NOCH;
    }
    portENTER_CRITICAL(&s_cfg_mux);
    int rc = 0;
    bool already = false;
    for (uint32_t i = 0; i < s_cfg.sub_count; i++) {
        if (strcmp(s_cfg.subs[i], channel_name) == 0) {
            already = true;
            break;
        }
    }
    if (already) {
        rc = 0;
    } else if (s_cfg.sub_count >= MAX_SUBS) {
        rc = ERR_RANGE;
    } else {
        strncpy(s_cfg.subs[s_cfg.sub_count], channel_name, CHANNEL_NAME_MAXLEN - 1);
        s_cfg.subs[s_cfg.sub_count][CHANNEL_NAME_MAXLEN - 1] = '\0';
        s_cfg.sub_count++;
    }
    portEXIT_CRITICAL(&s_cfg_mux);
    return rc;
}

int app_config_unsubscribe(const char *channel_name)
{
    portENTER_CRITICAL(&s_cfg_mux);
    int rc = ERR_STATE;
    for (uint32_t i = 0; i < s_cfg.sub_count; i++) {
        if (strcmp(s_cfg.subs[i], channel_name) == 0) {
            for (uint32_t j = i; j + 1 < s_cfg.sub_count; j++) {
                strcpy(s_cfg.subs[j], s_cfg.subs[j + 1]);
            }
            s_cfg.sub_count--;
            rc = 0;
            break;
        }
    }
    portEXIT_CRITICAL(&s_cfg_mux);
    return rc;
}

bool app_config_is_subscribed(const char *channel_name)
{
    portENTER_CRITICAL(&s_cfg_mux);
    bool found = false;
    for (uint32_t i = 0; i < s_cfg.sub_count; i++) {
        if (strcmp(s_cfg.subs[i], channel_name) == 0) {
            found = true;
            break;
        }
    }
    portEXIT_CRITICAL(&s_cfg_mux);
    return found;
}

size_t app_config_sub_count(void)
{
    portENTER_CRITICAL(&s_cfg_mux);
    size_t v = s_cfg.sub_count;
    portEXIT_CRITICAL(&s_cfg_mux);
    return v;
}

const char *app_config_sub_at(size_t index)
{
    // See app_config_get_mode: whole-buffer writes only, safe to read
    // without a lock once the index itself is known to be in range.
    return s_cfg.subs[index];
}

bool app_config_is_streaming(void)
{
    portENTER_CRITICAL(&s_cfg_mux);
    bool v = s_cfg.streaming;
    portEXIT_CRITICAL(&s_cfg_mux);
    return v;
}

int app_config_start_streaming(void)
{
    portENTER_CRITICAL(&s_cfg_mux);
    int rc = 0;
    if (s_cfg.streaming || s_cfg.sub_count == 0) {
        rc = ERR_STATE;
    } else {
        s_cfg.streaming = true;
    }
    portEXIT_CRITICAL(&s_cfg_mux);
    return rc;
}

int app_config_stop_streaming(void)
{
    portENTER_CRITICAL(&s_cfg_mux);
    int rc = 0;
    if (!s_cfg.streaming) {
        rc = ERR_STATE;
    } else {
        s_cfg.streaming = false;
    }
    portEXIT_CRITICAL(&s_cfg_mux);
    return rc;
}

esp_err_t app_config_save(void)
{
    config_state_t snapshot;
    portENTER_CRITICAL(&s_cfg_mux);
    snapshot = s_cfg;
    portEXIT_CRITICAL(&s_cfg_mux);

    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_blob(h, NVS_KEY, &snapshot, sizeof(snapshot));
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    return err;
}

esp_err_t app_config_load(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &h);
    if (err != ESP_OK) {
        return err;
    }

    config_state_t loaded;
    size_t sz = sizeof(loaded);
    err = nvs_get_blob(h, NVS_KEY, &loaded, &sz);
    nvs_close(h);

    if (err == ESP_OK && sz == sizeof(loaded)) {
        loaded.streaming = false; // never resume streaming automatically after a restart
        portENTER_CRITICAL(&s_cfg_mux);
        s_cfg = loaded;
        portEXIT_CRITICAL(&s_cfg_mux);
    }
    return err;
}
