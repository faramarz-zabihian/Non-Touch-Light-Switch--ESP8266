//
// Created by asus on ۲۳/۰۴/۲۰۲۱.
//
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "connect.h"
#include <memory.h>
#include <event_groups.h>
#include <esp_timer.h>
//#include <app_utils.h>
#include "sdkconfig.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event_loop.h"
#include "tcpip_adapter.h"

//#include "../../../../ESP8266_RTOS_SDK/components/coap/libcoap/ext/tinydtls/numeric.h"


#define GOT_IPV4_BIT BIT(0)
#define GOT_IPV6_BIT BIT(1)

#ifdef CONFIG_EXAMPLE_CONNECT_IPV6
#define CONNECTED_BITS (GOT_IPV4_BIT | GOT_IPV6_BIT)
#if defined(CONFIG_EXAMPLE_CONNECT_IPV6_PREF_LOCAL_LINK)
#define EXAMPLE_CONNECT_PREFERRED_IPV6_TYPE 0
#elif defined(CONFIG_EXAMPLE_CONNECT_IPV6_PREF_GLOBAL)
#define EXAMPLE_CONNECT_PREFERRED_IPV6_TYPE 1
#endif
#else
#define CONNECTED_BITS (GOT_IPV4_BIT)
#endif

static EventGroupHandle_t s_connect_event_group = NULL;
static ip4_addr_t s_ip_addr;
static unsigned long start_time;
unsigned int timeout_val = 60;
#ifdef CONFIG_EXAMPLE_CONNECT_IPV6
static ip6_addr_t s_ipv6_addr;
#endif

static const char *TAG = "connect";
bool is_connected = false;

bool WIFI_IS_READY() {
    return wifi_is_running() && is_connected;
}

static void reset_counter(const char *m) {
    start_time = 0;
}

static bool check_bounds() {
    if (start_time == 0) {
        start_time = esp_timer_get_time();
        return true;
    }
    return (esp_timer_get_time() - start_time) < timeout_val * 1000000; //true if timeout
}

static void on_wifi_disconnect(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
    is_connected = false;
    system_event_sta_disconnected_t *event = (system_event_sta_disconnected_t *) event_data;
    ESP_LOGI(TAG, "Wi-Fi disconnected, trying to reconnect...");
    if (check_bounds()) {
        if (event->reason == WIFI_REASON_BASIC_RATE_NOT_SUPPORT) {
            /*Switch to 802.11 bgn mode */
            esp_wifi_set_protocol(ESP_IF_WIFI_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
        };
        ESP_ERROR_CHECK(esp_wifi_connect());
    } else
	{
		wifi_disconnect();
	}
}

#ifdef CONFIG_EXAMPLE_CONNECT_IPV6

static void on_wifi_connect(void *arg, esp_event_base_t event_base,
                            int32_t event_id, void *event_data) {
    tcpip_adapter_create_ip6_linklocal(TCPIP_ADAPTER_IF_STA);
    reset_counter("on connect");
}

#endif

static void on_got_ip(void *arg, esp_event_base_t event_base,
                      int32_t event_id, void *event_data) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
    memcpy(&s_ip_addr, &event->ip_info.ip, sizeof(s_ip_addr));
    xEventGroupSetBits(s_connect_event_group, GOT_IPV4_BIT);
    is_connected = true;
}

#ifdef CONFIG_EXAMPLE_CONNECT_IPV6

static void on_got_ipv6(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data) {
    ip_event_got_ip6_t *event = (ip_event_got_ip6_t *) event_data;
    memcpy(&s_ipv6_addr, &event->ip6_info.ip, sizeof(s_ipv6_addr));
    if (EXAMPLE_CONNECT_PREFERRED_IPV6_TYPE) {
        if (ip6_addr_isglobal(&s_ipv6_addr)) {
            xEventGroupSetBits(s_connect_event_group, GOT_IPV6_BIT);
        }
    } else {
        xEventGroupSetBits(s_connect_event_group, GOT_IPV6_BIT);
    }
    is_connected = true;
}

#endif

static void register_events() {
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &on_wifi_disconnect, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_got_ip, NULL));

#ifdef CONFIG_EXAMPLE_CONNECT_IPV6
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &on_wifi_connect, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_GOT_IP6, &on_got_ipv6, NULL));
#endif

}

static void unregister_events() {
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &on_wifi_disconnect));
    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_got_ip));
#ifdef CONFIG_EXAMPLE_CONNECT_IPV6
    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_GOT_IP6, &on_got_ipv6));
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &on_wifi_connect));
#endif

}

static void stop(void) {
    unregister_events();
    esp_err_t err = esp_wifi_stop();
    reset_counter("stop");
    is_connected = false;
    if (s_connect_event_group)
        vEventGroupDelete(s_connect_event_group);
    s_connect_event_group = NULL;
    if (err != ESP_ERR_WIFI_NOT_INIT) {
        return;
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(esp_wifi_deinit());
}

/*
static void wifi_set_connection_info(const char *ssid, const char *passwd) {
	int ssid_len = min(strlen(ssid) ,sizeof(s_connection_ssid) - 1);
	strncpy(s_connection_ssid, ssid, ssid_len);
	int pass_len = min(strlen(passwd), sizeof(s_connection_passwd) -1);
	strncpy(s_connection_passwd, passwd, pass_len);
	s_connection_ssid[ssid_len] = 0;
	s_connection_passwd[pass_len] = 0;
}
*/

void wifi_disconnect(void) {
    if (!wifi_is_running())
        return;
    //return ESP_ERR_INVALID_STATE;
    stop();
    ESP_LOGI(TAG, "Disconnected");
}

bool wifi_is_running() {
    return NULL != s_connect_event_group;
}

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

esp_err_t wifi_connect(char *ssid, char *pass, unsigned int timeout_secs) {
    timeout_val = timeout_secs;

    ESP_LOGI(TAG, "In wifi_connect: ");
    if (wifi_is_running())
        return ESP_ERR_INVALID_STATE;

    s_connect_event_group = xEventGroupCreate();
    reset_counter("wifi connect 1");
    is_connected = false;

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    register_events();

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    wifi_config_t wifi_config = {0};
    strncpy((char *) &wifi_config.sta.ssid, ssid, min(sizeof(wifi_config.sta.ssid) - 1, strlen(ssid)));
    strncpy((char *) &wifi_config.sta.password, pass, min(sizeof(wifi_config.sta.password) - 1, strlen(pass)));

    ESP_LOGI(TAG, "Connecting to %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));

    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());
    //todo: this can be changed to a loop
    EventBits_t uxBits = xEventGroupWaitBits(s_connect_event_group, CONNECTED_BITS, true, true,
                                             timeout_val * 1000 / portTICK_PERIOD_MS);
    if ((uxBits & CONNECTED_BITS) > 0) {
        is_connected = true;
        ESP_LOGI(TAG, "IPv4 address: " IPSTR, IP2STR(&s_ip_addr));
#ifdef CONFIG_EXAMPLE_CONNECT_IPV6
        ESP_LOGI(TAG, "IPv6 address: " IPV6STR, IPV62STR(s_ipv6_addr));
#endif
        return ESP_OK;
    } else {
        stop();
        ESP_LOGE(TAG, "Wifi_Connect faailure, exit ");
        return ESP_FAIL;
    }
}
#undef min