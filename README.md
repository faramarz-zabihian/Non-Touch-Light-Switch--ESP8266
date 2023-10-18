# Non touch light switch

The board is designed using an ESP8266 and ESP IDF SDK (FreeRTOS).

Switch emits two different IR codes to the left and right of it. An IR receiver detects the IR code reflected by the obstacles and determines the direction of the movement. Before flipping the power, a light-emitting diode starts playing an animation indicating that a flip is about to occur. The switch activates  after 1 to 2.5 seconds of maintaining obstacle gesture. Any duration beyond these limits deactivates the flipping.

The printed circuit board also features a 5-volt power that can be controlled by the program, making it suitable to use as a bedside light.
