//
// Created by asus on ۲۴/۰۲/۲۰۲۱.
//
#include <include/config.h>
#include "connect.h"
#include "coap3/coap.h"
#include "common.h"
#include "coap_utils.h"


#ifndef AIVA_REQUEST_H
#define AIVA_REQUEST_H

//#define COAP_BLOCK_USE_LIBCOAP 1


#define REQUEST_WAIT_TIME 200



enum OBSERVE_STATE {
	MY_ESTABLISH,
	MY_CANCEL,
	MY_NONE
};

CoapClient *get_default_client();

class Request {

private:
    CoapClient *client = nullptr;
    coap_session_t *session;

	uint error_no = 0;
	bool ready = false;
	int flags = 0;
	coap_pdu_code_t method = COAP_REQUEST_CODE_GET;
	coap_block_t block = {.num = 0, .m = 0, .szx = 6};

	// client identity, pre-shared key
	//coap_str_const_t *user = NULL, *psk_key = NULL;
	inline method_t get_method() const { return method; }

	coap_optlist_t *options = nullptr;
	unsigned char _token_data[8];
	response_handler_t responseHandler = nullptr;
	coap_binary_t the_token = {0, _token_data};
	coap_string_t payload = {0, nullptr};

	static Request *reference(Request *item);

	static void free(Request *item);

	inline coap_session_t *getSession() {
		return session;
	}

	friend CoapClient;

private:
	//todo: to be removed
	//coap_address_t dst;

	bool reliable = false;
	/*
	int fired;*/
	uint16_t id = -1;
	bool confirmable = true;
	OBSERVE_STATE m_observe_state = MY_NONE;
	bool single_block_requested = false;

	uint16_t timeout_val = 5;
	bool timeout_set = false;

	void (*errorHandler)(const char *, uint) = nullptr;

	void clear_payload();

	void reset_start_time();

	inline bool getSingleBlockRequested() const { return single_block_requested; }

	bool still_waiting(timeval &time);


	bool dynamic_time_out = false;
public:
	static void release(Request *item);
	Request *setSession(coap_session_t *session);

	Request *Timeout(uint16_t seconds);

	inline Request *Timeout(uint16_t timeout, bool dynamic) {
		Timeout(timeout);
		dynamic_time_out = isObserve() && dynamic;
		return this;
	}

	Request *Observe(OBSERVE_STATE state);

	bool isReliable() const;

	void setReliable(bool reliable);

	/*void setUser(unsigned char *user, ssize_t len);
	void setPSK(unsigned char *psk, ssize_t len);*/

	const char *getUrl() const;

	const coap_binary_t &getToken() const;

	Request *setUrl(const char *address);

	int getId() const;

	void setId(int id);

	~Request();

	Request(coap_pdu_code_t method);

	Request(coap_pdu_code_t method, bool confirmable);

	Request *add_token(coap_binary_t *token);

	Request *set_payload(const unsigned char *text, size_t len);


	friend class coapClient;

	unsigned long start_time = 0;

	friend coap_response_t response_handler(coap_session_t *session,
                                 const coap_pdu_t *sent,
                                 const coap_pdu_t *received,
                                 const coap_mid_t id);


	void add_option(int option, unsigned char *data, size_t size);

	Request *Handler(response_handler_t handler);

	friend void start_listener_task(void *);

	static Request *new_get();

	static Request *new_post();

	int SendAndLeave();

	Request *setBlockSize(uint16_t size);

	bool IsBlockSizeSet();

	bool isObserve() const;

	uint getState() const;

	void handle_data(const uint8_t *string, size_t len);

	Request *no_wait();

	Request *OnError(void (*pFunction)(const char *, uint));

	void setError(const char *message, uint errNo);

	Request *set_confirmable(bool confirmable);

	friend void removeRequest(Request &req);

	bool hasError();

	bool is_expired(timeval now);


	uint8_t ref = 0;

	int SendAndWait();


	Request *getObservationCancelRequest();

	bool isCancelObserve() const;

    void SessionRelease();

    void SessionReference();
};


/*void removeRequest(Request &req);
Request * find_request_by_token(coap_context_t *ctx, coap_session_t *session, coap_pdu_t *pdu);*/

#endif //AIVA_REQUEST_H
