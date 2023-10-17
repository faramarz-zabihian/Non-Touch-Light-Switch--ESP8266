#include <vector>

#ifndef AIVA_AIVA_MESSAGE_H
#define AIVA_AIVA_MESSAGE_H

#define CN_CBOR

#ifdef CBOR
#include "cbor.h"
struct app_message {
    coap_binary_t clientId;
    std::vector<uint8_t> cmds;
    ~app_message() {
        cmds.clear();
    }
    coap_binary_t to_binary_t() {
        auto cbor_cmds = cbor_new_definite_array(cmds.size());
        auto cid = cn_cbor_build_bytestring(clientId.s, clientId.length);

        cbor_item_t *root = cbor_new_definite_array(2);
        cbor_array_push(root, cbor_move(cid));
        cbor_array_push(root, cbor_move(cbor_cmds));

        for (std::vector<uint8_t>::iterator it = cmds.begin(); it != cmds.end(); it++)
            cbor_array_push(cbor_cmds, cbor_move(cbor_build_uint8((*it)))); // fill commands

        unsigned char* result;
        size_t buffer_size, length = cbor_serialize_alloc(root, &result, &buffer_size);
        cbor_decref(&root);
        return coap_binary_t { length, result};
    }

    static app_message *from_binary_t(uint8_t *cbor_data, unsigned int len) {
        app_message *am = new app_message();
        cbor_load_result res;
        cbor_item_t *root_array = cbor_load(cbor_data, len, &res);
        if (res.error.code == CBOR_ERR_NONE) // error check
            if (cbor_isa_array(root_array)) // check type
                if (cbor_array_size(root_array) == 2) { //check size
                    cbor_item_t *item_0 = cbor_array_get(root_array, 0); //clientId
                    if (cbor_isa_bytestring(item_0) && cbor_bytestring_is_definite(item_0)) { // clientId
                        size_t len = cbor_bytestring_length(item_0);// clientId len
                        am->clientId.length = len;
                        am->clientId.s = cbor_bytestring_handle(item_0);
                        cbor_item_t *array_0 = cbor_array_get(root_array, 1);
                        // after client there is an array
                        for (int i = 0; i < cbor_array_size(array_0); i++) {
                            cbor_item_t *t = cbor_array_get(array_0, i); // int,string, maps ...
                            if(cbor_isa_uint(t))
                                am->cmds.push_back(cbor_get_uint8(t));
                        }
                        return am;
                    }
                }
        return nullptr;
    }
};
#endif
#ifdef CN_CBOR

#include "cn-cbor/cn-cbor.h"
#include "app_address.h"

struct device_message {
    uint8_t cmd = 0;
    app_address *from = nullptr;
    app_address *to = nullptr;

    ~device_message() {
        if (from)
            delete from;
        if (to)
            delete to;
    }

    static void to_binary_t(std::vector<device_message *> *list, unsigned char *&result, size_t &length) {
        cn_cbor_errback err;
        cn_cbor *c_root = cn_cbor_array_create(&err);
        for (auto it = list->cbegin(); it != list->cend(); it++) {
            auto dm = *it;
            cn_cbor *c_item = cn_cbor_array_create(&err);
            cn_cbor_array_append(c_item, dm->from ? dm->from->to_cbor() : cn_cbor_null_create(&err), &err);
            cn_cbor_array_append(c_item, dm->to ? dm->to->to_cbor() : cn_cbor_null_create(&err), &err);
            cn_cbor_array_append(c_item, cn_cbor_int_create(dm->cmd, &err), &err);
            cn_cbor_array_append(c_root, c_item, &err);
        }
        if (list->size() > 0) {
            size_t buffer_size = 1000;
            result = (uint8_t *) malloc(buffer_size);
            length = cn_cbor_encoder_write(result, 0, buffer_size, c_root);
            cn_cbor_free(c_root);
        } else {
            length = 0;
            result = nullptr;
        }
    }

    static std::vector<device_message *> *from_binary_t(uint8_t *cbor_data, unsigned int len) {
        cn_cbor_errback res;
        res.err = CN_CBOR_NO_ERROR;
        res.pos = 0;
        cn_cbor *root = cn_cbor_decode(cbor_data, len, &res); // should be array
        if(res.err != CN_CBOR_NO_ERROR) {
            if(root)
                cn_cbor_free(root);
            return nullptr;
        }

        auto list = new std::vector<device_message *>();
        if (root->type == CN_CBOR_ARRAY) {
            cn_cbor *el;
            for (int i = 0; (el = cn_cbor_index(root, i)) != nullptr; i++) {
                if (el->type == CN_CBOR_ARRAY) {
                    auto dm = new device_message();
                    dm->from = app_address::from_cbor(cn_cbor_index(el, 0));
                    dm->to = app_address::from_cbor(cn_cbor_index(el, 1));
                    dm->cmd = cn_cbor_index(el, 2)->v.sint;
                    list->push_back(dm);
                } else
                    break;
            }
        }
        cn_cbor_free(root);
        return list;
    }
};

#endif

#endif //AIVA_AIVA_MESSAGE_H
