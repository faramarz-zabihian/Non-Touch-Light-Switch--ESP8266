#pragma once

enum ErrorEnum {
    SEND_PROBLEM = -1,
    NO_ERROR = 0,
};

coap_session_t *get_session();
inline CoapClient *get_session_client(coap_session_t *s) {
    return s ? (CoapClient *) coap_session_get_app_data(s) : nullptr;
}

void clear_session();
void config_build_url_variables();
const char * get_observation_resource_url();
const char * get_report_resource_url();
void reset_connection_data();

