//
// Created by asus on ۱۶/۰۲/۲۰۲۱.
//

#include "common.h"
#include <algorithm>
#include <cstdio>
#include <string>
#include <netdb.h>
#include <mutex>
#include "CoapClient.h"
#include "request.h"
#include <netdb.h>
#include <esp_log.h>
#include "coap3/coap.h"

#define PING_DEFAULT_SECONDS 90
#define FOR_EACH(list, el)  for(auto & el : (list))
#define FOR_EACHC(list, el) for(auto & el : (list))
#define TAG "CoAPClient"

bool quit_all_flag = false;
static std::mutex hold_requests;
static std::mutex hold_context;

void start_listener_task(void *c1);


static void set_blocksize(method_t method, coap_optlist_t *&optList, coap_block_t &block, size_t payload_len);

static inline int check_token(const coap_pdu_t *received, coap_binary_t the_token) {
    coap_bin_const_t rec_token = coap_pdu_get_token(received);
    return rec_token.length == the_token.length &&
           memcmp(rec_token.s, the_token.s, the_token.length) == 0;
}

Request *CoapClient::find_request_by_id(coap_session_t *session, const coap_pdu_t *pdu) {
    //todo: perhaps context and sessions should match too
    coap_mid_t mid = coap_pdu_get_mid(pdu);
    hold_requests.lock();
    FOR_EACHC(requests, it) {
        if (mid == it->getId()) {
            hold_requests.unlock();
            return it;
        }
    }
    hold_requests.unlock();
    return nullptr;
}


Request *CoapClient::find_request_by_token(coap_session_t *session, const coap_pdu_t *pdu) {
    hold_requests.lock();
    FOR_EACH(requests, it) {
        if (check_token(pdu, it->getToken())) {
            hold_requests.unlock();
            return it;
        }
    }
    hold_requests.unlock();
    return nullptr;
}


static coap_pdu_t *
coap_new_request(coap_context_t *ctx, coap_session_t *session, coap_pdu_code_t m, coap_optlist_t **options,
                 size_t length, unsigned char *data, coap_pdu_type_t msgtype, coap_binary_t &the_token,
                 bool data_in_blocks,
                 coap_block_t &block) {
    coap_pdu_t *pdu;
    (void) ctx;

    if (!(pdu = coap_new_pdu(msgtype, m, session)))
        return nullptr;

    /*pdu->type = msgtype;
    pdu->tid = coap_new_message_id(session);
    pdu->code = m;*/

    // token must be added before any option
    if (!coap_add_token(pdu, the_token.length, the_token.s)) {
        coap_log(LOG_DEBUG, "cannot add token to request\n");
    }

    if (options)
        coap_add_optlist_pdu(pdu, options);

    if (length) {
        if (!data_in_blocks)
            coap_add_data(pdu, length, data);
        else {
            unsigned char buf[4];
            coap_add_option(pdu,
                            COAP_OPTION_SIZE1,  // 60
                            coap_encode_var_safe(buf, sizeof(buf), length),
                            buf);

            coap_add_block(pdu, length, data, block.num, block.szx);
        }
    }
    return pdu;
}

coap_response_t response_handler(
        coap_session_t *session,
        const coap_pdu_t *sent,
        const coap_pdu_t *received,
        const coap_mid_t id) {
    coap_opt_t *block_opt;
    coap_opt_iterator_t opt_iter;

    coap_pdu_code_t rcv_code = coap_pdu_get_code(received);
    coap_pdu_type_t rcv_type = coap_pdu_get_type(received);
    //unsigned char buf[4];
    size_t len;
    const uint8_t *databuf;
    //printf("\n in(%u)->", received->tid);
    //coap_show_pdu(LOG_INFO, received);
    auto *cl = (CoapClient *) coap_session_get_app_data(session);

    Request *req = cl->find_request_by_token(session, received);
    if (!req)
        req = cl->find_request_by_id(session, received);

    if (!req) {
        if (!sent &&
            (rcv_type == COAP_MESSAGE_CON ||
             rcv_type == COAP_MESSAGE_NON)) {
            puts("sent rst for:");
            // this causes the server to forget about get observers
            //coap_send_rst(session, received);
            return COAP_RESPONSE_FAIL;
        }
        puts("request not found");
        return COAP_RESPONSE_OK;
    }
    assert(req);
    if (rcv_type == COAP_MESSAGE_RST) {
        // server did not recognize our pdu, so release request
        req->ready = true;
        printf("reset for req(%d)\n", req->id);
        coap_log(LOG_INFO, "got RST\n");
        return COAP_RESPONSE_OK;
    }

    // output the received data, if any
    if (COAP_RESPONSE_CLASS(rcv_code) == 2) {
        //coap_show_pdu(LOG_INFO, received);
        coap_get_data(received, &len, &databuf); //important: data might be an empty msg with len = 0
        req->handle_data(databuf, len);

        // Check if Block2 option is set
        block_opt = coap_check_option(received, COAP_OPTION_BLOCK2, &opt_iter);
        if (!req->getSingleBlockRequested() && block_opt) { // handle Block2
            // TODO: check if we are looking at the correct block number
            if (coap_opt_block_num(block_opt) == 0) {
                // See if
                // rve is set in first response
                req->ready = coap_check_option(received, COAP_OPTION_OBSERVE, &opt_iter) == nullptr;
            }
            return COAP_RESPONSE_OK;
        }
    } else {      // no 2.05
        if (COAP_RESPONSE_CLASS(rcv_code) >= 4) {
            int errNo = (rcv_code >> 5) * 100 + (rcv_code & 0x1F);
            req->setError("resource not found", errNo);
            if (coap_get_data(received, &len, &databuf)) {
                fprintf(stderr, " ");
                while (len--) {
                    fprintf(stderr, "%c", isprint(*databuf) ? *databuf : '.');
                    databuf++;
                }
            }
            fprintf(stderr, "\n");
        }
    }
    req->ready = coap_check_option(received, COAP_OPTION_OBSERVE, &opt_iter) == nullptr;
    return COAP_RESPONSE_OK;
}


coap_session_t *CoapClient::create_session(coap_address_t &src_addr) {
    coap_session_t *s = coap_new_client_session(ctx, nullptr, &src_addr, COAP_PROTO_UDP);
    if (s) {
        clear_error();
        coap_session_set_max_retransmit(s, 4);
        coap_session_set_app_data(s, this);
    }
    return s;
}

coap_session_t *CoapClient::create_session_psk(coap_address_t &dst_addr, const char *identity, uint8_t identity_len,
                                               const uint8_t *key,
                                               unsigned key_len, coap_proto_t proto) {
//    printf("ident(%s) key(%.*s)\n", identity,  key_len, key );

    static coap_dtls_cpsk_t dtls_psk;
    static char client_sni[256];
    coap_session_t *s;
    coap_context_set_block_mode(ctx,
                                COAP_BLOCK_USE_LIBCOAP | COAP_BLOCK_SINGLE_BODY);

#ifndef CONFIG_MBEDTLS_TLS_CLIENT
    ESP_LOGE(TAG, "MbedTLS (D)TLS Client Mode not configured");
        goto clean_up;
#endif
    memset(client_sni, 0, sizeof(client_sni));
    memset(&dtls_psk, 0, sizeof(dtls_psk));
    dtls_psk.version = COAP_DTLS_CPSK_SETUP_VERSION;
    dtls_psk.validate_ih_call_back = nullptr;
    dtls_psk.ih_call_back_arg = NULL;
    /*char uri_host[] = { "192.168.1.5" };
    memcpy(client_sni, uri_host, strlen(uri_host)); //

    dtls_psk.client_sni = client_sni;*/
    dtls_psk.psk_info.identity.s = (uint8_t *) identity;
    dtls_psk.psk_info.identity.length = identity_len;  // !!!! attention null should not count
    dtls_psk.psk_info.key.s = key;
    dtls_psk.psk_info.key.length = key_len; // !!!! attention null should not count
    clear_error();
    s = coap_new_client_session_psk2(ctx, nullptr, &dst_addr, proto,
                                     &dtls_psk);
    assert(s);
    if(s) {
        coap_session_set_max_retransmit(s, 4);
        coap_session_set_app_data(s, this);
    }
    return s;
}

coap_mid_t CoapClient::send(Request *req, coap_session_t *session) {
    // before sending should check if context listener has been created
    coap_mid_t tid = COAP_INVALID_TID;
    coap_pdu_t *pdu;
    uint8_t buf[4];

    assert(ctx);
    if (ctx == nullptr)
        return COAP_INVALID_TID;

    if (!req->isCancelObserve()) {
        if (req->IsBlockSizeSet())
            set_blocksize(req->method, req->options, req->block, req->payload.length);

        if (req->isObserve()) {
            if (req->the_token.length == 0) {
                req->the_token.
                        length = 8;
                coap_prng(req->the_token.s, req->the_token.length);
            }

            coap_insert_optlist(
                    &req->options,
                    coap_new_optlist(COAP_OPTION_OBSERVE,
                                     coap_encode_var_safe(buf, sizeof(buf), COAP_OBSERVE_ESTABLISH),
                                     buf
                    )
            );
        }
        pdu = coap_new_request(ctx,
                               session,
                               req->method,
                               &req->options,
                               req->payload.length,
                               req->payload.s,
                               req->confirmable ? COAP_MESSAGE_CON : COAP_MESSAGE_NON,
                               req->the_token,
                               req->IsBlockSizeSet(),
                               req->block
        );
        if (!pdu)
            goto error;

    } else //OBSERVE_CANCEL
    {
        pdu = coap_pdu_init(COAP_MESSAGE_CON,
                            COAP_REQUEST_CODE_GET,
                            coap_new_message_id(session),
                            coap_session_max_pdu_size(session));

        if (!coap_add_token(pdu, req->the_token.length, req->the_token.s)) {
            coap_log(LOG_DEBUG, "cannot add token to request\n");
        }

        /*for (coap_optlist_t *option = req->options; option; option = option->next) {
            switch (option->number) {
                case COAP_OPTION_URI_HOST : //  3
//                case COAP_OPTION_URI_QUERY : //  15
                    if (!coap_add_option(pdu, option->number, option->length, option->data)) {
                        puts("uri host error");
                        goto
                                error;
                    }
                    break;
                default:;
            }
        }*/
        coap_insert_optlist(
                &req->options,
                coap_new_optlist(COAP_OPTION_OBSERVE,
                                 coap_encode_var_safe(buf, sizeof(buf), COAP_OBSERVE_CANCEL),
                                 buf
                )
        );

        coap_add_optlist_pdu(pdu, &req->options);
        /*if (!coap_add_option(pdu,
                             COAP_OPTION_OBSERVE,  // 6
                             coap_encode_var_safe(buf,
                                                  sizeof(buf), COAP_OBSERVE_CANCEL),
                             buf)) {
            puts("cannot add COAP_OBSERVE_CANCEL to request");
            coap_log(LOG_DEBUG, "cannot add OBSERVE_CANCEL to request\n");
            goto error;
        }*/
        if (req->IsBlockSizeSet()) {
            req->block.num = 0;
            req->block.m = 0;
            coap_add_option(pdu,
                            COAP_OPTION_BLOCK2,  // 23
                            coap_encode_var_safe(buf,
                                                 sizeof(buf),
                                                 (req->block.num << 4 | req->block.m << 3 | req->block.szx)),
                            buf);
        }
        //coap_show_pdu(LOG_INFO, pdu);
    }

/*
	if (debug) {
		coap_show_pdu(LOG_INFO, pdu);
	}
*/
    hold_context.lock();
    tid = coap_send(session, pdu);
    hold_context.unlock();
    req->id = tid;
    if (tid > 0) {
        Request::reference(req);
        req->SessionReference();
        req->reset_start_time();
        hold_requests.lock();
        requests.push_back(req);
        hold_requests.unlock();
        return tid;
    }
    error:
    char buff[100];
    sprintf(buff, "Error tid(%d) type(%s)\n", tid, req->method == COAP_REQUEST_CODE_GET ? "get" : "post");
    req->setError(buff, SEND_PROBLEM);
    coap_delete_pdu(pdu);
    return tid+1;
}

static ulong id_gen = 1;

CoapClient::CoapClient(coap_nack_handler_t nackHandler, coap_event_handler_t eventHandler) {
    quit_all_flag = false;
    id = id_gen++;
    coap_set_log_level(LOG_INFO);
    ctx = coap_new_context(nullptr); // can listen to multiple ports

    assert(ctx);

    auto task_Created = xTaskCreate(start_listener_task,
                                    "start listener task", 4096,
                                    this, 5, &task_handle);
    if (task_Created != pdPASS) {
        ESP_LOGE(TAG, "can not create listener");
        error = 501;
        coap_free_context(ctx);
        ctx = nullptr;
        return;
    }

    coap_register_option(ctx, COAP_OPTION_BLOCK2);
    coap_register_event_handler(ctx, eventHandler);
    coap_register_nack_handler(ctx, nackHandler);
    coap_register_response_handler(ctx, response_handler);
    coap_context_set_keepalive(ctx, PING_DEFAULT_SECONDS);
    coap_register_pong_handler(ctx, [](coap_session_t *session,
                                       const coap_pdu_t *received,
                                       const coap_mid_t mid) {
        CoapClient *cl = (CoapClient *) coap_session_get_app_data(session);
        hold_requests.lock();
        FOR_EACH(cl->requests, r)
        {
            if (r->isObserve() && r->dynamic_time_out)
                r->reset_start_time();
        };
        hold_requests.unlock();
/*        time_t rawtime;
        struct tm * timeinfo;
        char buffer [80];
        time (&rawtime);
        timeinfo = localtime (&rawtime);
        strftime (buffer,80,"Pong %I:%M:%S.\n",timeinfo);
        printf("%s ",buffer);*/
    });
}

/* Called after processing the options from the commandline to set
 * Block1 or Block2 depending on method. */

static void set_blocksize(method_t method, coap_optlist_t *&optList, coap_block_t &block, size_t payload_len) {
    static unsigned char buf[4];        // hack: temporarily take encoded bytes
    uint16_t opt;
    unsigned int opt_length;

    if (method != COAP_REQUEST_DELETE) {
        opt = method == COAP_REQUEST_GET ? COAP_OPTION_BLOCK2 : COAP_OPTION_BLOCK1;

        block.m = (opt == COAP_OPTION_BLOCK1) &&
                  ((1ull << (block.szx + 4)) < payload_len);

        opt_length = coap_encode_var_safe(buf, sizeof(buf),
                                          (block.num << 4 | block.m << 3 | block.szx));

        coap_insert_optlist(&optList, coap_new_optlist(opt, opt_length, buf));
    }
}

void CoapClient::setPingInterval(size_t secs) {
    coap_context_set_keepalive(this->ctx, secs);
}

void CoapClient::quitAll() {
    //todo: lock, unlock
    quit_all_flag = true;
}

CoapClient::~CoapClient() {
    hold_context.lock();
    hold_requests.lock();
    if (task_handle)
        vTaskDelete(task_handle);

    coap_register_event_handler(ctx, nullptr);
    coap_register_nack_handler(ctx, nullptr);
    coap_register_response_handler(ctx, nullptr);
    coap_register_ping_handler(ctx, nullptr);
    coap_register_pong_handler(ctx, nullptr);


    FOR_EACHC(requests, r) {
        r->SessionRelease();
        Request::release(r); // deleting unfinished requests
    }
    requests.clear();
    coap_free_context(ctx);
    ctx = nullptr;
    hold_requests.unlock();
    hold_context.unlock();
}

CoapClient *CoapClient::reference(CoapClient *item) {
    ++item->ref;
    return item;
}

void CoapClient::release(CoapClient *item) {
    if (item) {
        assert(item->ref > 0);
        --item->ref;
        if (item->ref == 0)
            free(item);
    }
}

void CoapClient::free(CoapClient *item) {
    if (!item)
        return;
    assert(item->ref == 0);
    if (item->ref)
        return;
    delete item;
}


void start_listener_task(void *c1) {
    auto *client = (CoapClient *) c1;
    timeval now;
    while (!quit_all_flag) { // && client->requests.empty())
        client->counter++; // indicating loop is running
        hold_context.lock();
        coap_io_process(client->ctx, REQUEST_WAIT_TIME);
        //coap_run_once(client->ctx, REQUEST_WAIT_TIME);
        hold_context.unlock();
        gettimeofday(&now, nullptr);
        if (hold_requests.try_lock()) {
            auto it = client->requests.cbegin();
            while (it != client->requests.cend()) {
                Request *r = *it;
                if (!r->still_waiting(now)) {
                    it = client->requests.erase(it);
                    r->SessionRelease();
                    Request::release(r);// decrease ref Count for removing from the list
                } else
                    it++;
            }
            hold_requests.unlock();
        }
        vTaskDelay(10 / portTICK_RATE_MS);
    }
    puts("listener task closed!!!");
    client->task_handle = nullptr;
    CoapClient::release(client);
    vTaskDelete(nullptr);
}

/*
void CoapClient::setErrorHandler(CoAPErrorHandler handler) {
	CoapClient::error_handler = handler;
}
*/
