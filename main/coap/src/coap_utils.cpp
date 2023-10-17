//
// Created by asus on 9/9/2021.
//

#include <coap3/coap.h>
#include <lwip/netdb.h>
#include <../include/coap_utils.h>
#include <esp_log.h>

#define BUFSIZE 100

static uint16_t get_default_port(const coap_uri_t *u) {
    return coap_uri_scheme_is_secure(u) ? COAPS_DEFAULT_PORT : COAP_DEFAULT_PORT;
}

bool is_session_established(coap_session_t *session)
{
    return coap_session_get_state(session) ==  COAP_SESSION_STATE_ESTABLISHED;
}

int resolve_address(const coap_str_const_t *server, coap_address_t &dst_addr, uint16_t port) {

    struct addrinfo *ainfo;
    struct addrinfo hints;
    static char addrstr[256];
    int error;

    memset(addrstr, 0, sizeof(addrstr));
    if (server->length)
        memcpy(addrstr, server->s, server->length);
    else
        memcpy(addrstr, "localhost", 9);

    memset((char *) &hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_family = AF_UNSPEC;

    error = getaddrinfo(addrstr, NULL, &hints, &ainfo);
    if (error != 0) {
        printf("error in socket getaddrinfo %d", error);
        return -1;
    }

    coap_address_init(&dst_addr);
    dst_addr.size = ainfo->ai_addrlen;
    memcpy(&dst_addr.addr, ainfo->ai_addr, ainfo->ai_addrlen);
    if (ainfo->ai_family == AF_INET6) {
        dst_addr.addr.sin6.sin6_family = AF_INET6;
        dst_addr.addr.sin6.sin6_port = htons(port);
    } else {
        dst_addr.addr.sin.sin_family = AF_INET;
        dst_addr.addr.sin.sin_port = htons(port);
    }
    dst_addr.size = ainfo->ai_addrlen;
    freeaddrinfo(ainfo);
    return 0;
}

coap_optlist_t *process_uri(coap_uri_t &uri, int create_uri_opts_port) {
    unsigned char portbuf[2];
    unsigned char _buf[BUFSIZE];
    unsigned char *buf = _buf;
    size_t buflen;
    int res;
    coap_optlist_t *optlist = NULL;
    if (uri.port != get_default_port(&uri) && create_uri_opts_port) {
        coap_insert_optlist(&optlist,
                            coap_new_optlist(COAP_OPTION_URI_PORT,
                                             coap_encode_var_safe(portbuf, sizeof(portbuf),
                                                                  (uri.port & 0xffff)),
                                             portbuf));
    }

    if (uri.path.length) {
        buflen = BUFSIZE;
        if (uri.path.length > BUFSIZE)
            coap_log(LOG_WARNING, "URI path will be truncated (max buffer %d)\n", BUFSIZE);
        res = coap_split_path(uri.path.s, uri.path.length, buf, &buflen);
        //todo: if there is a uri path, a options must be created an
        while (res--) {
            coap_insert_optlist(&optlist,
                                coap_new_optlist(COAP_OPTION_URI_PATH,
                                                 coap_opt_length(buf),
                                                 coap_opt_value(buf)));

            buf += coap_opt_size(buf);
        }
    }
    //todo: if there is a uri query, options must be updated
    if (uri.query.length) {
        buflen = BUFSIZE;
        buf = _buf;
        res = coap_split_query(uri.query.s, uri.query.length, buf, &buflen);

        while (res--) {
            coap_insert_optlist(&optlist,
                                coap_new_optlist(COAP_OPTION_URI_QUERY,
                                                 coap_opt_length(buf),
                                                 coap_opt_value(buf)));

            buf += coap_opt_size(buf);
        }
    }
    return optlist;
}

int get_uri_options(const char *address, coap_optlist_t **optList) {
    coap_uri_t uri;
    memset(&uri, 0, sizeof(uri));
    if (coap_split_uri((uint8_t *) address, strlen(address), &uri) < 0) {
        ESP_LOGE(__FUNCTION__, "URI_ERROR");
        return -1;
    }
    *optList = process_uri(uri, 0);
    return 0;
}

/*

coap_optlist_t *process_uri(char *address, int create_uri_opts, int reliable, coap_uri_t &uri) {
	unsigned char portbuf[2];
	unsigned char _buf[BUFSIZE];
	unsigned char *buf = _buf;
	size_t buflen;
	int res;

	coap_optlist_t *optlist = NULL;

	*/
/* split arg into Uri-* options  *//*

	if (coap_split_uri((unsigned char *) address, strlen(address), &uri) < 0) {
		coap_log(LOG_ERR, "invalid CoAP URI\n");
		return NULL;
	}

	if (uri.scheme == COAP_URI_SCHEME_COAPS && !reliable && !coap_dtls_is_supported()) {
		coap_log(LOG_EMERG,
				 "coaps URI scheme not supported in this version of libcoap\n");
		return NULL;
	}

	if ((uri.scheme == COAP_URI_SCHEME_COAPS_TCP || (uri.scheme == COAP_URI_SCHEME_COAPS && reliable)) &&
		!coap_tls_is_supported()) {
		coap_log(LOG_EMERG,
				 "coaps+tcp URI scheme not supported in this version of libcoap\n");
		return NULL;
	}

	if (uri.scheme == COAP_URI_SCHEME_COAP_TCP) {
		*/
/* coaps+tcp caught above *//*

		coap_log(LOG_EMERG,
				 "coap+tcp URI scheme not supported in this version of libcoap\n");
		return NULL;
	}

	if (uri.port != get_default_port(&uri) && create_uri_opts) {
		coap_insert_optlist(&optlist,
							coap_new_optlist(COAP_OPTION_URI_PORT,
											 coap_encode_var_safe(portbuf, sizeof(portbuf),
																  (uri.port & 0xffff)),
											 portbuf));
	}

	if (uri.path.length) {
		buflen = BUFSIZE;
		if (uri.path.length > BUFSIZE)
			coap_log(LOG_WARNING, "URI path will be truncated (max buffer %d)\n", BUFSIZE);
		res = coap_split_path(uri.path.s, uri.path.length, buf, &buflen);
		//todo: if there is a uri path, a options must be created an
		while (res--) {
			coap_insert_optlist(&optlist,
								coap_new_optlist(COAP_OPTION_URI_PATH,
												 coap_opt_length(buf),
												 coap_opt_value(buf)));

			buf += coap_opt_size(buf);
		}
	}
	//todo: if there is a uri query, options must be updated
	if (uri.query.length) {
		buflen = BUFSIZE;
		buf = _buf;
		res = coap_split_query(uri.query.s, uri.query.length, buf, &buflen);

		while (res--) {
			coap_insert_optlist(&optlist,
								coap_new_optlist(COAP_OPTION_URI_QUERY,
												 coap_opt_length(buf),
												 coap_opt_value(buf)));

			buf += coap_opt_size(buf);
		}
	}
	return optlist;
}
*/
/*
coap_optlist_t *copy_optlist(coap_optlist_t *src) {
	coap_optlist_t *copy = nullptr;
	for (coap_optlist_t *option = src; option; option = option->next) {
		{
			coap_optlist_t *ol = coap_new_optlist(
					option->number,
					option->length,
					option->data);

			coap_insert_optlist(&copy, ol);
		}
	}
	return copy;
}

char *build_url_from_pdu(coap_pdu_t *pdu) {
	coap_opt_iterator_t opt_iter;;
	char *path = nullptr;
	coap_option_iterator_init(pdu, &opt_iter, COAP_OPT_ALL);
	while (coap_opt_t *op = coap_option_next(&opt_iter)) {
		if( opt_iter.type == COAP_OPTION_URI_PATH) {
			int len = coap_opt_length(op);
			path = (char *) coap_malloc(len + 1);
			memccpy(path, coap_opt_value(op), 1, len);
			path[len] = '\0';
		}
	}
	return nullptr;
}


char *build_url_from_options(coap_optlist_t *list) {

	char *path = nullptr;
	char *port = nullptr;
	char *query = nullptr;
	coap_optlist_t *option = list;
	while (option) {
		switch (option->number) {
			case COAP_OPTION_URI_HOST:
			case COAP_OPTION_URI_PATH:
			case COAP_OPTION_URI_PORT:
			case COAP_OPTION_URI_QUERY: {
				int len = option->length;
				char *buf = (char *) malloc(len + 1);
				memset(buf, 1, len + 1);
				memccpy(buf, option->data, 1, len);
				buf[len] = '\0';
				switch (option->number) {
					case COAP_OPTION_URI_PATH:
						path = buf;
						break;
					case COAP_OPTION_URI_PORT:
						port = buf;
						break;
					case COAP_OPTION_URI_QUERY:
						query = buf;
						break;
					default:
						free(buf);
						break;

				}
			}
		}
		option = list->next;
	}
	return path;
}

*/