//
// Created by asus on 8/23/2021.
//
#include "driver/dimmer.h"

#ifndef HELLO_WORLD_LEDC_H
#define HELLO_WORLD_LEDC_H

#define LIGHT_CHANNEL 0
#define BACKLIGHT_CHANNEL 1
#define LEDC_FRQ  20000

struct SensorChannel
{
    ledc_channel_config_t sensor;
    bool enabled = true;
    uint8_t last_value;
    SensorChannel(ledc_channel_config_t channel, bool active, uint8_t value): sensor(channel), enabled(active), last_value(value)
    {
        //enabled = active;

    }
};


#endif //HELLO_WORLD_LEDC_H
