#include <string.h>
#include <stdlib.h>
#include <esp_timer.h>
#include <connect.h>
#include <esp_netif.h>
#include <cn-cbor/cn-cbor.h>
#include "esp_smartconfig.h"
#include <smartconfig_ack.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "app_utils.h"
#include "tcpip_adapter.h"



extern struct production_struct device_keys;

#define EXAMPLE_ESP_SMARTCOFNIG_TYPE      SC_TYPE_ESPTOUCH_V2
#define CONNECTION_RETRY_COUNT 20


/* FreeRTOS event group to signal when we are connected & ready to make a request */
/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
static EventGroupHandle_t s_wifi_event_group = NULL;
static const int CONNECTED_BIT = BIT0;
static const int ESPTOUCH_DONE_BIT = BIT1;
static const char *TAG = "my smartconfig";
static bool resetConfig = false;

#ifndef min
#define min(a, b)  ((a)<(b)?(a):(b))
#endif
/*static uint8_t ssid[33] = {0};
static uint8_t password[65] = {0};
static uint8_t rvd_data[127] = {0};*/
static user_pass_t local_connection;
static user_pass_handler_t save_connection_data = NULL;

static int connect_retry_count=0;

static void smartConfig_task(void *parm);

static void send_acknowledge();

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data) {

    if (event_base == SC_EVENT) {
        switch (event_id) {
            case SC_EVENT_SCAN_DONE:
                ESP_LOGI(TAG, "SC_EVENT_SCAN_DONE");
                break;
            case SC_EVENT_FOUND_CHANNEL:
                ESP_LOGI(TAG, "SC_EVENT_FOUND_CHANNEL");
                break;
            case SC_EVENT_SEND_ACK_DONE:
                ESP_LOGI(TAG, "SC_EVENT_SEND_ACK_DONE");
//                puts("send ack done event");
                xEventGroupSetBits(s_wifi_event_group, ESPTOUCH_DONE_BIT);
                break;
            case SC_EVENT_GOT_SSID_PSWD:
                ESP_LOGI(TAG, "SC_EVENT_GOT_SSID_PSWD");
                smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *) event_data;
                wifi_config_t wifi_config;
                //todo: check for empty or invalid user/pass
                if (strlen((char *) evt->ssid) == 0) {
                    ESP_LOGI(TAG, "Empty SSID");
                    resetConfig = true;
                    return;
                }

                bzero(&wifi_config, sizeof(wifi_config_t));
                bzero(&local_connection, sizeof(user_pass_t));

                if (evt->type == SC_TYPE_ESPTOUCH_V2) {
                    uint8_t rvd_data[127];
                    memset(rvd_data, 0, sizeof(rvd_data));
                    ESP_ERROR_CHECK(esp_smartconfig_get_rvd_data(rvd_data, sizeof(rvd_data)))

                    int pos;
                    for (pos = sizeof(rvd_data) - 1; pos >= 0; pos--)
                        if (rvd_data[pos] != 0)
                            break;
                    // using device_keys.aeskey in hash seems unsecure
                    uint8_t *hash = crptoHash256(device_keys.aeskey, strlen(device_keys.aeskey));
                    if (memcmp(rvd_data + 2, hash, 5) != 0 || rvd_data[0] != 0x83 || rvd_data[1] != 0x45) // hashes and array start dont match
                    {
                        resetConfig = true;
                        ESP_LOGE(TAG, "input data format error");
                        return;
                    }

                    cn_cbor_errback res = {0};
                    cn_cbor *root;
                    root = cn_cbor_decode(rvd_data, pos + 1, &res);
                    if (res.err != CN_CBOR_NO_ERROR) {
                        ESP_LOGE(TAG, "rvd data error");
                        return;
                    }

                    cn_cbor *aesHash = cn_cbor_index(root, 0);
                    if (memcmp(aesHash->v.bytes, hash, 5) == 0) // hashes match
                    {
                        int ssidLen = min(strlen((char *) evt->ssid), sizeof(local_connection.ssid) - 1);
                        memcpy(wifi_config.sta.ssid, evt->ssid, ssidLen);

                        int passwdLen = min(strlen((char *) evt->password), sizeof(local_connection.passwd) - 1);
                        memcpy(wifi_config.sta.password, evt->password, passwdLen);

                        wifi_config.sta.bssid_set = evt->bssid_set;
                        if (wifi_config.sta.bssid_set == true) {
                            memcpy(wifi_config.sta.bssid, evt->bssid, sizeof(wifi_config.sta.bssid));
                        }
                        ESP_ERROR_CHECK(esp_wifi_disconnect());

                        /*printf("testing connection with ssid:%s pass:%s\n",
                               wifi_config.sta.ssid,
                               wifi_config.sta.password
                        );*/
                        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));

                        esp_err_t res = esp_wifi_connect();
                        if (res != ESP_OK) {
                            ESP_ERROR_CHECK(res);
                            return;
                        }
                        send_acknowledge(evt);

                        memcpy(local_connection.ssid,
                               evt->ssid,
                               ssidLen
                        );
                        memcpy(local_connection.passwd,
                               evt->password,
                               passwdLen
                        );
                        cn_cbor *item = cn_cbor_index(root, 1);
                        memset(local_connection.serverAddress, 0, sizeof(local_connection.serverAddress));
                        memcpy(local_connection.serverAddress, item->v.str, item->length);
                        item = cn_cbor_index(root, 2);
                        memset(local_connection.devicePass, 0, sizeof(local_connection.devicePass));
                        memcpy(local_connection.devicePass, item->v.str, item->length);
                        /*puts("data copied to local_connection");*/
                    }/* else
                        puts("AES keys dont match");*/
                }
                break;
            default:
                ESP_LOGI(TAG, "base_id(%s, %u)\n", (char *) "SC_EVENT", event_id);
                break;
        }
    } else if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "WIFI_EVENT_STA_START");
                xTaskCreate(smartConfig_task, "smartconfig_task", 4 * 1024, NULL, 3, NULL);
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGI(TAG, "WIFI_EVENT_STA_DISCONNECTED");
                xEventGroupClearBits(s_wifi_event_group, CONNECTED_BIT);
                if(!resetConfig) {
                    connect_retry_count++;
                    connect_retry_count < CONNECTION_RETRY_COUNT ? esp_wifi_connect() : (resetConfig = true);
                }
                break;
            case WIFI_EVENT_STA_CONNECTED:
                //todo: somehow should check serverAddress availability and userPassword and then save
                //todo: switch should be reset by a remote IR Code sequence
                if (save_connection_data) {
                    save_connection_data(&local_connection);
                }
                wifi_disconnect();
                ESP_LOGI(TAG, "WIFI_EVENT_STA_CONNECTED");
                break;
            default:
                ESP_LOGI(TAG, "base_id(%s, %u)\n", (char *) "WIFI_EVENT", event_id);
                break;
        }
    } else if (event_base == IP_EVENT) {
        switch (event_id) {
            case IP_EVENT_STA_GOT_IP:
                ESP_LOGI(TAG, "IP_EVENT_STA_GOT_IP");
                xEventGroupSetBits(s_wifi_event_group, CONNECTED_BIT);
                break;
            default:
                ESP_LOGI(TAG, "base_id(%s, %u)\n", (char *) "IP_EVENT", event_id);
                break;
        }
    } else {
        ESP_LOGI(TAG, "base_id(%s, %u)\n", event_base, event_id);
    }
}

static inline void send_acknowledge(smartconfig_event_got_ssid_pswd_t *evt) {
//    puts("sending ack ...");
    esp_err_t err = sc_send_ack_start(evt->type, evt->token, evt->cellphone_ip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Send smartconfig ACK error: %d", err);
        return;
    }
}

static void smartConfig_task(void *parm) {
    EventBits_t uxBits;
    ESP_ERROR_CHECK(esp_smartconfig_set_type(EXAMPLE_ESP_SMARTCOFNIG_TYPE));
    esp_esptouch_set_timeout(30);
    smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
    cfg.esp_touch_v2_enable_crypt = true;
    cfg.esp_touch_v2_key = device_keys.aeskey;
    ESP_ERROR_CHECK(esp_smartconfig_start(&cfg));
    ESP_LOGI(TAG, "listenning for ssid");

    unsigned int millis = esp_timer_get_time() / 1000 + 60000;
    while ((esp_timer_get_time() / 1000) < millis) {
        uxBits = xEventGroupWaitBits(s_wifi_event_group, CONNECTED_BIT | ESPTOUCH_DONE_BIT, true, false,
                                     1000 / portTICK_RATE_MS);
        if(resetConfig)
            break;
        if (uxBits & CONNECTED_BIT) {
            millis += 60000; // another 60 secs
            ESP_LOGI(TAG, "Connected to ap");
        }
        if (uxBits & ESPTOUCH_DONE_BIT) {
            ESP_LOGI(TAG, "task over");
            break;
        }
        fflush(stdout);
    }
    ESP_ERROR_CHECK(esp_smartconfig_stop())
    puts("smart stopped");
    ESP_ERROR_CHECK(esp_wifi_disconnect())
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler))
    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler))
    ESP_ERROR_CHECK(esp_event_handler_unregister(SC_EVENT, ESP_EVENT_ANY_ID, &event_handler))
    vEventGroupDelete(s_wifi_event_group);
    esp_wifi_stop();
    resetConfig = false;
    s_wifi_event_group = NULL; // smartConfig is is not running now
    vTaskDelete(NULL);
}

bool smartConfig_is_running() {
    return s_wifi_event_group != NULL;
}

void app_smart_config(user_pass_handler_t saver) {
    /*puts("");
    puts("");*/
    ESP_LOGD(TAG, "started\n");
    resetConfig = false;
    connect_retry_count = 0;
    save_connection_data = saver;
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init())

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT()
    ESP_ERROR_CHECK(esp_wifi_init(&cfg))
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE))
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA))
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL))
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL))
    ESP_ERROR_CHECK(esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL))
    ESP_ERROR_CHECK(esp_wifi_start())
}
