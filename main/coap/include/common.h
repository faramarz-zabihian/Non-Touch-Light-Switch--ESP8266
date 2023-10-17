//
// Created by asus on ۰۸/۰۳/۲۰۲۱.
//

#ifndef COAP_COMMON_H
#define COAP_COMMON_H

#include <coap3/coap.h>

#ifdef __GNUC__
#define UNUSED_PARAM __attribute__ ((unused))
#else /* not a GCC */
#define UNUSED_PARAM
#endif /* GCC */

typedef unsigned char method_t;
typedef void (*response_handler_t) (const uint8_t *, unsigned int len);


#endif //COAP_COMMON_H
