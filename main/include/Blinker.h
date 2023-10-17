#ifndef BLINKER_H
#define BLINKER_H

typedef enum {
    STATE_OFF = 0,
    STATE_ON = 1
} SWITCH_STATE;

typedef enum {
    ON_PIN,
    OFF_PIN,
    ERR_PIN,
    REL_PIN
} INDICATOR_PIN;

typedef void (*switcher)(INDICATOR_PIN led, SWITCH_STATE state, void * report);

class Blinker {
    long last_millis_check = 0;
    int switch_holds = false;
    int next_millis_check = 0;
    switcher pin_driver;
    uint16_t animation_duration;

    void (*reset)();

    int startDelay = 200;

    bool hasError = false;
    INDICATOR_PIN lit_led;
    bool anim_led_on = false;

    void setPin(INDICATOR_PIN led, bool state) {
        setPin(led, state, nullptr);
    }

    void setPin(INDICATOR_PIN led, bool state, void * report) {
        if (pin_driver != nullptr)
            pin_driver(led, state ? STATE_ON : STATE_OFF, report);
    }

    void animation(bool state) {
        setPin((lit_led == ON_PIN) ? OFF_PIN : ON_PIN, state);
    }


    void animation_reset() {
        anim_led_on = false;
        animation(false);
    }

    void animate() {
        animation(anim_led_on = !anim_led_on);
    }


    virtual int getDuration(long dur) {
        constexpr static const int times[] = {2, 4, 6, 8};
        const int time_lapse = animation_duration / (sizeof(times) / sizeof(int));

        if (dur < startDelay + time_lapse)
            return -1;

        dur -= startDelay;

        if (dur > (2 * animation_duration)) // rejection area
            return -2;

        if (dur >= animation_duration) // acceptance area
            return 0;

        int dv = (dur - 1) / time_lapse;
        return time_lapse / times[dv];
    }

public:

    INDICATOR_PIN get_on_led() const {
        return lit_led;
    }

    void switch_on(bool state, void * report) {
        lit_led = state ? ON_PIN : OFF_PIN;
        setPin(OFF_PIN, !state);
        setPin(ERR_PIN, state && hasError);
        setPin(ON_PIN, state && !hasError);
        setPin(REL_PIN, state, report); // driving relay
    }

    void switch_on(bool state) {
        switch_on(state, nullptr);
    }


    void init(bool on) {
        next_millis_check = 0;
        (*reset)();
        switch_on(on);
    }

    Blinker(void(*reset)(), switcher led_switcher) {
        this->reset = reset;
        this->pin_driver = led_switcher;
        animation_duration = 1000;
    }

    void setAnimationDuration(uint16_t animationDuration) {
        animation_duration = animationDuration;
    }

    /* void setError(bool error) {
         hasError = error;
         set_leds(ON_INDICATOR);
     }*/

    //    void setError
    void setStartDelay(int delayInMillis) {
        startDelay = delayInMillis;
    }

    void check(bool inProximity, unsigned int millis) {
        if (millis <= next_millis_check)
            return;

        if (inProximity || switch_holds) // start blinking
        {
            if (inProximity && last_millis_check == 0) {
                last_millis_check = millis; // start of chronometer
                return;
            }

            int wait = getDuration(millis - last_millis_check);

            switch (wait) {
                case -2:  // duration is too long, canel every thing
                    animation_reset();
                    if (!inProximity) {
                        switch_holds = false;
                        last_millis_check = 0;
                    }
                    next_millis_check = 0;
                    return;

                case -1 : // nothing happend yet, last_millis_check remains unchanged and no animation
                    next_millis_check = 0;
                    animation_reset();
                    return;

                case 0:  // switch happened

                    if (!switch_holds) {
                        switch_holds = true;
                        next_millis_check = millis;
                        return;
                    } else if (!inProximity) {
                        switch_on(!get_state());
                        last_millis_check = 0;
                        next_millis_check = 0;
                        switch_holds = false;
                    } else {
                        // switch holds and inProximity, i.e. time to remove obstacle
                        animate();
                        next_millis_check = millis + 10;
                        return;
                    }
                    return;

                default: // there has been some duration
                    animate();
                    next_millis_check = millis + wait;
                    return;
            }
        }
        switch_holds = false;
        last_millis_check = 0;
        animation_reset();
        return;
    }

    bool get_state() {
        return lit_led == ON_PIN;
    }
};

#endif