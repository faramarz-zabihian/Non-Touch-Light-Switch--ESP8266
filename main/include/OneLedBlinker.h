#include <math.h>

class OneLedBlinker {
    long last_millis_check = 0;
    int switch_holds = false;
    int next_millis_check = 0;
    switcher pin_driver = NULL;
    uint16_t animation_duration;

    void (*reset)();

    int startDelay = 200;

    bool anim_led_on = false;
    bool current_state = false;

    void setPin(INDICATOR_PIN led_index, SWITCH_STATE val) {
        setPin(led_index, val, nullptr);
    }

    void setPin(INDICATOR_PIN led_index, SWITCH_STATE val, void *report) {
        if (pin_driver != NULL)
            pin_driver(led_index, val, report);
    }

    void animation(bool state) {
        setPin(ON_PIN, (state ? STATE_ON : STATE_OFF));
    }
    void animate() {
        anim_led_on = !anim_led_on;
        animation(anim_led_on);
    }

    void animation_reset() {
        anim_led_on = false;
        animation(current_state); // resets led to starting state
    }


    virtual int getDuration(long dur) {
        constexpr static const int times[] = {2, 4, 6, 8};
        int time_lapse = animation_duration / (sizeof(times) / sizeof(int));

        if (dur < startDelay + time_lapse)
            return -1;

        dur -= startDelay;

        if (dur > (2 * animation_duration)) // rejection area
            return -2;

        if (dur >= animation_duration) // acceptance area
            return 0;

        int dv = (dur-1) /time_lapse;
        return time_lapse / times[dv];
    }

public:
    SWITCH_STATE get_state()
    {
        return current_state ? STATE_ON : STATE_OFF;
    }
    void switch_on(SWITCH_STATE state) {
        switch_on(state, nullptr) ;
    }
    void switch_on(SWITCH_STATE state, void *report) {
        current_state = (state == STATE_ON);
        setPin(REL_PIN, state, report);
    }

    void init(SWITCH_STATE state) {
        next_millis_check = 0;
        (*reset)();
        switch_on(state);
    }

    OneLedBlinker(void(*reset)(), switcher led_switcher) {
        this->reset = reset;
        this->pin_driver = led_switcher;
        animation_duration =  1000;
    }

    void setAnimationDuration(uint16_t animationDuration) {
        animation_duration = animationDuration;
    }


    void setStartDelay(int delayInMillis) {
        startDelay = delayInMillis;
    }

    void check(bool inProximity, unsigned int millis) {
        if (millis <= next_millis_check)
            return;

        if (inProximity || switch_holds) // start blinking
        {
            if (inProximity && last_millis_check == 0) {
                last_millis_check = millis; // ادامه دادن ضرورت ندارد
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

                case 0:
                    if (!switch_holds) {
                        switch_holds = true;
                        next_millis_check = millis;
                        return;
                    } else if (!inProximity) {
                        last_millis_check = 0;
                        next_millis_check = 0;
                        switch_holds = false;
                        switch_on(current_state ? STATE_OFF : STATE_ON);
                    } else {
                        // switch holds and inProximity, i.e. time to remove obstacle
                        animate();
                        next_millis_check = millis + 10; // this is to keep animating
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

};
