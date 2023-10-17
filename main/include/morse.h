#ifndef MORSE_H
#define MORSE_H

#include <stdint-gcc.h>
#include <cstring>
#include <FreeRTOS.h>
#include <task.h>
#include <Blinker.h>

//
// Created by asus on ۱۲/۰۵/۲۰۲۱.
//
class Morse {
    void (*player)(SWITCH_STATE);
    #define MORSE_DOT_DELAY 50
    void (*resettle)();

    void toggle_led(SWITCH_STATE state, uint8_t delay) {
        if (player) {
            player(state);
            vTaskDelay((delay * MORSE_DOT_DELAY) / portTICK_RATE_MS);
        }
    }

    void flashSequence(const char *dashdot_seq) {
        int i = 0;
        while (dashdot_seq[i]) {
            toggle_led(STATE_ON, dashdot_seq[i] == '.' ? 1 : 3); // dash ot dot
            toggle_led(STATE_OFF, 1);   // space between dashes and dots
            i++;
        }
        toggle_led(STATE_OFF, 3); // gap between letters
    }

/*
    const char *to_morse_code(char ch) {
        static const char *letters[] = {
                ".-",     // A
                "-...",   // B
                "-.-.",   // C
                "-..",    // D
                ".",      // E
                "..-.",   // F
                "--.",    // G
                "....",   // H
                "..",     // I
                ".---",   // J
                "-.-",    // K
                ".-..",   // L
                "--",     // M
                "-.",     // N
                "---",    // O
                ".--.",   // P
                "--.-",   // Q
                ".-.",    // R
                "...",    // S
                "-",      // T
                "..-",    // U
                "...-",   // V
                ".--",    // W
                "-..-",   // X
                "-.--",   // Y
                "--.."    // Z
        };
        const char *numbers[] = {
                "-----",   // 0
                ".----",   // 1
                "..---",   // 2
                "...--",   // 3
                "....-",   // 4
                ".....",   // 5
                "-....",   // 6
                "--...",   // 7
                "---..",   // 8
                "----."    // 9
        };

        if (ch >= 'A' && ch <= 'Z')
            return letters[ch - 'A'];
        if (ch >= 'a' && ch <= 'z')
            return letters[ch - 'a'];
        else if (ch >= '0' && ch <= '9')
            return numbers[ch - '0'];

        return NULL;
    }
*/

public:
    Morse(void (*toggler)(SWITCH_STATE), void (*resettle)()) : player(toggler), resettle(resettle) {
    }

    void play_morse(const char *text) {
        for (int i = 0; i < strlen(text); i++)
            if (text[i] == ' ')
                toggle_led(STATE_OFF, 7);
            else
                flashSequence(text);
     //           flashSequence(to_morse_code(text[i]));
        if (resettle)
            resettle();
    }
};

#endif
