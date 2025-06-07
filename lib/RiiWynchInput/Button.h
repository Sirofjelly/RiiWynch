#pragma once
#include <Arduino.h>
#include <functional>

class Button {
public:
    using ButtonCallback = std::function<void()>;

    Button(uint8_t pin, uint8_t pullup_type = INPUT_PULLUP, bool active_low = true);
    
    void update();
    
    void onPress(ButtonCallback callback);
    void onRelease(ButtonCallback callback);
    void onLongPress(ButtonCallback callback, unsigned long duration = 500);
    void onHold(ButtonCallback callback, unsigned long repeat_interval = 200);

    bool isPressed() const;

private:
    uint8_t _pin;
    bool _active_low;
    
    int _state;
    int _last_state;
    unsigned long _last_debounce_time = 0;
    unsigned long _press_start_time = 0;
    unsigned long _last_repeat_time = 0;

    ButtonCallback _press_callback = nullptr;
    ButtonCallback _release_callback = nullptr;
    ButtonCallback _long_press_callback = nullptr;
    ButtonCallback _hold_callback = nullptr;
    
    unsigned long _long_press_duration = 500;
    unsigned long _hold_repeat_interval = 200;
    bool _long_press_triggered = false;
    
    static const unsigned long DEBOUNCE_DELAY = 50;
}; 