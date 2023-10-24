#include "led.h"

struct RGB {
    bool red;
    bool green;
    bool blue;
};

static RGB get_rgb(LedColor color) {
    switch (color) {
        case LedColor::RED:
            return { true, false, false };
        case LedColor::GREEN:
            return { false, true, false };
        case LedColor::BLUE:
            return { false, false, true };
        case LedColor::CYAN:
            return { false, true, true };
        case LedColor::MAGENTA:
            return { true, false, true };
        case LedColor::YELLOW:
            return { true, true, false };
        case LedColor::WHITE:
            return { true, true, true };
        case LedColor::OFF:
        default:
            return { false, false, false };
    }
}

void Led::set_color(LedColor color) {
    RGB rgb = get_rgb(color);
    digitalWrite(pin_red, rgb.red ? LOW : HIGH);
    digitalWrite(pin_green, rgb.green ? LOW : HIGH);
    digitalWrite(pin_blue, rgb.blue ? LOW : HIGH);
}

void Led::set_color(uint8_t color) {
    LedColor c = LedColor::OFF;
    switch (color) {
        case 'R':
        case 'r':
            c = LedColor::RED;
            break;
        case 'G':
        case 'g':
            c = LedColor::GREEN;
            break;
        case 'B':
        case 'b':
            c = LedColor::BLUE;
            break;
        case 'C':
        case 'c':
            c = LedColor::CYAN;
            break;
        case 'M':
        case 'm':
            c = LedColor::MAGENTA;
            break;
        case 'Y':
        case 'y':
            c = LedColor::YELLOW;
            break;
        case 'W':
        case 'w':
        default:
            c = LedColor::WHITE;
            break;
    }
    set_color(c);
}
