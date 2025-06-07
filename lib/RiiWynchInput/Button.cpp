#include "Button.h"

Button::Button(uint8_t pin, uint8_t pullup_type, bool active_low)
    : _pin(pin), _active_low(active_low) {
    pinMode(_pin, pullup_type);
    _state = digitalRead(_pin);
    _last_state = _state;
}

void Button::update() {
    int reading = digitalRead(_pin);

    // If the switch changed, due to noise or pressing:
    if (reading != _last_state) {
        _last_debounce_time = millis();
    }

    if ((millis() - _last_debounce_time) > DEBOUNCE_DELAY) {
        // if the button state has changed:
        if (reading != _state) {
            _state = reading;
            bool is_pressed_now = (_state == (_active_low ? LOW : HIGH));

            if (is_pressed_now) {
                // Button was just pressed
                _press_start_time = millis();
                _long_press_triggered = false;
                _last_repeat_time = 0;
                if (_press_callback) {
                    _press_callback();
                }
            } else {
                // Button was just released
                if (_release_callback) {
                    _release_callback();
                }
            }
        }
    }

    // Check for long press and hold
    bool is_currently_pressed = (_state == (_active_low ? LOW : HIGH));
    if (is_currently_pressed) {
        unsigned long press_duration = millis() - _press_start_time;

        // Long press event (triggers once)
        if (_long_press_callback && !_long_press_triggered && press_duration >= _long_press_duration) {
            _long_press_callback();
            _long_press_triggered = true;
        }

        // Hold event (triggers repeatedly)
        if (_hold_callback && press_duration >= _long_press_duration) {
            if (_last_repeat_time == 0 || (millis() - _last_repeat_time >= _hold_repeat_interval)) {
                 if(_last_repeat_time != 0 || _long_press_callback == nullptr) {
                    _hold_callback();
                }
                _last_repeat_time = millis();
            }
        }
    }

    _last_state = reading;
}

void Button::onPress(ButtonCallback callback) {
    _press_callback = callback;
}

void Button::onRelease(ButtonCallback callback) {
    _release_callback = callback;
}

void Button::onLongPress(ButtonCallback callback, unsigned long duration) {
    _long_press_callback = callback;
    _long_press_duration = duration;
}

void Button::onHold(ButtonCallback callback, unsigned long repeat_interval) {
    _hold_callback = callback;
    _hold_repeat_interval = repeat_interval;
}

bool Button::isPressed() const {
    return _state == (_active_low ? LOW : HIGH);
} 