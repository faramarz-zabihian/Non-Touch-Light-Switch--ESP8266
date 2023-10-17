//
// Created by asus on 7/28/2021.
//
#ifndef COAP_CONFIG_H
#define COAP_CONFIG_H
#include <esp_log.h>
#include "CoapClient.h"
class CoAPConfig {
private:
    const char *address = nullptr;
private:
    coap_uri_t uri;
    coap_address_t dst_addr;
    coap_session_t *session = nullptr;
    CoapClient *client = nullptr;
public:
    CoapClient *getClient() const;
    const coap_uri_t &getUri() const;
    coap_session_t *getSession() const;
    void setClient(CoapClient *client);
    void setClient(CoapClient *client, const char *identity, const uint8_t *key, unsigned int key_len, coap_proto_t proto);
    CoAPConfig(const char *addr, bool reliable);
    CoAPConfig(CoapClient *client, const char *addr, bool reliable);
    CoAPConfig(CoapClient *client, const char *addr, const char *identity, const uint8_t *key, unsigned int key_len,
               coap_proto_t proto, bool reliable);
    ~CoAPConfig();
};
#endif //COAP_CONFIG_H
