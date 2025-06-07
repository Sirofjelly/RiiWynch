#include "StateManager_remote.h"

StateManager_remote::StateManager_remote()
    : _currentState(State::START),
      _targetPercentage(0),
      _shownPercentage(0),
      _lastActivityTime(0),
      _lastAnimationTime(0) {}

StateManager_remote::State StateManager_remote::getState() const {
    return _currentState;
}

void StateManager_remote::switchToStart() {
    _currentState = State::START;
}

void StateManager_remote::switchToMenu() {
    _currentState = State::MENU;
    resetMenuActivityTimer();
}

int StateManager_remote::getTargetPercentage() const {
    return _targetPercentage;
}

int StateManager_remote::getShownPercentage() const {
    return _shownPercentage;
}

void StateManager_remote::increasePercentage(int step) {
    if (_targetPercentage < 100) {
        _targetPercentage = min(100, _targetPercentage + step);
    }
}

void StateManager_remote::decreasePercentage(int step) {
    if (_targetPercentage > 0) {
        _targetPercentage = max(0, _targetPercentage - step);
    }
}

bool StateManager_remote::updateShownPercentage(int smooth_step, unsigned long interval) {
    if (_shownPercentage == _targetPercentage) {
        return false;
    }

    if (millis() - _lastAnimationTime > interval) {
        int step = (_shownPercentage < _targetPercentage) ? smooth_step : -smooth_step;
        _shownPercentage += step;

        // Prevent overshoot
        if ((step > 0 && _shownPercentage > _targetPercentage) || (step < 0 && _shownPercentage < _targetPercentage)) {
            _shownPercentage = _targetPercentage;
        }
        
        _lastAnimationTime = millis();
        return true;
    }
    return false;
}

void StateManager_remote::resetMenuActivityTimer() {
    _lastActivityTime = millis();
}

bool StateManager_remote::isMenuTimedOut(unsigned long timeout) const {
    return millis() - _lastActivityTime > timeout;
}

void StateManager_remote::setTargetPercentage(int percentage) {
    _targetPercentage = percentage;
} 