//
// Created by asus on ۲۳/۰۴/۲۰۲۱.
//

#pragma once

#ifdef __cplusplus
extern "C" {
#endif


#include "esp_err.h"
#include "tcpip_adapter.h"
#include <event_groups.h>

#define EXAMPLE_INTERFACE TCPIP_ADAPTER_IF_STA

/**
 * @brief Configure Wi-Fi or Ethernet, connect, wait for IP
 *
 * This all-in-one helper function is used in protocols examples to
 * reduce the amount of boilerplate in the example.
 *
 * It is not intended to be used in real world applications.
 * See examples under examples/wifi/getting_started/ and examples/ethernet/
 * for more complete Wi-Fi or Ethernet initialization code.
 *
 * Read "Establishing Wi-Fi or Ethernet Connection" section in
 * examples/protocols/README.md for more information about this function.
 *
 * @return ESP_OK on successful connection
 */

#define APP_WIFI_EVENT_CONNECTED (1ul << 0)
#define APP_WIFI_EVENT_DISCONNECTED (1ul << 1)
#define APP_WIFI_EVENT_GOT_IP4  (1ul<< 2)
#define APP_WIFI_EVENT_GOT_IP6  (1ul<<3)

typedef void (*app_connection_status)(int state);
typedef  struct {
    char ssid[33];
    char passwd[33];
    char serverAddress[33];
    uint8_t devicePass[33];
    uint8_t devicePassLen;
} user_pass_t;

struct production_struct
{
    char model[21]; // esp8266-S12
    char serial[13]; // production serial no
    char firmware[13]; // software app id
    char application[17]; // software version id
    char aeskey[17];
    char identity[18];
};

esp_err_t wifi_connect(char *ssid, char *pass, unsigned int timeout_secs);

/**
 * @brief Configure stdin and stdout to use blocking I/O
 *
 * This helper function is used in ASIO examples. It wraps installing the
 * UART driver and configuring VFS layer to use UART driver for console I/O.
 */
esp_err_t example_configure_stdin_stdout(void);

/**
 * @brief Configure SSID and password
 */

typedef void (* user_pass_handler_t)(user_pass_t *) ;
//esp_connection_info_t
void wifi_set_connection_info(const char *ssid, const char *passwd);
void app_smart_config(user_pass_handler_t saver);
bool smartConfig_is_running();
bool WIFI_IS_READY();
bool wifi_is_running();
void wifi_disconnect(void);

#ifdef __cplusplus
}
#endif
