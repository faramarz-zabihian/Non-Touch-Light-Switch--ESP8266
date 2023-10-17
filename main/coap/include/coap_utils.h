//
// Created by asus on ۲۵/۰۲/۲۰۲۱.
//
#ifndef COAP_UTILS_H
#define COAP_UTILS_H
#include "coap3/coap.h"

int resolve_address(const coap_str_const_t *server, coap_address_t &dst_addr, uint16_t port);
coap_optlist_t * process_uri(coap_uri_t &uri, int create_uri_opts_port);
bool is_session_established(coap_session_t *session);
int get_uri_options(const char *address, coap_optlist_t **optList);
#endif //COAP_UTILS_H
