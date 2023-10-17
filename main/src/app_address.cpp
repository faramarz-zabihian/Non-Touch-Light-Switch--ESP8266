#include "../include/app_address.h"
#include "stdio.h"

//
// Created by asus on ۵/۲۸/۲۰۲۱.
//
app_address::~app_address() {
    if (topic.s)
        free(topic.s);
    topic.s = nullptr;
    topic.length = 0;
    if (key.s)
        free(key.s);
    key.s = nullptr;
    key.length = 0;
}

cn_cbor *app_address::to_cbor() {

    cn_cbor_errback res;
    cn_cbor *root = cn_cbor_array_create(&res);
    cn_cbor_array_append(root, cn_cbor_data_create(topic.s, topic.length, &res), &res);
    cn_cbor_array_append(root, cn_cbor_data_create(key.s, key.length, &res), &res);
    return root;
}


app_address *app_address::from_cbor(cn_cbor *root) {
    if (root == nullptr)
        return nullptr;

    if (CN_CBOR_IS_ARRAY(root)) {
        cn_cbor *el;
        int size = 0;
        for (int i = 0; (el = cn_cbor_index(root, i)) != nullptr; i++)
            size++;
        if (size == 2) {
            cn_cbor *c_topic = cn_cbor_index(root, 0); // cbor topic
            cn_cbor *c_key = cn_cbor_index(root, 1); // cbor key

            app_address *addr = new app_address();
            if (c_topic) {
                STR_TO_BIN_T(addr->topic, c_topic->v.bytes, c_topic->length);
            }
            if (c_key) {
                STR_TO_BIN_T(addr->key, c_key->v.bytes, c_key->length);
            } else
                puts("c_key is null");

            return addr;
        }
    }
    return nullptr;
}