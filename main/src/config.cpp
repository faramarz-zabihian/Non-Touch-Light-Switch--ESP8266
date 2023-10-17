#include <CoapClient.h>
#include <CoAPConfig.h>
#include <connect.h>
#include <config.h>
#include <coap/include/coap_utils.h>
#include <include/MonitorMemory.h>
#include <include/app_utils.h>
//
// Created by asus on 8/30/2021.
//

#define TAG "main_config"
static CoapClient *coap_client = nullptr;
static coap_session_t *session = nullptr;
static char *report_resource_url = nullptr;
static char *observation_resource_url = nullptr;
static char *host_resource_url = nullptr;

user_pass_t connection_data;
#define HOST_URL "coaps://%s"
#define POST_URL    "%s/device?report"
#define OBSERVE_URL "%s/device?%s"


production_struct device_keys;

bool ExecuteDelayedTask(void (*func)(), uint8_t seconds, uint8_t stkBlocks, const char *name);

#define printAlloc(b, t, f, ...)                  \
    do {                                          \
        memset(buff, 0, sizeof(b));               \
        sprintf(b, f, __VA_ARGS__);                       \
        t = (char *) calloc(strlen(b) + 1, 1);  \
        memcpy(t, b, strlen(b));                  \
    } while(0);

void load_setup_data() {
}

void config_build_url_variables() {
    char buff[96];
    // host_resource_url should be first
    printAlloc(buff, host_resource_url, HOST_URL, connection_data.serverAddress);
    printAlloc(buff, observation_resource_url, OBSERVE_URL, host_resource_url, device_keys.identity);
    printAlloc(buff, report_resource_url, POST_URL, host_resource_url);
}

#undef printAlloc

const char *get_host_resource_url() {
    return host_resource_url;
}

const char *get_observation_resource_url() {
    return observation_resource_url;
}

const char *get_report_resource_url() {
    return report_resource_url;
}


static void nackHandler(coap_session_t *session,
                        const coap_pdu_t *sent,
                        coap_nack_reason_t reason,
                        const coap_mid_t id
) {
    switch (reason) {
        case COAP_NACK_TOO_MANY_RETRIES:
            ESP_LOGD(TAG, "COAP_NACK_TOO_MANY_RETRIES");
            break;

        case COAP_NACK_NOT_DELIVERABLE:
            ESP_LOGI(TAG, "COAP_NACK_NOT_DELIVERABLE");
            break;
        case COAP_NACK_RST:
            ESP_LOGI(TAG, "COAP_NACK_RST");
            break;
        case COAP_NACK_TLS_FAILED:
            ESP_LOGI(TAG, "COAP_NACK_TLS_FAILED");
            break;
        case COAP_NACK_ICMP_ISSUE:
            ESP_LOGI(TAG, "COAP_NACK_ICMP_ISSUE");
            break;
    }
    CoapClient *cl = (CoapClient *) coap_session_get_app_data(session);
    cl->raise_error();
};

static int ev_handler(coap_session_t *session, coap_event_t event) {
    //CoapClient *cl = (CoapClient *) coap_session_get_app_data(session);
    switch (event) {
        case COAP_EVENT_DTLS_CONNECTED:
            ESP_LOGI(TAG, "COAP_EVENT_DTLS_CONNECTED");
            break;
        case COAP_EVENT_DTLS_RENEGOTIATE:
            ESP_LOGI(TAG, "COAP_EVENT_DTLS_RENEGOTIATE");
            break;
/*.........................*/
        case COAP_EVENT_DTLS_ERROR:
            ESP_LOGI(TAG, "COAP_EVENT_DTLS_ERROR");
//            cl->raise_error();
            break;
        case COAP_EVENT_TCP_CONNECTED:
            ESP_LOGI(TAG, "COAP_EVENT_TCP_CONNECTED");
            break;
/*.........................*/
        case COAP_EVENT_TCP_FAILED:
            ESP_LOGI(TAG, "COAP_EVENT_TCP_FAILED");
            //cl->raise_error();
            break;
        case COAP_EVENT_DTLS_CLOSED:
            ESP_LOGI(TAG, "COAP_EVENT_DTLS_CLOSED");
      //      cl->raise_error();
            break;
        case COAP_EVENT_TCP_CLOSED:
            ESP_LOGI(TAG, "COAP_EVENT_TCP_CLOSED");
//            cl->raise_error();
            break;
/*.........................*/
        case COAP_EVENT_SESSION_CONNECTED:
            ESP_LOGI(TAG, "COAP_EVENT_SESSION_CONNECTED");
            break;
        case COAP_EVENT_SESSION_CLOSED:
            ESP_LOGI(TAG, "COAP_EVENT_SESSION_CLOSED");
            break;
        case COAP_EVENT_SESSION_FAILED    :
            ESP_LOGI(TAG, "COAP_EVENT_SESSION_FAILED");
            //cl->raise_error();
            break;
        default:
            ESP_LOGI(TAG, "nack event handler(%d)\n", event);
            break;
    }
    return 0;
}

static int validate_protocol_scheme(coap_uri_t &uri, bool reliable) {
    if (uri.scheme == COAP_URI_SCHEME_COAPS && !reliable && !coap_dtls_is_supported()) {
        ESP_LOGE(__FUNCTION__, "URI_SCHEME");
        return -1;
    } else if ((uri.scheme == COAP_URI_SCHEME_COAPS_TCP || (uri.scheme == COAP_URI_SCHEME_COAPS && reliable)) &&
               !coap_tls_is_supported()) {
        ESP_LOGE(__FUNCTION__, "URI_SCHEME");
        return -1;
    }
    return 0;
}


void create_coap_session(bool reliable) {
    static coap_address_t dst_addr;
    static coap_uri_t uri;

    memset(&uri, 0, sizeof(uri));
    const char *host_url = get_host_resource_url();
    if (coap_split_uri((uint8_t *) host_url, strlen(host_url), &uri) < 0) {
        ESP_LOGE(__FUNCTION__, "URI_ERROR");
        return;
    }

    if (validate_protocol_scheme(uri, reliable) != 0)
        return;

    char *host_name = (char *) calloc(1, uri.host.length + 1);
    if (host_name == NULL) {
        ESP_LOGE(TAG, "calloc failed");
        return;
    }
    memcpy(host_name, uri.host.s, uri.host.length);

    if (resolve_address(coap_make_str_const(host_name), dst_addr, uri.port) != 0) {
        free(host_name);
        ESP_LOGE(__FUNCTION__, "ADDRESS NOT RESOLVED");
        return;
    }
    free(host_name);
    session = coap_client->create_session_psk(dst_addr, device_keys.identity, strlen(device_keys.identity),
                                              connection_data.devicePass, 16, COAP_PROTO_DTLS);
    //coap_session_send_ping(session); // force communication
    if (!session)
        ESP_LOGE(TAG, "failure creating session");
}


static std::mutex hold_session;

void clear_session() {
    if (!session)
        return;
    hold_session.lock();
    coap_session_release(session);
    session = nullptr;
    hold_session.unlock();
}


static inline CoapClient *get_client() {
//    puts("new client being created");
    CoapClient *client = new CoapClient(nackHandler, ev_handler);
    client->setPingInterval(90); // connectes to server
    if (client->has_error()) {
        delete client;
        return nullptr;
    }
    return client;
}

void reset_connection_data() {
    memset(&connection_data, 0, sizeof(connection_data));
}

coap_session_t *get_session() {
    if (session)
        return session;

    if (coap_client && coap_client->has_error()) {
        delete coap_client;
        coap_client = nullptr;
    }

    if (!coap_client) {
        coap_client = get_client();
        if (!coap_client)
            return nullptr;
    }

    if (hold_session.try_lock()) {
        if (session) // in case another thread has created session
            hold_session.unlock();
        else
            ExecuteDelayedTask([]() {
                create_coap_session(false);
                hold_session.unlock();
            }, 0, 6, "create session");
    }

    return
            session;
}




/*
extern  "C" void print_address(coap_address_t *dst)
{
    char addr[16];
    inet_ntop(AF_INET, &dst->addr.sin.sin_addr, addr, sizeof(addr));
    printf("inet_ntop address: %s\n", addr);
    printf("inet_ntoa address: %s\n", inet_ntoa(dst->addr.sin.sin_addr));
}
*/

