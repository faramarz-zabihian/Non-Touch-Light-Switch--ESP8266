# Non touch light switch

The board is designed using an ESP8266 and ESP IDF SDK (FreeRTOS).

Switch emits two different IR codes to the left and right of it. An IR receiver detects the IR code reflected by the obstacles and determines the direction of the movement. A light-emitting diode signals an impending flip after an obstacle is detected. The switch is activated if the obstacle is maintained for 1 to 2.5 seconds, and deactivated if the duration is longer.

The printed circuit board also features a 5-volt power that can be controlled by the program, making it suitable to use as a bedside light.
