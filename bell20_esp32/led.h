#pragma once

#include <Arduino.h>

enum class LedColor {
    OFF,
    RED,
    GREEN,
    BLUE,
    CYAN,
    MAGENTA,
    YELLOW,
    WHITE
};

class Led {
public:
    Led(uint8_t r, uint8_t g, uint8_t b)
        : pin_red(r)
        , pin_green(g)
        , pin_blue(b)
    { }

    void set_color(LedColor color);
    void set_color(uint8_t color);
private:
    const uint8_t pin_red;
    const uint8_t pin_green;
    const uint8_t pin_blue;
};
