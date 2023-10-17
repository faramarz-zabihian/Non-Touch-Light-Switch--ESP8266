//
// Created by asus on ۱۶/۰۲/۲۰۲۱.
//
#include <coap3/coap.h>
#include <string>
#include "common.h"
#include <mutex>
#include <list>


#ifndef AIVA_COAPCLIENT_H
#define AIVA_COAPCLIENT_H
class Request;

class CoapClient {
private:
    ulong id;
    int counter = 0;
    int reliable = 0;
    bool error = false;
    coap_context_t *ctx = nullptr;
    bool is_listening = false;
    TaskHandle_t task_handle = nullptr;
    std::list<Request *> requests;

    Request *find_request_by_id(coap_session_t *session, const coap_pdu_t *pdu);

    Request *find_request_by_token(coap_session_t *session, const coap_pdu_t *pdu);

    friend void start_listener_task(void *c1);

    friend coap_response_t response_handler(coap_session_t *session,
                                 const coap_pdu_t *sent,
                                 const coap_pdu_t *received,
                                 const coap_mid_t id);


public:

    bool inline has_error() { return error; }
    void inline raise_error() { error = true; }
    void inline clear_error() { error = false; }

    ~CoapClient();

    static CoapClient *reference(CoapClient *item);

    static void release(CoapClient *item);

    static void free(CoapClient *item);

    CoapClient(coap_nack_handler_t nackHandler, coap_event_handler_t eventHandler);

    static void quitAll();

    coap_session_t *create_session(coap_address_t &src_addr);

    coap_mid_t send(Request *req, coap_session_t *session);

    void setPingInterval(size_t secs);

    uint8_t ref = 0;

    coap_session_t *
    create_session_psk(coap_address_t &dst_addr, const char *identity, uint8_t identity_len, const uint8_t *key,
                       unsigned key_len, coap_proto_t proto);
};

/*class CoAPSession
{
    coap_session_t * session;
    CoapClient *context;
};*/
#endif //AIVA_COAPCLIENT_H