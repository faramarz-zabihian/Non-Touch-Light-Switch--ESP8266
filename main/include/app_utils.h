//
// Created by asus on ۱۰/۰۵/۲۰۲۱.
//

#ifndef HELLO_WORLD_UTILS_H
#define HELLO_WORLD_UTILS_H

uint8_t *crptoHash256(const char * s, size_t inlen);

#ifdef __cplusplus
extern "C" {
#endif

#include "connect.h"

void write_connection_data_to_flash(user_pass_t *info);

void config_load_connection_data_from_flash(user_pass_t *sta);
void erase_connection_data_from_flash();
const char * to_morse_code(char ch);
void config_load_mfg_keys_from_factory();

#ifdef __cplusplus
}
#endif

#endif //HELLO_WORLD_UTILS_H
