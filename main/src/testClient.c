//
// Created by asus on 9/5/2021.
//

/* CoAP client Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

/*
 * WARNING
 * libcoap is not multi-thread safe, so only this thread must make any coap_*()
 * calls.  Any external (to this thread) data transmitted in/out via libcoap
 * therefore has to be passed in/out by xQueue*() via this thread.
 */

#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/param.h>
#include <esp_netif.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"
#include "coap3/coap.h"

#define COAP_DEFAULT_TIME_SEC 10
#define EXAMPLE_COAP_PSK_KEY "0123456789123457"
#define EXAMPLE_COAP_PSK_IDENTITY "far"
#define COAP_DEFAULT_DEMO_URI "coaps://192.168.1.5/time"

const static char *TAG = "CoAP_client";

static int resp_wait = 1;
static coap_optlist_t *optlist = NULL;
static int wait_ms;

static coap_response_t
message_handler(coap_session_t *session,
				const coap_pdu_t *sent,
				const coap_pdu_t *received,
				const coap_mid_t mid) {
	const unsigned char *data = NULL;
	size_t data_len;
	size_t offset;
	size_t total;
	coap_pdu_code_t rcvd_code = coap_pdu_get_code(received);

	if (COAP_RESPONSE_CLASS(rcvd_code) == 2) {
		if (coap_get_data_large(received, &data_len, &data, &offset, &total)) {
			if (data_len != total) {
				printf("Unexpected partial data received offset %u, length %u\n", offset, data_len);
			}
			printf("Received:\n%.*s\n", (int) data_len, data);
			resp_wait = 0;
		}
		return COAP_RESPONSE_OK;
	}
	printf("%d.%02d", (rcvd_code >> 5), rcvd_code & 0x1F);
	if (coap_get_data_large(received, &data_len, &data, &offset, &total)) {
		printf(": ");
		while (data_len--) {
			printf("%c", isprint(*data) ? *data : '.');
			data++;
		}
	}
	printf("\n");
	resp_wait = 0;
	return COAP_RESPONSE_OK;
}

static void
coap_log_handler (coap_log_t level, const char *message)
{
	uint32_t esp_level = ESP_LOG_INFO;
	char *cp = strchr(message, '\n');

	if (cp)
		ESP_LOG_LEVEL(esp_level, TAG, "%.*s", (int)(cp-message), message);
	else
		ESP_LOG_LEVEL(esp_level, TAG, "%s", message);
}

static coap_address_t *
coap_get_address(coap_uri_t *uri)
{
	static coap_address_t dst_addr;
	char *phostname = NULL;
	struct addrinfo hints;
	struct addrinfo *addrres;
	int error;
	char tmpbuf[INET6_ADDRSTRLEN];

	phostname = (char *) calloc(1, uri->host.length + 1);
	if (phostname == NULL) {
		ESP_LOGE(TAG, "calloc failed");
		return NULL;
	}
	memcpy(phostname, uri->host.s, uri->host.length);

	memset((char *) &hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_family = AF_UNSPEC;

	error = getaddrinfo(phostname, NULL, &hints, &addrres);
	if (error != 0) {
		ESP_LOGE(TAG, "DNS lookup failed for destination address %s. error: %d", phostname, error);
		free(phostname);
		return NULL;
	}
	if (addrres == NULL) {
		ESP_LOGE(TAG, "DNS lookup %s did not return any addresses", phostname);
		free(phostname);
		return NULL;
	}
	free(phostname);
	coap_address_init(&dst_addr);
	switch (addrres->ai_family) {
		case AF_INET:
			memcpy(&dst_addr.addr.sin, addrres->ai_addr, sizeof(dst_addr.addr.sin));
			dst_addr.addr.sin.sin_port = htons(uri->port);
			inet_ntop(AF_INET, &dst_addr.addr.sin.sin_addr, tmpbuf, sizeof(tmpbuf));
			ESP_LOGI(TAG, "DNS lookup succeeded. IP=%s", tmpbuf);
			break;
		case AF_INET6:
			memcpy(&dst_addr.addr.sin6, addrres->ai_addr, sizeof(dst_addr.addr.sin6));
			dst_addr.addr.sin6.sin6_port = htons(uri->port);
			inet_ntop(AF_INET6, &dst_addr.addr.sin6.sin6_addr, tmpbuf, sizeof(tmpbuf));
			ESP_LOGI(TAG, "DNS lookup succeeded. IP=%s", tmpbuf);
			break;
		default:
			ESP_LOGE(TAG, "DNS lookup response failed");
			return NULL;
	}
	freeaddrinfo(addrres);

	return &dst_addr;
}

static int
coap_build_optlist(coap_uri_t *uri)
{
#define BUFSIZE 40
	unsigned char _buf[BUFSIZE];
	unsigned char *buf;
	size_t buflen;
	int res;

	optlist = NULL;

	if (uri->scheme == COAP_URI_SCHEME_COAPS && !coap_dtls_is_supported()) {
		ESP_LOGE(TAG, "MbedTLS (D)TLS Client Mode not configured");
		return 0;
	}
	if (uri->scheme == COAP_URI_SCHEME_COAPS_TCP && !coap_tls_is_supported()) {
		ESP_LOGE(TAG, "CoAP server uri->+tcp:// scheme is not supported");
		return 0;
	}

	if (uri->path.length) {
		buflen = BUFSIZE;
		buf = _buf;
		res = coap_split_path(uri->path.s, uri->path.length, buf, &buflen);

		while (res--) {
			coap_insert_optlist(&optlist,
								coap_new_optlist(COAP_OPTION_URI_PATH,
												 coap_opt_length(buf),
												 coap_opt_value(buf)));

			buf += coap_opt_size(buf);
		}
	}

	if (uri->query.length) {
		buflen = BUFSIZE;
		buf = _buf;
		res = coap_split_query(uri->query.s, uri->query.length, buf, &buflen);

		while (res--) {
			coap_insert_optlist(&optlist,
								coap_new_optlist(COAP_OPTION_URI_QUERY,
												 coap_opt_length(buf),
												 coap_opt_value(buf)));

			buf += coap_opt_size(buf);
		}
	}
	return 1;
}

static coap_session_t *
coap_start_psk_session(coap_context_t *ctx, coap_address_t *dst_addr, coap_uri_t *uri)
{
	static coap_dtls_cpsk_t dtls_psk;
	static char client_sni[256];

	memset(client_sni, 0, sizeof(client_sni));
	memset(&dtls_psk, 0, sizeof(dtls_psk));
	dtls_psk.version = COAP_DTLS_CPSK_SETUP_VERSION;
	dtls_psk.validate_ih_call_back = NULL;
	dtls_psk.ih_call_back_arg = NULL;
	if (uri->host.length)
		memcpy(client_sni, uri->host.s, MIN(uri->host.length, sizeof(client_sni) - 1));
	else
		memcpy(client_sni, "localhost", 9);
	dtls_psk.client_sni = client_sni;
	dtls_psk.psk_info.identity.s = (const uint8_t *) EXAMPLE_COAP_PSK_IDENTITY;
	dtls_psk.psk_info.identity.length = sizeof(EXAMPLE_COAP_PSK_IDENTITY) - 1;
	dtls_psk.psk_info.key.s = (const uint8_t *) EXAMPLE_COAP_PSK_KEY;
	dtls_psk.psk_info.key.length = sizeof(EXAMPLE_COAP_PSK_KEY) - 1; // sizeof = strlen(s)+ 1
	return coap_new_client_session_psk2(ctx, NULL, dst_addr,
										uri->scheme == COAP_URI_SCHEME_COAPS ? COAP_PROTO_DTLS : COAP_PROTO_TLS,
										&dtls_psk);
}
extern bool WIFI_IS_READY();
#define WAIT_FOR_NETWORK() while(!WIFI_IS_READY()) { vTaskDelay(5000 / portTICK_PERIOD_MS); }

void coap_example_client(void *p) {
	coap_address_t *dst_addr;
	static coap_uri_t uri;
	const char       *server_uri = COAP_DEFAULT_DEMO_URI;
	coap_context_t *ctx = NULL;
	coap_session_t *session = NULL;
	coap_pdu_t *request = NULL;
	unsigned char token[8];
	size_t tokenlength;
#if defined(CONFIG_MBEDTLS_DYNAMIC_BUFFER)
#error example wont work atleast in psk mode
	/*CONFIG_MBEDTLS_DYNAMIC_FREE_PEER_CERT=y
    CONFIG_MBEDTLS_DYNAMIC_FREE_CONFIG_DATA=y*/
#endif



    WAIT_FOR_NETWORK();
	printf("start heap(%d)\n", esp_get_free_heap_size());
	/* Set up the CoAP logging */
	coap_set_log_handler(coap_log_handler);
	coap_set_log_level(0);

	/* Set up the CoAP context */
	ctx = coap_new_context(NULL);
	if (!ctx) {
		ESP_LOGE(TAG, "coap_new_context() failed");
		goto clean_up;
	}

	coap_context_set_block_mode(ctx,
								COAP_BLOCK_USE_LIBCOAP | COAP_BLOCK_SINGLE_BODY);

	coap_register_response_handler(ctx, message_handler);

	if (coap_split_uri((const uint8_t *) server_uri, strlen(server_uri), &uri) == -1) {
		ESP_LOGE(TAG, "CoAP server uri error");
		goto clean_up;
	}
	if (!coap_build_optlist(&uri))
		goto clean_up;

	dst_addr = coap_get_address(&uri);
	if (!dst_addr)
		goto clean_up;

	/*
	 * Note that if the URI starts with just coap:// (not coaps://) the
	 * session will still be plain text.
	 *
	 * coaps+tcp:// is NOT yet supported by the libcoap->mbedtls interface
	 * so COAP_URI_SCHEME_COAPS_TCP will have failed in a test above,
	 * but the code is left in for completeness.
	 */
	printf("after context heap(%d)\n", esp_get_free_heap_size());
	session = coap_start_psk_session(ctx, dst_addr, &uri);
	printf("after session heap(%d)\n", esp_get_free_heap_size());
	if (!session) {
		ESP_LOGE(TAG, "coap_new_client_session() failed");
		goto clean_up;
	}
	while (1) {

		request = coap_new_pdu(coap_is_mcast(dst_addr) ? COAP_MESSAGE_NON : COAP_MESSAGE_CON,
							   COAP_REQUEST_CODE_GET, session);
		if (!request) {
			ESP_LOGE(TAG, "coap_new_pdu() failed");
			goto clean_up;
		}
		/* Add in an unique token */
		coap_session_new_token(session, &tokenlength, token);
		coap_add_token(request, tokenlength, token);

		coap_add_optlist_pdu(request, &optlist);

		resp_wait = 1;

		WAIT_FOR_NETWORK();
		printf("before send heap(%d)\n", esp_get_free_heap_size());
		coap_mid_t id = coap_send(session, request);
		printf("id(%d)\n", id);

		wait_ms = COAP_DEFAULT_TIME_SEC * 1000;
		if (id != COAP_INVALID_MID)
			while (resp_wait) {
				//vTaskDelay(200/portTick)
				printf("before io(%d)\n", esp_get_free_heap_size());
				//int result = coap_io_process(ctx, wait_ms > 1000 ? 1000 : wait_ms);
				int result = coap_io_process(ctx, wait_ms > 5000 ? 5000 : wait_ms);
				if (result >= 0) {
					if (result >= wait_ms) {
						ESP_LOGE(TAG, "No response from server");
						break;
					} else {
						wait_ms -= result;
					}
				}
			}
		for(int countdown = 3; countdown >= 0; countdown--) {
			ESP_LOGI(TAG, "%d... ", countdown);
			vTaskDelay(1000 / portTICK_PERIOD_MS);
		}
		ESP_LOGI(TAG, "Starting again!");
	}

	clean_up:
	if (optlist) {
		coap_delete_optlist(optlist);
		optlist = NULL;
	}
	if (session) {
		coap_session_release(session);
	}
	if (ctx) {
		coap_free_context(ctx);
	}
	coap_cleanup();

	ESP_LOGI(TAG, "Finished");
	vTaskDelete(NULL);
}
