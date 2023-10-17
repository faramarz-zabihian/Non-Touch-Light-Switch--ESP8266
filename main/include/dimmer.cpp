//
// Created by asus on 8/23/2021.
//

#include "dimmer.h"
//#include "driver/ledc.h"

ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_HIGH_SPEED_MODE,
        .duty_resolution  = LEDC_TIMER_13_BIT,
        .timer_num        = LEDC_TIMER_0,
        .freq_hz          = LEDC_FRQ  // Set output frequency at 5 kHz
};

SensorChannel sensor_channels[2] = {
        {{
                 .gpio_num   = 1,
                 .speed_mode = ledc_timer.speed_mode,
                 .channel    = LEDC_CHANNEL_0,
                 .timer_sel  = ledc_timer.timer_num,
                 .duty       = 0,
                 .hpoint = 0,
         },
                true,
                0
        },
        {{
                 .gpio_num   = 1,
                 .speed_mode = ledc_timer.speed_mode,
                 .channel    = LEDC_CHANNEL_1,
                 .timer_sel  = ledc_timer.timer_num,
                 .duty       = 0,
                 .hpoint = 0,
         },
                true,
                0
        }
};

void reset_ledc() {
    ledc_fade_func_uninstall(); // removes all channels

    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    for (int i = 0; i < BACKLIGHT_CHANNEL; i++)
        if (sensor_channels[i].enabled)
            ESP_ERROR_CHECK(ledc_channel_config(&(sensor_channels[i].Sensor)));
    ledc_fade_func_install(0);
}


void set_dimming_level(SensorChannel &output, unsigned int value) {
// intensity par(ip) 2 to 6 // user values ofr for 1..5 intensities levels
// == 0:off
//  > 0:on
//  > 1 ? intensity = (ip-1)/5
    if (value == 0 || value == 1) {
        output.enabled = false;
        ledc_stop(output.sensor.speed_mode, output.sensor.channel, value);
        ledc_fade_func_uninstall(); // !! uninstall
        ledc_fade_func_install(0); //!! re-install
    } else {
        ESP_ERROR_CHECK(ledc_set_duty(output.sensor.speed_mode, output.sensor.channel, (value - 1) * LEDC_FRQ / 5));
        ESP_ERROR_CHECK(ledc_update_duty(output.sensor.speed_mode, output.sensor.channel));
    }
}

