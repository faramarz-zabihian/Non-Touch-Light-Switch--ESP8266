//
// Created by asus on ۵/۲۸/۲۰۲۱.
//

#ifndef AIVA_APP_ADDRESS_H
#define AIVA_APP_ADDRESS_H

#include <cn-cbor/cn-cbor.h>
#include "coap3/coap.h"

#define STR_TO_BIN_T(d,src,l)  (d).length = l; (d).s = (unsigned  char *) malloc(l); memcpy((d).s, src, l)
#define CN_CBOR_IS_ARRAY(el) (el->type == CN_CBOR_ARRAY)

struct app_address
{
    coap_binary_t topic = {0, nullptr};
    coap_binary_t key = {0, nullptr} ;
    static app_address *from_cbor(cn_cbor *root);
    cn_cbor * to_cbor() ;
   /* inline std::string to_string() {
        return  (topic.s ? std::string( (char *) topic.s, topic.length) : std::string("") ) + "?" +
                (key.s ?  std::string( (char *) key.s, key.length) : std::string (""));
    }*/
    ~app_address();
};


#endif //AIVA_APP_ADDRESS_H
