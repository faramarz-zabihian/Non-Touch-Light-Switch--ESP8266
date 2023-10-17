//
// Created by asus on ۲۴/۰۲/۲۰۲۱.
//
#include "CoapClient.h"
#include "request.h"
#include "coap_utils.h"
#include <algorithm>
#include <cmath>
#include <esp_timer.h>
#include <esp_log.h>
#include <list>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstring>
#include <netdb.h>

#define FLAGS_BLOCK 0x01
// Request is a pure user_id representation of a request, which might be as new Request("coap:/192.../p1/p2/res
// upon sending properries
//
// 1- simple getters/setters
// 2- complex getters/setters
// 3- static methods
// 4- constructors, object methods

Request::~Request() {
    clear_payload();
    if (options)
        coap_delete_optlist(options);
}


Request::Request(coap_pdu_code_t method) {
    this->method = method;
}

Request::Request(coap_pdu_code_t method, bool confirmable) : Request(method) {
    this->confirmable = confirmable;
}

Request *Request::new_get() {
    return reference(new Request(COAP_REQUEST_CODE_GET));
}

Request *Request::new_post() {
    return reference(new Request(COAP_REQUEST_CODE_POST));
}

Request *Request::set_confirmable(bool confirmable) {
    this->confirmable = confirmable;
    return this;
}

Request *Request::add_token(coap_binary_t *token) {
    memset((char *) the_token.s, 0, sizeof(_token_data));
    the_token.length = std::min(sizeof(_token_data), token->length);
    if (the_token.length > 0) {
        memcpy((char *) the_token.s, token->s, the_token.length);
    }
    return this;
}

void Request::clear_payload() {
    if (payload.s) {
        std::free(payload.s);
        payload.s = nullptr;
        payload.length = 0;
    }
}

Request *Request::set_payload(const unsigned char *text, size_t len) {

    clear_payload();
    if (len <= 0 || text == nullptr)
        return this;
    payload.length = len;
    payload.s = (unsigned char *) std::malloc(len);
    assert(payload.s);
    memcpy(payload.s, text, len);
    return this;
}

void Request::add_option(int option, unsigned char data[], size_t size) {
    coap_insert_optlist(&options, coap_new_optlist(option, size, data));
}

void Request::setError(const char *message, uint errNo) {
    error_no = errNo;
    if (errorHandler)
        errorHandler(message, errNo);
}

void Request::handle_data(const uint8_t *string, size_t len) {
    if (isObserve()) {
        if (dynamic_time_out)
            reset_start_time();
    } else
        ready = true;

    if (responseHandler && len > 0)
        responseHandler(string, len);
}


Request *Request::Handler(response_handler_t handler) {
    responseHandler = handler;
    return this;
}

int Request::getId() const {
    return id;
}

void Request::setId(int tid) {
    Request::id = tid;
}

Request *Request::OnError(void (*pFunction)(const char *, uint)) {
    errorHandler = pFunction;
    return this;
}

Request *Request::setUrl(const char *address) {
    if (options)
        coap_delete_optlist(options);
    get_uri_options(address, &options);
    return this;
}

int Request::SendAndWait() {
    int id = SendAndLeave();
    if (id <0)
        return id;

    timeval now;
    gettimeofday(&now, 0);
    while (still_waiting(now)) {
        vTaskDelay(REQUEST_WAIT_TIME / portTICK_RATE_MS);
        gettimeofday(&now, 0);
    }
    return id;
}


int Request::SendAndLeave() {
    int id = COAP_INVALID_TID;

    if (hasError())
        return id;

    if (isObserve())
        confirmable = true;

    if (!client) {
        puts("getClient was null!!");
    }

    if (client->has_error())
        return id;

    if (!session) {
        ESP_LOGE(__FUNCTION__ , "null session");
        return id;
    }
    else  {
        id = client->send(this, session);
    }
    return id;
}

const coap_binary_t &Request::getToken() const {
    return the_token;
}

inline bool Request::isReliable() const {
    return Request::reliable;
}

inline void Request::setReliable(bool reliable) {
    Request::reliable = reliable;
}

bool Request::isObserve() const {
    return (get_method() == COAP_REQUEST_CODE_GET) && (m_observe_state == MY_ESTABLISH);
}
bool Request::isCancelObserve() const {
    return (get_method() == COAP_REQUEST_CODE_GET) && (m_observe_state == MY_CANCEL);
}
bool Request::IsBlockSizeSet() {
    return (flags & FLAGS_BLOCK) == FLAGS_BLOCK;
}

bool Request::hasError() {
    return error_no != 0;
}


static inline int cmp_token(coap_binary_t t0, coap_binary_t t1) {
    return t0.length == t1.length && memcmp(t0.s, t1.s, t1.length) == 0;
}


static inline int check_token(coap_pdu_t *received, coap_binary_t the_token) {
    coap_bin_const_t r_token = coap_pdu_get_token(received);
    return r_token.length == the_token.length &&
           memcmp(r_token.s, the_token.s, the_token.length) == 0;
}

Request *Request::setSession(coap_session_t *session) {
    assert(session);
    Request::session = session;
    Request::client = get_session_client(session);
    return this;
}

Request *Request::Observe(OBSERVE_STATE state) {
    if (method != COAP_REQUEST_CODE_GET)
        return this;
    m_observe_state = state;
    return this;
}

/*
void Request::setUser(unsigned char *user, ssize_t len) {
    Request::user = coap_new_str_const(user, len);
}
*/

/*void Request::setPSK(unsigned char *psk, ssize_t len) {
    Request::psk_key = coap_new_str_const(psk, len);
}*/



Request *Request::setBlockSize(uint16_t size) {
    block.num = size;
    if (size) {
        block.szx = (coap_fls(size >> 4) - 1) & 0x07;
        single_block_requested = 1;
    }
    flags |= FLAGS_BLOCK;
    return this;
}

Request *Request::Timeout(uint16_t seconds) {
    timeout_val = seconds; // to micros
    timeout_set = true;
    return this;
}

bool Request::is_expired(timeval time) {
    if (isObserve() && !timeout_set)
        return false;
    // caution do not subtract unsigned ints
    return (time.tv_sec * 1000 + time.tv_usec / 1000) > (start_time + (timeout_val * 1000)); // cmparing two positives
}

void Request::reset_start_time() {
    timeval now;
    gettimeofday(&now, 0);
    start_time = now.tv_sec * 1000 + now.tv_usec / 1000;
}

Request *Request::reference(Request *item) {
    ++item->ref;
    return item;
}

void Request::release(Request *item) {
    if (item) {
        assert(item->ref > 0);
        --item->ref;
        if (item->ref == 0) {
            free(item);
        }
    }
}

void Request::free(Request *item) {
    if (!item)
        return;
    assert(item->ref == 0);
    if (item->ref)
        return;
    delete item;
}

/*Request *Request::getObservationCancelRequest() {
    Request *r = new_get()
            ->setUrl(address)
            ->Observe(MY_CANCEL)
            ->add_token(&the_token);
    r->session = session;
    return r;
}*/

bool Request::still_waiting(timeval & time) {
    return (!ready && !hasError() && !is_expired(time) && !client->has_error());
}

void Request::SessionReference() {
    coap_session_reference(session);
}

void Request::SessionRelease() {
    coap_session_release(session);
}
